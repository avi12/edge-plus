//! Starting the panel when Windows starts, through the per-user Run key.
//!
//! The Run key itself is the one source of truth: whether the entry exists is
//! read from it, never remembered elsewhere. Task Manager's "Startup apps"
//! switch does not touch the Run value; it writes a flag under a sibling
//! `StartupApproved\Run` key, so that flag is honoured too.

use std::io;
use std::os::windows::ffi::OsStrExt;

use windows_sys::Win32::Foundation::{ERROR_FILE_NOT_FOUND, ERROR_SUCCESS};
use windows_sys::Win32::System::Registry::{
    RegCloseKey, RegCreateKeyExW, RegDeleteValueW, RegOpenKeyExW, RegQueryValueExW, RegSetValueExW,
    HKEY, HKEY_CURRENT_USER, KEY_QUERY_VALUE, KEY_SET_VALUE, REG_OPTION_NON_VOLATILE, REG_SZ,
};

// Bit 0 of the first byte of a StartupApproved value: set when Task Manager
// has turned the entry off (0x02 / 0x06 are on, 0x03 / 0x07 are off).
const APPROVED_DISABLED_BIT: u8 = 1;

/// The pair of registry keys Windows reads a startup entry from.
#[derive(Clone, Copy, Debug)]
pub struct StartupKeys<'a> {
    /// Holds the command line to run, under the entry's name.
    pub run: &'a str,
    /// Holds Task Manager's on / off flag, under the same name.
    pub approved: &'a str,
}

/// The real per-user keys; tests point at a throwaway pair instead.
pub const USER_STARTUP_KEYS: StartupKeys<'static> = StartupKeys {
    run: r"Software\Microsoft\Windows\CurrentVersion\Run",
    approved: r"Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run",
};

/// Whether Windows will run the entry at sign-in: the Run value exists and
/// Task Manager has not switched it off.
#[must_use]
pub fn is_startup_enabled(keys: StartupKeys, name: &str) -> bool {
    let has_run_value = matches!(read_value(keys.run, name), Ok(Some(_)));
    if !has_run_value {
        return false;
    }
    let approved = read_value(keys.approved, name).ok().flatten();
    !approved.is_some_and(|bytes| is_disabled_flag(&bytes))
}

/// Make Windows run `command` at sign-in, replacing an earlier entry and
/// clearing any Task Manager "disabled" flag so the switch really means on.
///
/// # Errors
/// Returns an error if the Run key cannot be written.
pub fn enable_startup(keys: StartupKeys, name: &str, command: &str) -> io::Result<()> {
    let key = open_key(keys.run, KEY_SET_VALUE, true)?;
    let name_wide = wide(name);
    let command_wide = wide(command);
    let byte_len = std::mem::size_of_val(command_wide.as_slice());
    // SAFETY: the key is live; the name is NUL-terminated and the data slice
    // and its byte length describe the same wide buffer.
    let status = unsafe {
        RegSetValueExW(
            key,
            name_wide.as_ptr(),
            0,
            REG_SZ,
            command_wide.as_ptr().cast(),
            byte_len.try_into().unwrap_or(u32::MAX),
        )
    };
    close(key);
    check(status)?;
    delete_value(keys.approved, name)
}

/// Stop Windows running the entry at sign-in.
///
/// # Errors
/// Returns an error if the Run key exists but cannot be written.
pub fn disable_startup(keys: StartupKeys, name: &str) -> io::Result<()> {
    delete_value(keys.run, name)?;
    delete_value(keys.approved, name)
}

fn is_disabled_flag(bytes: &[u8]) -> bool {
    bytes
        .first()
        .is_some_and(|first| first & APPROVED_DISABLED_BIT != 0)
}

// A value's raw bytes, or None when the key or the value does not exist.
fn read_value(subkey: &str, name: &str) -> io::Result<Option<Vec<u8>>> {
    let key = match open_key(subkey, KEY_QUERY_VALUE, false) {
        Ok(key) => key,
        Err(error) if error.kind() == io::ErrorKind::NotFound => return Ok(None),
        Err(error) => return Err(error),
    };
    let name_wide = wide(name);
    let mut size: u32 = 0;
    // SAFETY: the key is live; a null data pointer only asks for the size.
    let probe = unsafe {
        RegQueryValueExW(
            key,
            name_wide.as_ptr(),
            std::ptr::null(),
            std::ptr::null_mut(),
            std::ptr::null_mut(),
            std::ptr::addr_of_mut!(size),
        )
    };
    if probe == ERROR_FILE_NOT_FOUND {
        close(key);
        return Ok(None);
    }
    if probe != ERROR_SUCCESS {
        close(key);
        return Err(io::Error::from_raw_os_error(probe.cast_signed()));
    }
    let mut bytes = vec![0_u8; size as usize];
    // SAFETY: `bytes` is `size` bytes long, as the size pointer says.
    let status = unsafe {
        RegQueryValueExW(
            key,
            name_wide.as_ptr(),
            std::ptr::null(),
            std::ptr::null_mut(),
            bytes.as_mut_ptr(),
            std::ptr::addr_of_mut!(size),
        )
    };
    close(key);
    check(status)?;
    bytes.truncate(size as usize);
    Ok(Some(bytes))
}

