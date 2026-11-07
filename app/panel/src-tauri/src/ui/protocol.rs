//! The messages between the core and its window, one JSON object per line on
//! the window's stdin and stdout. This file is their one definition: `cargo
//! test` exports them to ui/protocol.g.ts, which the window imports.

use serde::{Deserialize, Serialize};
#[cfg(test)]
use ts_rs::TS;

use crate::platform::settings::{ColorMode, Feature};
use crate::ui::inject_offer::InjectOffer;

/// What a theme row says about the theme, beside its name.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub enum ThemeBadge {
    Store,
    Local,
    InUse,
    InstalledInEdge,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct ThemeRow {
    pub id: String,
    pub name: String,
    pub badges: Vec<ThemeBadge>,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct ThemeOption {
    pub id: String,
    pub name: String,
}

/// The theme library. `options` are the themes a picker offers after Edge's
/// default theme, whose id is empty.
#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct ThemingView {
    pub options: Vec<ThemeOption>,
    pub light: String,
    pub dark: String,
    pub own: Vec<ThemeRow>,
    pub edge: Vec<ThemeRow>,
}

/// Which switch a card is: echoed back in `Action::SetSwitch`.
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
    CyberpunkSkin,
    ReadingTheme,
    StartWithWindows,
    AutoInject,
    AutoUpdate,
}

/// The Edge build the panel reads, and whether it is newer than the one Edge+
/// supports.
#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct EdgeBuild {
    pub version: String,
    pub is_updated: bool,
}

/// Why a card's switch is held.
#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(
    tag = "kind",
    rename_all = "camelCase",
    rename_all_fields = "camelCase"
)]
#[cfg_attr(test, derive(TS))]
pub enum CardHold {
    /// On, and held until Edge+ supports this build.
    Paused { edge: EdgeBuild },
    /// Off, and cannot be turned on until Edge+ supports this build.
    Unavailable { edge: EdgeBuild },
    /// Works only while this feature is on, which it is not.
    Needs { feature: Feature },
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct CardView {
    pub key: SwitchKey,
    pub is_on: bool,
    pub is_locked: bool,
    pub hold: Option<CardHold>,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct UpdateView {
    pub version: String,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct FooterView {
    pub author: String,
    pub author_uri: String,
    pub version: String,
}

/// The state the window renders; its words are the window's own.
#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub struct View {
    pub product_name: String,
    /// The id the core registered (name and icon) for its toasts; the window
    /// takes it too, so the taskbar shows it as Edge+.
    pub app_user_model_id: String,
    /// The icon file the core wrote, for the title bar and the taskbar.
    pub icon: String,
    pub is_edge_running: bool,
    /// None when the build cannot be read.
    pub edge: Option<EdgeBuild>,
    /// The features this build pauses.
    pub paused: Vec<Feature>,
    pub cards: Vec<CardView>,
    pub theming: ThemingView,
    pub inject: InjectOffer,
    pub update: Option<UpdateView>,
    pub footer: FooterView,
}

#[derive(Clone, Debug, PartialEq, Eq, Deserialize)]
#[serde(
    tag = "kind",
    rename_all = "camelCase",
    rename_all_fields = "camelCase"
)]
#[cfg_attr(test, derive(TS))]
pub enum Action {
    SetSwitch {
        key: SwitchKey,
        is_on: bool,
    },
    Inject,
    RestartToUpdate,
    AddStoreTheme {
        url: String,
    },
    AddLocalTheme {
        path: String,
    },
    RemoveTheme {
        id: String,
    },
    ReorderThemes {
        ids: Vec<String>,
    },
    /// Alt+Up (-1) or Alt+Down (1) on a theme in the list.
    MoveTheme {
        id: String,
        offset: i32,
    },
    AssignTheme {
        mode: ColorMode,
        id: String,
    },
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[cfg_attr(test, derive(TS))]
pub struct Bounds {
    pub x: i32,
    pub y: i32,
    pub width: i32,
    pub height: i32,
}

/// What the core asks of the window. On the wire a request's `type` is the
/// variant's camelCase name and its `data` the payload (absent for none).
#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(tag = "type", content = "data", rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub enum CoreMessage {
    /// Before the first view, on a dev swap: where the window it replaces was.
    Place(Bounds),
    View(Box<View>),
    Notice(String),
    Status(String),
    BringBack,
    Close,
}

