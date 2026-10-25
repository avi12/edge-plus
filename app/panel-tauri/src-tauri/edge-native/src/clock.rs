//! Local time (GetLocalTime): std only knows UTC.

use windows_sys::Win32::Foundation::SYSTEMTIME;
use windows_sys::Win32::System::SystemInformation::GetLocalTime;

/// `2026-10-25T16:04:57.123`, the log's timestamp.
#[must_use]
pub fn local_timestamp() -> String {
    let time = local_time();
    format!(
        "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds
    )
}

/// `16:04:57`, for the window's "Panel updated" line.
#[must_use]
pub fn local_time_of_day() -> String {
    let time = local_time();
    format!("{:02}:{:02}:{:02}", time.wHour, time.wMinute, time.wSecond)
}

fn local_time() -> SYSTEMTIME {
    // SAFETY: SYSTEMTIME is plain data, for which all zeros is valid.
    let mut time: SYSTEMTIME = unsafe { std::mem::zeroed() };
    // SAFETY: the out pointer is a live local.
    unsafe {
        GetLocalTime(&raw mut time);
    }
    time
}

#[cfg(test)]
mod tests {
    use super::*;

    fn has_shape(text: &str, shape: &str) -> bool {
        let is_same_length = text.len() == shape.len();
        is_same_length
            && text
                .chars()
                .zip(shape.chars())
                .all(|(actual, wanted)| match wanted {
                    '9' => actual.is_ascii_digit(),
                    _ => actual == wanted,
                })
    }

    #[test]
    fn formats_the_timestamp() {
        let timestamp = local_timestamp();
        assert!(
            has_shape(&timestamp, "9999-99-99T99:99:99.999"),
            "{timestamp}"
        );
    }

    #[test]
    fn formats_the_time_of_day() {
        let time_of_day = local_time_of_day();
        assert!(has_shape(&time_of_day, "99:99:99"), "{time_of_day}");
    }
}
