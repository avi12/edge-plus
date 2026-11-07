// edgeplus-split.dll — injected into Edge's browser process to grow the native
// split beyond two panes; this file is the DLL lifecycle only. Everything N-pane
// is gated on count > 2, so a stock two-pane split is untouched.
#include <windows.h>
#include <cstdlib>
#include "edge/edge.h"
#include "core/handles.h"
#include "core/verify.h"
#include "edge/hooks.h"

using namespace edgeplus;

namespace {

constinit HMODULE g_self = nullptr;
constinit HANDLE g_unloadEvent = nullptr;
constinit HANDLE g_configEvent = nullptr;

// @share UnloadEvent
constexpr wchar_t kUnloadEvent[] = L"Local\\edgeplus-split-unload";

// @share ConfigEvent
constexpr wchar_t kConfigEvent[] = L"Local\\edgeplus-config-changed";

// In the order of the main loop's handle array.
enum class WaitedEvent : size_t {
  kUnload,
  kConfig
};
constexpr DWORD kWaitedEventCount = 2;

// msedge.dll may still be loading when Init runs.
constexpr int kModuleWaitAttempts = 100;
constexpr DWORD kModuleWaitStepMilliseconds = 50;

constexpr DWORD kScriptDrainTimeoutMilliseconds = 5000;

// Close the signal events so the panel sees this DLL as gone (a stale leftover
// it can free), not as one still running.
void ReleaseEvents() {
  CloseHandle(g_unloadEvent);
  CloseHandle(g_configEvent);
}

DWORD WINAPI Init(LPVOID) {
  HMODULE edgeModule = nullptr;
  for (int i = 0; i < kModuleWaitAttempts && !edgeModule; i++) {
    edgeModule = GetModuleHandleW(edge155::kModule);
    if (!edgeModule) {
      Sleep(kModuleWaitStepMilliseconds);
    }
  }
  if (!edgeModule) {
    Log("[err] %ls not found", edge155::kModule);
    ReleaseEvents();
    return 0;
  }
  edge::Bind(edgeModule);
  Log("[init] edgeplus-split loaded. %ls base=%p", edge155::kModule,
      reinterpret_cast<void*>(edge::g_base));

  if (!verify::Ok()) {
    Log("[verify] FAILED - refusing to hook (wrong Edge build or unexpected "
        "code)");
    ReleaseEvents();
    return 0;
  }

  if (!hooks::InstallAll()) {
    ReleaseEvents();
    return 0;
  }
  Log("[init] hooks installed; directional-add menu armed");

  const HANDLE events[kWaitedEventCount] = {g_unloadEvent, g_configEvent};
  while (true) {
    const DWORD which =
        WaitForMultipleObjects(kWaitedEventCount, events, FALSE, INFINITE);
    const bool isConfigChange =
        which == WAIT_OBJECT_0 + static_cast<DWORD>(WaitedEvent::kConfig);
    if (isConfigChange) {
      control::OnConfigChanged();
      continue;
    }
    break;
  }
  Log("[unload] signalled; removing hooks");
  hooks::RemoveAll();
  ReleaseEvents();
  // A script reply Edge still holds would jump into this DLL once it is gone:
  // wait for them to finish, and stay loaded (harmlessly) if one never does.
  if (!edge::WaitForScriptCallbacks(kScriptDrainTimeoutMilliseconds)) {
    Log("[unload] script replies still pending; staying loaded");
    return 0;
  }
  Log("[unload] edgeplus-split detached");
  FreeLibraryAndExitThread(g_self, 0);
  return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
  const bool isProcessAttach = reason == DLL_PROCESS_ATTACH;
  if (isProcessAttach) {
    g_self = module;
    DisableThreadLibraryCalls(module);
    // Reset first so a quick off->on re-toggle does not self-unload. Created
    // here, not in Init: the unload event existing is how the panel tells a
    // loading DLL from a stale leftover, and it must never free the first.
    g_unloadEvent = CreateEventW(nullptr, /*bManualReset=*/TRUE,
                                 /*bInitialState=*/FALSE, kUnloadEvent);
    ResetEvent(g_unloadEvent);
    g_configEvent = CreateEventW(nullptr, /*bManualReset=*/FALSE,
                                 /*bInitialState=*/FALSE, kConfigEvent);
    // Init waits and hooks, which must not run under the loader lock.
    const handles::UniqueHandle initThread{
        CreateThread(nullptr, 0, Init, nullptr, 0, nullptr)};
  }
  return TRUE;
}
