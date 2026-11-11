//! Each change is installed through Edge's own loader, persisted, and pushed to
//! the running Edge, so switching needs no restart.

use std::collections::HashSet;
use std::fs;
use std::path::{Path, PathBuf};
use std::thread;
use std::time::Duration;

use crate::dll::{dll_channel, dll_sync, dll_wear, theme_hold};
use crate::edge::edge_user_data;
use crate::platform::actions::action_error::{ActionError, ActionResult};
use crate::platform::actions::panel_log;
use crate::platform::paths;
use crate::platform::settings::{self, ColorMode, Settings, ThemeEntry, ThemeSource, Theming};
use crate::themes::package::theme_manifest::ThemeFileError;
use crate::themes::package::{crx, edge_loader, theme_packer};
use crate::themes::sources::installed_themes::{self, InstalledTheme};
use crate::themes::sources::{external_extensions, store_package, store_url};
use crate::ui::wording;

/// Short, so the new theme is on screen for as short a time as can be: it is
/// put back the moment Edge reports wearing it.
/// How long a removal waits for Edge to move off the theme before uninstalling it.
const LEAVE_THEME_WAIT: Duration = Duration::from_secs(3);
const EDGE_SWITCH_WAIT: Duration = Duration::from_secs(6);
const INSTALL_WATCH_INTERVAL: Duration = Duration::from_millis(25);
const RENAME_WATCH_POLLS: u32 = 60;
const RENAME_WATCH_INTERVAL: Duration = Duration::from_millis(500);
const CRX_EXTENSION: &str = "crx";
/// Edge installs a re-installed local theme a moment later; the DLL is asked to
/// re-apply the active theme once that has landed.
const REAPPLY_DELAYS: [Duration; 3] = [
    Duration::from_millis(1500),
    Duration::from_millis(2500),
    Duration::from_millis(5000),
];

/// A local theme as Edge's loader takes it.
struct LocalCrx {
    id: String,
    version: String,
    crx: Vec<u8>,
}

/// `assign_to` is the Windows mode the new theme is set for once Edge wears it, when there is one; without
/// one the theme is installed and left off.
pub fn add_store(url: &str, assign_to: Option<ColorMode>) -> ActionResult<()> {
    let Some(parsed) = store_url::parse(url) else {
        return Err(ActionError::User(wording::NOT_A_STORE_LINK.to_owned()));
    };
    hold_unless_used(&parsed.id, assign_to);
    external_extensions::install_store(&parsed.id, parsed.store)?;
    // The link's slug may be cut short, so the name comes from the theme's own
    // manifest, read from its package in the store; the slug stands in when the
    // store cannot be reached, and the name is put right once Edge has it.
    let manifest_name = store_package::name(&parsed.id, parsed.store);
    let has_manifest_name = manifest_name.is_some();
    let name = manifest_name
        .or(parsed.name)
        .unwrap_or_else(|| parsed.id.clone());
    let id = parsed.id.clone();
    if !has_manifest_name {
        rename_when_installed(id.clone());
    }
    change(|theming| {
        theming.add_or_replace(ThemeEntry {
            id: parsed.id,
            name,
            source: ThemeSource::Store,
            origin: url.trim().to_owned(),
        });
    })?;
    if let Some(mode) = assign_to {
        assign_when_worn(id, mode);
    }
    Ok(())
}

/// A folder, .zip or .crx; `assign_to` as for `add_store`.
pub fn add_local(path: &Path, assign_to: Option<ColorMode>) -> ActionResult<()> {
    let local = local_theme_crx(path)?;
    let stored = stored_crx(&local.id);
    fs::create_dir_all(paths::themes_root())?;
    fs::write(&stored, &local.crx)?;
    hold_unless_used(&local.id, assign_to);
    external_extensions::install_local(&local.id, &stored, &local.version)?;
    let stem = path
        .file_stem()
        .map(|stem| stem.to_string_lossy().into_owned())
        .filter(|stem| !stem.is_empty());
    let name = manifest_name(&local.crx)
        .or(stem)
        .unwrap_or_else(|| local.id.clone());
    let id = local.id.clone();
    change(|theming| {
        theming.add_or_replace(ThemeEntry {
            id: local.id,
            name,
            source: ThemeSource::Local,
            origin: path.to_string_lossy().into_owned(),
        });
    })?;
    if let Some(mode) = assign_to {
        assign_when_worn(id, mode);
    }
    Ok(())
}

/// Edge turns on a theme it has just installed. A theme added without Shift is
/// to stay installed and off, so the DLL is told to keep it so, unless it is
/// one of the themes the panel already wears for a mode (installing it again is
/// an update, and must not take it off).
fn hold_unless_used(id: &str, assign_to: Option<ColorMode>) {
    let is_used_now = assign_to.is_some();
    if is_used_now {
        theme_hold::release(id);
    }
    let is_assigned = {
        let theming = Settings::load().theming;
        theming.light == id || theming.dark == id
    };
    if !is_used_now && !is_assigned {
        theme_hold::hold(id);
    }
}

