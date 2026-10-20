// focus.h — is Edge the app the user is in? One concern: the one test both the
// colour hook (which dims the selected tab of an unfocused Edge) and the tab
// cover (which must match that fill) ask, so they can never disagree. Win32 only.
#pragma once
#include <windows.h>

namespace fixedge::focus {

// Whether the window the user is in belongs to this Edge process.
inline bool IsEdgeForeground() {
  DWORD foregroundProcess = 0;
  GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
  return foregroundProcess == GetCurrentProcessId();
}

}  // namespace fixedge::focus
