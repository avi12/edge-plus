//! Edge's external-extension loader installs only a .crx. The signing key is
//! kept per source path, so adding the same theme again keeps its extension id.
//!
//! The key file holds Windows CNG's `RSAPRIVATEBLOB`, the format the earlier
//! panels wrote into the same folder, so a theme they packed keeps its id; keep
//! the format and the file names.

use std::fs;
use std::io::{self, Cursor, Write};
use std::path::{Path, PathBuf};

use edge_native::RsaKey;
use sha2::{Digest, Sha256};
use zip::write::{SimpleFileOptions, ZipWriter};

use crate::themes::package::crx::{self, CrxHeaderField, KeyProofField, SignedDataField};
use crate::themes::package::extension_id;
use crate::themes::package::protobuf::length_delimited_field;
use crate::themes::package::theme_manifest::{self, ThemeFileResult, invalid};
use crate::ui::wording;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PackedTheme {
    pub id: String,
    pub version: String,
    pub crx: Vec<u8>,
}

const KEY_BITS: u32 = 2048;
const CRX_ID_BYTES: usize = 16;
const KEY_FILE_NAME_BYTES: usize = 8;
const KEY_EXTENSION: &str = "key";
const REVISION_EXTENSION: &str = "rev";
const ZIP_SEPARATOR: &str = "/";

/// `source` is a folder, or a .zip with its manifest.json at the top.
pub fn pack(source: &Path, key_directory: &Path) -> ThemeFileResult<PackedTheme> {
    let zip_bytes = if source.is_dir() {
        zip_folder(source)?
    } else {
        fs::read(source)?
    };
    theme_manifest::validate(&zip_bytes)?;
    fs::create_dir_all(key_directory)?;
    let key_file = key_directory.join(key_file_name(source));
    let (zip, version) = theme_manifest::revise(&zip_bytes, next_revision(&key_file)?)?;
    let signer = load_or_create_key(&key_file)?;
    let public_key = signer.public_key_der()?;
    let id = extension_id::from_public_key(&public_key);

    let crx_id = &Sha256::digest(&public_key)[..CRX_ID_BYTES];
    let signed_header_data = length_delimited_field(SignedDataField::CrxId, crx_id);
    let signature = signer.sign_sha256(&signed_payload(&signed_header_data, &zip)?)?;

    let mut proof = length_delimited_field(KeyProofField::PublicKey, &public_key);
    proof.extend_from_slice(&length_delimited_field(
        KeyProofField::Signature,
        &signature,
    ));
    let mut header = length_delimited_field(CrxHeaderField::RsaProof, &proof);
    header.extend_from_slice(&length_delimited_field(
        CrxHeaderField::SignedHeaderData,
        &signed_header_data,
    ));
    let mut packed = crx::MAGIC.to_vec();
    packed.extend_from_slice(&crx::FORMAT_VERSION.to_le_bytes());
    packed.extend_from_slice(&little_endian_length(header.len())?);
    packed.extend_from_slice(&header);
    packed.extend_from_slice(&zip);
    Ok(PackedTheme {
        id,
        version,
        crx: packed,
    })
}

/// What a CRX3 signature covers: the fixed prefix, the signed header data's
/// length and bytes, then the ZIP.
pub fn signed_payload(signed_header_data: &[u8], zip: &[u8]) -> ThemeFileResult<Vec<u8>> {
    let mut payload = crx::SIGNED_DATA_PREFIX.to_vec();
    payload.extend_from_slice(&little_endian_length(signed_header_data.len())?);
    payload.extend_from_slice(signed_header_data);
    payload.extend_from_slice(zip);
    Ok(payload)
}

#[must_use]
pub fn key_file_name(source: &Path) -> String {
    let normalized = source.to_string_lossy().to_lowercase();
    let hash = Sha256::digest(normalized.as_bytes());
    let mut prefix = [0u8; KEY_FILE_NAME_BYTES];
    prefix.copy_from_slice(&hash[..KEY_FILE_NAME_BYTES]);
    format!("{:016x}.{KEY_EXTENSION}", u64::from_be_bytes(prefix))
}

fn little_endian_length(length: usize) -> ThemeFileResult<[u8; 4]> {
    u32::try_from(length)
        .map(u32::to_le_bytes)
        .map_err(|_| invalid(wording::THEME_TOO_LARGE))
}

/// Each pack gets a higher version, which is what makes Edge replace the
/// installed copy.
fn next_revision(key_file: &Path) -> io::Result<u16> {
    let counter_file = key_file.with_extension(REVISION_EXTENSION);
    let previous = if counter_file.exists() {
        theme_manifest::parse_digits(fs::read_to_string(&counter_file)?.trim()).unwrap_or(0)
    } else {
        0
    };
    let next = previous.checked_add(1).unwrap_or(1);
    fs::write(&counter_file, next.to_string())?;
    Ok(next)
}

fn load_or_create_key(key_file: &Path) -> io::Result<RsaKey> {
    if key_file.exists() {
        return RsaKey::import_private_blob(&fs::read(key_file)?);
    }
    let key = RsaKey::generate(KEY_BITS)?;
    fs::write(key_file, key.export_private_blob()?)?;
    Ok(key)
}

