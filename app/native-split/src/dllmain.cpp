// fixedge-split.dll — injected into Edge's browser process to grow the native
// split beyond two panes. Authorized personal customization of the user's own
// Edge (the fix-edge repo). Pinned to one msedge.dll build via offsets.h.
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

// msedge.dll may still be loading when Init runs: poll for it this many times,
// this far apart, before giving up.
constexpr int kModuleWaitAttempts = 100;
constexpr DWORD kModuleWaitStepMilliseconds = 50;

DWORD WINAPI Init(LPVOID) {
  HMODULE edgeModule = nullptr;
  for (int i = 0; i < kModuleWaitAttempts && !edgeModule; i++) {
    edgeModule = GetModuleHandleW(edge::kModule);
    if (!edgeModule) {
      Sleep(kModuleWaitStepMilliseconds);
    }
  }
  if (!edgeModule) {
    Log("[err] %ls not found", edge::kModule);
    return 0;
  }
  edge::Bind(edgeModule);
  Log("[init] fixedge-split loaded. %ls base=%p", edge::kModule,
      reinterpret_cast<void*>(edge::g_base));

  // Fail safe: the offsets are specific to one Edge build. Refuse to hook anything
  // if this isn't that exact build or the hook sites don't look like code, so a
  // stale or mismatched DLL can never corrupt a browser it wasn't reversed against.
  if (!verify::Ok()) {
    Log("[verify] FAILED - refusing to hook (wrong Edge build or unexpected "
        "code)");
    return 0;
  }

  if (!hooks::InstallAll()) {
    return 0;
  }
  Log("[init] hooks installed; directional-add menu armed");

  // Toggle-off from the app: wait for the unload signal, then cleanly detach.
  // Manual-reset; reset first so a quick off->on re-toggle does not self-unload.
  HANDLE unloadEvent = CreateEventW(nullptr, /*bManualReset=*/TRUE,
                                    /*bInitialState=*/FALSE, kUnloadEvent);
  ResetEvent(unloadEvent);
  WaitForSingleObject(unloadEvent, INFINITE);
  Log("[unload] signalled; removing hooks");
  hooks::RemoveAll();
  if (unloadEvent) {
    CloseHandle(unloadEvent);
  }
  Log("[unload] fixedge-split detached");
  FreeLibraryAndExitThread(g_self, 0);
  return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
  const bool isProcessAttach = reason == DLL_PROCESS_ATTACH;
  if (isProcessAttach) {
    g_self = module;
    DisableThreadLibraryCalls(module);
    // Init waits and hooks, which must not run under the loader lock.
    CreateThread(nullptr, 0, Init, nullptr, 0, nullptr);
  }
  return TRUE;
}
