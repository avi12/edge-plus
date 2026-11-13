//! Edge+ as an installed program, per user and without administrator rights:
//! an entry in Add or remove programs whose Uninstall runs the launcher with
//! `--uninstall`, a Start menu shortcut, and the removal of both together with
//! everything else Edge+ put on the machine. It is winui-svelte's `install` and
//! `uninstall` for an app whose always-on process is this core, run as the
//! lifecycle hooks `winui-svelte-launcher` calls (`--install`, `--updated`,
//! `--obsolete`, `--uninstall`).

use std::fs;
use std::io;
use std::path::{Path, PathBuf};
use std::time::Duration;

use edge_native::Hive;
use winui_svelte_launcher::{
    Application, LaunchFlags, Launcher, LifecycleEvent, STAGING_SUFFIX, Version, app_folder,
    dispatch, run_hook,
};

use crate::platform::identity::{app_identity, product};
use crate::platform::shared;
use crate::setup::hidden_powershell::{hidden_powershell, quoted};
use crate::setup::install_layout::{self, CORE_NAME, LAUNCHER_NAME};
use crate::setup::startup_entry::{self, StartupKeys};

/// How long the install hook of the new version may take.
const INSTALL_HOOK_TIMEOUT: Duration = Duration::from_secs(30);

/// What the launcher crate asks of Edge+: the program it starts, and what to do
/// at each moment of its life.
pub struct EdgePlus;

impl Application for EdgePlus {
    fn executable_name(&self) -> &str {
        CORE_NAME
    }

    fn on_lifecycle(&self, event: LifecycleEvent, version: &Version) -> io::Result<()> {
        match event {
            LifecycleEvent::Install | LifecycleEvent::Updated => {
                register(&install_layout::root(), *version)
            }
            LifecycleEvent::Obsolete => Ok(()),
            LifecycleEvent::Uninstall => uninstall(),
        }
    }
}

/// Runs the hook the arguments ask for; None is a normal start.
#[must_use]
pub fn dispatch_hook(arguments: &[String], running: &Version) -> Option<io::Result<()>> {
    dispatch(&EdgePlus, arguments, running)
}

const UNINSTALL_KEY: &str = r"Software\Microsoft\Windows\CurrentVersion\Uninstall\EdgePlus";
const TOAST_APP_KEY: &str = r"Software\Classes\AppUserModelId";
const START_MENU_PROGRAMS: &str = r"Microsoft\Windows\Start Menu\Programs";
const SHORTCUT_EXTENSION: &str = ".lnk";
/// Lets the unload signal reach the DLL, and Edge release it, before the root
/// is deleted.
const DLL_RELEASE_SECONDS: u32 = 3;

fn shortcut_path() -> PathBuf {
    let app_data = std::env::var_os("APPDATA").map_or_else(|| PathBuf::from("."), PathBuf::from);
    app_data
        .join(START_MENU_PROGRAMS)
        .join(format!("{}{SHORTCUT_EXTENSION}", product::NAME))
}

fn command_for(launcher: &Path, argument: &str) -> String {
    format!("\"{}\" \"{argument}\"", launcher.display())
}

/// Idempotent: run again after an update to refresh the version Add or remove
/// programs shows.
pub fn register(root: &Path, version: Version) -> io::Result<()> {
    let launcher = install_layout::launcher(root);
    let uninstall = command_for(&launcher, LifecycleEvent::Uninstall.switch());
    let icon = app_identity::icon_path();
    let values = [
        ("DisplayName", product::NAME.to_owned()),
        ("DisplayVersion", version.to_string()),
        ("Publisher", product::AUTHOR.to_owned()),
        ("InstallLocation", root.display().to_string()),
        ("DisplayIcon", icon.display().to_string()),
        ("UninstallString", uninstall.clone()),
        ("QuietUninstallString", uninstall),
    ];
    for (name, value) in values {
        edge_native::write_string(Hive::CurrentUser, UNINSTALL_KEY, name, &value)?;
    }
    startup_entry::repoint_to(&launcher)?;
    write_shortcut(&launcher, root, &icon)
}

fn write_shortcut(launcher: &Path, root: &Path, icon: &Path) -> io::Result<()> {
    let shortcut = shortcut_path();
    if let Some(folder) = shortcut.parent() {
        fs::create_dir_all(folder)?;
    }
    let script = format!(
        "$shell = New-Object -ComObject WScript.Shell\n\
         $link = $shell.CreateShortcut({})\n\
         $link.TargetPath = {}\n\
         $link.WorkingDirectory = {}\n\
         $link.IconLocation = {}\n\
         $link.Description = {}\n\
         $link.Save()\n",
        quoted(&shortcut.display().to_string()),
        quoted(&launcher.display().to_string()),
        quoted(&root.display().to_string()),
        quoted(&icon.display().to_string()),
        quoted(product::NAME),
    );
    if edge_native::start_detached(&hidden_powershell(&script)) {
        return Ok(());
    }
    Err(io::Error::other(
        "the Start menu shortcut could not be written",
    ))
}

