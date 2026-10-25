//! A theme the user edits live is re-installed into the running Edge, so a
//! broken edit must be caught here rather than hot-swapped in. Its messages are
//! the user's.

use std::fmt;
use std::io::{self, Cursor, Read, Write};

use serde_json::{Map, Value};
use zip::ZipArchive;
use zip::result::ZipError;
use zip::write::{SimpleFileOptions, ZipWriter};

use crate::ui::wording;

pub const MANIFEST_NAME: &str = "manifest.json";

/// The keys of an extension manifest that a theme is read through.
pub mod manifest_keys {
    pub const NAME: &str = "name";
    pub const VERSION: &str = "version";
    pub const THEME: &str = "theme";
    pub const IMAGES: &str = "images";
    pub const COLORS: &str = "colors";
    pub const TINTS: &str = "tints";
    pub const DEFAULT_LOCALE: &str = "default_locale";
}

const MAX_THEME_BYTES: u64 = 64 * 1024 * 1024;
const MAX_IMAGE_BYTES: u64 = 16 * 1024 * 1024;
const MAX_VERSION_PARTS: usize = 4;
const REVISION_PART: usize = 3;
const VERSION_SEPARATOR: char = '.';
const COLOR_CHANNEL_MAX: i64 = 255;
const RGB_CHANNELS: usize = 3;
const RGBA_CHANNELS: usize = 4;
const ALPHA_INDEX: usize = 3;
const TINT_PARTS: usize = 3;
const TINT_UNCHANGED: f64 = -1.0;
const PNG_MAGIC: &[u8] = &[0x89, b'P', b'N', b'G'];
const JPEG_MAGIC: &[u8] = &[0xff, 0xd8, 0xff];
const UTF8_BYTE_ORDER_MARK: &[u8] = &[0xef, 0xbb, 0xbf];

/// Why a theme's files could not be used.
#[derive(Debug)]
pub enum ThemeFileError {
    /// What is wrong with the theme, worded for the user (wording.rs).
    Invalid(String),
    /// The files could not be read or written; for the log.
    Unreadable(io::Error),
}

pub type ThemeFileResult<T> = Result<T, ThemeFileError>;

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

pub fn invalid(message: impl Into<String>) -> ThemeFileError {
    ThemeFileError::Invalid(message.into())
}

/// Digits only, as a version part or a revision count is written: no sign, no
/// space.
#[must_use]
pub fn parse_digits(text: &str) -> Option<u16> {
    let is_digits = !text.is_empty() && text.bytes().all(|byte| byte.is_ascii_digit());
    if !is_digits {
        return None;
    }
    text.parse().ok()
}

type Archive<'a> = ZipArchive<Cursor<&'a [u8]>>;

/// Rejects what Edge's theme loader would trip over.
pub fn validate(zip_bytes: &[u8]) -> ThemeFileResult<()> {
    let mut archive = open(zip_bytes)?;
    let total: u64 = (0..archive.len())
        .filter_map(|index| archive.by_index_raw(index).ok().map(|entry| entry.size()))
        .sum();
    if total > MAX_THEME_BYTES {
        return Err(invalid(wording::THEME_TOO_LARGE));
    }
    let manifest = read_manifest(&mut archive)?;
    let has_name = manifest
        .get(manifest_keys::NAME)
        .is_some_and(Value::is_string);
    if !has_name {
        return Err(invalid(wording::MANIFEST_HAS_NO_NAME));
    }
    parse_version(&manifest)?;
    let Some(theme) = manifest
        .get(manifest_keys::THEME)
        .and_then(Value::as_object)
    else {
        return Err(invalid(wording::MANIFEST_HAS_NO_THEME));
    };
    if let Some(images) = theme.get(manifest_keys::IMAGES).and_then(Value::as_object) {
        for (key, value) in images {
            let Some(path) = value.as_str() else {
                return Err(invalid(wording::image_not_a_path(key)));
            };
            check_image(&mut archive, key, path)?;
        }
    }
    if let Some(colors) = theme.get(manifest_keys::COLORS).and_then(Value::as_object) {
        for (key, value) in colors {
            check_color(key, value)?;
        }
    }
    if let Some(tints) = theme.get(manifest_keys::TINTS).and_then(Value::as_object) {
        for (key, value) in tints {
            check_tint(key, value)?;
        }
    }
    Ok(())
}

