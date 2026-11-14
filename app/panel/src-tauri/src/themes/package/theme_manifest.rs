//! A theme the user edits live is re-installed into the running Edge, so a
//! broken edit must be caught here rather than hot-swapped in. Its messages are
//! the user's.

use std::fmt;
use std::io::{self, Cursor, Read, Write};

use serde_json::{Map, Value};
use zip::ZipArchive;
use zip::result::ZipError;
use zip::write::{SimpleFileOptions, ZipWriter};

use crate::themes::package::manifest::{Manifest, Version};
use crate::themes::package::schema::{self, SchemaError};
use crate::ui::wording;

pub(crate) const MANIFEST_NAME: &str = "manifest.json";

/// The keys of an extension manifest that a theme is read through.
pub(crate) mod manifest_keys {
    pub(crate) const NAME: &str = "name";
    pub(crate) const VERSION: &str = "version";
    pub(crate) const THEME: &str = "theme";
    pub(crate) const DEFAULT_LOCALE: &str = "default_locale";
}

const MAX_THEME_BYTES: u64 = 64 * 1024 * 1024;
const MAX_IMAGE_BYTES: u64 = 16 * 1024 * 1024;
const REVISION_PART: usize = 3;
const VERSION_SEPARATOR: char = '.';
const PNG_MAGIC: &[u8] = &[0x89, b'P', b'N', b'G'];
const JPEG_MAGIC: &[u8] = &[0xff, 0xd8, 0xff];
const UTF8_BYTE_ORDER_MARK: &[u8] = &[0xef, 0xbb, 0xbf];

/// Why a theme's files could not be used.
#[derive(Debug)]
pub(crate) enum ThemeFileError {
    /// What is wrong with the theme, worded for the user (wording.rs).
    Invalid(String),
    /// The files could not be read or written; for the log.
    Unreadable(io::Error),
}

pub(crate) type ThemeFileResult<T> = Result<T, ThemeFileError>;

impl From<io::Error> for ThemeFileError {
    fn from(error: io::Error) -> Self {
        Self::Unreadable(error)
    }
}

impl fmt::Display for ThemeFileError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Invalid(message) => formatter.write_str(message),
            Self::Unreadable(error) => error.fmt(formatter),
        }
    }
}

pub(crate) fn invalid(message: impl Into<String>) -> ThemeFileError {
    ThemeFileError::Invalid(message.into())
}

type Archive<'a> = ZipArchive<Cursor<&'a [u8]>>;

/// Rejects what Edge's theme loader would trip over.
pub(crate) fn validate(zip_bytes: &[u8]) -> ThemeFileResult<()> {
    let mut archive = open(zip_bytes)?;
    let total: u64 = (0..archive.len())
        .filter_map(|index| archive.by_index_raw(index).ok().map(|entry| entry.size()))
        .sum();
    if total > MAX_THEME_BYTES {
        return Err(invalid(wording::THEME_TOO_LARGE));
    }
    let manifest = read_manifest(&mut archive)?;
    if manifest.name.is_none() {
        return Err(invalid(wording::MANIFEST_HAS_NO_NAME));
    }
    if manifest.format.is_none() {
        return Err(invalid(wording::MANIFEST_HAS_NO_MANIFEST_VERSION));
    }
    if manifest.version.is_none() {
        return Err(invalid(wording::MANIFEST_HAS_NO_VERSION));
    }
    let Some(theme) = manifest.theme else {
        return Err(invalid(wording::MANIFEST_HAS_NO_THEME));
    };
    for (key, path) in theme.images.iter().flatten() {
        check_image(&mut archive, key, path)?;
    }
    Ok(())
}

