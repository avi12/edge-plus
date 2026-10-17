// Build the Tauri app and embed the native-split DLL.
//
// The C++ DLL has its own build (app/native-split/build.cmd, MSVC + ninja). This
// script does not own that build: it uses the already-built DLL, and only falls
// back to invoking build.cmd when the DLL is missing. The DLL is copied into
// OUT_DIR, where payload.rs embeds it with include_bytes!. Re-runs when the DLL
// changes, so a rebuilt DLL is re-embedded.

use std::path::{Path, PathBuf};
use std::process::Command;

fn main() {
    tauri_build::build();

    let manifest = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR"));
    let native_split = manifest.join("..").join("..").join("native-split");
    let dll = native_split.join("build").join("fixedge-split.dll");

    if !dll.exists() {
        let _ = Command::new("cmd")
            .args(["/c", "build.cmd"])
            .current_dir(&native_split)
            .status();
    }
    assert!(
        dll.exists(),
        "native-split DLL not found at {} - build it with app/native-split/build.cmd",
        dll.display()
    );

    let out_dir = PathBuf::from(std::env::var("OUT_DIR").expect("OUT_DIR"));
    std::fs::copy(&dll, out_dir.join("fixedge-split.dll")).expect("copy fixedge-split.dll");

    println!("cargo:rerun-if-changed={}", dll.display());

    export_pinned_build(&native_split.join("src").join("offsets.h"));
}

// The DLL is pinned to one msedge.dll build, named in offsets.h (its single
// home, the one verify.h checks). Hand that build and module name to the panel
// as compile-time env vars (read in src/edge_build.rs), so the panel compares
// against exactly what the embedded DLL was reversed against.
fn export_pinned_build(offsets: &Path) {
    let source = std::fs::read_to_string(offsets).expect("read offsets.h");
    let version = [
        "kVersionMajor",
        "kVersionMinor",
        "kVersionBuild",
        "kVersionRevision",
    ]
    .map(|name| constant_value(&source, name))
    .join(".");
    let module = constant_value(&source, "kModule");
    let module = module.trim_start_matches('L').trim_matches('"');
    println!("cargo:rustc-env=FIXEDGE_PINNED_EDGE_BUILD={version}");
    println!("cargo:rustc-env=FIXEDGE_PINNED_EDGE_MODULE={module}");
    println!("cargo:rerun-if-changed={}", offsets.display());
}

// The right-hand side of `inline constexpr <type> <name>[...] = <value>;`.
fn constant_value(source: &str, name: &str) -> String {
    source
        .lines()
        .map(str::trim)
        .filter(|line| line.starts_with("inline constexpr"))
        .find_map(|line| {
            let (declaration, value) = line.split_once('=')?;
            let declared = declaration.split_whitespace().last()?;
            let is_named = declared.trim_end_matches("[]") == name;
            is_named.then(|| value.trim().trim_end_matches(';').trim().to_owned())
        })
        .unwrap_or_else(|| panic!("offsets.h has no constant {name}"))
}
