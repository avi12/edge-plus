//! Edge+ as an MSIX package: whether this run is the package's, and how it
//! updates itself. A package's folder is read-only, so an update is the newer
//! package installed over it by Windows once the core has exited.

use std::path::Path;

/// The `Application` the manifest (app/package/AppxManifest.xml) declares.
const APPLICATION_ID: &str = "App";
const POWERSHELL_COMMAND: &str =
    "powershell.exe -NoProfile -NonInteractive -WindowStyle Hidden -EncodedCommand";

/// True when the exe runs from the package, not from a checkout or a copy.
#[must_use]
pub fn is_packaged() -> bool {
    edge_native::package_family_name().is_some()
}

/// What starts the package's app: its family name and the application's id.
#[must_use]
pub fn app_user_model_id() -> Option<String> {
    edge_native::package_family_name().map(|family| format!("{family}!{APPLICATION_ID}"))
}

/// Waits for the core to exit, so the package is free to be replaced, installs
/// the newer one over it, then starts the app again: the new one, or the old
/// one if the install was declined.
#[must_use]
pub fn update_script(core_process_id: u32, package: &Path, app_user_model_id: &str) -> String {
    format!(
        "Wait-Process -Id {core_process_id} -ErrorAction SilentlyContinue\n\
         Add-AppxPackage -Path {} -ForceUpdateFromAnyVersion\n\
         Start-Process {}\n",
        quoted(&package.display().to_string()),
        quoted(&format!(r"shell:AppsFolder\{app_user_model_id}"))
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
    fn waits_then_installs_then_restarts() {
        let script = update_script(
            42,
            Path::new(r"C:\it's\EdgePlus-0.2.0.msix"),
            "avi12.EdgePlus_abc!App",
        );
        assert_eq!(
            "Wait-Process -Id 42 -ErrorAction SilentlyContinue\n\
             Add-AppxPackage -Path 'C:\\it''s\\EdgePlus-0.2.0.msix' -ForceUpdateFromAnyVersion\n\
             Start-Process 'shell:AppsFolder\\avi12.EdgePlus_abc!App'\n",
            script,
            "waits for the core, installs the package, then starts the app, quotes escaped"
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
