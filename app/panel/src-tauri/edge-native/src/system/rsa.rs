//! A 2048-bit RSA key in Windows CNG. Its file form is CNG's `RSAPRIVATEBLOB`,
//! the format the earlier panels wrote, so a theme they packed keeps its id.

use std::io;
use std::ptr::{null, null_mut};

use windows_sys::Win32::Security::Cryptography::{
    BCRYPT_ALG_HANDLE, BCRYPT_KEY_HANDLE, BCRYPT_PAD_PKCS1, BCRYPT_PKCS1_PADDING_INFO,
    BCRYPT_RSA_ALGORITHM, BCRYPT_RSAPRIVATE_BLOB, BCRYPT_RSAPUBLIC_BLOB, BCRYPT_SHA256_ALG_HANDLE,
    BCRYPT_SHA256_ALGORITHM, BCryptCloseAlgorithmProvider, BCryptDestroyKey, BCryptExportKey,
    BCryptFinalizeKeyPair, BCryptGenerateKeyPair, BCryptHash, BCryptImportKeyPair,
    BCryptOpenAlgorithmProvider, BCryptSignHash, BCryptVerifySignature,
};
use windows_sys::core::PCWSTR;

const SHA256_BYTES: usize = 32;
/// `BCRYPT_RSAKEY_BLOB`: magic, bit length, then four byte counts.
const KEY_BLOB_HEADER_BYTES: usize = 24;
const PUBLIC_EXPONENT_LENGTH_OFFSET: usize = 8;
const MODULUS_LENGTH_OFFSET: usize = 12;
const U32_BYTES: usize = 4;
/// DER's short length form holds up to 127; past that the length's own byte
/// count follows this flag.
const DER_LONG_LENGTH_FLAG: u8 = 0x80;
const DER_SIGN_BIT: u8 = 0x80;
const DER_SEQUENCE: u8 = 0x30;
const DER_INTEGER: u8 = 0x02;
const DER_BIT_STRING: u8 = 0x03;
/// A BIT STRING's leading count of unused bits.
const NO_UNUSED_BITS: u8 = 0;
/// The `AlgorithmIdentifier` SEQUENCE for `rsaEncryption` (1.2.840.113549.1.1.1)
/// with its NULL parameters.
const RSA_ENCRYPTION_ALGORITHM: [u8; 15] = [
    0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00,
];

pub struct RsaKey {
    algorithm: BCRYPT_ALG_HANDLE,
    key: BCRYPT_KEY_HANDLE,
}

// SAFETY: CNG key and algorithm handles may be used from any thread.
unsafe impl Send for RsaKey {}

impl RsaKey {
    pub fn generate(bits: u32) -> io::Result<Self> {
        let mut rsa = Self::empty()?;
        // SAFETY: `algorithm` is open and `key` a valid out-pointer; the key is
        // finalized before anything uses it.
        unsafe {
            check(
                BCryptGenerateKeyPair(rsa.algorithm, &raw mut rsa.key, bits, 0),
                "generate a signing key",
            )?;
            check(BCryptFinalizeKeyPair(rsa.key, 0), "finish the signing key")?;
        }
        Ok(rsa)
    }

    pub fn import_private_blob(blob: &[u8]) -> io::Result<Self> {
        let mut rsa = Self::empty()?;
        // SAFETY: `algorithm` is open, `blob` is valid for its length, and `key`
        // is a valid out-pointer.
        let status = unsafe {
            BCryptImportKeyPair(
                rsa.algorithm,
                null_mut(),
                BCRYPT_RSAPRIVATE_BLOB,
                &raw mut rsa.key,
                blob.as_ptr(),
                byte_count(blob.len())?,
                0,
            )
        };
        check(status, "import the signing key")?;
        Ok(rsa)
    }

    pub fn export_private_blob(&self) -> io::Result<Vec<u8>> {
        self.export(BCRYPT_RSAPRIVATE_BLOB)
    }

    /// The DER `SubjectPublicKeyInfo` (`rsaEncryption`), as a CRX3 header carries it.
    pub fn public_key_der(&self) -> io::Result<Vec<u8>> {
        let blob = self.export(BCRYPT_RSAPUBLIC_BLOB)?;
        let exponent_length = read_u32(&blob, PUBLIC_EXPONENT_LENGTH_OFFSET)?;
        let modulus_length = read_u32(&blob, MODULUS_LENGTH_OFFSET)?;
        let exponent_end = KEY_BLOB_HEADER_BYTES + exponent_length;
        let modulus_end = exponent_end + modulus_length;
        let exponent = blob
            .get(KEY_BLOB_HEADER_BYTES..exponent_end)
            .ok_or_else(|| io::Error::other("bad public key blob"))?;
        let modulus = blob
            .get(exponent_end..modulus_end)
            .ok_or_else(|| io::Error::other("bad public key blob"))?;
        let mut numbers = der_unsigned(modulus);
        numbers.extend_from_slice(&der_unsigned(exponent));
        let rsa_public_key = der(DER_SEQUENCE, &numbers);
        let mut bit_string = vec![NO_UNUSED_BITS];
        bit_string.extend_from_slice(&rsa_public_key);
        let mut info = RSA_ENCRYPTION_ALGORITHM.to_vec();
        info.extend_from_slice(&der(DER_BIT_STRING, &bit_string));
        Ok(der(DER_SEQUENCE, &info))
    }

