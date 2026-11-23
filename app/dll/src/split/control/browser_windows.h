// browser_windows.h — which top-level windows are Edge's browser windows, for
// the handlers that walk every one of them.
#pragma once
#include <windows.h>
#include <cstring>

namespace edgeplus::control {

inline constexpr wchar_t kBrowserWindowClass[] = L"Chrome_WidgetWin_1";
inline constexpr int kClassNameLength = 64;

// It may still be a toast or bubble; callers check for a tab strip.
inline bool IsOurBrowserClassWindow(HWND hwnd) {
  DWORD processId = 0;
  GetWindowThreadProcessId(hwnd, &processId);
  const bool isOurProcess = processId == GetCurrentProcessId();
  if (!isOurProcess) {
    return false;
  }
  wchar_t className[kClassNameLength] = {};
  GetClassNameW(hwnd, className, kClassNameLength);
  return wcscmp(className, kBrowserWindowClass) == 0;
}

}  // namespace edgeplus::control
