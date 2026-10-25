//! A 2048-bit RSA key in Windows CNG. Its file form is CNG's RSAPRIVATEBLOB,
//! the format the earlier panels wrote, so a theme they packed keeps its id.

use std::io;

pub struct RsaKey {}

impl RsaKey {
    pub fn generate(bits: u32) -> io::Result<Self> {
        let _ = bits;
        todo!()
    }

    pub fn import_private_blob(blob: &[u8]) -> io::Result<Self> {
        let _ = blob;
        todo!()
    }

    pub fn export_private_blob(&self) -> io::Result<Vec<u8>> {
        todo!()
    }

    /// The DER SubjectPublicKeyInfo (rsaEncryption), as a CRX3 header carries it.
    pub fn public_key_der(&self) -> io::Result<Vec<u8>> {
        todo!()
    }

    /// RSASSA-PKCS1-v1_5 over the SHA-256 of `data`.
    pub fn sign_sha256(&self, data: &[u8]) -> io::Result<Vec<u8>> {
        let _ = data;
        todo!()
    }
}
