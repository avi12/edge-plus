//! The paused-features toast. An unpackaged app needs its `AppUserModelID`
//! registered (`HKCU\Software\Classes\AppUserModelId\<id>`, `DisplayName` and
//! `IconUri`) and set on the process before Windows shows its toasts.

use std::io;
use std::path::Path;
use std::sync::{Mutex, PoisonError};

use windows::Data::Xml::Dom::XmlDocument;
use windows::Foundation::TypedEventHandler;
use windows::UI::Notifications::{ToastNotification, ToastNotificationManager};
use windows::core::HSTRING;
use windows_sys::Win32::UI::Shell::SetCurrentProcessExplicitAppUserModelID;

use crate::kernel::wide::to_wide;
use crate::system::registry::{self, Hive};

const APP_ID_KEY: &str = r"Software\Classes\AppUserModelId";
const DISPLAY_NAME_VALUE: &str = "DisplayName";
const ICON_URI_VALUE: &str = "IconUri";

/// A toast's Activated handler fires only while the toast object lives, so
/// every toast shown is kept for the life of the process (one per Edge build).
static SHOWN: Mutex<Vec<ToastNotification>> = Mutex::new(Vec::new());

pub fn register_toast_app(app_id: &str, display_name: &str, icon: &Path) -> io::Result<()> {
    let key = format!(r"{APP_ID_KEY}\{app_id}");
    registry::write_string(Hive::CurrentUser, &key, DISPLAY_NAME_VALUE, display_name)?;
    registry::write_string(
        Hive::CurrentUser,
        &key,
        ICON_URI_VALUE,
        &icon.to_string_lossy(),
    )?;
    let app_id = to_wide(app_id);
    // SAFETY: the id is NUL-terminated and lives across the call.
    let result = unsafe { SetCurrentProcessExplicitAppUserModelID(app_id.as_ptr()) };
    windows::core::HRESULT(result)
        .ok()
        .map_err(io::Error::other)
}

/// `on_click` runs when the user clicks the toast while this process lives.
pub fn show_toast(
    app_id: &str,
    title: &str,
    body: &str,
    on_click: impl Fn() + Send + Sync + 'static,
) -> io::Result<()> {
    show(app_id, &toast_xml(title, body), on_click).map_err(io::Error::other)
}

fn show(
    app_id: &str,
    xml: &str,
    on_click: impl Fn() + Send + Sync + 'static,
) -> windows::core::Result<()> {
    let document = XmlDocument::new()?;
    document.LoadXml(&HSTRING::from(xml))?;
    let toast = ToastNotification::CreateToastNotification(&document)?;
    toast.Activated(&TypedEventHandler::new(move |_, _| {
        on_click();
        Ok(())
    }))?;
    ToastNotificationManager::CreateToastNotifierWithId(&HSTRING::from(app_id))?.Show(&toast)?;
    SHOWN
        .lock()
        .unwrap_or_else(PoisonError::into_inner)
        .push(toast);
    Ok(())
}

fn toast_xml(title: &str, body: &str) -> String {
    format!(
        "<toast><visual><binding template=\"ToastGeneric\">\
         <text>{}</text><text>{}</text>\
         </binding></visual></toast>",
        escape_xml(title),
        escape_xml(body)
    )
}

fn escape_xml(text: &str) -> String {
    let mut escaped = String::with_capacity(text.len());
    for character in text.chars() {
        match character {
            '&' => escaped.push_str("&amp;"),
            '<' => escaped.push_str("&lt;"),
            '>' => escaped.push_str("&gt;"),
            '"' => escaped.push_str("&quot;"),
            '\'' => escaped.push_str("&apos;"),
            _ => escaped.push(character),
        }
    }
    escaped
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn escapes_the_text() {
        assert_eq!(
            escape_xml(r#"a < b & "c" > 'd'"#),
            "a &lt; b &amp; &quot;c&quot; &gt; &apos;d&apos;"
        );
    }

    #[test]
    fn builds_a_document_windows_parses() {
        let xml = toast_xml("Paused <split>", "Edge 155 & later");
        let document = XmlDocument::new().expect("an XML document");
        document
            .LoadXml(&HSTRING::from(xml))
            .expect("the toast XML parses");
        let texts = document
            .GetElementsByTagName(&HSTRING::from("text"))
            .expect("text nodes");
        assert_eq!(texts.Length().expect("a count"), 2);
        let title = texts
            .Item(0)
            .expect("a title")
            .InnerText()
            .expect("its text");
        assert_eq!(title, "Paused <split>");
    }
}
