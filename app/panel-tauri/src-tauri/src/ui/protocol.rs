//! The messages between the core and its window, one JSON object per line on
//! the window's stdin and stdout. This file is their one definition: `cargo
//! test` exports them to ui/protocol.g.ts, which the window imports.

use serde::{Deserialize, Serialize};
#[cfg(test)]
use ts_rs::TS;

use crate::platform::settings::ColorMode;

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct CardWording {
    pub title: String,
    pub description: String,
    /// A Segoe Fluent Icons code point.
    pub glyph: u32,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct ThemeRow {
    pub id: String,
    pub name: String,
    pub badges: Vec<String>,
    pub remove_label: String,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct ThemeOption {
    pub id: String,
    pub label: String,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct ThemingLabels {
    pub store_link_placeholder: String,
    pub add: String,
    pub add_local: String,
    pub pick_file: String,
    pub pick_folder: String,
    pub when_light: String,
    pub when_dark: String,
    pub theme_list_name: String,
    pub reorder_help: String,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct ThemingView {
    pub labels: ThemingLabels,
    pub options: Vec<ThemeOption>,
    pub light: String,
    pub dark: String,
    pub own: Vec<ThemeRow>,
    pub edge: Vec<ThemeRow>,
    /// Shown in place of both lists while there are no themes.
    pub empty: Option<String>,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct InjectView {
    pub wording: CardWording,
    pub button: String,
    pub is_enabled: bool,
}

/// The card the Inject card appears under, and the offer it shows (None while
/// none is offered).
#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct InjectSlot {
    pub offer: Option<InjectView>,
}

/// Which switch a card is: echoed back in Action::SetSwitch.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub enum SwitchKey {
    NativeSplit,
    Theming,
    QuietDevMode,
    ProtocolHandlers,
    EnhancedFind,
    WebUiFollowsTheme,
    LivePreview,
    TabShape,
    ReadingTheme,
    StartWithWindows,
    AutoInject,
    AutoUpdate,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct CardView {
    pub key: SwitchKey,
    pub wording: CardWording,
    pub is_on: bool,
    pub is_locked: bool,
    pub note: String,
    pub theming: Option<ThemingView>,
    pub inject: Option<InjectSlot>,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct SectionView {
    pub name: String,
    pub cards: Vec<CardView>,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct UpdateView {
    pub wording: CardWording,
    pub button: String,
    pub restarting: String,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct FooterView {
    pub by: String,
    pub author: String,
    pub author_uri: String,
    pub version: String,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct View {
    pub product_name: String,
    pub open_edge_hint: Option<String>,
    pub build_notice: String,
    pub sections: Vec<SectionView>,
    pub update: Option<UpdateView>,
    pub footer: FooterView,
    pub drop_hint: String,
}

#[derive(Clone, Debug, PartialEq, Eq, Deserialize)]
#[serde(tag = "kind", rename_all = "camelCase", rename_all_fields = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub enum Action {
    SetSwitch { key: SwitchKey, is_on: bool },
    Inject,
    RestartToUpdate,
    AddStoreTheme { url: String },
    AddLocalTheme { path: String },
    RemoveTheme { id: String },
    ReorderThemes { ids: Vec<String> },
    /// Alt+Up (-1) or Alt+Down (1) on a theme in the list.
    MoveTheme { id: String, offset: i32 },
    AssignTheme { mode: ColorMode, id: String },
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[cfg_attr(test, derive(TS))]
pub struct Bounds {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(tag = "type", rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub enum CoreMessage {
    /// Before the first view, on a dev swap: where the window it replaces was.
    Place { bounds: Bounds },
    View { view: Box<View> },
    Reply { id: u32, error: Option<String> },
    Notice { text: String },
    Status { line: String },
    BringBack,
    Close,
}

#[derive(Clone, Debug, PartialEq, Eq, Deserialize)]
#[serde(tag = "type", rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub enum UiMessage {
    Refresh,
    Action { id: u32, action: Action },
    Bounds { bounds: Bounds },
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fmt::Write;
    use std::path::Path;

    /// Writes ui/protocol.g.ts from the types above; tools\lint.ps1 fails a
    /// committed copy that disagrees.
    #[test]
    fn export_the_window_types() {
        let mut generated = String::from(
            "// Generated from src-tauri/src/ui/protocol.rs by its export test (cargo test).\n\
             // Do not edit: change protocol.rs and run the test again.\n\n",
        );
        for declaration in [
            CardWording::decl(),
            ThemeRow::decl(),
            ThemeOption::decl(),
            ThemingLabels::decl(),
            ThemingView::decl(),
            InjectView::decl(),
            InjectSlot::decl(),
            SwitchKey::decl(),
            CardView::decl(),
            SectionView::decl(),
            UpdateView::decl(),
            FooterView::decl(),
            View::decl(),
            ColorMode::decl(),
            Action::decl(),
            Bounds::decl(),
            CoreMessage::decl(),
            UiMessage::decl(),
        ] {
            let _ = writeln!(generated, "export {declaration}\n");
        }
        let target = Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("..")
            .join("ui")
            .join("protocol.g.ts");
        let current = std::fs::read_to_string(&target).unwrap_or_default();
        if current != generated {
            std::fs::write(&target, generated).expect("write ui/protocol.g.ts");
        }
    }
}
