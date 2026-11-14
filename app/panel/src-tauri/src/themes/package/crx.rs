//! A CRX3 file (read here, written by `theme_packer`) is a "Cr24" magic, the
//! format version, the header's length, a `CrxFileHeader` protobuf carrying the
//! RSA public key, then a ZIP. The extension id is derived from that public
//! key; the version is the "version" of the ZIP's manifest.json.

use std::io::{Cursor, Read};

use serde_json::Value;
use zip::ZipArchive;

use crate::themes::package::extension_id;
use crate::themes::package::protobuf::{self, Field, FieldNumber};
use crate::themes::package::theme_manifest::{
    MANIFEST_NAME, ThemeFileResult, invalid, manifest_keys,
};
use crate::ui::wording;

#[derive(Clone, Debug, PartialEq, Eq)]
pub(crate) struct CrxInfo {
    pub id: String,
    pub version: String,
}

/// The signature and what it covers are read back only by the packer's tests.
#[derive(Clone, Debug, PartialEq, Eq)]
#[cfg_attr(
    not(test),
    expect(dead_code, reason = "the packer's tests verify the signature")
)]
pub(crate) struct CrxProof {
    pub public_key: Vec<u8>,
    pub signature: Vec<u8>,
    pub signed_header_data: Vec<u8>,
}

/// The fields of `CrxFileHeader`, as protobuf field numbers.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u64)]
pub(crate) enum CrxHeaderField {
    RsaProof = 2,
    SignedHeaderData = 10_000,
}

/// The fields of `AsymmetricKeyProof`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u64)]
pub(crate) enum KeyProofField {
    PublicKey = 1,
    Signature = 2,
}

/// The fields of `SignedData`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u64)]
pub(crate) enum SignedDataField {
    CrxId = 1,
}

impl FieldNumber for CrxHeaderField {
    fn number(self) -> u64 {
        self as u64
    }
}

impl FieldNumber for KeyProofField {
    fn number(self) -> u64 {
        self as u64
    }
}

impl FieldNumber for SignedDataField {
    fn number(self) -> u64 {
        self as u64
    }
}

pub(crate) const MAGIC: &[u8; 4] = b"Cr24";
pub(crate) const FORMAT_VERSION: u32 = 3;
/// The magic, the format version and the header length, each four bytes.
pub(crate) const PREAMBLE_LENGTH: usize = 12;
/// What every CRX3 signature covers, ahead of the signed header data.
pub(crate) const SIGNED_DATA_PREFIX: &[u8] = b"CRX3 SignedData\0";

const VERSION_OFFSET: usize = 4;
const HEADER_LENGTH_OFFSET: usize = 8;
const U32_BYTES: usize = 4;

pub(crate) fn read(bytes: &[u8]) -> ThemeFileResult<CrxInfo> {
    let proof = read_proof(bytes)?;
    Ok(CrxInfo {
        id: extension_id::from_public_key(&proof.public_key),
        version: read_zip_version(zip(bytes)?)?,
    })
}

/// Only the first RSA proof is read.
pub(crate) fn read_proof(bytes: &[u8]) -> ThemeFileResult<CrxProof> {
    let header_end = PREAMBLE_LENGTH + header_length(bytes)?;
    let header = protobuf::read_fields(bytes, PREAMBLE_LENGTH, header_end)?;
    let proof_field = first(&header, CrxHeaderField::RsaProof);
    let Some(proof_field) = proof_field.filter(|field| field.length > 0) else {
        return Err(invalid(wording::NO_PUBLIC_KEY_IN_HEADER));
    };
    let proof_end = proof_field.start + proof_field.length;
    let proof = protobuf::read_fields(bytes, proof_field.start, proof_end)?;
    let key = first(&proof, KeyProofField::PublicKey);
    let Some(key) = key.filter(|field| field.length > 0) else {
        return Err(invalid(wording::NO_PUBLIC_KEY_IN_PROOF));
    };
    let payload_of = |field: Option<Field>| -> Vec<u8> {
        field
            .map(|field| field.payload(bytes).to_vec())
            .unwrap_or_default()
    };
    Ok(CrxProof {
        public_key: key.payload(bytes).to_vec(),
        signature: payload_of(first(&proof, KeyProofField::Signature)),
        signed_header_data: payload_of(first(&header, CrxHeaderField::SignedHeaderData)),
    })
}

const MESSAGE_TOKEN_START: &str = "__MSG_";
const MESSAGE_TOKEN_END: &str = "__";
const LOCALES_FOLDER: &str = "_locales";
const MESSAGES_FILE: &str = "messages.json";
const MESSAGE_FIELD: &str = "message";

pub(crate) fn zip(bytes: &[u8]) -> ThemeFileResult<&[u8]> {
    Ok(&bytes[PREAMBLE_LENGTH + header_length(bytes)?..])
}

pub(crate) fn read_zip_version(zip_bytes: &[u8]) -> ThemeFileResult<String> {
    read_manifest(zip_bytes)?
        .get(manifest_keys::VERSION)
        .and_then(Value::as_str)
        .map(ToOwned::to_owned)
        .ok_or_else(|| invalid(wording::NO_VERSION_IN_CRX_MANIFEST))
}

