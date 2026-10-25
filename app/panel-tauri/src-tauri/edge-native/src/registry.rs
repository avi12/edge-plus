//! The registry values Edge+ reads and writes, under HKCU or HKLM.

use std::io;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Hive {
    CurrentUser,
    LocalMachine,
}

/// `value` None is the key's default value. None when the key or value is
/// missing or not a string.
#[must_use]
pub fn read_string(hive: Hive, key: &str, value: Option<&str>) -> Option<String> {
    let _ = (hive, key, value);
    todo!()
}

/// None when the key or value is missing or not REG_BINARY.
#[must_use]
pub fn read_binary(hive: Hive, key: &str, value: &str) -> Option<Vec<u8>> {
    let _ = (hive, key, value);
    todo!()
}

/// Creates the key if it is missing; writes REG_SZ.
pub fn write_string(hive: Hive, key: &str, value: &str, data: &str) -> io::Result<()> {
    let _ = (hive, key, value, data);
    todo!()
}

/// A missing key or value is already the state wanted.
pub fn delete_value(hive: Hive, key: &str, value: &str) -> io::Result<()> {
    let _ = (hive, key, value);
    todo!()
}

/// A missing key is already the state wanted.
pub fn delete_tree(hive: Hive, key: &str) -> io::Result<()> {
    let _ = (hive, key);
    todo!()
}