/// Copies this exe and the launcher beside it into the root, runs the install
/// hook in the new version and starts it. Run by the exe a user downloaded.
pub fn install_here(version: Version) -> io::Result<()> {
    let executable = std::env::current_exe()?;
    let source = executable
        .parent()
        .ok_or_else(|| io::Error::other("the exe has no folder"))?;
    let root = install_layout::root();
    let launcher = Launcher::new(EdgePlus, root.clone());
    let staging = PathBuf::from(format!(
        "{}{STAGING_SUFFIX}",
        app_folder(&root, version).display()
    ));
    let _ = fs::remove_dir_all(&staging);
    fs::create_dir_all(&staging)?;
    fs::copy(&executable, staging.join(CORE_NAME))?;
    launcher.promote_staging(version)?;
    // The launcher may be running (a reinstall); a stale one still works.
    let _ = fs::copy(source.join(LAUNCHER_NAME), install_layout::launcher(&root));
    let installed = app_folder(&root, version).join(CORE_NAME);
    // A hook that fails must not stop the first start.
    let _ = run_hook(
        &installed,
        LifecycleEvent::Install,
        &version,
        INSTALL_HOOK_TIMEOUT,
    );
    launcher.forward(
        &[],
        LaunchFlags {
            is_first_run: true,
            is_restarted: false,
        },
    )?;
    Ok(())
}

/// What `uninstall` hands to a detached PowerShell: end every process running
/// from the root (Edge+'s own, never Edge's), tell a loaded DLL to unload
/// itself from Edge, which keeps running, then delete the root.
#[must_use]
pub fn removal_script(root: &Path) -> String {
    format!(
        "$root = {}\n\
         Get-CimInstance Win32_Process | Where-Object {{ $_.ExecutablePath -and \
         $_.ExecutablePath.StartsWith($root + '\\', [StringComparison]::OrdinalIgnoreCase) }} | \
         ForEach-Object {{ Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }}\n\
         Start-Sleep -Seconds 1\n\
         try {{ [System.Threading.EventWaitHandle]::OpenExisting({}).Set() | Out-Null }} catch {{ }}\n\
         Start-Sleep -Seconds {DLL_RELEASE_SECONDS}\n\
         Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue\n",
        quoted(&root.display().to_string()),
        quoted(shared::UNLOAD_EVENT),
    )
}

/// Removes the startup entry, the Add or remove programs entry, the shortcut and
/// the toast registration, then leaves the deletion of the root, which holds the
/// settings and the loaded DLL, to a script that outlives this process.
pub fn uninstall() -> io::Result<()> {
    let root = install_layout::root();
    startup_entry::disable(&StartupKeys::user(), startup_entry::NAME)?;
    edge_native::delete_tree(Hive::CurrentUser, UNINSTALL_KEY)?;
    edge_native::delete_tree(
        Hive::CurrentUser,
        &format!(r"{TOAST_APP_KEY}\{}", app_identity::APP_ID),
    )?;
    let _ = fs::remove_file(shortcut_path());
    if edge_native::start_detached(&hidden_powershell(&removal_script(&root))) {
        return Ok(());
    }
    Err(io::Error::other("the removal could not be started"))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn removal_ends_only_processes_from_the_root_then_unloads_the_dll() {
        let script = removal_script(Path::new(r"C:\Users\it's\AppData\Local\EdgePlus"));
        assert!(
            script.contains(r"$root = 'C:\Users\it''s\AppData\Local\EdgePlus'"),
            "the root, quoted"
        );
        let stop = script.find("Stop-Process").expect("stops processes");
        let unload = script
            .find(r"OpenExisting('Local\edgeplus-split-unload')")
            .expect("signals the DLL");
        let delete = script.find("Remove-Item").expect("deletes the root");
        assert!(
            stop < unload && unload < delete,
            "stop first so the watcher cannot reinject, then unload, then delete"
        );
        assert!(
            script.contains("StartsWith($root"),
            "only what runs from the root is stopped"
        );
        assert!(
            !script.to_lowercase().contains("msedge"),
            "Edge is never named"
        );
    }

    #[test]
    fn the_uninstall_entry_runs_the_launcher_with_the_switch() {
        assert_eq!(
            r#""C:\root\edgeplus-launcher.exe" "--uninstall""#,
            command_for(
                Path::new(r"C:\root\edgeplus-launcher.exe"),
                LifecycleEvent::Uninstall.switch()
            ),
        );
    }
}
