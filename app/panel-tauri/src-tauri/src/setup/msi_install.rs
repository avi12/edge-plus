//! A copy the MSI installed (app/installer) lives under Program Files, where it
//! cannot swap its own exe, so it updates by running the newer release's MSI
//! once the core has exited. Package.wxs writes the key read here and names the
//! shortcuts after the product.

use std::fmt::Write as _;
use std::path::{Path, PathBuf};

use edge_native::Hive;

use crate::platform::paths;

const REGISTRY_KEY_PATH: &str = r"Software\EdgePlus";
const INSTALL_DIR_VALUE: &str = "InstallDir";
const SHORTCUT_EXTENSION: &str = ".lnk";
/// The installer's properties that keep or drop each shortcut.
const DESKTOP_SHORTCUT_PROPERTY: &str = "DESKTOPSHORTCUT";
const START_MENU_SHORTCUT_PROPERTY: &str = "STARTMENUSHORTCUT";
const POWERSHELL_COMMAND: &str =
    "powershell.exe -NoProfile -NonInteractive -WindowStyle Hidden -EncodedCommand";
const SEPARATORS: [char; 2] = ['\\', '/'];

/// True when the installer recorded the folder this exe runs from.
#[must_use]
pub fn is_installed_at(executable: &Path) -> bool {
    let recorded = edge_native::read_string(
        Hive::LocalMachine,
        REGISTRY_KEY_PATH,
        Some(INSTALL_DIR_VALUE),
    );
    recorded.is_some_and(|folder| is_same_folder(Path::new(&folder), executable.parent()))
}

/// Ignores case and a trailing separator, as the installer may write one.
#[must_use]
pub fn is_same_folder(folder: &Path, other: Option<&Path>) -> bool {
    let Some(other) = other else {
        return false;
    };
    let (Some(folder), Some(other)) = (comparable(folder), comparable(other)) else {
        return false;
    };
    folder == other
}

/// The full path, folded, without a trailing separator unless it is a root.
fn comparable(folder: &Path) -> Option<String> {
    let full = std::path::absolute(folder).ok()?;
    let text = full.to_string_lossy();
    let is_root = full.parent().is_none();
    let trimmed = if is_root {
        &text[..]
    } else {
        text.trim_end_matches(SEPARATORS)
    };
    Some(paths::fold_case(trimmed))
}

/// Where the installer puts a shortcut in a desktop or Start menu folder.
#[must_use]
pub fn shortcut_file(folder: &Path, product_name: &str) -> PathBuf {
    folder.join(format!("{product_name}{SHORTCUT_EXTENSION}"))
}

/// Unattended with a progress bar (Windows still asks to allow it), and a
/// shortcut the user removed stays removed: the installer would otherwise put
/// both back.
#[must_use]
pub fn installer_arguments(
    msi: &Path,
    has_desktop_shortcut: bool,
    has_start_menu_shortcut: bool,
) -> String {
    let mut arguments = format!("/i \"{}\" /passive /norestart", msi.display());
    if !has_desktop_shortcut {
        let _ = write!(arguments, " {DESKTOP_SHORTCUT_PROPERTY}=\"\"");
    }
    if !has_start_menu_shortcut {
        let _ = write!(arguments, " {START_MENU_SHORTCUT_PROPERTY}=\"\"");
    }
    arguments
}

/// Waits for the core to exit, so the installer finds its exe free, then
/// starts the exe again: the new one, or the old one if the installer was
/// declined.
#[must_use]
pub fn update_script(core_process_id: u32, installer_arguments: &str, executable: &Path) -> String {
    format!(
        "Wait-Process -Id {core_process_id} -ErrorAction SilentlyContinue\n\
         Start-Process msiexec.exe -ArgumentList {} -Wait\n\
         Start-Process {}\n",
        quoted(installer_arguments),
        quoted(&executable.display().to_string())
    )
}

/// A hidden PowerShell running the script, encoded so no path in it needs
/// quoting on the command line.
#[must_use]
pub fn hidden_powershell(script: &str) -> String {
    let utf16: Vec<u8> = script.encode_utf16().flat_map(u16::to_le_bytes).collect();
    format!("{POWERSHELL_COMMAND} {}", base64(&utf16))
}

/// A PowerShell single-quoted string, where only a quote needs escaping.
fn quoted(text: &str) -> String {
    format!("'{}'", text.replace('\'', "''"))
}

const BASE64_ALPHABET: &[u8; 64] =
    b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const BASE64_PADDING: char = '=';
const BASE64_GROUP_BYTES: usize = 3;
const BASE64_SEXTET_MASK: u32 = 0b11_1111;

