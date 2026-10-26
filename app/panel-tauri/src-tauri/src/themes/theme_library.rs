//! Each change is installed through Edge's own loader, persisted, and pushed to
//! the running Edge, so switching needs no restart.

use std::fs;
use std::path::{Path, PathBuf};
use std::thread;
use std::time::Duration;

use crate::dll::{dll_channel, dll_sync};
use crate::edge::edge_user_data;
use crate::platform::action_error::{ActionError, ActionResult};
use crate::platform::settings::{self, ColorMode, Settings, ThemeEntry, ThemeSource, Theming};
use crate::platform::{panel_log, paths};
use crate::themes::installed_themes::{self, InstalledTheme};
use crate::themes::theme_manifest::ThemeFileError;
use crate::themes::{crx, external_extensions, store_url, theme_packer};
use crate::ui::wording;

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

pub fn add_store(url: &str) -> ActionResult<()> {
    let Some(parsed) = store_url::parse(url) else {
        return Err(ActionError::User(wording::NOT_A_STORE_LINK.to_owned()));
    };
    external_extensions::install_store(&parsed.id, parsed.store)?;
    let name = parsed
        .name
        .or_else(|| installed_name(&parsed.id))
        .unwrap_or_else(|| parsed.id.clone());
    change(|theming| {
        theming.add_or_replace(ThemeEntry {
            id: parsed.id,
            name,
            source: ThemeSource::Store,
            origin: url.trim().to_owned(),
        });
    })
}

/// A folder, .zip or .crx.
pub fn add_local(path: &Path) -> ActionResult<()> {
    let local = local_theme_crx(path)?;
    let stored = stored_crx(&local.id);
    fs::create_dir_all(paths::themes_root())?;
    fs::write(&stored, &local.crx)?;
    external_extensions::install_local(&local.id, &stored, &local.version)?;
    let stem = path
        .file_stem()
        .map(|stem| stem.to_string_lossy().into_owned())
        .filter(|stem| !stem.is_empty());
    let name = stem.unwrap_or_else(|| local.id.clone());
    change(|theming| {
        theming.add_or_replace(ThemeEntry {
            id: local.id,
            name,
            source: ThemeSource::Local,
            origin: path.to_string_lossy().into_owned(),
        });
    })
}

pub fn remove(id: &str) -> ActionResult<()> {
    external_extensions::uninstall(id)?;
    change(|theming| theming.remove(id))
}

pub fn reorder(ids: &[String]) -> ActionResult<()> {
    change(|theming| theming.reorder(ids))
}

pub fn assign(mode: ColorMode, id: &str) -> ActionResult<()> {
    change(|theming| theming.assign(mode, id))
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

/// The panel's own themes are installed in Edge too; each is listed once.
#[must_use]
pub fn edge_themes_not_in(theming: &Theming) -> Vec<InstalledTheme> {
    installed_themes::scan(&edge_user_data::default_user_data())
        .into_iter()
        .filter(|theme| !theming.themes.iter().any(|own| own.id == theme.id))
        .collect()
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
    packed.map_err(|error| match error {
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

/// A store link without a slug names nothing itself.
fn installed_name(id: &str) -> Option<String> {
    installed_themes::scan(&edge_user_data::default_user_data())
        .into_iter()
        .find(|theme| theme.id == id)
        .map(|theme| theme.name)
}
