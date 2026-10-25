//! Local time (GetLocalTime): std only knows UTC.

/// `2026-10-25T16:04:57.123`, the log's timestamp.
#[must_use]
pub fn local_timestamp() -> String {
    todo!("GetLocalTime, formatted yyyy-MM-ddTHH:mm:ss.fff")
}

/// `16:04:57`, for the window's "Panel updated" line.
#[must_use]
pub fn local_time_of_day() -> String {
    todo!("GetLocalTime, formatted HH:mm:ss")
}
