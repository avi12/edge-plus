//! 32 characters of a..p: the first 16 bytes of the SHA-256 of the extension's
//! public key, each nibble spelled as a letter.

use sha2::{Digest, Sha256};

pub const LENGTH: usize = 32;
const HASH_BYTES: usize = LENGTH / 2;
const ALPHABET: &[u8; 16] = b"abcdefghijklmnop";
const NIBBLE_BITS: u32 = 4;
const NIBBLE_MASK: u8 = 0xf;

#[must_use]
pub fn is_valid(value: &str) -> bool {
    let first = char::from(ALPHABET[0]);
    let last = char::from(ALPHABET[ALPHABET.len() - 1]);
    value.len() == LENGTH && value.chars().all(|letter| (first..=last).contains(&letter))
}

#[must_use]
pub fn from_public_key(public_key: &[u8]) -> String {
    let hash = Sha256::digest(public_key);
    let mut id = String::with_capacity(LENGTH);
    for &byte in &hash[..HASH_BYTES] {
        id.push(char::from(ALPHABET[usize::from(byte >> NIBBLE_BITS)]));
        id.push(char::from(ALPHABET[usize::from(byte & NIBBLE_MASK)]));
    }
    id
}

#[cfg(test)]
mod tests {
    use super::*;

    const THEME_ID: &str = "aaaabbbbccccddddeeeeffffgggghhhh";

    #[test]
    fn extension_ids_are_thirty_two_letters_a_to_p() {
        assert!(is_valid(THEME_ID), "a..p, 32 long");
        assert!(!is_valid(&THEME_ID[..31]), "31 is too short");
        assert!(
            !is_valid(&format!("{}q", &THEME_ID[..31])),
            "q is not in the alphabet"
        );
        let from_key = from_public_key(&[1, 2, 3]);
        assert!(is_valid(&from_key), "an id from a key is an id");
        assert_eq!(
            from_key,
            from_public_key(&[1, 2, 3]),
            "the same key, the same id"
        );
    }
}
