//! Removes what versions before 2026-10-15 installed for the extension-based
//! architecture. Best-effort and idempotent.

use std::fs;
use std::path::Path;

use edge_native::Hive;

use crate::platform::paths;

const HOST_KEY: &str = r"Software\Microsoft\Edge\NativeMessagingHosts\com.avi.fix_edge";
const EXTENSION_FOLDER: &str = "extension";
/// The host was swapped in by renaming the running copy aside, so several
/// "fixedge-host.exe[.ticks].retired" may remain beside the live one.
const HOST_EXECUTABLE_PREFIX: &str = "fixedge-host.exe";
const FILES: [&str; 3] = ["native-host.json", "status.json", "extension.stamp"];

pub fn run() {
    let data_root = paths::data_root();
    delete_directory(&data_root.join(EXTENSION_FOLDER));
    for name in FILES {
        delete_file(&data_root.join(name));
    }
    delete_host_executables(&data_root);
    let _ = edge_native::delete_tree(Hive::CurrentUser, HOST_KEY);
}

fn delete_host_executables(data_root: &Path) {
    let Ok(entries) = fs::read_dir(data_root) else {
        return;
    };
    let prefix = paths::fold_case(HOST_EXECUTABLE_PREFIX);
    for entry in entries.flatten() {
        let name = paths::fold_case(&entry.file_name().to_string_lossy());
        if name.starts_with(&prefix) {
            delete_file(&entry.path());
        }
    }
}

fn delete_directory(path: &Path) {
    if path.is_dir() {
        let _ = fs::remove_dir_all(path);
    }
}

fn delete_file(path: &Path) {
    if path.is_file() {
        let _ = fs::remove_file(path);
    }
}
