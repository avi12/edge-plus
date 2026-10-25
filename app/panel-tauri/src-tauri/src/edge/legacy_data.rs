//! Before Edge+ had its name, its data lived in %LOCALAPPDATA%\FixEdge. An
//! install from then carries over into the folder named for Edge+ the first
//! time the core starts. Once the new folder has settings it is never touched
//! again, so a user of the renamed panel is never overwritten.

use std::fs;
use std::io;
use std::path::Path;

use crate::platform::paths;

/// The settings come last, so a copy cut short is tried again on the next
/// start.
pub fn carry(
    legacy_root: &Path,
    root: &Path,
    settings_file_name: &str,
    skipped_file_names: &[&str],
) -> bool {
    let is_carry_needed = legacy_root.is_dir() && !root.join(settings_file_name).is_file();
    if !is_carry_needed {
        return false;
    }
    let is_copied = copy_into(
        legacy_root,
        root,
        skipped_file_names,
        Some(settings_file_name),
    )
    .and_then(|()| copy_settings(legacy_root, root, settings_file_name))
    .is_ok();
    if !is_copied {
        return false;
    }
    // Held open by a DLL that has not unloaded yet: the copy is what counts.
    let _ = fs::remove_dir_all(legacy_root);
    true
}

/// Never over a settings file that appeared meanwhile.
fn copy_settings(legacy_root: &Path, root: &Path, settings_file_name: &str) -> io::Result<()> {
    let legacy_settings = legacy_root.join(settings_file_name);
    if !legacy_settings.is_file() {
        return Ok(());
    }
    let settings = root.join(settings_file_name);
    if settings.exists() {
        return Err(io::Error::from(io::ErrorKind::AlreadyExists));
    }
    fs::copy(legacy_settings, settings).map(|_| ())
}

/// `held_back_file_name` is only held back in the folder the carry started
/// from.
fn copy_into(
    from: &Path,
    to: &Path,
    skipped_file_names: &[&str],
    held_back_file_name: Option<&str>,
) -> io::Result<()> {
    fs::create_dir_all(to)?;
    let skipped: Vec<String> = skipped_file_names
        .iter()
        .chain(held_back_file_name.as_slice())
        .map(|name| paths::fold_case(name))
        .collect();
    for entry in fs::read_dir(from)? {
        let entry = entry?;
        let file_type = entry.file_type()?;
        let file_name = entry.file_name();
        if file_type.is_dir() {
            copy_into(
                &entry.path(),
                &to.join(&file_name),
                skipped_file_names,
                None,
            )?;
            continue;
        }
        let is_skipped = skipped.contains(&paths::fold_case(&file_name.to_string_lossy()));
        let target = to.join(&file_name);
        let should_copy = !is_skipped && !target.exists();
        if should_copy {
            fs::copy(entry.path(), target)?;
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use std::path::PathBuf;

    use super::*;

    const SETTINGS: &str = "settings.json";
    const SKIPPED_DLL: &str = "fixedge-split.dll";
    const SKIPPED_CONFIG: &str = "fixedge-config.bin";
    const SKIPPED: [&str; 2] = [SKIPPED_DLL, SKIPPED_CONFIG];

    /// Folders under a throwaway temp root, never the real data folder.
    struct TempRoot(PathBuf);

    impl TempRoot {
        fn new(name: &str) -> Self {
            Self(std::env::temp_dir().join(format!(
                "edgeplus-legacy-data-{name}-{}",
                std::process::id()
            )))
        }
    }

    impl Drop for TempRoot {
        fn drop(&mut self) {
            let _ = fs::remove_dir_all(&self.0);
        }
    }

    fn write(folder: &Path, name: &str, contents: &str) {
        fs::create_dir_all(folder).expect("create the folder");
        fs::write(folder.join(name), contents).expect("write the file");
    }

    #[test]
    fn carries_an_old_install() {
        let temp = TempRoot::new("carries");
        let legacy = temp.0.join("a-old");
        let root = temp.0.join("a-new");
        write(&legacy, SETTINGS, "{\"features\":1}");
        write(&legacy, "reading-themes.bin", "bin");
        write(&legacy, SKIPPED_DLL, "dll");
        write(&legacy, SKIPPED_CONFIG, "cfg");
        write(&legacy.join("themes"), "dusk.crx", "crx");

        let is_carried = carry(&legacy, &root, SETTINGS, &SKIPPED);

        assert!(is_carried, "an old install is carried over");
        assert_eq!(
            "{\"features\":1}",
            fs::read_to_string(root.join(SETTINGS)).expect("settings"),
            "the settings come across"
        );
        assert!(
            root.join("reading-themes.bin").is_file(),
            "its other files too"
        );
        assert!(
            root.join("themes").join("dusk.crx").is_file(),
            "and its folders"
        );
        assert!(!root.join(SKIPPED_DLL).exists(), "but not the old DLL");
        assert!(
            !root.join(SKIPPED_CONFIG).exists(),
            "nor its unused config file"
        );
        assert!(!legacy.exists(), "the old folder is removed once copied");
        assert!(
            !carry(&legacy, &root, SETTINGS, &SKIPPED),
            "a second run has nothing to do"
        );
    }

    #[test]
    fn leaves_a_new_install_alone() {
        let temp = TempRoot::new("leaves");
        let legacy = temp.0.join("b-old");
        let root = temp.0.join("b-new");
        write(&legacy, SETTINGS, "old");
        write(&root, SETTINGS, "new");

        let is_carried = carry(&legacy, &root, SETTINGS, &SKIPPED);

        assert!(
            !is_carried,
            "a folder with settings of its own is not carried into"
        );
        assert_eq!(
            "new",
            fs::read_to_string(root.join(SETTINGS)).expect("settings"),
            "its settings are kept"
        );
        assert!(legacy.is_dir(), "and the old folder is left as it was");
    }

    #[test]
    fn does_nothing_without_an_old_folder() {
        let temp = TempRoot::new("nothing");
        let root = temp.0.join("c-new");

        let is_carried = carry(&temp.0.join("c-old"), &root, SETTINGS, &SKIPPED);

        assert!(!is_carried && !root.exists(), "no old folder, no new one");
    }
}