/// The theme's own name; None when the manifest has none. A message token
/// (`__MSG_name__`) is resolved from the default locale's messages.json.
#[must_use]
pub(crate) fn read_zip_name(zip_bytes: &[u8]) -> Option<String> {
    let manifest = read_manifest(zip_bytes).ok()?;
    let name = manifest.get(manifest_keys::NAME)?.as_str()?.trim();
    let token_key = name
        .strip_prefix(MESSAGE_TOKEN_START)
        .and_then(|rest| rest.strip_suffix(MESSAGE_TOKEN_END));
    let resolved = match token_key {
        Some(key) => localized_message(zip_bytes, &manifest, key)?,
        None => name.to_owned(),
    };
    let trimmed = resolved.trim();
    (!trimmed.is_empty()).then(|| trimmed.to_owned())
}

/// `key` in the messages.json of the manifest's default locale; keys are not
/// case sensitive.
fn localized_message(zip_bytes: &[u8], manifest: &Value, key: &str) -> Option<String> {
    let locale = manifest.get(manifest_keys::DEFAULT_LOCALE)?.as_str()?;
    let mut archive = ZipArchive::new(Cursor::new(zip_bytes)).ok()?;
    let mut messages = String::new();
    archive
        .by_name(&format!("{LOCALES_FOLDER}/{locale}/{MESSAGES_FILE}"))
        .ok()?
        .read_to_string(&mut messages)
        .ok()?;
    let text = messages.strip_prefix('\u{feff}').unwrap_or(&messages);
    let messages: Value = serde_json::from_str(text).ok()?;
    let (_, entry) = messages
        .as_object()?
        .iter()
        .find(|(name, _)| name.eq_ignore_ascii_case(key))?;
    entry.get(MESSAGE_FIELD)?.as_str().map(ToOwned::to_owned)
}

fn read_manifest(zip_bytes: &[u8]) -> ThemeFileResult<Value> {
    let mut archive =
        ZipArchive::new(Cursor::new(zip_bytes)).map_err(|_| invalid(wording::BAD_CRX_ZIP))?;
    let mut manifest = archive
        .by_name(MANIFEST_NAME)
        .map_err(|_| invalid(wording::NO_MANIFEST_IN_CRX))?;
    let mut text = String::new();
    manifest
        .read_to_string(&mut text)
        .map_err(|_| invalid(wording::BAD_CRX_MANIFEST))?;
    let text = text.strip_prefix('\u{feff}').unwrap_or(&text);
    serde_json::from_str(text).map_err(|_| invalid(wording::BAD_CRX_MANIFEST))
}

fn first(fields: &[Field], number: impl FieldNumber) -> Option<Field> {
    fields.iter().copied().find(|field| field.is_number(number))
}

fn header_length(bytes: &[u8]) -> ThemeFileResult<usize> {
    let is_crx3 = bytes.len() > PREAMBLE_LENGTH
        && bytes.starts_with(MAGIC)
        && read_u32(bytes, VERSION_OFFSET) == FORMAT_VERSION;
    if !is_crx3 {
        return Err(invalid(wording::NOT_A_CRX3));
    }
    let length = read_u32(bytes, HEADER_LENGTH_OFFSET) as usize;
    let is_inside = length <= bytes.len() - PREAMBLE_LENGTH;
    if !is_inside {
        return Err(invalid(wording::TRUNCATED_CRX_HEADER));
    }
    Ok(length)
}

/// Little-endian, at an offset the preamble check has already bounded.
fn read_u32(bytes: &[u8], offset: usize) -> u32 {
    let mut word = [0u8; U32_BYTES];
    word.copy_from_slice(&bytes[offset..offset + U32_BYTES]);
    u32::from_le_bytes(word)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn not_a_crx_is_refused() {
        assert!(read(&[1, 2, 3]).is_err(), "three bytes are not a CRX");
        let wrong_version = [b"Cr24".as_slice(), &[2, 0, 0, 0, 0, 0, 0, 0, 0]].concat();
        assert!(read(&wrong_version).is_err(), "CRX2 is refused");
        let overlong = [b"Cr24".as_slice(), &[3, 0, 0, 0, 0xff, 0, 0, 0, 0]].concat();
        assert!(
            read(&overlong).is_err(),
            "a header longer than the file is refused"
        );
    }

    #[test]
    fn a_name_is_read_plain_or_from_the_default_locale() {
        use crate::themes::package::theme_manifest::tests::zip_of;

        let plain = zip_of(&[(MANIFEST_NAME, br#"{"name":" Plain "}"#)]);
        assert_eq!(Some("Plain".to_owned()), read_zip_name(&plain));
        let localized = zip_of(&[
            (
                MANIFEST_NAME,
                br#"{"name":"__MSG_extName__","default_locale":"en"}"#,
            ),
            (
                "_locales/en/messages.json",
                br#"{"EXTNAME":{"message":"Phantom Liberty Browser Theme"}}"#,
            ),
        ]);
        assert_eq!(
            Some("Phantom Liberty Browser Theme".to_owned()),
            read_zip_name(&localized),
            "the key matches whatever its case"
        );
        let unresolved = zip_of(&[(MANIFEST_NAME, br#"{"name":"__MSG_x__"}"#)]);
        assert_eq!(None, read_zip_name(&unresolved), "no locale, no name");
    }
}