/// Standard base64 with padding, as `-EncodedCommand` reads it; the only use, so
/// no crate for it.
fn base64(bytes: &[u8]) -> String {
    let mut encoded = String::with_capacity(bytes.len().div_ceil(BASE64_GROUP_BYTES) * 4);
    for group in bytes.chunks(BASE64_GROUP_BYTES) {
        let mut padded = [0_u8; BASE64_GROUP_BYTES];
        padded[..group.len()].copy_from_slice(group);
        // Three bytes make 24 bits, read out as four six-bit letters.
        let bits =
            (u32::from(padded[0]) << 16) | (u32::from(padded[1]) << 8) | u32::from(padded[2]);
        let sextets = [18, 12, 6, 0].map(|shift| (bits >> shift) & BASE64_SEXTET_MASK);
        // A short last group carries one letter more than its bytes.
        let letter_count = group.len() + 1;
        for (index, sextet) in sextets.into_iter().enumerate() {
            let is_letter = index < letter_count;
            if is_letter {
                let position = usize::try_from(sextet).expect("a sextet fits usize");
                encoded.push(char::from(BASE64_ALPHABET[position]));
            } else {
                encoded.push(BASE64_PADDING);
            }
        }
    }
    encoded
}

#[cfg(test)]
mod tests {
    use super::*;

    const MSI: &str = r"C:\Users\A\AppData\Local\EdgePlus\update\EdgePlus-0.0.2.msi";

    fn decode_base64(text: &str) -> Vec<u8> {
        let mut bits = 0_u32;
        let mut bit_count = 0;
        let mut bytes = Vec::new();
        for character in text.bytes().filter(|&byte| byte != b'=') {
            let value = BASE64_ALPHABET
                .iter()
                .position(|&letter| letter == character)
                .expect("a base64 letter");
            bits = (bits << 6) | u32::try_from(value).expect("fits");
            bit_count += 6;
            if bit_count >= 8 {
                bit_count -= 8;
                bytes.push(u8::try_from((bits >> bit_count) & 0xFF).expect("a byte"));
            }
        }
        bytes
    }

    #[test]
    fn matches_the_recorded_folder() {
        assert!(
            is_same_folder(
                Path::new(r"C:\Program Files\Edge+\"),
                Some(Path::new(r"C:\Program Files\Edge+"))
            ),
            "the installer's trailing separator does not matter"
        );
        assert!(
            is_same_folder(
                Path::new(r"C:\Program Files\Edge+"),
                Some(Path::new(r"c:\program files\edge+"))
            ),
            "nor does case"
        );
        assert!(
            !is_same_folder(
                Path::new(r"C:\Program Files\Edge+"),
                Some(Path::new(r"C:\Tools\Edge+"))
            ),
            "a portable copy elsewhere is not the installed one"
        );
        assert!(
            !is_same_folder(Path::new(r"C:\Program Files\Edge+"), None),
            "an exe with no folder is not either"
        );
        assert_eq!(
            PathBuf::from(r"C:\Users\Public\Desktop\Edge+.lnk"),
            shortcut_file(Path::new(r"C:\Users\Public\Desktop"), "Edge+"),
            "a shortcut is named after the product"
        );
    }

    #[test]
    fn keeps_the_shortcuts_as_they_are() {
        let msi = Path::new(MSI);
        assert_eq!(
            format!("/i \"{MSI}\" /passive /norestart"),
            installer_arguments(msi, true, true),
            "both shortcuts present: the installer's defaults keep them"
        );
        assert_eq!(
            format!("/i \"{MSI}\" /passive /norestart DESKTOPSHORTCUT=\"\""),
            installer_arguments(msi, false, true),
            "a removed desktop shortcut stays removed"
        );
        assert_eq!(
            format!("/i \"{MSI}\" /passive /norestart DESKTOPSHORTCUT=\"\" STARTMENUSHORTCUT=\"\""),
            installer_arguments(msi, false, false),
            "and so does a removed Start menu one"
        );
    }

    #[test]
    fn waits_then_installs_then_restarts() {
        let script = update_script(42, "/i \"x\" /passive", Path::new(r"C:\it's\edgeplus.exe"));
        assert_eq!(
            "Wait-Process -Id 42 -ErrorAction SilentlyContinue\n\
             Start-Process msiexec.exe -ArgumentList '/i \"x\" /passive' -Wait\n\
             Start-Process 'C:\\it''s\\edgeplus.exe'\n",
            script,
            "waits for the core, installs, then starts the exe, quotes escaped"
        );
        let command = hidden_powershell(&script);
        let encoded = command.rsplit(' ').next().expect("the encoded script");
        let utf16: Vec<u16> = decode_base64(encoded)
            .chunks(2)
            .map(|pair| u16::from_le_bytes([pair[0], pair[1]]))
            .collect();
        assert_eq!(
            script,
            String::from_utf16(&utf16).expect("UTF-16"),
            "the hidden PowerShell carries the script encoded"
        );
        assert!(
            command.starts_with("powershell.exe ") && command.contains("-WindowStyle Hidden"),
            "in a hidden window"
        );
    }

    #[test]
    fn encodes_base64_with_padding() {
        assert_eq!("", base64(b""));
        assert_eq!("Zg==", base64(b"f"));
        assert_eq!("Zm8=", base64(b"fo"));
        assert_eq!("Zm9v", base64(b"foo"));
        assert_eq!("Zm9vYmFy", base64(b"foobar"));
    }
}