    /// RSASSA-PKCS1-v1_5 over the SHA-256 of `data`.
    pub fn sign_sha256(&self, data: &[u8]) -> io::Result<Vec<u8>> {
        let digest = sha256(data)?;
        let padding = sha256_padding();
        let mut length = 0u32;
        // SAFETY: a size query (null output) on a live key; the padding struct
        // and the digest outlive the call.
        let sized = unsafe {
            BCryptSignHash(
                self.key,
                (&raw const padding).cast(),
                digest.as_ptr(),
                byte_count(digest.len())?,
                null_mut(),
                0,
                &raw mut length,
                BCRYPT_PAD_PKCS1,
            )
        };
        check(sized, "size the signature")?;
        let mut signature = vec![0u8; length as usize];
        // SAFETY: `signature` is `length` bytes, as the size query reported.
        let status = unsafe {
            BCryptSignHash(
                self.key,
                (&raw const padding).cast(),
                digest.as_ptr(),
                byte_count(digest.len())?,
                signature.as_mut_ptr(),
                length,
                &raw mut length,
                BCRYPT_PAD_PKCS1,
            )
        };
        check(status, "sign the theme")?;
        signature.truncate(length as usize);
        Ok(signature)
    }

    /// Whether `signature` is this key's RSASSA-PKCS1-v1_5 over the SHA-256 of
    /// `data`.
    pub fn verify_sha256(&self, data: &[u8], signature: &[u8]) -> io::Result<bool> {
        let digest = sha256(data)?;
        let padding = sha256_padding();
        // SAFETY: a live key; the padding struct, the digest and the signature
        // outlive the call.
        let status = unsafe {
            BCryptVerifySignature(
                self.key,
                (&raw const padding).cast(),
                digest.as_ptr(),
                byte_count(digest.len())?,
                signature.as_ptr(),
                byte_count(signature.len())?,
                BCRYPT_PAD_PKCS1,
            )
        };
        Ok(status >= 0)
    }

    /// The provider open and no key yet; Drop releases whichever is held.
    fn empty() -> io::Result<Self> {
        let mut algorithm: BCRYPT_ALG_HANDLE = null_mut();
        // SAFETY: a valid out-pointer and a constant algorithm name.
        let status = unsafe {
            BCryptOpenAlgorithmProvider(&raw mut algorithm, BCRYPT_RSA_ALGORITHM, null(), 0)
        };
        check(status, "open the RSA provider")?;
        Ok(Self {
            algorithm,
            key: null_mut(),
        })
    }

    fn export(&self, blob_type: PCWSTR) -> io::Result<Vec<u8>> {
        let mut length = 0u32;
        // SAFETY: a size query (null output) on a live key.
        let sized = unsafe {
            BCryptExportKey(
                self.key,
                null_mut(),
                blob_type,
                null_mut(),
                0,
                &raw mut length,
                0,
            )
        };
        check(sized, "size the key export")?;
        let mut blob = vec![0u8; length as usize];
        // SAFETY: `blob` is `length` bytes, as the size query reported.
        let status = unsafe {
            BCryptExportKey(
                self.key,
                null_mut(),
                blob_type,
                blob.as_mut_ptr(),
                length,
                &raw mut length,
                0,
            )
        };
        check(status, "export the key")?;
        blob.truncate(length as usize);
        Ok(blob)
    }
}

impl Drop for RsaKey {
    fn drop(&mut self) {
        // SAFETY: each handle is released once, and only when it was opened.
        unsafe {
            if !self.key.is_null() {
                BCryptDestroyKey(self.key);
            }
            BCryptCloseAlgorithmProvider(self.algorithm, 0);
        }
    }
}

fn sha256_padding() -> BCRYPT_PKCS1_PADDING_INFO {
    BCRYPT_PKCS1_PADDING_INFO {
        pszAlgId: BCRYPT_SHA256_ALGORITHM,
    }
}