/// What the window asks of the core.
#[derive(Clone, Debug, PartialEq, Eq, Deserialize)]
#[serde(tag = "type", content = "data", rename_all = "camelCase")]
#[cfg_attr(test, derive(TS))]
pub enum UiMessage {
    /// Sent instead of opening when the Windows App runtime is not installed;
    /// the window's process then ends.
    RuntimeMissing,
    Refresh,
    Action(Action),
    Bounds(Bounds),
}

/// A request frame: `{"id":N,"type":..,"data":..,"timestamp":ms}`.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct RequestFrame<Message> {
    pub id: u32,
    #[serde(flatten)]
    pub message: Message,
    #[serde(default)]
    pub timestamp: u64,
}

/// A response frame: `{"id":N,"response":..}` or `{"id":N,"error":".."}`.
#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct ResponseFrame {
    pub id: u32,
    #[serde(flatten)]
    pub outcome: Outcome,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(untagged)]
pub enum Outcome {
    Answered { response: serde_json::Value },
    Failed { error: String },
}

impl ResponseFrame {
    /// The answer to a request that returns nothing: `response: null`, or the
    /// error's sentence.
    #[must_use]
    pub fn of(id: u32, error: Option<String>) -> Self {
        let outcome = match error {
            Some(error) => Outcome::Failed { error },
            None => Outcome::Answered {
                response: serde_json::Value::Null,
            },
        };
        Self { id, outcome }
    }
}

/// One line from the window: a request to answer, or the answer to one of the
/// core's own requests (which the core does not track).
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Incoming {
    Request(RequestFrame<UiMessage>),
    Response,
}

/// A frame with a `type` is a request; one without answers a request.
pub fn parse_incoming(line: &str) -> Result<Incoming, serde_json::Error> {
    let value: serde_json::Value = serde_json::from_str(line)?;
    let is_request = value.get("type").is_some();
    if is_request {
        serde_json::from_value(value).map(Incoming::Request)
    } else {
        Ok(Incoming::Response)
    }
}

/// The wire names (`type`) and TypeScript data types of every message, kept
/// beside the enums; a test checks them against the enums' own declarations.
#[cfg(test)]
const CORE_MESSAGE_TYPES: [(&str, &str); 6] = [
    ("place", "Bounds"),
    ("view", "View"),
    ("notice", "string"),
    ("status", "string"),
    ("bringBack", "void"),
    ("close", "void"),
];

#[cfg(test)]
const UI_MESSAGE_TYPES: [(&str, &str); 4] = [
    ("runtimeMissing", "void"),
    ("refresh", "void"),
    ("action", "Action"),
    ("bounds", "Bounds"),
];

#[cfg(test)]
mod tests {
    use super::*;
    use std::fmt::Write;
    use std::path::Path;

    fn message_types(constant: &str, table: &[(&str, &str)]) -> String {
        let mut text = format!("export const {constant} = {{\n");
        for (name, _) in table {
            let _ = writeln!(text, "  {name}: \"{name}\",");
        }
        text.push_str("} as const;\n\n");
        text
    }

    /// Writes ui/protocol.g.ts from the types above; tools\lint.ps1 fails a
    /// committed copy that disagrees.
    #[test]
    fn export_the_window_types() {
        let mut generated = String::from(
            "// Generated from src-tauri/src/ui/protocol.rs by its export test (cargo test).\n\
             // Do not edit: change protocol.rs and run the test again.\n\n\
             import type { ProtocolWithReturn } from \"winui-svelte/messaging\";\n\n",
        );
        for declaration in [
            Feature::decl(),
            ThemeBadge::decl(),
            ThemeRow::decl(),
            ThemeOption::decl(),
            ThemingView::decl(),
            SwitchKey::decl(),
            EdgeBuild::decl(),
            CardHold::decl(),
            CardView::decl(),
            InjectOffer::decl(),
            UpdateView::decl(),
            FooterView::decl(),
            View::decl(),
            ColorMode::decl(),
            Action::decl(),
            Bounds::decl(),
        ] {
            let _ = writeln!(generated, "export {declaration}\n");
        }
        generated.push_str(&message_types("CoreMessageType", &CORE_MESSAGE_TYPES));
        generated.push_str(&message_types("UiMessageType", &UI_MESSAGE_TYPES));
        generated.push_str("export interface EdgePlusProtocolMap {\n");
        for (constant, table) in [
            ("CoreMessageType", CORE_MESSAGE_TYPES.as_slice()),
            ("UiMessageType", UI_MESSAGE_TYPES.as_slice()),
        ] {
            for (name, data) in table {
                let _ = writeln!(
                    generated,
                    "  [{constant}.{name}]: ProtocolWithReturn<{data}, void>;"
                );
            }
        }
        generated.push_str("}\n");
        let target = Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("..")
            .join("ui")
            .join("protocol.g.ts");
        let current = std::fs::read_to_string(&target).unwrap_or_default();
        if current != generated {
            std::fs::write(&target, generated).expect("write ui/protocol.g.ts");
        }
    }

