//! The DLL's config channel.
//!
//! The panel records the desired state to a small binary file and signals a
//! named event; the loaded DLL re-reads the file and applies it live. The byte
//! layout mirrors `struct Wire` in `app/native-split/src/config.h` exactly, and
//! the event name mirrors `kConfigEvent` in `app/native-split/src/dllmain.cpp`.

use std::io;
use std::path::Path;

use windows_sys::Win32::Foundation::CloseHandle;
use windows_sys::Win32::System::Threading::{OpenEventW, SetEvent, EVENT_MODIFY_STATE};

// Must match config::kIdCapacity in app/native-split/src/config.h.
const ID_CAPACITY: usize = 64;
const WIRE_SIZE: usize = 4 * 4 + 2 * ID_CAPACITY;
// Must match kConfigEvent in app/native-split/src/dllmain.cpp.
const CONFIG_EVENT: &str = r"Local\fixedge-config-changed";

/// Write the desired state to the config file the DLL reads (atomically, so a
/// read never sees half a file). `light_id` / `dark_id` are extension ids (32
/// characters) or empty for "use the default theme".
///
/// # Errors
/// Returns any filesystem error from writing or swapping in the file.
pub fn write_config(
    path: &Path,
    split_active: bool,
    theming_active: bool,
    light_id: &str,
    dark_id: &str,
) -> io::Result<()> {
    let mut wire = [0u8; WIRE_SIZE];
    wire[0..4].copy_from_slice(&u32::from(split_active).to_le_bytes());
    wire[4..8].copy_from_slice(&u32::from(theming_active).to_le_bytes());
    write_id(&mut wire, 8, 16, light_id);
    write_id(&mut wire, 12, 16 + ID_CAPACITY, dark_id);

    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)?;
    }
    let temporary = path.with_extension("bin.tmp");
    std::fs::write(&temporary, wire)?;
    std::fs::rename(&temporary, path)
}

fn write_id(wire: &mut [u8], length_offset: usize, id_offset: usize, id: &str) {
    let bytes = id.as_bytes();
    let length = bytes.len().min(ID_CAPACITY);
    let length_u32 = u32::try_from(length).unwrap_or(0);
    wire[length_offset..length_offset + 4].copy_from_slice(&length_u32.to_le_bytes());
    wire[id_offset..id_offset + length].copy_from_slice(&bytes[..length]);
}

/// Signal a loaded DLL to re-read the config file and apply it live (a no-op if
/// no DLL is loaded to receive it).
pub fn signal_config_changed() {
    let name: Vec<u16> = CONFIG_EVENT.encode_utf16().chain([0]).collect();
    // SAFETY: a null handle is handled; a valid one is set then closed once.
    unsafe {
        let handle = OpenEventW(EVENT_MODIFY_STATE, 0, name.as_ptr());
        if handle.is_null() {
            return;
        }
        SetEvent(handle);
        CloseHandle(handle);
    }
}
