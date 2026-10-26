//! The registry values Edge+ reads and writes, under HKCU or HKLM.

use std::ffi::c_void;
use std::io;

use windows_sys::Win32::Foundation::{
    ERROR_FILE_NOT_FOUND, ERROR_MORE_DATA, ERROR_SUCCESS, WIN32_ERROR,
};
use windows_sys::Win32::System::Registry::{
    HKEY, HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, KEY_SET_VALUE, REG_OPTION_NON_VOLATILE,
    REG_ROUTINE_FLAGS, REG_SAM_FLAGS, REG_SZ, RRF_RT_REG_BINARY, RRF_RT_REG_SZ, RegCloseKey,
    RegCreateKeyExW, RegDeleteTreeW, RegDeleteValueW, RegGetValueW, RegOpenKeyExW, RegSetValueExW,
};

use crate::kernel::wide::{from_wide, to_wide};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Hive {
    CurrentUser,
    LocalMachine,
}

impl Hive {
    fn root(self) -> HKEY {
        match self {
            Self::CurrentUser => HKEY_CURRENT_USER,
            Self::LocalMachine => HKEY_LOCAL_MACHINE,
        }
    }
}

/// `value` None is the key's default value. None when the key or value is
/// missing or not a string.
#[must_use]
pub fn read_string(hive: Hive, key: &str, value: Option<&str>) -> Option<String> {
    // RRF_RT_REG_SZ also takes a REG_EXPAND_SZ, expanded, as .NET's GetValue
    // does, and always NUL-terminates.
    let data = read_value(hive, key, value, RRF_RT_REG_SZ)?;
    let (pairs, _) = data.as_chunks::<{ size_of::<u16>() }>();
    let units: Vec<u16> = pairs.iter().map(|&pair| u16::from_le_bytes(pair)).collect();
    Some(from_wide(&units).to_string_lossy().into_owned())
}

/// None when the key or value is missing or not `REG_BINARY`.
#[must_use]
pub fn read_binary(hive: Hive, key: &str, value: &str) -> Option<Vec<u8>> {
    read_value(hive, key, Some(value), RRF_RT_REG_BINARY)
}

/// Creates the key if it is missing; writes `REG_SZ`.
pub fn write_string(hive: Hive, key: &str, value: &str, data: &str) -> io::Result<()> {
    let key = OpenKey::create(hive, key)?;
    let value = to_wide(value);
    let data = to_wide(data);
    let byte_length = u32::try_from(size_of_val(data.as_slice()))
        .map_err(|_| io::Error::from(io::ErrorKind::InvalidInput))?;
    // SAFETY: the key is open with KEY_SET_VALUE; the name is NUL-terminated
    // and the data pointer and byte length describe the same live buffer,
    // terminator included, as REG_SZ wants.
    let status = unsafe {
        RegSetValueExW(
            key.0,
            value.as_ptr(),
            0,
            REG_SZ,
            data.as_ptr().cast(),
            byte_length,
        )
    };
    to_result(status)
}

/// A missing key or value is already the state wanted.
pub fn delete_value(hive: Hive, key: &str, value: &str) -> io::Result<()> {
    let key = match OpenKey::open(hive, key, KEY_SET_VALUE) {
        Ok(key) => key,
        Err(error) if is_missing(&error) => return Ok(()),
        Err(error) => return Err(error),
    };
    let value = to_wide(value);
    // SAFETY: the key is open with KEY_SET_VALUE; the name is NUL-terminated.
    let status = unsafe { RegDeleteValueW(key.0, value.as_ptr()) };
    ignoring_missing(status)
}

/// A missing key is already the state wanted.
pub fn delete_tree(hive: Hive, key: &str) -> io::Result<()> {
    let key = to_wide(key);
    // SAFETY: the hive is a predefined key; the subkey is NUL-terminated.
    let status = unsafe { RegDeleteTreeW(hive.root(), key.as_ptr()) };
    ignoring_missing(status)
}

fn read_value(
    hive: Hive,
    key: &str,
    value: Option<&str>,
    wanted_type: REG_ROUTINE_FLAGS,
) -> Option<Vec<u8>> {
    let key = to_wide(key);
    let value = value.map(to_wide);
    let value_pointer = value.as_ref().map_or(std::ptr::null(), Vec::as_ptr);
    let mut byte_length = 0u32;
    // The value can grow between asking its size and reading it.
    loop {
        let mut data = vec![0u8; byte_length as usize];
        let data_pointer: *mut c_void = if data.is_empty() {
            std::ptr::null_mut()
        } else {
            data.as_mut_ptr().cast()
        };
        // SAFETY: the hive is predefined; key and value names are
        // NUL-terminated (or null for the default value) and live across the
        // call; data is null to ask the size, or byte_length bytes long.
        let status = unsafe {
            RegGetValueW(
                hive.root(),
                key.as_ptr(),
                value_pointer,
                wanted_type,
                std::ptr::null_mut(),
                data_pointer,
                &raw mut byte_length,
            )
        };
        let is_size_only = status == ERROR_SUCCESS && data.is_empty() && byte_length != 0;
        let should_read_again = status == ERROR_MORE_DATA || is_size_only;
        if should_read_again {
            continue;
        }
        if status != ERROR_SUCCESS {
            return None;
        }
        data.truncate(byte_length as usize);
        return Some(data);
    }
}

