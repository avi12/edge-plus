//! Where an installed Edge+ lives: one root per user (`%LOCALAPPDATA%\EdgePlus`)
//! holding a stable launcher and one `app-x.y.z` folder per version. The layout,
//! the versions and the staging rename are `winui-svelte-launcher`'s; this is
//! only the app's own names for them.

use std::path::{Path, PathBuf};

use winui_svelte_launcher::install_root_of;

use crate::platform::paths;

pub const CORE_NAME: &str = "edgeplus.exe";
pub const LAUNCHER_NAME: &str = "edgeplus-launcher.exe";

#[must_use]
pub fn root() -> PathBuf {
    paths::data_root()
}

/// The stable program in the root that the Start menu, the Add or remove
/// programs entry and the Run entry start.
#[must_use]
pub fn launcher(root: &Path) -> PathBuf {
    root.join(LAUNCHER_NAME)
}

/// True when this exe runs from an `app-x.y.z` folder of the root: the one
/// install that updates itself and can be uninstalled.
#[must_use]
pub fn is_installed_run() -> bool {
    let Ok(executable) = std::env::current_exe() else {
        return false;
    };
    let installed_root = executable.parent().and_then(install_root_of);
    installed_root.is_some_and(|installed| {
        paths::fold_case(&installed.to_string_lossy())
            == paths::fold_case(&root().to_string_lossy())
    })
}
