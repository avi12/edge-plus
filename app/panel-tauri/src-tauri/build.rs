// Builds the Tauri app and embeds the split DLL.
//
// The DLL is rebuilt (app/native-split/build.cmd) whenever any of its sources
// is newer than it, not only when it is missing: a stale one, carried silently,
// once replaced Edge's current DLL at panel start. The fresh DLL is copied into
// OUT_DIR, where dll/payload.rs embeds it.

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

fn main() {
    tauri_build::build();

    let manifest = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR"));
    let native_split = manifest.join("..").join("..").join("native-split");
    let dll = native_split.join("build").join(DLL_NAME);
    let sources = split_sources(&native_split);
    for source in &sources {
        println!("cargo:rerun-if-changed={}", source.display());
    }
    println!(
        "cargo:rerun-if-changed={}",
        native_split.join("src").display()
    );

    let is_stale = modified(&dll).is_none_or(|built| {
        sources
            .iter()
            .any(|source| modified(source).is_some_and(|changed| changed > built))
    });
    if is_stale {
        // By its full path: cmd skips the current folder when
        // NoDefaultCurrentDirectoryInExePath is set.
        let status = Command::new("cmd")
            .arg("/c")
            .arg(native_split.join("build.cmd"))
            .current_dir(&native_split)
            .status()
            .expect("run app/native-split/build.cmd");
        assert!(status.success(), "app/native-split/build.cmd failed");
    }
    assert!(
        dll.exists(),
        "edgeplus carries {DLL_NAME} inside itself and cannot find it at {}. Build it first: app\\native-split\\build.cmd",
        dll.display()
    );
    let out_dir = PathBuf::from(std::env::var("OUT_DIR").expect("OUT_DIR"));
    fs::copy(&dll, out_dir.join(DLL_NAME)).expect("copy the split DLL into OUT_DIR");
    println!("cargo:rerun-if-changed={}", dll.display());

    embed_window_kit(&manifest.join("..").join("dist").join("window"), &out_dir);
}

// A release build carries the window kit (`pnpm build:window`) when one is
// built; a dev build runs the checkout's TypeScript instead (window_kit.rs).
fn embed_window_kit(kit: &Path, out_dir: &Path) {
    println!("cargo::rustc-check-cfg=cfg(window_kit)");
    println!("cargo:rerun-if-changed={}", kit.display());
    let is_release = std::env::var("PROFILE").is_ok_and(|profile| profile == RELEASE_PROFILE);
    if !is_release || !kit.join(KIT_ENTRY).exists() {
        return;
    }
    let archive = out_dir.join(KIT_ARCHIVE);
    let mut writer =
        zip::ZipWriter::new(fs::File::create(&archive).expect("create the kit archive"));
    let options = zip::write::SimpleFileOptions::default()
        .compression_method(zip::CompressionMethod::Deflated)
        .large_file(true);
    let mut files = Vec::new();
    collect_all(kit, &mut files);
    files.sort();
    for file in &files {
        let name = file
            .strip_prefix(kit)
            .expect("inside the kit")
            .to_string_lossy()
            .replace('\\', "/");
        writer.start_file(name, options).expect("add a kit file");
        std::io::copy(
            &mut fs::File::open(file).expect("read a kit file"),
            &mut writer,
        )
        .expect("copy a kit file");
    }
    writer.finish().expect("finish the kit archive");
    let digest = Sha256::digest(fs::read(&archive).expect("read the kit archive"));
    let mut name = String::new();
    for byte in &digest[..KIT_NAME_BYTES] {
        let _ = write!(name, "{byte:02x}");
    }
    println!("cargo:rustc-env=EDGEPLUS_WINDOW_KIT_NAME={name}");
    println!("cargo:rustc-cfg=window_kit");
}

fn collect_all(folder: &Path, files: &mut Vec<PathBuf>) {
    for entry in fs::read_dir(folder).expect("read the kit").flatten() {
        let path = entry.path();
        if path.is_dir() {
            collect_all(&path, files);
        } else {
            files.push(path);
        }
    }
}

fn split_sources(native_split: &Path) -> Vec<PathBuf> {
    let mut sources: Vec<PathBuf> = BUILD_FILES
        .iter()
        .map(|name| native_split.join(name))
        .collect();
    collect(&native_split.join("src"), &mut sources);
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
