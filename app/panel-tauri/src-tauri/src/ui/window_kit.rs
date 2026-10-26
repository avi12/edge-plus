//! Where the window's process comes from. A dev run starts the checkout's
//! TypeScript with the Node on PATH. A published exe carries the window kit
//! (`pnpm build:window`: Node, the bundled scripts, dynwinrt, the Windows App
//! runtime's packages), which build.rs zips into the exe and the first window
//! unpacks under the data folder, once per kit.

#[cfg(window_kit)]
use std::fs;
use std::io;
use std::path::{Path, PathBuf};

use crate::platform::actions::panel_log;
use crate::platform::paths;

const NODE: &str = "node";
const NODE_FLAGS: [&str; 1] = ["--disable-warning=ExperimentalWarning"];
const SOURCE_ENTRY: &str = "main.ts";
#[cfg(window_kit)]
const KIT_NODE: &str = "node.exe";
#[cfg(window_kit)]
const KIT_ENTRY: &str = "main.mjs";
const RUNTIME_FOLDER: &str = "runtime";
/// In install order: the framework before the packages that depend on it.
const RUNTIME_PACKAGES: [&str; 4] = [
    "Microsoft.WindowsAppRuntime.2.msix",
    "Microsoft.WindowsAppRuntime.DDLM.2.msix",
    "Microsoft.WindowsAppRuntime.Main.2.msix",
    "Microsoft.WindowsAppRuntime.Singleton.2.msix",
];

#[cfg(window_kit)]
const KIT: &[u8] = include_bytes!(concat!(env!("OUT_DIR"), "/window-kit.zip"));
#[cfg(window_kit)]
const KIT_NAME: &str = env!("EDGEPLUS_WINDOW_KIT_NAME");

/// The program to start and its arguments.
pub struct WindowCommand {
    pub program: PathBuf,
    pub arguments: Vec<String>,
    /// The kit's folder, which holds the runtime packages; None in a dev run.
    pub kit: Option<PathBuf>,
}

/// The window's scripts in this checkout, when this exe was built from one.
#[must_use]
pub fn dev_sources() -> Option<PathBuf> {
    let checkout = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("..")
        .canonicalize()
        .ok()?;
    let sources = checkout.join("ui");
    // Both canonical, so both carry the same \\?\ prefix.
    let executable = std::env::current_exe().ok()?.canonicalize().ok()?;
    let is_dev_run = sources.join(SOURCE_ENTRY).exists() && executable.starts_with(&checkout);
    is_dev_run.then_some(sources)
}

#[must_use]
pub fn locate() -> Option<WindowCommand> {
    if let Some(sources) = dev_sources() {
        let mut arguments: Vec<String> = NODE_FLAGS.iter().map(|&flag| flag.to_owned()).collect();
        arguments.push(sources.join(SOURCE_ENTRY).to_string_lossy().into_owned());
        return Some(WindowCommand {
            program: PathBuf::from(NODE),
            arguments,
            kit: None,
        });
    }
    unpacked_kit()
}

#[cfg(window_kit)]
fn unpacked_kit() -> Option<WindowCommand> {
    let folder = paths::window_root().join(KIT_NAME);
    if !folder.join(KIT_ENTRY).exists() {
        if let Err(error) = unpack(&folder) {
            panel_log::write_error("unpacking the window failed", &error);
            return None;
        }
    }
    remove_other_kits(&folder);
    Some(WindowCommand {
        program: folder.join(KIT_NODE),
        arguments: vec![folder.join(KIT_ENTRY).to_string_lossy().into_owned()],
        kit: Some(folder),
    })
}

#[cfg(not(window_kit))]
fn unpacked_kit() -> Option<WindowCommand> {
    panel_log::write(&format!(
        "this exe carries no window kit and runs outside its checkout ({})",
        paths::window_root().display()
    ));
    None
}

/// Into a sibling folder first, renamed into place once whole, so a cut-short
/// unpack is never taken for a kit.
#[cfg(window_kit)]
fn unpack(folder: &Path) -> io::Result<()> {
    let mut partial = folder.as_os_str().to_owned();
    partial.push(".part");
    let partial = PathBuf::from(partial);
    let _ = fs::remove_dir_all(&partial);
    zip::ZipArchive::new(io::Cursor::new(KIT))
        .and_then(|mut archive| archive.extract(&partial))
        .map_err(io::Error::other)?;
    fs::rename(&partial, folder)
}

/// An older kit still running keeps its files, so its removal waits for a
/// later start.
#[cfg(window_kit)]
fn remove_other_kits(current: &Path) {
    let Ok(entries) = fs::read_dir(paths::window_root()) else {
        return;
    };
    for entry in entries.flatten() {
        let path = entry.path();
        if path != current {
            let _ = fs::remove_dir_all(&path);
        }
    }
}

/// Installs the Windows App runtime the kit carries, for this user.
pub fn install_runtime(kit: &Path) -> io::Result<()> {
    let packages: Vec<PathBuf> = RUNTIME_PACKAGES
        .iter()
        .map(|name| kit.join(RUNTIME_FOLDER).join(name))
        .collect();
    let packages: Vec<&Path> = packages.iter().map(PathBuf::as_path).collect();
    edge_native::install_packages(&packages)
}
