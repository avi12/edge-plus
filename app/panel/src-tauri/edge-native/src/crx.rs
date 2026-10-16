//! Reading a local theme `.crx`'s extension id and version.
//!
//! A CRX3 file is a `Cr24` header (a `CrxFileHeader` protobuf carrying the RSA
//! public key) followed by a ZIP. The extension id is the first 16 bytes of the
//! SHA-256 of that public key, each nibble mapped to `a..p`; the version is the
//! `version` field of the ZIP's `manifest.json`. Mirrors the CRX half of the
//! old `ThemeInstaller.cs`.

use std::io::{self, Cursor, Read};

use sha2::{Digest, Sha256};

const ID_ALPHABET: &[u8; 16] = b"abcdefghijklmnop";

/// A local theme's identity, read from its `.crx`.
pub struct CrxInfo {
    pub id: String,
    pub version: String,
}

/// Read the extension id and version from CRX3 `bytes`.
///
/// # Errors
/// Returns an error if the bytes are not a CRX3 file, carry no public key, or
/// have no `version` in their `manifest.json`.
pub fn read_crx(bytes: &[u8]) -> io::Result<CrxInfo> {
    let public_key = read_public_key(bytes)?;
    let id = id_from_public_key(&public_key);
    let version = read_version(bytes)?;
    Ok(CrxInfo { id, version })
}

fn id_from_public_key(public_key: &[u8]) -> String {
    let hash = Sha256::digest(public_key);
    let mut id = String::with_capacity(32);
    for &byte in &hash[..16] {
        id.push(char::from(ID_ALPHABET[(byte >> 4) as usize]));
        id.push(char::from(ID_ALPHABET[(byte & 0xf) as usize]));
    }
    id
}

fn header_length(bytes: &[u8]) -> io::Result<usize> {
    let is_crx3 = bytes.len() > 12
        && &bytes[0..4] == b"Cr24"
        && u32::from_le_bytes([bytes[4], bytes[5], bytes[6], bytes[7]]) == 3;
    if !is_crx3 {
        return Err(invalid("not a CRX3 file"));
    }
    Ok(u32::from_le_bytes([bytes[8], bytes[9], bytes[10], bytes[11]]) as usize)
}

// Walk the CrxFileHeader protobuf: its repeated AsymmetricKeyProof (field 2)
// carries the RSA public key (that proof's field 1).
fn read_public_key(bytes: &[u8]) -> io::Result<Vec<u8>> {
    let header_end = 12 + header_length(bytes)?;
    let mut offset = 12;
    while offset < header_end {
        let (field, wire_type, next) = read_tag(bytes, offset)?;
        offset = next;
        if wire_type != 2 {
            return Err(invalid("unexpected CRX header field"));
        }
        let (length, data_start) = read_varint(bytes, offset)?;
        offset = data_start;
        if field == 2 {
            return read_proof_public_key(bytes, offset, offset + length);
        }
        offset += length;
    }
    Err(invalid("no public key in CRX header"))
}

fn read_proof_public_key(bytes: &[u8], start: usize, end: usize) -> io::Result<Vec<u8>> {
    let mut offset = start;
    while offset < end {
        let (field, wire_type, next) = read_tag(bytes, offset)?;
        offset = next;
        let (length, data_start) = read_varint(bytes, offset)?;
        offset = data_start;
        if field == 1 && wire_type == 2 {
            return Ok(bytes[offset..offset + length].to_vec());
        }
        offset += length;
    }
    Err(invalid("no public key in key proof"))
}

fn read_version(bytes: &[u8]) -> io::Result<String> {
    let zip_start = 12 + header_length(bytes)?;
    let cursor = Cursor::new(&bytes[zip_start..]);
    let mut archive = zip::ZipArchive::new(cursor).map_err(|_| invalid("bad .crx ZIP"))?;
    let mut manifest = archive
        .by_name("manifest.json")
        .map_err(|_| invalid("no manifest.json in .crx"))?;
    let mut text = String::new();
    manifest.read_to_string(&mut text)?;
    let parsed: serde_json::Value =
        serde_json::from_str(&text).map_err(|_| invalid("bad manifest.json"))?;
    parsed
        .get("version")
        .and_then(serde_json::Value::as_str)
        .map(ToOwned::to_owned)
        .ok_or_else(|| invalid("no version in manifest.json"))
}

fn read_tag(data: &[u8], offset: usize) -> io::Result<(usize, usize, usize)> {
    let (tag, next) = read_varint(data, offset)?;
    Ok((tag >> 3, tag & 0x7, next))
}

fn read_varint(data: &[u8], mut offset: usize) -> io::Result<(usize, usize)> {
    let mut value: usize = 0;
    let mut shift = 0u32;
    loop {
        let piece = *data
            .get(offset)
            .ok_or_else(|| invalid("truncated varint"))?;
        offset += 1;
        value |= usize::from(piece & 0x7f) << shift;
        if piece & 0x80 == 0 {
            return Ok((value, offset));
        }
        shift += 7;
    }
}

fn invalid(message: &str) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, message)
}
