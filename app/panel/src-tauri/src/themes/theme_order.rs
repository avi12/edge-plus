//! One list holds the panel's own themes and the ones only Edge knows, in the
//! order `Theming.order` gives; it is the order of the list and of the light /
//! dark pickers.

use std::collections::HashMap;

use crate::platform::settings::ThemeEntry;
use crate::themes::sources::installed_themes::InstalledTheme;

/// A theme of the combined list.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum Listed<'theme> {
    Own(&'theme ThemeEntry),
    Edge(&'theme InstalledTheme),
}

impl Listed<'_> {
    #[must_use]
    pub(crate) fn id(&self) -> &str {
        match self {
            Self::Own(theme) => &theme.id,
            Self::Edge(theme) => &theme.id,
        }
    }
}

/// Every theme in the order of `order`; one it does not name goes after the
/// named ones (own themes first in their order, then Edge's in its scan order),
/// and an id in `order` with no theme is ignored.
#[must_use]
pub(crate) fn combined<'theme>(
    own: &'theme [ThemeEntry],
    edge: &'theme [InstalledTheme],
    order: &[String],
) -> Vec<Listed<'theme>> {
    let mut listed: Vec<Listed<'theme>> = own
        .iter()
        .map(Listed::Own)
        .chain(edge.iter().map(Listed::Edge))
        .collect();
    listed.sort_by_key(|theme| {
        order
            .iter()
            .position(|id| id == theme.id())
            .unwrap_or(usize::MAX)
    });
    listed
}

/// Negative `offset` is up; stops at either end.
#[must_use]
pub(crate) fn moved(ids: &[String], id: &str, offset: i32) -> Vec<String> {
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
pub(crate) fn ordered(themes: &[ThemeEntry], ids: &[String]) -> Vec<ThemeEntry> {
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

    fn installed(id: &str) -> InstalledTheme {
        InstalledTheme {
            id: id.to_owned(),
            name: id.to_uppercase(),
        }
    }

    fn combined_ids(own: &[&str], edge: &[&str], order: &[&str]) -> String {
        let own: Vec<ThemeEntry> = own.iter().map(|id| entry(id)).collect();
        let edge: Vec<InstalledTheme> = edge.iter().map(|id| installed(id)).collect();
        combined(&own, &edge, &ids(order))
            .iter()
            .map(Listed::id)
            .collect::<Vec<_>>()
            .join(",")
    }

    #[test]
    fn combined_mixes_own_and_edge_themes_in_one_order() {
        assert_eq!(
            "a,b,x,y",
            combined_ids(&["a", "b"], &["x", "y"], &[]),
            "no order: own first, then Edge's"
        );
        assert_eq!(
            "x,a,y,b",
            combined_ids(&["a", "b"], &["x", "y"], &["x", "a", "y", "b"]),
            "an order mixes them"
        );
        assert_eq!(
            "y,a,b,x",
            combined_ids(&["a", "b"], &["x", "y"], &["gone", "y"]),
            "unnamed ones go last in the default order, gone ids are ignored"
        );
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
            order: Vec::new(),
            light_history: Vec::new(),
            dark_history: Vec::new(),
        };
        theming.reorder(&ids(&["c", "b", "a"]));
        assert_eq!(
            "c,b,a",
            joined(&theming.themes),
            "Theming reorders its themes"
        );
        assert_eq!(ids(&["c", "b", "a"]), theming.order, "and keeps the order");
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
