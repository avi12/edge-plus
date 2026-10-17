//! Turning a local theme folder or `.zip` into a signed `.crx`.
//!
//! Edge's external-extension loader installs only a `.crx`, so a theme given as
//! an unpacked folder or a ZIP is packed here: zipped, signed with an RSA key
//! (Windows CNG, no crypto crate) and wrapped in a CRX3 header. The key is kept
//! per source path, so adding the same theme again keeps its extension id.

use std::fs;
use std::io::{self, Cursor, Write};
use std::path::{Path, PathBuf};
use std::ptr::{null, null_mut};

use sha2::{Digest, Sha256};
use windows_sys::core::PCWSTR;
use windows_sys::Win32::Security::Cryptography::{
    BCryptCloseAlgorithmProvider, BCryptDestroyKey, BCryptExportKey, BCryptFinalizeKeyPair,
    BCryptGenerateKeyPair, BCryptImportKeyPair, BCryptOpenAlgorithmProvider, BCryptSignHash,
    BCRYPT_ALG_HANDLE, BCRYPT_KEY_HANDLE, BCRYPT_PAD_PKCS1, BCRYPT_PKCS1_PADDING_INFO,
    BCRYPT_RSAPRIVATE_BLOB, BCRYPT_RSAPUBLIC_BLOB, BCRYPT_RSA_ALGORITHM, BCRYPT_SHA256_ALGORITHM,
};

use crate::crx::id_from_public_key;
use crate::manifest;

const KEY_BITS: u32 = 2048;
const CRX_MAGIC: &[u8; 4] = b"Cr24";
const CRX_VERSION: u32 = 3;
const SIGNED_DATA_PREFIX: &[u8] = b"CRX3 SignedData\0";
// CrxFileHeader fields: the RSA proof, and the signed `crx_id` payload.
const FIELD_RSA_PROOF: u64 = 2;
const FIELD_SIGNED_HEADER_DATA: u64 = 10_000;
const FIELD_PUBLIC_KEY: u64 = 1;
const FIELD_SIGNATURE: u64 = 2;
const FIELD_CRX_ID: u64 = 1;
const CRX_ID_BYTES: usize = 16;
const KEY_FILE_NAME_BYTES: usize = 8;
// BCRYPT_RSAKEY_BLOB: magic, bit length, then four byte counts; 24 bytes.
const KEY_BLOB_HEADER: usize = 24;
const OFFSET_PUBLIC_EXPONENT_LENGTH: usize = 8;
const OFFSET_MODULUS_LENGTH: usize = 12;
// DER pieces of a SubjectPublicKeyInfo for rsaEncryption.
const DER_SEQUENCE: u8 = 0x30;
const DER_INTEGER: u8 = 0x02;
const DER_BIT_STRING: u8 = 0x03;
const RSA_ENCRYPTION_ALGORITHM: [u8; 15] = [
    0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00,
];

/// A theme ready to hand to Edge's external-extension loader.
pub struct PackedTheme {
    pub id: String,
    pub version: String,
    pub crx: Vec<u8>,
}

/// Pack the theme at `source` (an unpacked folder, or a `.zip` with its
/// `manifest.json` at the top) into a signed CRX3. `key_dir` holds the per-theme
/// signing keys.
///
/// # Errors
/// Returns an error if the source cannot be read or zipped, has no
/// `manifest.json` with a version, or the key cannot be made or used.
pub fn pack_theme(source: &Path, key_dir: &Path) -> io::Result<PackedTheme> {
    let zip_bytes = if source.is_dir() {
        zip_folder(source)?
    } else {
        fs::read(source)?
    };
    manifest::validate(&zip_bytes)?;
    fs::create_dir_all(key_dir)?;
    let key_file = key_dir.join(key_file_name(source));
    let (zip_bytes, version) = manifest::revise(&zip_bytes, next_revision(&key_file)?)?;
    let signer = Signer::load_or_create(&key_file)?;
    let public_key = signer.public_key_der()?;
    let id = id_from_public_key(&public_key);

    let crx_id = Sha256::digest(&public_key)[..CRX_ID_BYTES].to_vec();
    let signed_header_data = field(FIELD_CRX_ID, &crx_id);
    let mut to_sign = SIGNED_DATA_PREFIX.to_vec();
    to_sign.extend_from_slice(&u32_le(signed_header_data.len())?);
    to_sign.extend_from_slice(&signed_header_data);
    to_sign.extend_from_slice(&zip_bytes);
    let signature = signer.sign(&Sha256::digest(&to_sign))?;

    let mut proof = field(FIELD_PUBLIC_KEY, &public_key);
    proof.extend_from_slice(&field(FIELD_SIGNATURE, &signature));
    let mut header = field(FIELD_RSA_PROOF, &proof);
    header.extend_from_slice(&field(FIELD_SIGNED_HEADER_DATA, &signed_header_data));

    let mut crx = CRX_MAGIC.to_vec();
    crx.extend_from_slice(&CRX_VERSION.to_le_bytes());
    crx.extend_from_slice(&u32_le(header.len())?);
    crx.extend_from_slice(&header);
    crx.extend_from_slice(&zip_bytes);
    Ok(PackedTheme { id, version, crx })
}