/// With Shift, Edge's own behaviour is kept: it installs the theme and makes it
/// active, and the theme is set for the Windows mode as soon as Edge reports
/// wearing it, so a flip of the mode keeps it. A theme Edge already had is not
/// installed again and so is not switched to: when Edge has not worn it after
/// `EDGE_SWITCH_WAIT`, or there is no DLL in Edge to report, the theme is set
/// for the mode and Edge+ puts it on.
fn assign_when_worn(id: String, mode: ColorMode) {
    thread::spawn(move || {
        let polls = EDGE_SWITCH_WAIT.as_millis() / INSTALL_WATCH_INTERVAL.as_millis();
        for _ in 0..polls {
            thread::sleep(INSTALL_WATCH_INTERVAL);
            let is_worn =
                dll_channel::is_loaded() && worn_theme_id().as_deref() == Some(id.as_str());
            if is_worn {
                break;
            }
        }
        let _ = change(|theming| theming.assign(mode, &id));
    });
}

/// A store link names its theme by its slug, which the store may cut short;
/// once Edge has installed the theme, the name its own manifest gives replaces it.
fn rename_when_installed(id: String) {
    thread::spawn(move || {
        for _ in 0..RENAME_WATCH_POLLS {
            thread::sleep(RENAME_WATCH_INTERVAL);
            let Some(name) = installed_name(&id) else {
                continue;
            };
            let _ = change(|theming| {
                if let Some(theme) = theming.themes.iter_mut().find(|theme| theme.id == id) {
                    theme.name = name;
                }
            });
            return;
        }
    });
}

/// The name a theme's own manifest gives it.
fn manifest_name(crx_bytes: &[u8]) -> Option<String> {
    crx::zip(crx_bytes).ok().and_then(crx::read_zip_name)
}

/// A theme shows the name its manifest gives; the folder, file or link slug it
/// was added under is only the fallback, and themes added before that are
/// renamed here from their stored copy (local) or Edge's install (store).
pub fn sync_local_names() {
    let _gate = settings::gate();
    let mut settings = Settings::load();
    let mut is_changed = false;
    for theme in &mut settings.theming.themes {
        let found = match theme.source {
            ThemeSource::Local => fs::read(stored_crx(&theme.id))
                .ok()
                .and_then(|bytes| manifest_name(&bytes)),
            ThemeSource::Store => installed_name(&theme.id),
        };
        let Some(name) = found else {
            continue;
        };
        if theme.name != name {
            theme.name = name;
            is_changed = true;
        }
    }
    if !is_changed {
        return;
    }

    if let Err(error) = settings.save() {
        panel_log::write_error("renaming the local themes from their manifests", &error);
    }
}

pub fn remove(id: &str) -> ActionResult<()> {
    let installed = installed_ids();
    change(|theming| theming.remove(id, |earlier| installed.contains(earlier)))?;
    uninstall_once_not_worn(id)
}

const DEFAULT_PROFILE: &str = "Default";

/// The theme Edge wears now, the panel's own included; empty for Edge's default
/// theme, None when unknown. The DLL's report while it is in Edge (exact at
/// once), else what Edge's Preferences say it will wear.
#[must_use]
pub fn worn_theme_id() -> Option<String> {
    let reported = dll_channel::is_loaded()
        .then(dll_wear::read)
        .flatten()
        .map(|wear| wear.theme_id);
    reported.or_else(|| {
        let preferences = edge_user_data::read_json(
            &edge_user_data::default_user_data()
                .join(DEFAULT_PROFILE)
                .join(edge_user_data::PREFERENCES_FILE),
        )?;
        Some(installed_themes::active_theme_id(Some(&preferences)).unwrap_or_default())
    })
}

pub fn reorder(ids: &[String]) -> ActionResult<()> {
    change(|theming| theming.reorder(ids))
}

pub fn assign(mode: ColorMode, id: &str) -> ActionResult<()> {
    change(|theming| theming.assign(mode, id))
}

/// A theme whose files are gone is taken off light and dark, so Edge goes back to its default theme.
pub fn unassign(id: &str) -> ActionResult<()> {
    let installed = installed_ids();
    change(|theming| theming.unassign(id, |earlier| installed.contains(earlier)))
}

/// A theme that fails its checks is refused before anything is installed, so
/// what Edge has is left as it was.
pub fn refresh_local(id: &str, origin: &str) -> ActionResult<()> {
    let local = local_theme_crx(Path::new(origin))?;
    if local.id != id {
        return Err(ActionError::User(wording::IDENTITY_CHANGED.to_owned()));
    }
    let stored = stored_crx(id);
    fs::write(&stored, &local.crx)?;
    external_extensions::install_local(id, &stored, &local.version)?;
    {
        let _gate = settings::gate();
        dll_sync::apply(&Settings::load());
    }
    for delay in REAPPLY_DELAYS {
        thread::sleep(delay);
        dll_channel::signal_config_changed();
    }
    Ok(())
}