fn zip_folder(root: &Path) -> ThemeFileResult<Vec<u8>> {
    let mut files = Vec::new();
    collect_files(root, &mut files)?;
    let mut writer = ZipWriter::new(Cursor::new(Vec::new()));
    for file in files {
        let Ok(relative) = file.strip_prefix(root) else {
            continue;
        };
        let name = relative
            .iter()
            .map(|part| part.to_string_lossy())
            .collect::<Vec<_>>()
            .join(ZIP_SEPARATOR);
        writer
            .start_file(name, SimpleFileOptions::default())
            .map_err(io::Error::other)?;
        writer.write_all(&fs::read(&file)?)?;
    }
    Ok(writer.finish().map_err(io::Error::other)?.into_inner())
}

fn collect_files(folder: &Path, files: &mut Vec<PathBuf>) -> io::Result<()> {
    for entry in fs::read_dir(folder)? {
        let path = entry?.path();
        if path.is_dir() {
            collect_files(&path, files)?;
        } else {
            files.push(path);
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use std::io::Read;

    use zip::ZipArchive;

    use super::*;
    use crate::themes::package::theme_manifest::MANIFEST_NAME;
    use crate::themes::package::theme_manifest::tests::zip_of;

    const PNG: &[u8] = &[0x89, b'P', b'N', b'G', 1, 2, 3];

    fn temporary_folder(purpose: &str) -> PathBuf {
        let folder =
            std::env::temp_dir().join(format!("edgeplus-{purpose}-{}", std::process::id()));
        let _ = fs::remove_dir_all(&folder);
        fs::create_dir_all(&folder).expect("temporary folder");
        folder
    }

    #[test]
    fn a_packed_folder_reads_back_signed_and_stable() {
        let root = temporary_folder("pack");
        let theme = root.join("theme");
        fs::create_dir_all(theme.join("images")).expect("folder");
        fs::write(
            theme.join(MANIFEST_NAME),
            r#"{"manifest_version":3,"name":"t","version":"1.2.3","theme":{"images":{"theme_frame":"images/frame.png"},"colors":{"frame":[1,2,3,0.5]},"tints":{"buttons":[-1,0.5,1]}}}"#,
        )
        .expect("manifest");
        fs::write(theme.join("images").join("frame.png"), PNG).expect("image");
        let keys = root.join("keys");

        let first = pack(&theme, &keys).expect("pack");
        let info = crx::read(&first.crx).expect("read");
        assert_eq!(first.id, info.id, "the id reads back from the CRX");
        assert_eq!("1.2.3.1", info.version, "the first pack is revision 1");
        assert_eq!(
            "1.2.3.1", first.version,
            "the packer reports the stamped version"
        );

        let proof = crx::read_proof(&first.crx).expect("proof");
        let key_file = keys.join(key_file_name(&theme));
        let key = RsaKey::import_private_blob(&fs::read(key_file).expect("key file")).expect("key");
        assert_eq!(
            key.public_key_der().expect("public key"),
            proof.public_key,
            "the header carries the kept key"
        );
        let payload = signed_payload(
            &proof.signed_header_data,
            crx::zip(&first.crx).expect("zip"),
        )
        .expect("payload");
        let is_signed = key
            .verify_sha256(&payload, &proof.signature)
            .expect("verify");
        assert!(
            is_signed,
            "the signature verifies against the header's own key"
        );
        let crx_id = &Sha256::digest(&proof.public_key)[..CRX_ID_BYTES];
        assert!(
            proof.signed_header_data.ends_with(crx_id),
            "the signed header names the key's crx id"
        );

        let second = pack(&theme, &keys).expect("pack again");
        assert_eq!(first.id, second.id, "packing again keeps the id");
        assert_eq!(
            "1.2.3.2", second.version,
            "packing again raises the revision"
        );
        let zip_bytes = crx::zip(&second.crx).expect("zip");
        let mut archive = ZipArchive::new(Cursor::new(zip_bytes)).expect("archive");
        let mut image = Vec::new();
        archive
            .by_name("images/frame.png")
            .expect("folders are zipped with '/'")
            .read_to_end(&mut image)
            .expect("image");
        assert_eq!(PNG, image);
        fs::remove_dir_all(root).expect("clean up");
    }

    #[test]
    fn a_zip_packs_like_a_folder() {
        let root = temporary_folder("zip");
        let zip_path = root.join("theme.zip");
        fs::write(
            &zip_path,
            zip_of(&[(MANIFEST_NAME, br#"{"name":"z","version":"2","theme":{}}"#)]),
        )
        .expect("zip");
        let packed = pack(&zip_path, &root.join("keys")).expect("pack");
        assert_eq!(
            "2.0.0.1", packed.version,
            "a short version is padded to three parts"
        );
        fs::remove_dir_all(root).expect("clean up");
    }

    #[test]
    fn the_key_file_is_named_by_the_lower_cased_path() {
        let upper = key_file_name(Path::new(r"C:\Themes\Dusk"));
        assert_eq!(
            upper,
            key_file_name(Path::new(r"c:\themes\dusk")),
            "case does not matter"
        );
        let name = Path::new(&upper);
        let is_hex = name
            .file_stem()
            .and_then(|stem| stem.to_str())
            .is_some_and(|stem| {
                stem.len() == 16 && stem.chars().all(|digit| digit.is_ascii_hexdigit())
            });
        assert!(is_hex, "sixteen hex digits");
        assert_eq!(
            Some(KEY_EXTENSION),
            name.extension().and_then(|extension| extension.to_str())
        );
    }
}
