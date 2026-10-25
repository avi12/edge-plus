// Builds the Tauri app and embeds the split DLL.
//
// The DLL is rebuilt (app/native-split/build.cmd) whenever any of its sources
// is newer than it, not only when it is missing: a stale one, carried silently,
// once replaced Edge's current DLL at panel start. The fresh DLL is copied into
// OUT_DIR, where dll/payload.rs embeds it.

use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::SystemTime;

const DLL_NAME: &str = "edgeplus-split.dll";
const SOURCE_EXTENSIONS: [&str; 2] = ["h", "cpp"];
const BUILD_FILES: [&str; 2] = ["edgeplus-split.buildscript", "build.cmd"];

fn main() {
    tauri_build::build();

    let manifest = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR"));
    let native_split = manifest.join("..").join("..").join("native-split");
    let dll = native_split.join("build").join(DLL_NAME);
    let sources = split_sources(&native_split);
    for source in &sources {
        println!("cargo:rerun-if-changed={}", source.display());
    }
    println!("cargo:rerun-if-changed={}", native_split.join("src").display());

    let is_stale = modified(&dll).is_none_or(|built| sources.iter().any(|source| modified(source).is_some_and(|changed| changed > built)));
    if is_stale {
        let status = Command::new("cmd")
            .args(["/c", "build.cmd"])
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
}

fn split_sources(native_split: &Path) -> Vec<PathBuf> {
    let mut sources: Vec<PathBuf> = BUILD_FILES.iter().map(|name| native_split.join(name)).collect();
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
    fs::metadata(path).and_then(|metadata| metadata.modified()).ok()
}