/// Every re-pack needs a higher version: Edge only replaces an installed
/// extension with a newer one. The original's first three parts are kept.
pub fn revise(zip_bytes: &[u8], revision: u16) -> ThemeFileResult<(Vec<u8>, String)> {
    let mut source = open(zip_bytes)?;
    let mut manifest = read_manifest(&mut source)?;
    let mut parts = parse_version(&manifest)?;
    parts.resize(REVISION_PART, 0);
    parts.push(revision);
    let version = parts
        .iter()
        .map(u16::to_string)
        .collect::<Vec<_>>()
        .join(&VERSION_SEPARATOR.to_string());
    manifest.insert(
        manifest_keys::VERSION.to_owned(),
        Value::String(version.clone()),
    );

    let mut writer = ZipWriter::new(Cursor::new(Vec::new()));
    for index in 0..source.len() {
        let entry = source.by_index_raw(index).map_err(not_a_zip)?;
        let is_manifest = entry.name() == MANIFEST_NAME;
        if is_manifest {
            continue;
        }
        writer.raw_copy_file(entry).map_err(zip_write_failed)?;
    }
    writer
        .start_file(MANIFEST_NAME, SimpleFileOptions::default())
        .map_err(zip_write_failed)?;
    writer.write_all(Value::Object(manifest).to_string().as_bytes())?;
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

fn read_manifest(archive: &mut Archive<'_>) -> ThemeFileResult<Map<String, Value>> {
    let bytes = read_entry(archive, MANIFEST_NAME, MAX_IMAGE_BYTES)?;
    let text = bytes.strip_prefix(UTF8_BYTE_ORDER_MARK).unwrap_or(&bytes);
    match serde_json::from_slice::<Value>(text) {
        Ok(Value::Object(manifest)) => Ok(manifest),
        Ok(_) => Err(invalid(wording::MANIFEST_NOT_AN_OBJECT)),
        // The parser's own message is for a developer; its position is kept.
        Err(error) => Err(invalid(wording::manifest_not_json_at(error.line()))),
    }
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

fn check_color(key: &str, value: &Value) -> ThemeFileResult<()> {
    let Some(channels) = value.as_array() else {
        return Err(invalid(wording::color_not_a_list(key)));
    };
    let is_rgb = channels.len() == RGB_CHANNELS;
    let is_rgba = channels.len() == RGBA_CHANNELS;
    if !is_rgb && !is_rgba {
        return Err(invalid(wording::color_wrong_count(key)));
    }
    for (index, channel) in channels.iter().enumerate() {
        let is_alpha = index == ALPHA_INDEX;
        // A fraction is not a channel: 255.0 has no integer value here.
        let is_valid = if is_alpha {
            channel
                .as_f64()
                .is_some_and(|alpha| (0.0..=1.0).contains(&alpha))
        } else {
            channel
                .as_i64()
                .is_some_and(|byte| (0..=COLOR_CHANNEL_MAX).contains(&byte))
        };
        if !is_valid {
            return Err(invalid(wording::color_out_of_range(key)));
        }
    }
    Ok(())
}

fn check_tint(key: &str, value: &Value) -> ThemeFileResult<()> {
    let parts = value.as_array().filter(|parts| parts.len() == TINT_PARTS);
    let Some(parts) = parts else {
        return Err(invalid(wording::tint_wrong_count(key)));
    };
    let is_valid = parts.iter().all(|part| {
        part.as_f64().is_some_and(|number| {
            let is_unchanged = (number - TINT_UNCHANGED).abs() < f64::EPSILON;
            is_unchanged || (0.0..=1.0).contains(&number)
        })
    });
    if !is_valid {
        return Err(invalid(wording::tint_out_of_range(key)));
    }
    Ok(())
}

fn parse_version(manifest: &Map<String, Value>) -> ThemeFileResult<Vec<u16>> {
    let Some(text) = manifest.get(manifest_keys::VERSION).and_then(Value::as_str) else {
        return Err(invalid(wording::MANIFEST_HAS_NO_VERSION));
    };
    let pieces: Vec<&str> = text.split(VERSION_SEPARATOR).collect();
    if pieces.len() > MAX_VERSION_PARTS {
        return Err(invalid(wording::not_a_valid_version(text)));
    }
    pieces
        .into_iter()
        .map(parse_digits)
        .collect::<Option<Vec<u16>>>()
        .ok_or_else(|| invalid(wording::not_a_valid_version(text)))
}

#[cfg(test)]
pub mod tests {
    use super::*;

    /// A ZIP of `entries`, each a name and its bytes.
    pub fn zip_of(entries: &[(&str, &[u8])]) -> Vec<u8> {
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
                r#"{"name":"t","version":"1","theme":{"images":{"x":"gone.png"}}}"#,
                "image x (gone.png): gone.png is missing",
            ),
            (
                r#"{"name":"t","version":"1","theme":{"images":{"x":"manifest.json"}}}"#,
                "image x (manifest.json) is not a PNG or JPEG",
            ),
            (
                r#"{"name":"t","version":"1","theme":{"colors":{"frame":[300,0,0]}}}"#,
                "color frame has a value out of range",
            ),
            (
                r#"{"name":"t","version":"1","theme":{"colors":{"frame":[1.5,0,0]}}}"#,
                "color frame has a value out of range",
            ),
            (
                r#"{"name":"t","version":"1","theme":{"colors":{"frame":[0,0]}}}"#,
                "color frame needs 3 or 4 numbers",
            ),
            (
                r#"{"name":"t","version":"1","theme":{"tints":{"b":[2,0,0]}}}"#,
                "tint b has a value out of range",
            ),
            (
                r#"{"name":"t","version":"1"}"#,
                "manifest.json has no \"theme\" section",
            ),
            (
                r#"{"name":"t","version":"1.x","theme":{}}"#,
                "\"1.x\" is not a valid version",
            ),
            (r#"{"version":"1","theme":{}}"#, "manifest.json has no name"),
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
        let manifest = br#"{"name":"t","version":"1","theme":{"colors":{"frame":[1,2,3,0.5]},"tints":{"buttons":[-1,0.5,1]}}}"#;
        assert!(validate(&zip_of(&[(MANIFEST_NAME, manifest)])).is_ok());
        assert_eq!(Some(12), parse_digits("12"));
        assert_eq!(None, parse_digits("+1"));
        assert_eq!(None, parse_digits(""));
        assert_eq!(None, parse_digits("70000"));
        assert_eq!(
            Some("\"1.2.3.4.5\" is not a valid version".to_owned()),
            refusal(r#"{"name":"t","version":"1.2.3.4.5","theme":{}}"#)
        );
    }

    #[test]
    fn revise_stamps_the_revision_after_three_parts() {
        let zip = zip_of(&[
            (MANIFEST_NAME, br#"{"name":"z","version":"2","theme":{}}"#),
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
