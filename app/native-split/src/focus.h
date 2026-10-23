// focus.h — is Edge the app the user is in? One test, so the colour hook and
// the tab cover that must match its fill can never disagree.
#pragma once
#include <windows.h>

namespace fixedge::focus {

inline bool IsEdgeForeground() {
  DWORD foregroundProcess = 0;
  GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
  return foregroundProcess == GetCurrentProcessId();
}

}  // namespace fixedge::focus
