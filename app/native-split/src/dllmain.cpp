// fixedge-split.dll — injected into Edge's browser process to grow the native
// split beyond two panes. Authorized personal customization of the user's own
// Edge (the fix-edge repo). Pinned to msedge.dll 155.0.4283.45 via offsets.h.
//
// This translation unit is deliberately thin: DLL lifecycle only. Each concern
// lives in its own header — edge.h (the binding SSOT), pane_tree.h (the pure
// layout model), panes.h (container growth), geometry.h (the layout detour),
// hooks.h (MinHook wiring). Everything N-pane is gated on count > 2, so a stock
// two-pane split is byte-for-byte untouched.
#include <windows.h>
#include <cstdlib>
#include "edge.h"
#include "verify.h"
#include "hooks.h"

using namespace fixedge;

namespace {

HMODULE g_self = nullptr;

// The companion app toggles the feature off by signalling this; we then remove
// the hooks and drop out of Edge without a restart.
constexpr wchar_t kUnloadEvent[] = L"Local\\fixedge-split-unload";

int ReadTargetPanes() {
  wchar_t buf[16];
  if (GetEnvironmentVariableW(L"FIXEDGE_PANES", buf, 16)) {
    return static_cast<int>(wcstol(buf, nullptr, 10));
  }
  return 1;  // stock
}

DWORD WINAPI Init(LPVOID) {
  HMODULE edge_module = nullptr;
  for (int i = 0; i < 100 && !edge_module; ++i) {  // msedge.dll may still be loading
    edge_module = GetModuleHandleW(edge::kModule);
    if (!edge_module) Sleep(50);
  }
  if (!edge_module) {
    Log("[err] %ls not found", edge::kModule);
    return 0;
  }
  edge::Bind(edge_module);
  Log("[init] fixedge-split loaded. %ls base=%p", edge::kModule, reinterpret_cast<void*>(edge::g_base));

  // Fail safe: the offsets are specific to one Edge build. Refuse to hook anything
  // if this isn't that exact build or the hook sites don't look like code, so a
  // stale or mismatched DLL can never corrupt a browser it wasn't reversed against.
  if (!verify::Ok()) {
    Log("[verify] FAILED - refusing to hook (wrong Edge build or unexpected code)");
    return 0;
  }

  hooks::g_target_panes = ReadTargetPanes();
  Log("[init] target panes = %d", hooks::g_target_panes);

  if (!hooks::InstallAll()) return 0;
  Log("[init] hooks installed; split into %d panes armed", hooks::g_target_panes);

  // Toggle-off from the app: wait for the unload signal, then cleanly detach.
  // Manual-reset; reset first so a quick off->on re-toggle does not self-unload.
  HANDLE unload = CreateEventW(nullptr, TRUE, FALSE, kUnloadEvent);
  ResetEvent(unload);
  WaitForSingleObject(unload, INFINITE);
  Log("[unload] signalled; removing hooks");
  hooks::RemoveAll();
  if (unload) CloseHandle(unload);
  Log("[unload] fixedge-split detached");
  FreeLibraryAndExitThread(g_self, 0);
  return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_self = module;
    DisableThreadLibraryCalls(module);
    CreateThread(nullptr, 0, Init, nullptr, 0, nullptr);  // off the loader lock
  }
  return TRUE;
}
