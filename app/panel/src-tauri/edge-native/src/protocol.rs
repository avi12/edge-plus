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
// struct Wire's fields, in order: five u32s, then the two ids.
const SPLIT_ACTIVE_OFFSET: usize = 0;
const THEMING_ACTIVE_OFFSET: usize = 4;
const PROTOCOL_HANDLERS_ACTIVE_OFFSET: usize = 8;
const LIGHT_LENGTH_OFFSET: usize = 12;
const DARK_LENGTH_OFFSET: usize = 16;
const LIGHT_ID_OFFSET: usize = 20;
const DARK_ID_OFFSET: usize = LIGHT_ID_OFFSET + ID_CAPACITY;
const WIRE_SIZE: usize = DARK_ID_OFFSET + ID_CAPACITY;
// Must match kConfigEvent in app/native-split/src/dllmain.cpp.
const CONFIG_EVENT: &str = r"Local\fixedge-config-changed";

/// What the DLL should do: which of its features are on, and the theme for
/// each Windows mode (an extension id, or empty for the default theme).
pub struct DllConfig<'a> {
    pub split_active: bool,
    pub theming_active: bool,
    /// Ask to handle links like Chrome (`protocol_handlers.h`).
    pub protocol_handlers_active: bool,
    pub light_id: &'a str,
    pub dark_id: &'a str,
}

/// Write the desired state to the config file the DLL reads (atomically, so a
/// read never sees half a file).
///
/// # Errors
/// Returns any filesystem error from writing or swapping in the file.
pub fn write_config(path: &Path, config: &DllConfig) -> io::Result<()> {
    let wire = encode(config);
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)?;
    }
    let temporary = path.with_extension("bin.tmp");
    std::fs::write(&temporary, wire)?;
    std::fs::rename(&temporary, path)
}

fn encode(config: &DllConfig) -> [u8; WIRE_SIZE] {
    let mut wire = [0u8; WIRE_SIZE];
    write_flag(&mut wire, SPLIT_ACTIVE_OFFSET, config.split_active);
    write_flag(&mut wire, THEMING_ACTIVE_OFFSET, config.theming_active);
    write_flag(
        &mut wire,
        PROTOCOL_HANDLERS_ACTIVE_OFFSET,
        config.protocol_handlers_active,
    );
    write_id(
        &mut wire,
        LIGHT_LENGTH_OFFSET,
        LIGHT_ID_OFFSET,
        config.light_id,
    );
    write_id(
        &mut wire,
        DARK_LENGTH_OFFSET,
        DARK_ID_OFFSET,
        config.dark_id,
    );
    wire
}

fn write_flag(wire: &mut [u8], offset: usize, is_on: bool) {
    wire[offset..offset + 4].copy_from_slice(&u32::from(is_on).to_le_bytes());
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

#[cfg(test)]
mod tests {
    use super::*;

    fn u32_at(wire: &[u8], offset: usize) -> u32 {
        u32::from_le_bytes(wire[offset..offset + 4].try_into().expect("4 bytes"))
    }

    #[test]
    fn the_wire_is_config_h_s_148_bytes() {
        // config.h static_asserts the same size: 5 u32 fields + 2 ids of 64.
        assert_eq!(WIRE_SIZE, 148);
    }

    #[test]
    fn each_field_lands_where_config_h_reads_it() {
        let wire = encode(&DllConfig {
            split_active: false,
            theming_active: true,
            protocol_handlers_active: true,
            light_id: "abc",
            dark_id: "",
        });
        assert_eq!(u32_at(&wire, 0), 0);
        assert_eq!(u32_at(&wire, 4), 1);
        assert_eq!(u32_at(&wire, 8), 1);
        assert_eq!(u32_at(&wire, 12), 3);
        assert_eq!(u32_at(&wire, 16), 0);
        assert_eq!(&wire[20..23], b"abc");
    }
}
