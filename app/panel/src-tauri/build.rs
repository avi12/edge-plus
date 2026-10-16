// Build the Tauri app and embed the native-split DLL.
//
// The C++ DLL has its own build (app/native-split/build.cmd, MSVC + ninja). This
// script does not own that build: it uses the already-built DLL, and only falls
// back to invoking build.cmd when the DLL is missing. The DLL is copied into
// OUT_DIR, where payload.rs embeds it with include_bytes!. Re-runs when the DLL
// changes, so a rebuilt DLL is re-embedded.

use std::path::PathBuf;
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
}
