//! 32 characters of a..p: the first 16 bytes of the SHA-256 of the extension's
//! public key, each nibble spelled as a letter.

pub const LENGTH: usize = 32;

#[must_use]
pub fn is_valid(value: &str) -> bool {
    let _ = value;
    todo!()
}

#[must_use]
pub fn from_public_key(public_key: &[u8]) -> String {
    let _ = public_key;
    todo!()
}
