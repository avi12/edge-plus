//! The order of settings.json's themes array is the order of the list and of
//! the light / dark pickers.

use std::collections::HashMap;

use crate::platform::settings::ThemeEntry;

/// Negative `offset` is up; stops at either end.
#[must_use]
pub fn moved(ids: &[String], id: &str, offset: i32) -> Vec<String> {
    let Some(from) = ids.iter().position(|each| each == id) else {
        return ids.to_vec();
    };
    let last = ids.len() - 1;
    let offset = isize::try_from(offset).unwrap_or_default();
    let to = from.saturating_add_signed(offset).min(last);
    let mut moved = ids.to_vec();
    let taken = moved.remove(from);
    moved.insert(to, taken);
    moved
}

/// A theme `ids` does not name goes after the named ones (it was added while
/// the user was dragging); an id with no theme was removed meanwhile.
#[must_use]
pub fn ordered(themes: &[ThemeEntry], ids: &[String]) -> Vec<ThemeEntry> {
    let mut by_id: HashMap<&str, &ThemeEntry> = HashMap::new();
    for theme in themes {
        by_id.entry(theme.id.as_str()).or_insert(theme);
    }
    let mut ordered = Vec::with_capacity(themes.len());
    for id in ids {
        if let Some(theme) = by_id.remove(id.as_str()) {
            ordered.push(theme.clone());
        }
    }
    ordered.extend(
        themes
            .iter()
            .filter(|theme| by_id.contains_key(theme.id.as_str()))
            .cloned(),
    );
    ordered
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::platform::settings::{Settings, ThemeSource, Theming};

    fn ids(names: &[&str]) -> Vec<String> {
        names.iter().map(|&name| name.to_owned()).collect()
    }

    fn entry(id: &str) -> ThemeEntry {
        ThemeEntry {
            id: id.to_owned(),
            name: id.to_uppercase(),
            source: ThemeSource::default(),
            origin: String::new(),
        }
    }

    fn joined(themes: &[ThemeEntry]) -> String {
        themes
            .iter()
            .map(|theme| theme.id.as_str())
            .collect::<Vec<_>>()
            .join(",")
    }

    #[test]
    fn moved_stops_at_either_end() {
        let order = ids(&["a", "b", "c"]);
        assert_eq!("b,a,c", moved(&order, "b", -1).join(","), "up one");
        assert_eq!("a,c,b", moved(&order, "b", 1).join(","), "down one");
        assert_eq!(
            "a,b,c",
            moved(&order, "a", -1).join(","),
            "the first stays first"
        );
        assert_eq!(
            "a,b,c",
            moved(&order, "c", 1).join(","),
            "the last stays last"
        );
        assert_eq!(
            "b,c,a",
            moved(&order, "a", 10).join(","),
            "a long move stops at the end"
        );
        assert_eq!(
            "a,b,c",
            moved(&order, "x", 1).join(","),
            "an unknown id moves nothing"
        );
    }

    #[test]
    fn ordered_takes_the_dragged_order() {
        let themes = vec![entry("a"), entry("b"), entry("c")];
        assert_eq!(
            "c,a,b",
            joined(&ordered(&themes, &ids(&["c", "a", "b"]))),
            "the library takes the dragged order"
        );
        assert_eq!(
            "c,a,b",
            joined(&ordered(&themes, &ids(&["c", "a"]))),
            "a theme the order does not name keeps its place after the named ones"
        );
        assert_eq!(
            "b,a,c",
            joined(&ordered(&themes, &ids(&["gone", "b"]))),
            "a removed theme in the order is ignored"
        );
        assert_eq!(
            themes[0],
            ordered(&themes, &ids(&["a"]))[0],
            "the entries are kept whole"
        );

        let mut theming = Theming {
            themes,
            light: "b".to_owned(),
            dark: "c".to_owned(),
        };
        theming.reorder(&ids(&["c", "b", "a"]));
        assert_eq!(
            "c,b,a",
            joined(&theming.themes),
            "Theming reorders its themes"
        );
        let keeps_assignments = theming.light == "b" && theming.dark == "c";
        assert!(keeps_assignments, "a reorder keeps the assignments");
        let json = Settings {
            theming,
            ..Settings::default()
        }
        .to_json();
        let is_new_order = json.find("\"c\"") < json.find("\"a\"");
        assert!(
            is_new_order,
            "the file's themes array is written in the new order"
        );
    }
}