/// Edge reads its external-extensions key when it starts, so a theme whose key
/// went missing (something cleaned it, or Edge uninstalled it) is written back
/// from the settings and the stored copy, ahead of the next Edge start.
pub fn register_all(theming: &Theming) {
    for theme in &theming.themes {
        let registered = match theme.source {
            ThemeSource::Store => register_store(theme),
            ThemeSource::Local => register_local(&theme.id),
        };
        if let Err(error) = registered {
            panel_log::write_error(&format!("registering the theme {}", theme.name), &error);
        }
    }
}

fn register_store(theme: &ThemeEntry) -> std::io::Result<()> {
    match store_url::parse(&theme.origin) {
        Some(parsed) => external_extensions::install_store(&parsed.id, parsed.store),
        None => Ok(()),
    }
}

/// From the copy stored when it was added or last refreshed, so a theme whose
/// source folder has moved is still registered.
fn register_local(id: &str) -> std::io::Result<()> {
    let stored = stored_crx(id);
    let bytes = fs::read(&stored)?;
    let Ok(info) = crx::read(&bytes) else {
        return Ok(());
    };
    external_extensions::install_local(id, &stored, &info.version)
}

/// The panel's own themes are installed in Edge too; each is listed once, so
/// only the ones the panel does not own are returned, in Edge's scan order.
#[must_use]
pub fn edge_themes_not_in(theming: &Theming) -> Vec<InstalledTheme> {
    installed_themes::scan(&edge_user_data::default_user_data())
        .into_iter()
        .filter(|theme| !theming.themes.iter().any(|own| own.id == theme.id))
        .collect()
}

/// Only a theme Edge installed from the external-extensions key can be
/// uninstalled from here; Edge's own settings are never edited.
pub fn remove_edge(id: &str) -> ActionResult<()> {
    if !external_extensions::is_registered(id) {
        return Err(ActionError::User(
            wording::EDGE_THEME_NOT_REMOVABLE.to_owned(),
        ));
    }
    let installed = installed_ids();
    change(|theming| theming.forget_edge_theme(id, |earlier| installed.contains(earlier)))?;
    uninstall_once_not_worn(id)
}

fn stored_crx(id: &str) -> PathBuf {
    paths::themes_root().join(format!("{id}.{CRX_EXTENSION}"))
}

/// What is wrong with the theme is worded for the user; an I/O failure is
/// logged and said plainly.
fn local_theme_crx(source: &Path) -> ActionResult<LocalCrx> {
    let is_crx = source
        .extension()
        .is_some_and(|extension| extension.eq_ignore_ascii_case(CRX_EXTENSION));
    let packed = if is_crx {
        fs::read(source)
            .map_err(ThemeFileError::Unreadable)
            .and_then(|bytes| {
                let info = crx::read(&bytes)?;
                Ok(LocalCrx {
                    id: info.id,
                    version: info.version,
                    crx: bytes,
                })
            })
    } else {
        theme_packer::pack(source, &paths::theme_keys()).map(|packed| LocalCrx {
            id: packed.id,
            version: packed.version,
            crx: packed.crx,
        })
    };
    let checked = packed.and_then(|local| {
        edge_loader::check(crx::zip(&local.crx)?)?;
        Ok(local)
    });
    checked.map_err(|error| match error {
        ThemeFileError::Invalid(message) => ActionError::User(message),
        ThemeFileError::Unreadable(error) => {
            panel_log::write_error(
                &format!("reading the theme at {}", source.display()),
                &error,
            );
            ActionError::User(wording::THEME_FILES_UNREADABLE.to_owned())
        }
    })
}

fn change(edit: impl FnOnce(&mut Theming)) -> ActionResult<()> {
    let _gate = settings::gate();
    let mut settings = Settings::load();
    edit(&mut settings.theming);
    settings.save()?;
    dll_sync::apply(&settings);
    Ok(())
}

/// Edge goes back to its default theme the moment the theme it wears is
/// uninstalled. The settings were changed first, so the DLL is already putting
/// the theme the slot goes back to on; the uninstall waits until Edge has left
/// the removed theme, so it is never worn for the instant between.
fn uninstall_once_not_worn(id: &str) -> ActionResult<()> {
    if dll_channel::is_loaded() {
        let polls = LEAVE_THEME_WAIT.as_millis() / INSTALL_WATCH_INTERVAL.as_millis();
        for _ in 0..polls {
            if worn_theme_id().as_deref() != Some(id) {
                break;
            }
            thread::sleep(INSTALL_WATCH_INTERVAL);
        }
    }
    external_extensions::uninstall(id)?;
    Ok(())
}

/// The ids of the themes Edge has installed now: what a slot can go back to.
fn installed_ids() -> HashSet<String> {
    installed_themes::scan(&edge_user_data::default_user_data())
        .into_iter()
        .map(|theme| theme.id)
        .collect()
}

/// A store link without a slug names nothing itself.
fn installed_name(id: &str) -> Option<String> {
    installed_themes::scan(&edge_user_data::default_user_data())
        .into_iter()
        .find(|theme| theme.id == id)
        .map(|theme| theme.name)
}