fn sha256(data: &[u8]) -> io::Result<[u8; SHA256_BYTES]> {
    let mut digest = [0u8; SHA256_BYTES];
    // SAFETY: the SHA-256 pseudo-handle needs no opening; `data` and `digest`
    // are valid for their lengths.
    let status = unsafe {
        BCryptHash(
            BCRYPT_SHA256_ALG_HANDLE,
            null(),
            0,
            data.as_ptr(),
            byte_count(data.len())?,
            digest.as_mut_ptr(),
            byte_count(SHA256_BYTES)?,
        )
    };
    check(status, "hash the theme")?;
    Ok(digest)
}

fn der(tag: u8, payload: &[u8]) -> Vec<u8> {
    let mut bytes = vec![tag];
    let is_short_length = payload.len() < usize::from(DER_LONG_LENGTH_FLAG);
    if is_short_length {
        bytes.push(u8::try_from(payload.len()).unwrap_or(0));
    } else {
        let length = payload.len().to_be_bytes();
        let significant: Vec<u8> = length
            .iter()
            .copied()
            .skip_while(|&byte| byte == 0)
            .collect();
        let length_bytes = u8::try_from(significant.len()).unwrap_or(0);
        bytes.push(DER_LONG_LENGTH_FLAG | length_bytes);
        bytes.extend_from_slice(&significant);
    }
    bytes.extend_from_slice(payload);
    bytes
}

/// A positive DER INTEGER from big-endian magnitude bytes: a leading zero keeps
/// a set top bit from reading as negative.
fn der_unsigned(magnitude: &[u8]) -> Vec<u8> {
    let trimmed: Vec<u8> = magnitude
        .iter()
        .copied()
        .skip_while(|&byte| byte == 0)
        .collect();
    let mut payload = Vec::new();
    let needs_sign_byte = trimmed.first().is_none_or(|&byte| byte & DER_SIGN_BIT != 0);
    if needs_sign_byte {
        payload.push(0);
    }
    payload.extend_from_slice(&trimmed);
    der(DER_INTEGER, &payload)
}

fn read_u32(blob: &[u8], offset: usize) -> io::Result<usize> {
    blob.get(offset..offset + U32_BYTES)
        .and_then(|bytes| <[u8; U32_BYTES]>::try_from(bytes).ok())
        .map(|bytes| u32::from_le_bytes(bytes) as usize)
        .ok_or_else(|| io::Error::other("bad key blob"))
}

fn byte_count(length: usize) -> io::Result<u32> {
    u32::try_from(length).map_err(|_| io::Error::other("too much data to sign"))
}

fn check(status: i32, action: &str) -> io::Result<()> {
    if status >= 0 {
        return Ok(());
    }
    Err(io::Error::other(format!(
        "could not {action} (NTSTATUS {status:#x})"
    )))
}

#[cfg(test)]
mod tests {
    use super::*;

    const KEY_BITS: u32 = 2048;
    /// A 2048-bit key with exponent 65537 is always this long as SPKI DER.
    const SPKI_2048_BYTES: usize = 294;

    #[test]
    fn rsa_key_round_trips_through_its_blob_and_signs() {
        let key = RsaKey::generate(KEY_BITS).expect("generate");
        let blob = key.export_private_blob().expect("export");
        let imported = RsaKey::import_private_blob(&blob).expect("import");
        let public_key = key.public_key_der().expect("public key");
        assert_eq!(public_key, imported.public_key_der().expect("public key"));
        assert_eq!(SPKI_2048_BYTES, public_key.len());
        assert!(public_key.starts_with(&[DER_SEQUENCE, DER_LONG_LENGTH_FLAG | 2]));
        let has_algorithm = public_key
            .windows(RSA_ENCRYPTION_ALGORITHM.len())
            .any(|window| window == RSA_ENCRYPTION_ALGORITHM);
        assert!(has_algorithm);

        let signature = imported.sign_sha256(b"theme").expect("sign");
        assert_eq!(KEY_BITS as usize / 8, signature.len());
        assert!(key.verify_sha256(b"theme", &signature).expect("verify"));
        assert!(!key.verify_sha256(b"other", &signature).expect("verify"));
    }

    #[test]
    fn rsa_refuses_a_broken_blob() {
        assert!(RsaKey::import_private_blob(&[1, 2, 3]).is_err());
    }

    #[test]
    fn rsa_der_lengths_switch_to_the_long_form_at_128() {
        assert_eq!(vec![DER_INTEGER, 1, 5], der(DER_INTEGER, &[5]));
        let long = der(DER_SEQUENCE, &[0; 200]);
        assert_eq!(&[DER_SEQUENCE, DER_LONG_LENGTH_FLAG | 1, 200], &long[..3]);
        assert_eq!(vec![DER_INTEGER, 2, 0, 0x80], der_unsigned(&[0, 0, 0x80]));
    }
}
