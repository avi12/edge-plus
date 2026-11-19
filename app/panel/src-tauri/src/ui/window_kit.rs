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

/// A JSON array, program first: the command a dev run starts as the window
/// (`pnpm dev:real` points it at a relay to the window it keeps up).
const COMMAND_VARIABLE: &str = "EDGEPLUS_WINDOW_COMMAND";
const NODE: &str = "node";
const NODE_FLAGS: [&str; 1] = ["--disable-warning=ExperimentalWarning"];
#[cfg(window_kit)]
const KIT_NODE: &str = "node.exe";

/// The kit's scripts the core starts as processes of their own.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum Entry {
    /// The window, which a dev run may start through `EDGEPLUS_WINDOW_COMMAND`.
    Window,
    /// The notification-area icon, kept for the core's whole life.
    Tray,
}

impl Entry {
    /// Under the checkout's `ui` folder.
    const fn source(self) -> &'static str {
        match self {
            Self::Window => "main.ts",
            Self::Tray => "tray/tray.ts",
        }
    }

    /// In the kit, as `pnpm build:window` names the bundle.
    #[cfg(window_kit)]
    const fn bundle(self) -> &'static str {
        match self {
            Self::Window => "main.mjs",
            Self::Tray => "tray.mjs",
        }
    }
}
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
pub(crate) struct WindowCommand {
    pub program: PathBuf,
    pub arguments: Vec<String>,
    /// The kit's folder, which holds the runtime packages; None in a dev run.
    pub kit: Option<PathBuf>,
}

/// The window's scripts in this checkout, when this exe was built from one.
#[must_use]
pub(crate) fn dev_sources() -> Option<PathBuf> {
    let checkout = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("..")
        .canonicalize()
        .ok()?;
    let sources = checkout.join("ui");
    // Both canonical, so both carry the same \\?\ prefix.
    let executable = std::env::current_exe().ok()?.canonicalize().ok()?;
    let is_dev_run =
        sources.join(Entry::Window.source()).exists() && executable.starts_with(&checkout);
    is_dev_run.then_some(sources)
}

#[must_use]
pub(crate) fn locate(entry: Entry) -> Option<WindowCommand> {
    if let Some(sources) = dev_sources() {
        if entry == Entry::Window
            && let Some(command) = command_from_environment()
        {
            return Some(command);
        }
        let mut arguments: Vec<String> = NODE_FLAGS.iter().map(|&flag| flag.to_owned()).collect();
        arguments.push(sources.join(entry.source()).to_string_lossy().into_owned());
        return Some(WindowCommand {
            program: PathBuf::from(NODE),
            arguments,
            kit: None,
        });
    }
    unpacked_kit(entry)
}

/// Only a dev run honors it: a published exe never starts what its environment
/// names.
fn command_from_environment() -> Option<WindowCommand> {
    let named = std::env::var(COMMAND_VARIABLE).ok()?;
    let parts: Vec<String> = match serde_json::from_str(&named) {
        Ok(parts) => parts,
        Err(error) => {
            panel_log::write_error("EDGEPLUS_WINDOW_COMMAND is not a JSON array", &error);
            return None;
        }
    };
    let (program, arguments) = parts.split_first()?;
    Some(WindowCommand {
        program: PathBuf::from(program),
        arguments: arguments.to_vec(),
        kit: None,
    })
}

#[cfg(window_kit)]
fn unpacked_kit(entry: Entry) -> Option<WindowCommand> {
    let folder = paths::window_root().join(KIT_NAME);
    if !folder.join(entry.bundle()).exists() {
        if let Err(error) = unpack(&folder) {
            panel_log::write_error("unpacking the window failed", &error);
            return None;
        }
    }
    remove_other_kits(&folder);
    Some(WindowCommand {
        program: folder.join(KIT_NODE),
        arguments: vec![folder.join(entry.bundle()).to_string_lossy().into_owned()],
        kit: Some(folder),
    })
}

#[cfg(not(window_kit))]
fn unpacked_kit(_entry: Entry) -> Option<WindowCommand> {
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
pub(crate) fn install_runtime(kit: &Path) -> io::Result<()> {
    let packages: Vec<PathBuf> = RUNTIME_PACKAGES
        .iter()
        .map(|name| kit.join(RUNTIME_FOLDER).join(name))
        .collect();
    let packages: Vec<&Path> = packages.iter().map(PathBuf::as_path).collect();
    edge_native::install_packages(&packages)
}