// Delete one value; a missing key or value is already the state we want.
fn delete_value(subkey: &str, name: &str) -> io::Result<()> {
    let key = match open_key(subkey, KEY_SET_VALUE, false) {
        Ok(key) => key,
        Err(error) if error.kind() == io::ErrorKind::NotFound => return Ok(()),
        Err(error) => return Err(error),
    };
    let name_wide = wide(name);
    // SAFETY: the key is live and the name is NUL-terminated.
    let status = unsafe { RegDeleteValueW(key, name_wide.as_ptr()) };
    close(key);
    if status == ERROR_FILE_NOT_FOUND {
        return Ok(());
    }
    check(status)
}

fn open_key(subkey: &str, access: u32, is_created: bool) -> io::Result<HKEY> {
    let subkey_wide = wide(subkey);
    let mut key: HKEY = std::ptr::null_mut();
    // SAFETY: HKCU is predefined; the NUL-terminated subkey and out-handle
    // pointer are valid for the call, and the caller closes the handle.
    let status = unsafe {
        if is_created {
            RegCreateKeyExW(
                HKEY_CURRENT_USER,
                subkey_wide.as_ptr(),
                0,
                std::ptr::null(),
                REG_OPTION_NON_VOLATILE,
                access,
                std::ptr::null(),
                std::ptr::addr_of_mut!(key),
                std::ptr::null_mut(),
            )
        } else {
            RegOpenKeyExW(
                HKEY_CURRENT_USER,
                subkey_wide.as_ptr(),
                0,
                access,
                std::ptr::addr_of_mut!(key),
            )
        }
    };
    check(status)?;
    Ok(key)
}

fn check(status: u32) -> io::Result<()> {
    if status == ERROR_SUCCESS {
        Ok(())
    } else {
        Err(io::Error::from_raw_os_error(status.cast_signed()))
    }
}

fn close(key: HKEY) {
    // SAFETY: a key opened above is closed exactly once.
    unsafe {
        RegCloseKey(key);
    }
}

fn wide(text: &str) -> Vec<u16> {
    std::ffi::OsStr::new(text)
        .encode_wide()
        .chain([0])
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use windows_sys::Win32::System::Registry::RegDeleteTreeW;

    const NAME: &str = "Edge+ test";

    fn keys(id: &str) -> (String, String) {
        (
            format!(r"Software\FixEdgeTest\{id}\Run"),
            format!(r"Software\FixEdgeTest\{id}\Approved"),
        )
    }

    fn cleanup(id: &str) {
        let root = wide(&format!(r"Software\FixEdgeTest\{id}"));
        // SAFETY: HKCU is predefined; the subkey is NUL-terminated.
        unsafe {
            RegDeleteTreeW(HKEY_CURRENT_USER, root.as_ptr());
        }
    }

    fn set_approved(subkey: &str, flag: u8) {
        let key = open_key(subkey, KEY_SET_VALUE, true).unwrap();
        let name = wide(NAME);
        let mut data = [0_u8; 12];
        data[0] = flag;
        // SAFETY: live key, NUL-terminated name, 12-byte buffer.
        unsafe {
            RegSetValueExW(
                key,
                name.as_ptr(),
                0,
                windows_sys::Win32::System::Registry::REG_BINARY,
                data.as_ptr(),
                12,
            );
        }
        close(key);
    }

    #[test]
    fn disabled_flag_is_bit_zero_of_first_byte() {
        assert!(is_disabled_flag(&[0x03, 0, 0]));
        assert!(is_disabled_flag(&[0x07]));
        assert!(!is_disabled_flag(&[0x02, 0, 0]));
        assert!(!is_disabled_flag(&[0x06]));
        assert!(!is_disabled_flag(&[]));
    }

    #[test]
    fn enable_then_disable_round_trips() {
        let (run, approved) = keys("roundtrip");
        let keys = StartupKeys {
            run: &run,
            approved: &approved,
        };
        cleanup("roundtrip");
        assert!(!is_startup_enabled(keys, NAME));
        enable_startup(keys, NAME, r#""C:\a b\x.exe" --background"#).unwrap();
        assert!(is_startup_enabled(keys, NAME));
        disable_startup(keys, NAME).unwrap();
        assert!(!is_startup_enabled(keys, NAME));
        // Disabling what is not there is not an error.
        disable_startup(keys, NAME).unwrap();
        cleanup("roundtrip");
    }

    #[test]
    fn task_manager_flag_shows_off_and_enabling_clears_it() {
        let (run, approved) = keys("approved");
        let keys = StartupKeys {
            run: &run,
            approved: &approved,
        };
        cleanup("approved");
        enable_startup(keys, NAME, "x.exe").unwrap();
        set_approved(&approved, 0x03);
        assert!(!is_startup_enabled(keys, NAME));
        enable_startup(keys, NAME, "x.exe").unwrap();
        assert!(is_startup_enabled(keys, NAME));
        cleanup("approved");
    }
}
