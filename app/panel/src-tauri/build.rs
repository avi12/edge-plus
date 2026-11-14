//! Builds the Tauri app and embeds the split DLL.
//!
//! The DLL is rebuilt (app/dll/build.cmd) whenever any of its sources
//! is newer than it, not only when it is missing: a stale one, carried silently,
//! once replaced Edge's current DLL at panel start. The fresh DLL is copied into
//! `OUT_DIR`, where dll/payload.rs embeds it.

use std::fmt::Write;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::SystemTime;

use sha2::{Digest, Sha256};

const DLL_NAME: &str = "edgeplus-split.dll";
const SOURCE_EXTENSIONS: [&str; 2] = ["h", "cpp"];
const BUILD_FILES: [&str; 2] = ["edgeplus-split.buildscript", "build.cmd"];
const RELEASE_PROFILE: &str = "release";
const KIT_ENTRY: &str = "main.mjs";
const KIT_ARCHIVE: &str = "window-kit.zip";
/// The unpacked kit's folder is named for this much of the archive's digest.
const KIT_NAME_BYTES: usize = 8;

fn context<T, E: std::fmt::Display>(result: Result<T, E>, what: &str) -> Result<T, String> {
    result.map_err(|error| format!("{what}: {error}"))
}

fn main() -> Result<(), String> {
    tauri_build::build();

    let manifest = PathBuf::from(context(
        std::env::var("CARGO_MANIFEST_DIR"),
        "CARGO_MANIFEST_DIR",
    )?);
    let dll_folder = manifest.join("..").join("..").join("dll");
    let dll = dll_folder.join("build").join(DLL_NAME);
    let sources = split_sources(&dll_folder);
    for source in &sources {
        println!("cargo:rerun-if-changed={}", source.display());
    }
    println!(
        "cargo:rerun-if-changed={}",
        dll_folder.join("src").display()
    );

    let is_stale = modified(&dll).is_none_or(|built| {
        sources
            .iter()
            .any(|source| modified(source).is_some_and(|changed| changed > built))
    });
    if is_stale {
        // By its full path: cmd skips the current folder when
        // NoDefaultCurrentDirectoryInExePath is set.
        let status = context(
            Command::new("cmd")
                .arg("/c")
                .arg(dll_folder.join("build.cmd"))
                .current_dir(&dll_folder)
                .status(),
            "run app/dll/build.cmd",
        )?;
        assert!(status.success(), "app/dll/build.cmd failed");
    }
    assert!(
        dll.exists(),
        "edgeplus carries {DLL_NAME} inside itself and cannot find it at {}. Build it first: app\\dll\\build.cmd",
        dll.display()
    );
    let out_dir = PathBuf::from(context(std::env::var("OUT_DIR"), "OUT_DIR")?);
    context(
        fs::copy(&dll, out_dir.join(DLL_NAME)),
        "copy the split DLL into OUT_DIR",
    )?;
    println!("cargo:rerun-if-changed={}", dll.display());

    embed_window_kit(&manifest.join("..").join("dist").join("window"), &out_dir)
}

// A release build carries the window kit (`pnpm build:window`) when one is
// built; a dev build runs the checkout's TypeScript instead (window_kit.rs).
fn embed_window_kit(kit: &Path, out_dir: &Path) -> Result<(), String> {
    println!("cargo::rustc-check-cfg=cfg(window_kit)");
    println!("cargo:rerun-if-changed={}", kit.display());
    let is_release = std::env::var("PROFILE").is_ok_and(|profile| profile == RELEASE_PROFILE);
    if !is_release || !kit.join(KIT_ENTRY).exists() {
        return Ok(());
    }
    let archive = out_dir.join(KIT_ARCHIVE);
    let mut writer = zip::ZipWriter::new(context(
        fs::File::create(&archive),
        "create the kit archive",
    )?);
    let options = zip::write::SimpleFileOptions::default()
        .compression_method(zip::CompressionMethod::Deflated)
        .large_file(true);
    let mut files = Vec::new();
    collect_all(kit, &mut files)?;
    files.sort();
    for file in &files {
        let name = file
            .strip_prefix(kit)
            .map_err(|error| format!("a kit file lies inside the kit: {error}"))?
            .to_string_lossy()
            .replace('\\', "/");
        context(writer.start_file(name, options), "add a kit file")?;
        let mut source = context(fs::File::open(file), "read a kit file")?;
        context(std::io::copy(&mut source, &mut writer), "copy a kit file")?;
    }
    context(writer.finish(), "finish the kit archive")?;
    let digest = Sha256::digest(context(fs::read(&archive), "read the kit archive")?);
    let mut name = String::new();
    for byte in digest.iter().take(KIT_NAME_BYTES) {
        let _ = write!(name, "{byte:02x}");
    }
    println!("cargo:rustc-env=EDGEPLUS_WINDOW_KIT_NAME={name}");
    println!("cargo:rustc-cfg=window_kit");
    Ok(())
}

fn collect_all(folder: &Path, files: &mut Vec<PathBuf>) -> Result<(), String> {
    for entry in context(fs::read_dir(folder), "read the kit")?.flatten() {
        let path = entry.path();
        if path.is_dir() {
            collect_all(&path, files)?;
        } else {
            files.push(path);
        }
    }
    Ok(())
}

fn split_sources(dll_folder: &Path) -> Vec<PathBuf> {
    let mut sources: Vec<PathBuf> = BUILD_FILES
        .iter()
        .map(|name| dll_folder.join(name))
        .collect();
    collect(&dll_folder.join("src"), &mut sources);
    sources
}

fn collect(folder: &Path, sources: &mut Vec<PathBuf>) {
    let Ok(entries) = fs::read_dir(folder) else {
        return;
    };
    for entry in entries.flatten() {
        let path = entry.path();
        if path.is_dir() {
            collect(&path, sources);
            continue;
        }
        let is_source = path
            .extension()
            .and_then(|extension| extension.to_str())
            .is_some_and(|extension| SOURCE_EXTENSIONS.contains(&extension));
        if is_source {
            sources.push(path);
        }
    }
}

fn modified(path: &Path) -> Option<SystemTime> {
    fs::metadata(path)
        .and_then(|metadata| metadata.modified())
        .ok()
}
