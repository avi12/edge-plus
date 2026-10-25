//! Each change is installed through Edge's own loader, persisted, and pushed to
//! the running Edge, so switching needs no restart.

use std::path::Path;

use crate::platform::action_error::ActionResult;
use crate::platform::settings::{ColorMode, Theming};
use crate::themes::installed_themes::InstalledTheme;

pub fn add_store(url: &str) -> ActionResult<()> {
    let _ = url;
    todo!()
}

/// A folder, .zip or .crx.
pub fn add_local(path: &Path) -> ActionResult<()> {
    let _ = path;
    todo!()
}

pub fn remove(id: &str) -> ActionResult<()> {
    let _ = id;
    todo!()
}

pub fn reorder(ids: &[String]) -> ActionResult<()> {
    let _ = ids;
    todo!()
}

pub fn assign(mode: ColorMode, id: &str) -> ActionResult<()> {
    let _ = (mode, id);
    todo!()
}

/// The panel's own themes are installed in Edge too; each is listed once.
#[must_use]
pub fn edge_themes_not_in(theming: &Theming) -> Vec<InstalledTheme> {
    let _ = theming;
    todo!()
}
