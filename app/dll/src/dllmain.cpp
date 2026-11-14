// edgeplus-split.dll — injected into Edge's browser process to grow the native
// split beyond two panes; this file is the DLL lifecycle only. Everything N-pane
// is gated on count > 2, so a stock two-pane split is untouched.
#include <windows.h>
#include <cstdlib>
#include <optional>
#include "edge/edge.h"
#include "core/handles.h"
#include "core/status.h"
#include "core/verify.h"
#include "edge/build_relation.h"
#include "edge/hooks.h"
#include "edge/resolve.h"

namespace edge = edgeplus::edge;
namespace edge155 = edgeplus::edge155;
namespace edge_build = edgeplus::edge_build;
namespace resolve = edgeplus::resolve;
namespace status = edgeplus::status;
namespace verify = edgeplus::verify;
namespace handles = edgeplus::handles;
namespace control = edgeplus::control;
namespace hooks = edgeplus::hooks;
using edgeplus::Log;

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

// Set on a throwaway Edge on the pinned build: find every entry by signature
// anyway and log the ones found anywhere but where offsets.h says, changing
// nothing (the way a table made from another build is checked against a live
// process).
constexpr wchar_t kResolveSelfTestVariable[] = L"EDGEPLUS_RESOLVE_SELFTEST";
// Set on a throwaway Edge on the pinned build: take the signature path as if the
// build were another one, so everything downstream of it runs for real (the hooks
// through the translation table, the features through the availability mask).
constexpr wchar_t kResolveForceVariable[] = L"EDGEPLUS_RESOLVE_FORCE";

constexpr wchar_t kForcePatchLineValue[] = L"patch";

[[nodiscard]] bool IsVariableSet(const wchar_t* name) {
  return GetEnvironmentVariableW(name, nullptr, 0) != 0;
}

// The relation the force variable asks to be treated as: "patch" is a security
// patch of the pinned line, anything else another build.
[[nodiscard]] edge_build::Relation ForcedRelation(const wchar_t* variable) {
  wchar_t value[16] = {};
  GetEnvironmentVariableW(variable, value, std::size(value));
  const bool isPatchLine = wcscmp(value, kForcePatchLineValue) == 0;
  return isPatchLine ? edge_build::Relation::kSamePatchLine
                     : edge_build::Relation::kOtherBuild;
}

// Decides whether the DLL may hook this Edge, and tells the panel how it stands
// (status.h). The pinned build is trusted as offsets.h has it; any other build is
// searched by signature (resolve.h), and the DLL stays only for the features
// whose every address was placed.
[[nodiscard]] bool Admit() {
  const std::optional<edge_build::Version> running = verify::RunningBuild();
  status::Wire wire{};
  wire.mode = status::kModeRefused;
  if (!running) {
    status::Write(wire);
    return false;
  }
  wire.major = static_cast<uint32_t>(running->major);
  wire.minor = static_cast<uint32_t>(running->minor);
  wire.build = static_cast<uint32_t>(running->build);
  wire.revision = static_cast<uint32_t>(running->revision);
  const bool isForced = IsVariableSet(kResolveForceVariable);
  const edge_build::Relation relation =
      isForced ? ForcedRelation(kResolveForceVariable)
               : edge_build::Classify(*running, verify::kPinnedBuild);
  const bool isPinned = relation == edge_build::Relation::kPinned;
  if (isPinned) {
    wire.mode = status::kModePinned;
    wire.availableFeatures = edge155::signatures::kEveryFeature;
    if (IsVariableSet(kResolveSelfTestVariable)) {
      const resolve::Outcome selfTest =
          resolve::Run(edge::g_base, ForcedRelation(kResolveSelfTestVariable),
                       resolve::RunMode::kSelfTest);
      Log("[resolve] self-test: %zu placed, %zu away from offsets.h",
          selfTest.resolvedEntries, selfTest.misplacedEntries);
    }
  } else {
    const resolve::Outcome outcome = resolve::Run(edge::g_base, relation);
    wire.availableFeatures = outcome.availableFeatures;
    wire.resolvedEntries = static_cast<uint32_t>(outcome.resolvedEntries);
    wire.totalEntries = static_cast<uint32_t>(outcome.totalEntries);
    const bool isAnyFeatureLeft = outcome.availableFeatures != 0;
    if (!isAnyFeatureLeft) {
      status::Write(wire);
      return false;
    }
    wire.mode = status::kModeResolved;
  }
  if (!verify::HookSitesLookSane()) {
    wire.mode = status::kModeRefused;
    wire.availableFeatures = 0;
    status::Write(wire);
    return false;
  }
  status::Write(wire);
  return true;
}

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

  if (!Admit()) {
    Log("[verify] FAILED - refusing to hook (unknown Edge build, or code that "
        "is not what it was reversed from)");
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

static BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
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
