//! Adding a theme through Edge's external-extension loader.
//!
//! A theme is added without touching Edge's ABI: we write an entry under Edge's
//! own external-extensions key and Edge's loader installs it. A store theme
//! points at the store's update URL (Edge installs it as a trusted theme, no
//! developer mode); a local theme points at a `.crx` on disk. Mirrors the
//! registry half of the old `ThemeInstaller.cs`.

use std::io;
use std::os::windows::ffi::OsStrExt;
use std::path::Path;

use windows_sys::Win32::Foundation::ERROR_SUCCESS;
use windows_sys::Win32::System::Registry::{
    RegCloseKey, RegCreateKeyExW, RegDeleteTreeW, RegSetValueExW, HKEY, HKEY_CURRENT_USER,
    KEY_WRITE, REG_OPTION_NON_VOLATILE, REG_SZ,
};

// Edge reads externally-added extensions from here (HKCU, per-user).
const EXTENSIONS_KEY: &str = r"Software\Microsoft\Edge\Extensions";
const CHROME_STORE_UPDATE_URL: &str = "https://clients2.google.com/service/update2/crx";
const EDGE_STORE_UPDATE_URL: &str = "https://edge.microsoft.com/extensionwebstorebase/v1/crx";

/// Which store a theme came from, so the matching update URL is used.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Store {
    Chrome,
    Edge,
}

/// Register a store theme by its extension id: Edge installs it from the store
/// as a trusted theme.
///
/// # Errors
/// Returns an error if the registry key cannot be created or written.
pub fn install_store(extension_id: &str, store: Store) -> io::Result<()> {
    let key = create_extension_key(extension_id)?;
    let url = match store {
        Store::Chrome => CHROME_STORE_UPDATE_URL,
        Store::Edge => EDGE_STORE_UPDATE_URL,
    };
    let result = set_string(key, "update_url", url);
    close(key);
    result
}

/// Register a local theme by the path to its `.crx` and the version from its
/// manifest, so Edge's loader installs it from disk.
///
/// # Errors
/// Returns an error if the registry key cannot be created or written.
pub fn install_local(extension_id: &str, crx_path: &Path, version: &str) -> io::Result<()> {
    let key = create_extension_key(extension_id)?;
    let path_text = crx_path.to_string_lossy();
    let result =
        set_string(key, "path", &path_text).and_then(|()| set_string(key, "version", version));
    close(key);
    result
}

/// Drop a theme's external-extensions entry (best effort); Edge removes the
/// extension on its next start.
pub fn uninstall(extension_id: &str) {
    let subkey = format!(r"{EXTENSIONS_KEY}\{extension_id}");
    let wide = wide(&subkey);
    // SAFETY: HKCU is a predefined key; a missing subkey returns an error we drop.
    unsafe {
        RegDeleteTreeW(HKEY_CURRENT_USER, wide.as_ptr());
    }
}

fn create_extension_key(extension_id: &str) -> io::Result<HKEY> {
    let subkey = format!(r"{EXTENSIONS_KEY}\{extension_id}");
    let wide = wide(&subkey);
    let mut key: HKEY = std::ptr::null_mut();
    // SAFETY: HKCU is predefined; the NUL-terminated subkey and out-handle
    // pointer are valid for the call, and the handle is closed by the caller.
    let status = unsafe {
        RegCreateKeyExW(
            HKEY_CURRENT_USER,
            wide.as_ptr(),
            0,
            std::ptr::null(),
            REG_OPTION_NON_VOLATILE,
            KEY_WRITE,
            std::ptr::null(),
            std::ptr::addr_of_mut!(key),
            std::ptr::null_mut(),
        )
    };
    if status == ERROR_SUCCESS {
        Ok(key)
    } else {
        Err(io::Error::from_raw_os_error(status.cast_signed()))
    }
}

fn set_string(key: HKEY, name: &str, value: &str) -> io::Result<()> {
    let name_wide = wide(name);
    let value_wide = wide(value);
    let byte_len = std::mem::size_of_val(value_wide.as_slice());
    // SAFETY: the key is live; the name is NUL-terminated and the data slice
    // and its byte length describe the same wide buffer.
    let status = unsafe {
        RegSetValueExW(
            key,
            name_wide.as_ptr(),
            0,
            REG_SZ,
            value_wide.as_ptr().cast(),
            byte_len.try_into().unwrap_or(u32::MAX),
        )
    };
    if status == ERROR_SUCCESS {
        Ok(())
    } else {
        Err(io::Error::from_raw_os_error(status.cast_signed()))
    }
}

fn close(key: HKEY) {
    // SAFETY: a key opened by RegCreateKeyExW is closed exactly once.
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