    #[test]
    fn the_type_tables_list_every_message() {
        for (declaration, table) in [
            (CoreMessage::decl(), CORE_MESSAGE_TYPES.as_slice()),
            (UiMessage::decl(), UI_MESSAGE_TYPES.as_slice()),
        ] {
            assert_eq!(declaration.matches("\"type\":").count(), table.len());
            for (name, _) in table {
                assert!(declaration.contains(&format!("\"type\": \"{name}\"")));
            }
        }
    }

    #[test]
    fn a_core_request_serialises_with_type_and_data() {
        let frame = RequestFrame {
            id: 3,
            message: CoreMessage::Notice("hi".to_owned()),
            timestamp: 9,
        };
        assert_eq!(
            serde_json::to_string(&frame).expect("a valid frame"),
            r#"{"id":3,"type":"notice","data":"hi","timestamp":9}"#
        );
        let frame = RequestFrame {
            id: 4,
            message: CoreMessage::BringBack,
            timestamp: 9,
        };
        assert_eq!(
            serde_json::to_string(&frame).expect("a valid frame"),
            r#"{"id":4,"type":"bringBack","timestamp":9}"#
        );
        let place = CoreMessage::Place(Bounds {
            x: 1,
            y: 2,
            width: 3,
            height: 4,
        });
        assert_eq!(
            serde_json::to_string(&place).expect("a valid frame"),
            r#"{"type":"place","data":{"x":1,"y":2,"width":3,"height":4}}"#
        );
    }

    #[test]
    fn a_window_request_parses() {
        let line = r#"{"id":7,"type":"action","data":{"kind":"inject"},"timestamp":5}"#;
        assert_eq!(
            parse_incoming(line).expect("a valid frame"),
            Incoming::Request(RequestFrame {
                id: 7,
                message: UiMessage::Action(Action::Inject),
                timestamp: 5,
            })
        );
        for line in [
            r#"{"id":1,"type":"refresh","data":null,"timestamp":5}"#,
            r#"{"id":1,"type":"refresh"}"#,
        ] {
            assert!(matches!(
                parse_incoming(line).expect("a valid frame"),
                Incoming::Request(RequestFrame {
                    message: UiMessage::Refresh,
                    ..
                })
            ));
        }
        let line = r#"{"id":2,"type":"bounds","data":{"x":1,"y":2,"width":3,"height":4}}"#;
        assert!(matches!(
            parse_incoming(line).expect("a valid frame"),
            Incoming::Request(RequestFrame {
                message: UiMessage::Bounds(_),
                ..
            })
        ));
    }

    #[test]
    fn a_response_is_recognised_and_tolerated() {
        assert_eq!(
            parse_incoming(r#"{"id":2,"response":null}"#).expect("a valid frame"),
            Incoming::Response
        );
        assert_eq!(
            parse_incoming(r#"{"id":2,"error":"no"}"#).expect("a valid frame"),
            Incoming::Response
        );
        assert!(parse_incoming("not json").is_err());
    }

    #[test]
    fn an_action_is_answered_with_and_without_an_error() {
        assert_eq!(
            serde_json::to_string(&ResponseFrame::of(7, None)).expect("a valid frame"),
            r#"{"id":7,"response":null}"#
        );
        assert_eq!(
            serde_json::to_string(&ResponseFrame::of(7, Some("nope".to_owned())))
                .expect("a valid frame"),
            r#"{"id":7,"error":"nope"}"#
        );
    }
}