/// Every re-pack needs a higher version: Edge only replaces an installed
/// extension with a newer one. The original's first three parts are kept.
pub(crate) fn revise(zip_bytes: &[u8], revision: u16) -> ThemeFileResult<(Vec<u8>, String)> {
    let mut source = open(zip_bytes)?;
    let mut raw = read_manifest_object(&mut source)?;
    let Some(Version(mut parts)) = read_manifest(&mut source)?.version else {
        return Err(invalid(wording::MANIFEST_HAS_NO_VERSION));
    };
    parts.resize(REVISION_PART, 0);
    parts.push(revision);
    let version = parts
        .iter()
        .map(u16::to_string)
        .collect::<Vec<_>>()
        .join(&VERSION_SEPARATOR.to_string());
    raw.insert(
        manifest_keys::VERSION.to_owned(),
        Value::String(version.clone()),
    );

    let mut writer = ZipWriter::new(Cursor::new(Vec::new()));
    for index in 0..source.len() {
        let entry = source.by_index_raw(index).map_err(not_a_zip)?;
        let is_manifest = entry.name().is_ok_and(|name| name == MANIFEST_NAME);
        if is_manifest {
            continue;
        }
        writer.raw_copy_file(entry).map_err(zip_write_failed)?;
    }
    writer
        .start_file(MANIFEST_NAME, SimpleFileOptions::default())
        .map_err(zip_write_failed)?;
    writer.write_all(Value::Object(raw).to_string().as_bytes())?;
    let finished = writer.finish().map_err(zip_write_failed)?;
    Ok((finished.into_inner(), version))
}

fn open(zip_bytes: &[u8]) -> ThemeFileResult<Archive<'_>> {
    ZipArchive::new(Cursor::new(zip_bytes)).map_err(not_a_zip)
}

fn not_a_zip(_: ZipError) -> ThemeFileError {
    invalid(wording::NOT_A_ZIP)
}

/// Writing into memory fails only as an I/O failure would.
fn zip_write_failed(error: ZipError) -> ThemeFileError {
    ThemeFileError::Unreadable(io::Error::other(error))
}

fn read_entry(archive: &mut Archive<'_>, name: &str, limit: u64) -> ThemeFileResult<Vec<u8>> {
    let entry = match archive.by_name(name) {
        Ok(entry) => entry,
        Err(ZipError::FileNotFound) => return Err(invalid(wording::entry_missing(name))),
        Err(error) => return Err(not_a_zip(error)),
    };
    if entry.size() > limit {
        return Err(invalid(wording::entry_too_large(name)));
    }
    let mut bytes = Vec::new();
    entry
        .take(limit)
        .read_to_end(&mut bytes)
        .map_err(|_| invalid(wording::NOT_A_ZIP))?;
    Ok(bytes)
}

/// `Manifest.parse(manifest.json)`: what is wrong is worded for the user.
fn read_manifest(archive: &mut Archive<'_>) -> ThemeFileResult<Manifest> {
    let bytes = manifest_bytes(archive)?;
    schema::parse::<Manifest>(&bytes).map_err(|error| match error {
        SchemaError::Syntax { line } => invalid(wording::manifest_not_json_at(line)),
        SchemaError::Shape { path, .. } if path == "." => invalid(wording::MANIFEST_NOT_AN_OBJECT),
        shape @ SchemaError::Shape { .. } => invalid(shape.sentence()),
    })
}

/// The manifest as it is written, to be put back with one key changed.
fn read_manifest_object(archive: &mut Archive<'_>) -> ThemeFileResult<Map<String, Value>> {
    let bytes = manifest_bytes(archive)?;
    match serde_json::from_slice::<Value>(&bytes) {
        Ok(Value::Object(manifest)) => Ok(manifest),
        _ => Err(invalid(wording::MANIFEST_NOT_AN_OBJECT)),
    }
}

fn manifest_bytes(archive: &mut Archive<'_>) -> ThemeFileResult<Vec<u8>> {
    let bytes = read_entry(archive, MANIFEST_NAME, MAX_IMAGE_BYTES)?;
    let text = bytes.strip_prefix(UTF8_BYTE_ORDER_MARK).unwrap_or(&bytes);
    Ok(text.to_vec())
}

