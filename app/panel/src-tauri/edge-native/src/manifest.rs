//! Checking a local theme before Edge ever sees it, and stamping its version.
//!
//! A theme the user edits live is re-installed into the running Edge, so a
//! broken edit must be caught here rather than hot-swapped in. [`validate`]
//! rejects what Edge's theme loader would trip over (a manifest that is not a
//! theme, a missing or non-image file, colours out of range); [`revise`] gives
//! every re-pack a higher four-part version, because Edge only replaces an
//! installed extension with a newer one.

use std::io::{self, Cursor, Read, Write};

const MANIFEST: &str = "manifest.json";
const MAX_THEME_BYTES: u64 = 64 * 1024 * 1024;
const MAX_IMAGE_BYTES: u64 = 16 * 1024 * 1024;
const MAX_VERSION_PARTS: usize = 4;
const REVISION_PART: usize = 3;
const COLOR_CHANNEL_MAX: i64 = 255;
const TINT_UNCHANGED: f64 = -1.0;
const PNG_MAGIC: &[u8] = &[0x89, b'P', b'N', b'G'];
const JPEG_MAGIC: &[u8] = &[0xff, 0xd8, 0xff];

type Archive<'a> = zip::ZipArchive<Cursor<&'a [u8]>>;

fn invalid(message: impl Into<String>) -> io::Error {
    io::Error::new(io::ErrorKind::InvalidData, message.into())
}

fn open(zip_bytes: &[u8]) -> io::Result<Archive<'_>> {
    zip::ZipArchive::new(Cursor::new(zip_bytes)).map_err(|_| invalid("not a valid ZIP"))
}

fn read_entry(archive: &mut Archive<'_>, name: &str, limit: u64) -> io::Result<Vec<u8>> {
    let entry = archive
        .by_name(name)
        .map_err(|_| invalid(format!("{name} is missing")))?;
    let mut bytes = Vec::new();
    entry.take(limit + 1).read_to_end(&mut bytes)?;
    if bytes.len() as u64 > limit {
        return Err(invalid(format!("{name} is too large")));
    }
    Ok(bytes)
}

fn read_manifest(archive: &mut Archive<'_>) -> io::Result<serde_json::Value> {
    let text = read_entry(archive, MANIFEST, MAX_IMAGE_BYTES)?;
    serde_json::from_slice(&text).map_err(|error| invalid(format!("manifest.json: {error}")))
}

/// Check that the theme in `zip_bytes` is one Edge can load safely.
///
/// # Errors
/// Returns what is wrong, worded for the user.
pub fn validate(zip_bytes: &[u8]) -> io::Result<()> {
    let mut archive = open(zip_bytes)?;
    let total: u64 = (0..archive.len())
        .filter_map(|index| archive.by_index_raw(index).ok().map(|entry| entry.size()))
        .sum();
    if total > MAX_THEME_BYTES {
        return Err(invalid("the theme is too large"));
    }
    let manifest = read_manifest(&mut archive)?;
    if !manifest
        .get("name")
        .is_some_and(serde_json::Value::is_string)
    {
        return Err(invalid("manifest.json has no name"));
    }
    parse_version(&manifest)?;
    let theme = manifest
        .get("theme")
        .and_then(serde_json::Value::as_object)
        .ok_or_else(|| invalid("manifest.json has no \"theme\" section"))?;
    if let Some(images) = theme.get("images").and_then(serde_json::Value::as_object) {
        for (key, value) in images {
            let path = value
                .as_str()
                .ok_or_else(|| invalid(format!("image {key} is not a path")))?;
            check_image(&mut archive, key, path)?;
        }
    }
    if let Some(colors) = theme.get("colors").and_then(serde_json::Value::as_object) {
        for (key, value) in colors {
            check_color(key, value)?;
        }
    }
    if let Some(tints) = theme.get("tints").and_then(serde_json::Value::as_object) {
        for (key, value) in tints {
            check_tint(key, value)?;
        }
    }
    Ok(())
}