// The theme's pack count, kept beside its key: each pack gets a higher version,
// which is what makes Edge replace the installed copy.
fn next_revision(key_file: &Path) -> io::Result<u16> {
    let counter_file = key_file.with_extension("rev");
    let previous: u16 = fs::read_to_string(&counter_file)
        .ok()
        .and_then(|text| text.trim().parse().ok())
        .unwrap_or(0);
    let next = previous.checked_add(1).unwrap_or(1);
    fs::write(counter_file, next.to_string())?;
    Ok(next)
}

fn key_file_name(source: &Path) -> String {
    let normalized = source.to_string_lossy().to_lowercase();
    let hash = Sha256::digest(normalized.as_bytes());
    let mut prefix = [0u8; KEY_FILE_NAME_BYTES];
    prefix.copy_from_slice(&hash[..KEY_FILE_NAME_BYTES]);
    format!("{:016x}.key", u64::from_be_bytes(prefix))
}

fn zip_folder(root: &Path) -> io::Result<Vec<u8>> {
    let mut files = Vec::new();
    collect_files(root, root, &mut files)?;
    let mut writer = zip::ZipWriter::new(Cursor::new(Vec::new()));
    let options = zip::write::SimpleFileOptions::default();
    for (relative, path) in files {
        writer
            .start_file(relative, options)
            .map_err(|error| io::Error::other(error.to_string()))?;
        writer.write_all(&fs::read(path)?)?;
    }
    let finished = writer
        .finish()
        .map_err(|error| io::Error::other(error.to_string()))?;
    Ok(finished.into_inner())
}

fn collect_files(root: &Path, folder: &Path, files: &mut Vec<(String, PathBuf)>) -> io::Result<()> {
    for entry in fs::read_dir(folder)? {
        let path = entry?.path();
        if path.is_dir() {
            collect_files(root, &path, files)?;
        } else if let Ok(relative) = path.strip_prefix(root) {
            let name = relative.to_string_lossy().replace('\\', "/");
            files.push((name, path));
        }
    }
    Ok(())
}

// --- protobuf and DER ---

fn u32_le(length: usize) -> io::Result<[u8; 4]> {
    u32::try_from(length)
        .map(u32::to_le_bytes)
        .map_err(|_| io::Error::other("theme is too large"))
}

fn varint(mut value: u64) -> Vec<u8> {
    let mut bytes = Vec::new();
    loop {
        let low = u8::try_from(value & 0x7f).unwrap_or(0);
        value >>= 7;
        if value == 0 {
            bytes.push(low);
            return bytes;
        }
        bytes.push(low | 0x80);
    }
}

// A length-delimited protobuf field (wire type 2).
fn field(number: u64, payload: &[u8]) -> Vec<u8> {
    let mut bytes = varint((number << 3) | 2);
    bytes.extend_from_slice(&varint(payload.len() as u64));
    bytes.extend_from_slice(payload);
    bytes
}