/// A key handle closed on drop.
struct OpenKey(HKEY);

impl OpenKey {
    fn open(hive: Hive, key: &str, access: REG_SAM_FLAGS) -> io::Result<Self> {
        let key = to_wide(key);
        let mut handle: HKEY = std::ptr::null_mut();
        // SAFETY: the hive is predefined; the subkey is NUL-terminated and the
        // out-handle is a live local.
        let status =
            unsafe { RegOpenKeyExW(hive.root(), key.as_ptr(), 0, access, &raw mut handle) };
        to_result(status).map(|()| Self(handle))
    }

    fn create(hive: Hive, key: &str) -> io::Result<Self> {
        let key = to_wide(key);
        let mut handle: HKEY = std::ptr::null_mut();
        // SAFETY: the hive is predefined; the subkey is NUL-terminated and the
        // out-handle is a live local; class, security and disposition are
        // optional.
        let status = unsafe {
            RegCreateKeyExW(
                hive.root(),
                key.as_ptr(),
                0,
                std::ptr::null(),
                REG_OPTION_NON_VOLATILE,
                KEY_SET_VALUE,
                std::ptr::null(),
                &raw mut handle,
                std::ptr::null_mut(),
            )
        };
        to_result(status).map(|()| Self(handle))
    }
}

impl Drop for OpenKey {
    fn drop(&mut self) {
        // SAFETY: the key was opened by open or create and is closed once.
        unsafe {
            RegCloseKey(self.0);
        }
    }
}

fn to_result(status: WIN32_ERROR) -> io::Result<()> {
    if status == ERROR_SUCCESS {
        return Ok(());
    }
    Err(io::Error::from_raw_os_error(status.cast_signed()))
}

fn ignoring_missing(status: WIN32_ERROR) -> io::Result<()> {
    if status == ERROR_FILE_NOT_FOUND {
        return Ok(());
    }
    to_result(status)
}

fn is_missing(error: &io::Error) -> bool {
    error.raw_os_error() == Some(ERROR_FILE_NOT_FOUND.cast_signed())
}

#[cfg(test)]
mod tests {
    use super::*;

    const VALUE: &str = "Greeting";

    /// A throwaway HKCU key, deleted when the test ends.
    struct TestKey(String);

    impl TestKey {
        fn new(purpose: &str) -> Self {
            Self(format!(
                r"Software\EdgePlusTests\{}-{purpose}",
                std::process::id()
            ))
        }
    }

    impl Drop for TestKey {
        fn drop(&mut self) {
            let _ = delete_tree(Hive::CurrentUser, &self.0);
        }
    }

    #[test]
    fn round_trips_a_string() {
        let key = TestKey::new("string");
        assert_eq!(read_string(Hive::CurrentUser, &key.0, Some(VALUE)), None);
        write_string(Hive::CurrentUser, &key.0, VALUE, "hello, wörld").expect("written");
        let read = read_string(Hive::CurrentUser, &key.0, Some(VALUE));
        assert_eq!(read.as_deref(), Some("hello, wörld"));
        assert_eq!(read_binary(Hive::CurrentUser, &key.0, VALUE), None);
    }

    #[test]
    fn reads_the_default_value() {
        let key = TestKey::new("default");
        write_string(Hive::CurrentUser, &key.0, "", "the default").expect("written");
        let read = read_string(Hive::CurrentUser, &key.0, None);
        assert_eq!(read.as_deref(), Some("the default"));
    }

    #[test]
    fn deletes_values_and_trees_even_when_missing() {
        let key = TestKey::new("delete");
        delete_value(Hive::CurrentUser, &key.0, VALUE).expect("a missing key is fine");
        write_string(Hive::CurrentUser, &key.0, VALUE, "x").expect("written");
        delete_value(Hive::CurrentUser, &key.0, VALUE).expect("deleted");
        delete_value(Hive::CurrentUser, &key.0, VALUE).expect("a missing value is fine");
        assert_eq!(read_string(Hive::CurrentUser, &key.0, Some(VALUE)), None);
        delete_tree(Hive::CurrentUser, &key.0).expect("deleted");
        delete_tree(Hive::CurrentUser, &key.0).expect("a missing key is fine");
    }

    #[test]
    fn reads_a_machine_string() {
        let product = read_string(
            Hive::LocalMachine,
            r"SOFTWARE\Microsoft\Windows NT\CurrentVersion",
            Some("ProductName"),
        );
        assert!(product.is_some_and(|name| name.contains("Windows")));
    }
}
