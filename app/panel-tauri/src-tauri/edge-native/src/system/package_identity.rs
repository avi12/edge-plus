//! The identity Windows gives the process when Edge+ runs from its MSIX package.

use windows_sys::Win32::Foundation::ERROR_SUCCESS;
use windows_sys::Win32::Storage::Packaging::Appx::GetCurrentPackageFamilyName;

use crate::kernel::wide::from_wide;

/// `PACKAGE_FAMILY_NAME_MAX_LENGTH` (64) and its NUL.
const FAMILY_NAME_CAPACITY: u32 = 65;

/// The package family name, or None when the exe runs outside a package.
#[must_use]
pub fn package_family_name() -> Option<String> {
    let mut length = FAMILY_NAME_CAPACITY;
    let mut buffer = [0_u16; FAMILY_NAME_CAPACITY as usize];
    // SAFETY: `length` is the capacity of `buffer`, which outlives the call.
    let status = unsafe { GetCurrentPackageFamilyName(&raw mut length, buffer.as_mut_ptr()) };
    let is_packaged = status == ERROR_SUCCESS;
    is_packaged.then(|| from_wide(&buffer).to_string_lossy().into_owned())
}