fn der(tag: u8, payload: &[u8]) -> Vec<u8> {
    let mut bytes = vec![tag];
    if payload.len() < 0x80 {
        bytes.push(u8::try_from(payload.len()).unwrap_or(0));
    } else {
        let length = payload.len().to_be_bytes();
        let significant: Vec<u8> = length.iter().copied().skip_while(|&b| b == 0).collect();
        bytes.push(0x80 | u8::try_from(significant.len()).unwrap_or(0));
        bytes.extend_from_slice(&significant);
    }
    bytes.extend_from_slice(payload);
    bytes
}

// A positive DER INTEGER from big-endian magnitude bytes.
fn der_unsigned(magnitude: &[u8]) -> Vec<u8> {
    let trimmed: Vec<u8> = magnitude.iter().copied().skip_while(|&b| b == 0).collect();
    let mut payload = Vec::new();
    if trimmed.first().is_none_or(|&b| b & 0x80 != 0) {
        payload.push(0);
    }
    payload.extend_from_slice(&trimmed);
    der(DER_INTEGER, &payload)
}

// --- the RSA key, through Windows CNG ---

struct Signer {
    algorithm: BCRYPT_ALG_HANDLE,
    key: BCRYPT_KEY_HANDLE,
}

impl Signer {
    fn load_or_create(key_file: &Path) -> io::Result<Self> {
        let algorithm = open_rsa()?;
        let blob = if let Ok(saved) = fs::read(key_file) {
            saved
        } else {
            let fresh = generate(algorithm)?;
            let exported = export(fresh, BCRYPT_RSAPRIVATE_BLOB);
            // SAFETY: `fresh` is a live key handle, destroyed once.
            unsafe { BCryptDestroyKey(fresh) };
            let exported = exported?;
            fs::write(key_file, &exported)?;
            exported
        };
        let mut key: BCRYPT_KEY_HANDLE = null_mut();
        // SAFETY: `algorithm` is open; `blob` is valid for its length; `key` is
        // a valid out-pointer.
        let status = unsafe {
            BCryptImportKeyPair(
                algorithm,
                null_mut(),
                BCRYPT_RSAPRIVATE_BLOB,
                &raw mut key,
                blob.as_ptr(),
                u32::try_from(blob.len()).unwrap_or(0),
                0,
            )
        };
        check(status, "import the signing key")?;
        Ok(Self { algorithm, key })
    }

    // The key as a DER SubjectPublicKeyInfo, the form a CRX carries.
    fn public_key_der(&self) -> io::Result<Vec<u8>> {
        let blob = export(self.key, BCRYPT_RSAPUBLIC_BLOB)?;
        let exponent_length = read_u32(&blob, OFFSET_PUBLIC_EXPONENT_LENGTH)?;
        let modulus_length = read_u32(&blob, OFFSET_MODULUS_LENGTH)?;
        let exponent_end = KEY_BLOB_HEADER + exponent_length;
        let modulus_end = exponent_end + modulus_length;
        let exponent = blob
            .get(KEY_BLOB_HEADER..exponent_end)
            .ok_or_else(|| io::Error::other("bad public key blob"))?;
        let modulus = blob
            .get(exponent_end..modulus_end)
            .ok_or_else(|| io::Error::other("bad public key blob"))?;
        let mut numbers = der_unsigned(modulus);
        numbers.extend_from_slice(&der_unsigned(exponent));
        let rsa_key = der(DER_SEQUENCE, &numbers);
        let mut bit_string = vec![0u8];
        bit_string.extend_from_slice(&rsa_key);
        let mut info = RSA_ENCRYPTION_ALGORITHM.to_vec();
        info.extend_from_slice(&der(DER_BIT_STRING, &bit_string));
        Ok(der(DER_SEQUENCE, &info))
    }

