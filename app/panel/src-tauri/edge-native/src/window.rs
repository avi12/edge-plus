//! Vertical-only resizing for the panel window.
//!
//! The window is a fixed width, so its left and right borders and corners must
//! not offer a horizontal resize cursor either. Windows picks that cursor from
//! the `WM_NCHITTEST` answer, so the window is subclassed and the answer is
//! rewritten: side borders become plain border, corners become top / bottom edges.

use windows_sys::Win32::Foundation::{HWND, LPARAM, LRESULT, WPARAM};
use windows_sys::Win32::UI::Shell::{DefSubclassProc, RemoveWindowSubclass, SetWindowSubclass};
use windows_sys::Win32::UI::WindowsAndMessaging::{
    HTBORDER, HTBOTTOM, HTBOTTOMLEFT, HTBOTTOMRIGHT, HTLEFT, HTRIGHT, HTTOP, HTTOPLEFT, HTTOPRIGHT,
    WM_NCDESTROY, WM_NCHITTEST,
};

/// Identifies this subclass among any others on the window.
const SUBCLASS_ID: usize = 0xED6E_0001;

/// The hit-test answer with every horizontal resize handle taken away.
const fn vertical_only(hit: u32) -> u32 {
    match hit {
        HTLEFT | HTRIGHT => HTBORDER,
        HTTOPLEFT | HTTOPRIGHT => HTTOP,
        HTBOTTOMLEFT | HTBOTTOMRIGHT => HTBOTTOM,
        other => other,
    }
}

unsafe extern "system" fn subclass_proc(
    hwnd: HWND,
    message: u32,
    wparam: WPARAM,
    lparam: LPARAM,
    _subclass_id: usize,
    _ref_data: usize,
) -> LRESULT {
    if message == WM_NCDESTROY {
        RemoveWindowSubclass(hwnd, Some(subclass_proc), SUBCLASS_ID);
        return DefSubclassProc(hwnd, message, wparam, lparam);
    }
    let result = DefSubclassProc(hwnd, message, wparam, lparam);
    if message == WM_NCHITTEST {
        // A negative answer (HTERROR, HTTRANSPARENT) is not a code to remap.
        if let Ok(hit) = u32::try_from(result) {
            return LRESULT::try_from(vertical_only(hit)).unwrap_or(result);
        }
    }
    result
}

/// Remove the horizontal resize handles from the window `hwnd` (a raw `HWND`).
/// Returns whether the window was subclassed. Must run on the window's thread.
#[must_use]
pub fn make_vertical_resize_only(hwnd: isize) -> bool {
    // SAFETY: `hwnd` names a live window of this process; the callback is a
    // `'static` function and removes itself when the window is destroyed.
    unsafe { SetWindowSubclass(hwnd as HWND, Some(subclass_proc), SUBCLASS_ID, 0) != 0 }
}

#[cfg(test)]
mod tests {
    use super::*;
    use windows_sys::Win32::UI::WindowsAndMessaging::{HTCAPTION, HTCLIENT};

    #[test]
    fn side_borders_lose_their_handle() {
        assert_eq!(vertical_only(HTLEFT), HTBORDER);
        assert_eq!(vertical_only(HTRIGHT), HTBORDER);
    }

    #[test]
    fn corners_become_top_and_bottom_edges() {
        assert_eq!(vertical_only(HTTOPLEFT), HTTOP);
        assert_eq!(vertical_only(HTTOPRIGHT), HTTOP);
        assert_eq!(vertical_only(HTBOTTOMLEFT), HTBOTTOM);
        assert_eq!(vertical_only(HTBOTTOMRIGHT), HTBOTTOM);
    }

    #[test]
    fn everything_else_is_untouched() {
        for hit in [HTCLIENT, HTCAPTION, HTTOP, HTBOTTOM] {
            assert_eq!(vertical_only(hit), hit);
        }
    }
}