fn check_image(archive: &mut Archive<'_>, key: &str, path: &str) -> io::Result<()> {
    let bytes = read_entry(archive, path, MAX_IMAGE_BYTES)
        .map_err(|error| invalid(format!("image {key} ({path}): {error}")))?;
    let is_image = bytes.starts_with(PNG_MAGIC) || bytes.starts_with(JPEG_MAGIC);
    if is_image {
        Ok(())
    } else {
        Err(invalid(format!(
            "image {key} ({path}) is not a PNG or JPEG"
        )))
    }
}

fn check_color(key: &str, value: &serde_json::Value) -> io::Result<()> {
    let channels = value
        .as_array()
        .ok_or_else(|| invalid(format!("color {key} is not a list")))?;
    let is_rgb = channels.len() == 3;
    let is_rgba = channels.len() == 4;
    if !is_rgb && !is_rgba {
        return Err(invalid(format!("color {key} needs 3 or 4 numbers")));
    }
    for (index, channel) in channels.iter().enumerate() {
        let is_alpha = index == 3;
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
            return Err(invalid(format!("color {key} has a value out of range")));
        }
    }
    Ok(())
}

fn check_tint(key: &str, value: &serde_json::Value) -> io::Result<()> {
    let parts = value
        .as_array()
        .filter(|parts| parts.len() == 3)
        .ok_or_else(|| invalid(format!("tint {key} needs 3 numbers")))?;
    let is_valid = parts.iter().all(|part| {
        part.as_f64()
            .is_some_and(|number| number == TINT_UNCHANGED || (0.0..=1.0).contains(&number))
    });
    if is_valid {
        Ok(())
    } else {
        Err(invalid(format!("tint {key} has a value out of range")))
    }
}

fn parse_version(manifest: &serde_json::Value) -> io::Result<Vec<u16>> {
    let text = manifest
        .get("version")
        .and_then(serde_json::Value::as_str)
        .ok_or_else(|| invalid("manifest.json has no version"))?;
    let parts: Result<Vec<u16>, _> = text.split('.').map(str::parse).collect();
    match parts {
        Ok(parts) if !parts.is_empty() && parts.len() <= MAX_VERSION_PARTS => Ok(parts),
        _ => Err(invalid(format!("\"{text}\" is not a valid version"))),
    }
}

/// A copy of the theme ZIP whose manifest version ends in `revision`, and that
/// version. Only the first three parts of the original are kept.
///
/// # Errors
/// Returns an error if the ZIP or its manifest cannot be read or rewritten.
pub fn revise(zip_bytes: &[u8], revision: u16) -> io::Result<(Vec<u8>, String)> {
    let mut archive = open(zip_bytes)?;
    let mut manifest = read_manifest(&mut archive)?;
    let mut parts = parse_version(&manifest)?;
    parts.resize(REVISION_PART, 0);
    parts.push(revision);
    let version = parts
        .iter()
        .map(u16::to_string)
        .collect::<Vec<_>>()
        .join(".");
    manifest["version"] = serde_json::Value::String(version.clone());

    let mut writer = zip::ZipWriter::new(Cursor::new(Vec::new()));
    let options = zip::write::SimpleFileOptions::default();
    for index in 0..archive.len() {
        let entry = archive
            .by_index_raw(index)
            .map_err(|error| io::Error::other(error.to_string()))?;
        let is_manifest = entry.name().is_ok_and(|name| name == MANIFEST);
        if !is_manifest {
            writer
                .raw_copy_file(entry)
                .map_err(|error| io::Error::other(error.to_string()))?;
        }
    }
    writer
        .start_file(MANIFEST, options)
        .map_err(|error| io::Error::other(error.to_string()))?;
    writer.write_all(manifest.to_string().as_bytes())?;
    let finished = writer
        .finish()
        .map_err(|error| io::Error::other(error.to_string()))?;
    Ok((finished.into_inner(), version))
}
