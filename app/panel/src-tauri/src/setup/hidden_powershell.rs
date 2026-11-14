//! Work that must outlive this process (restarting into an update, a Start menu
//! shortcut) is handed to a hidden PowerShell, whose script is encoded so no
//! path in it needs quoting on the command line.

const POWERSHELL_COMMAND: &str =
    "powershell.exe -NoProfile -NonInteractive -WindowStyle Hidden -EncodedCommand";

/// A hidden PowerShell running the script, encoded so no path in it needs
/// quoting on the command line.
#[must_use]
pub(crate) fn hidden_powershell(script: &str) -> String {
    let utf16: Vec<u8> = script.encode_utf16().flat_map(u16::to_le_bytes).collect();
    format!("{POWERSHELL_COMMAND} {}", base64(&utf16))
}

/// A PowerShell single-quoted string, where only a quote needs escaping.
#[must_use]
pub(crate) fn quoted(text: &str) -> String {
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
        for (slot, byte) in padded.iter_mut().zip(group) {
            *slot = *byte;
        }
        // Three bytes make 24 bits, read out as four six-bit letters.
        let bits = padded
            .iter()
            .fold(0_u32, |bits, &byte| (bits << 8) | u32::from(byte));
        let sextets = [18, 12, 6, 0].map(|shift| (bits >> shift) & BASE64_SEXTET_MASK);
        // A short last group carries one letter more than its bytes.
        let letter_count = group.len() + 1;
        for (index, sextet) in sextets.into_iter().enumerate() {
            let is_letter = index < letter_count;
            let letter = usize::try_from(sextet)
                .ok()
                .and_then(|position| BASE64_ALPHABET.get(position))
                .filter(|_| is_letter);
            encoded.push(letter.map_or(BASE64_PADDING, |&byte| char::from(byte)));
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
    fn carries_the_script_encoded_in_a_hidden_window() {
        let script = r"Wait-Process -Id 42
Start-Process 'C:\it''s'
";
        let command = hidden_powershell(script);
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
        assert_eq!("'it''s'", quoted("it's"), "a quote is doubled");
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