    // RSASSA-PKCS1-v1_5 over a SHA-256 digest.
    fn sign(&self, digest: &[u8]) -> io::Result<Vec<u8>> {
        let padding = BCRYPT_PKCS1_PADDING_INFO {
            pszAlgId: BCRYPT_SHA256_ALGORITHM,
        };
        let digest_length = u32::try_from(digest.len()).unwrap_or(0);
        let mut length = 0u32;
        // SAFETY: a size query (null output) on a live key; the padding struct
        // and digest outlive the call.
        let sized = unsafe {
            BCryptSignHash(
                self.key,
                (&raw const padding).cast(),
                digest.as_ptr(),
                digest_length,
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
                digest_length,
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
}

impl Drop for Signer {
    fn drop(&mut self) {
        // SAFETY: both handles are live and released once.
        unsafe {
            BCryptDestroyKey(self.key);
            BCryptCloseAlgorithmProvider(self.algorithm, 0);
        }
    }
}

fn open_rsa() -> io::Result<BCRYPT_ALG_HANDLE> {
    let mut algorithm: BCRYPT_ALG_HANDLE = null_mut();
    // SAFETY: a valid out-pointer and constant algorithm name.
    let status =
        unsafe { BCryptOpenAlgorithmProvider(&raw mut algorithm, BCRYPT_RSA_ALGORITHM, null(), 0) };
    check(status, "open the RSA provider")?;
    Ok(algorithm)
}

fn generate(algorithm: BCRYPT_ALG_HANDLE) -> io::Result<BCRYPT_KEY_HANDLE> {
    let mut key: BCRYPT_KEY_HANDLE = null_mut();
    // SAFETY: `algorithm` is open; the key is finalized before use.
    unsafe {
        check(
            BCryptGenerateKeyPair(algorithm, &raw mut key, KEY_BITS, 0),
            "generate a signing key",
        )?;
        check(BCryptFinalizeKeyPair(key, 0), "finish the signing key")?;
    }
    Ok(key)
}

fn export(key: BCRYPT_KEY_HANDLE, blob_type: PCWSTR) -> io::Result<Vec<u8>> {
    let mut length = 0u32;
    // SAFETY: a size query (null output), then a read into a buffer of that size.
    unsafe {
        check(
            BCryptExportKey(
                key,
                null_mut(),
                blob_type,
                null_mut(),
                0,
                &raw mut length,
                0,
            ),
            "size the key export",
        )?;
        let mut blob = vec![0u8; length as usize];
        check(
            BCryptExportKey(
                key,
                null_mut(),
                blob_type,
                blob.as_mut_ptr(),
                length,
                &raw mut length,
                0,
            ),
            "export the key",
        )?;
        blob.truncate(length as usize);
        Ok(blob)
    }
}

fn read_u32(blob: &[u8], offset: usize) -> io::Result<usize> {
    blob.get(offset..offset + 4)
        .map(|bytes| u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]) as usize)
        .ok_or_else(|| io::Error::other("bad key blob"))
}

fn check(status: i32, action: &str) -> io::Result<()> {
    if status >= 0 {
        Ok(())
    } else {
        Err(io::Error::other(format!(
            "could not {action} (NTSTATUS {status:#x})"
        )))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::read_crx;

    #[test]
    fn packed_folder_reads_back_with_the_same_id_and_a_stable_key() {
        let root = std::env::temp_dir().join(format!("fixedge-pack-{}", std::process::id()));
        let theme = root.join("theme");
        fs::create_dir_all(theme.join("images")).unwrap();
        fs::write(
            theme.join("manifest.json"),
            r#"{"manifest_version":3,"name":"t","version":"1.2.3","theme":{}}"#,
        )
        .unwrap();
        fs::write(theme.join("images").join("frame.png"), b"png").unwrap();

        let first = pack_theme(&theme, &root.join("keys")).unwrap();
        let info = read_crx(&first.crx).unwrap();
        assert_eq!(info.id, first.id);
        assert_eq!(info.version, "1.2.3.1");
        let second = pack_theme(&theme, &root.join("keys")).unwrap();
        assert_eq!(second.id, first.id);
        assert_eq!(second.version, "1.2.3.2");

        fs::write(
            theme.join("manifest.json"),
            r#"{"name":"t","version":"1.2.3","theme":{"images":{"theme_frame":"images/gone.png"}}}"#,
        )
        .unwrap();
        assert!(pack_theme(&theme, &root.join("keys")).is_err());
        fs::write(
            theme.join("manifest.json"),
            r#"{"name":"t","version":"1.2.3","theme":{"colors":{"frame":[300,0,0]}}}"#,
        )
        .unwrap();
        assert!(pack_theme(&theme, &root.join("keys")).is_err());
        fs::remove_dir_all(root).unwrap();
    }
}