fn check_image(archive: &mut Archive<'_>, key: &str, path: &str) -> ThemeFileResult<()> {
    let bytes = match read_entry(archive, path, MAX_IMAGE_BYTES) {
        Ok(bytes) => bytes,
        Err(ThemeFileError::Invalid(reason)) => {
            return Err(invalid(wording::image_unreadable(key, path, &reason)));
        }
        Err(error) => return Err(error),
    };
    let is_image = bytes.starts_with(PNG_MAGIC) || bytes.starts_with(JPEG_MAGIC);
    if !is_image {
        return Err(invalid(wording::image_not_png_or_jpeg(key, path)));
    }
    Ok(())
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use crate::themes::package::manifest;

    /// A ZIP of `entries`, each a name and its bytes.
    pub(crate) fn zip_of(entries: &[(&str, &[u8])]) -> Vec<u8> {
        let mut writer = ZipWriter::new(Cursor::new(Vec::new()));
        for (name, bytes) in entries {
            writer
                .start_file(*name, SimpleFileOptions::default())
                .expect("start");
            writer.write_all(bytes).expect("write");
        }
        writer.finish().expect("finish").into_inner()
    }

    fn refusal(manifest: &str) -> Option<String> {
        match validate(&zip_of(&[(MANIFEST_NAME, manifest.as_bytes())])) {
            Err(ThemeFileError::Invalid(message)) => Some(message),
            _ => None,
        }
    }

    #[test]
    fn broken_themes_are_refused_with_a_reason() {
        let cases = [
            (
                r#"{"name":"t","manifest_version":3,"version":"1","theme":{"images":{"x":"gone.png"}}}"#,
                "image x (gone.png): gone.png is missing",
            ),
            (
                r#"{"name":"t","manifest_version":3,"version":"1","theme":{"images":{"x":"manifest.json"}}}"#,
                "image x (manifest.json) is not a PNG or JPEG",
            ),
            (
                r#"{"name":"t","manifest_version":3,"version":"1","theme":{"colors":{"frame":[300,0,0]}}}"#,
                "theme.colors.frame: has a value out of range",
            ),
            (
                r#"{"name":"t","manifest_version":3,"version":"1","theme":{"colors":{"frame":[1.5,0,0]}}}"#,
                "theme.colors.frame: has a value out of range",
            ),
            (
                r#"{"name":"t","manifest_version":3,"version":"1","theme":{"colors":{"frame":[0,0]}}}"#,
                "theme.colors.frame: needs 3 or 4 numbers",
            ),
            (
                r#"{"name":"t","manifest_version":3,"version":"1","theme":{"tints":{"b":[2,0,0]}}}"#,
                "theme.tints.b: has a value out of range",
            ),
            (
                r#"{"name":"t","manifest_version":3,"version":"1"}"#,
                "manifest.json has no \"theme\" section",
            ),
            (
                r#"{"name":"t","manifest_version":3,"version":"1.x","theme":{}}"#,
                "version: \"1.x\" is not a valid version",
            ),
            (
                r#"{"manifest_version":3,"version":"1","theme":{}}"#,
                "manifest.json has no name",
            ),
            (
                r#"{"name":"t","version":"1","theme":{}}"#,
                "manifest.json has no manifest_version",
            ),
            (
                r#"{"name":"t","manifest_version":1,"version":"1","theme":{}}"#,
                "manifest_version: must be 2 or 3",
            ),
            // Said plainly, with its line, not as the parser's message.
            (
                "{\n  \"name\": oops }",
                "manifest.json is not valid JSON (line 2)",
            ),
        ];
        for (manifest, reason) in cases {
            assert_eq!(Some(reason.to_owned()), refusal(manifest), "{manifest}");
        }
    }

    #[test]
    fn a_good_theme_passes_and_versions_are_digits_only() {
        let manifest = br#"{"name":"t","manifest_version":3,"version":"1","theme":{"colors":{"frame":[1,2,3,0.5]},"tints":{"buttons":[-1,0.5,1]}}}"#;
        assert!(validate(&zip_of(&[(MANIFEST_NAME, manifest)])).is_ok());
        assert_eq!(Some(12), manifest::digits("12"));
        assert_eq!(None, manifest::digits("+1"));
        assert_eq!(None, manifest::digits(""));
        assert_eq!(None, manifest::digits("70000"));
        assert_eq!(
            Some("version: \"1.2.3.4.5\" is not a valid version".to_owned()),
            refusal(r#"{"name":"t","manifest_version":3,"version":"1.2.3.4.5","theme":{}}"#)
        );
    }

    #[test]
    fn revise_stamps_the_revision_after_three_parts() {
        let zip = zip_of(&[
            (
                MANIFEST_NAME,
                br#"{"name":"z","manifest_version":3,"version":"2","theme":{}}"#,
            ),
            ("images/a.png", b"png"),
        ]);
        let (revised, version) = revise(&zip, 7).expect("revise");
        assert_eq!("2.0.0.7", version);
        let mut archive = ZipArchive::new(Cursor::new(revised.as_slice())).expect("zip");
        assert!(archive.by_name("images/a.png").is_ok());
        let mut text = String::new();
        archive
            .by_name(MANIFEST_NAME)
            .expect("manifest")
            .read_to_string(&mut text)
            .expect("read");
        assert!(text.contains("\"2.0.0.7\""));
    }
}
