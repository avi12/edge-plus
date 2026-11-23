// scripts.h — running JavaScript in a page's frame, with or without an
// answer, and draining the script callbacks and detours still in flight
// before the DLL unloads.
#pragma once
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <string>
#include <utility>
#include "edge/api/binding.h"
#include "edge/api/strings.h"
#include "edge/api/web_contents.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

// What a finished script handed back: its string result, or nothing when the
// script returned something else (or threw).
struct ScriptResult {
  bool isString = false;
  std::string text;
};
using ScriptDone = std::function<void(const ScriptResult&)>;

// A hand-made base::OnceCallback<void(base::Value)>: a BindState we lay out
// ourselves (Edge only ever calls its invoke and destructor slots), holding the
// std::function to run. Edge destroys it exactly once, run or not.
struct ScriptCallbackState {
  alignas(8) uint8_t
      bindStateBase[edge155::off::BINDSTATE_first_bound_argument] = {};
  ScriptDone done;
};
// Callbacks Edge still holds. DLL unload waits for this to reach zero: a reply
// that arrived after the DLL was freed would jump into unmapped code.
inline constinit std::atomic<int> g_liveScriptCallbacks{0};

inline void __fastcall InvokeScriptCallback(void* bindState, void* value) {
  auto* state = static_cast<ScriptCallbackState*>(bindState);
  ScriptResult result;
  const uint8_t variantIndex =
      value ? Field<uint8_t>(value, edge155::off::VALUE_variant_index) : 0;
  result.isString = variantIndex == edge155::off::VALUE_string_index;
  if (result.isString) {
    result.text = StdStringText(value);
  }
  state->done(result);
}
// Edge hands back the state RunScriptInFrame released to it: own it again, and
// let the owner free it.
inline void __fastcall DestroyScriptCallback(const void* bindState) {
  const std::unique_ptr<const ScriptCallbackState> state{
      static_cast<const ScriptCallbackState*>(bindState)};
  g_liveScriptCallbacks--;
}

// Detours that count themselves while they run (RunningDetour): a hot one
// Edge may be inside when the DLL unloads. Unload waits for none to be
// running, as for the script callbacks, so no thread returns into freed code.
inline constinit std::atomic<int> g_runningDetours{0};

struct RunningDetour {
  RunningDetour() {
    g_runningDetours++;
  }
  ~RunningDetour() {
    g_runningDetours--;
  }
  RunningDetour(const RunningDetour&) = delete;
  RunningDetour& operator=(const RunningDetour&) = delete;
  RunningDetour(RunningDetour&&) = delete;
  RunningDetour& operator=(RunningDetour&&) = delete;
};

inline constexpr DWORD kDrainStepMs = 20;

// Whether `count` reached zero within `timeoutMs`.
[[nodiscard]] inline bool WaitForZero(const std::atomic<int>& count,
                                      DWORD timeoutMs) {
  for (DWORD waited = 0; waited < timeoutMs; waited += kDrainStepMs) {
    if (count == 0) {
      return true;
    }
    Sleep(kDrainStepMs);
  }
  return count == 0;
}

// Whether every script callback has been destroyed within `timeoutMs`.
[[nodiscard]] inline bool WaitForScriptCallbacks(DWORD timeoutMs) {
  return WaitForZero(g_liveScriptCallbacks, timeoutMs);
}

// Whether no counted detour is running within `timeoutMs`, the hooks being
// off. A step first: a thread MinHook let go of at a detour's first
// instruction has counted itself by then.
[[nodiscard]] inline bool WaitForRunningDetours(DWORD timeoutMs) {
  Sleep(kDrainStepMs);
  return WaitForZero(g_runningDetours, timeoutMs);
}

// Hand `utf8Script` to Edge to run in `frame`, in isolated world `worldId`
// (ours unless named), with `callbackSlot` as the callback Edge calls (null
// for none).
inline void ExecuteScriptInFrame(const void* frame, std::string_view utf8Script,
                                 const void** callbackSlot,
                                 int worldId = edge155::off::kScriptWorldId) {
  const OwnedU16String script(utf8Script);
  using ExecuteFn =
      void(__fastcall*)(const void*, const U16String*, const void**, int);
  Callable<ExecuteFn>(
      edge155::rva::RenderFrameHostImpl_ExecuteJavaScriptInIsolatedWorld)(
      frame, &script.text, callbackSlot, worldId);
}

// Run `utf8Script` in `frame`, in our isolated world, and pass `done` the
// script's string result on the UI thread later (pass an empty `done` to not
// wait).
inline bool RunScriptInFrame(const void* frame, const std::string& utf8Script,
                             ScriptDone done) {
  if (!frame) {
    return false;
  }
  const void* callbackSlot = nullptr;
  if (done) {
    auto state = std::make_unique<ScriptCallbackState>();
    using BaseCtor = void*(__fastcall*)(void*, void*, void*);
    Callable<BaseCtor>(edge155::rva::BindStateBase_ctor)(
        state.get(), reinterpret_cast<void*>(InvokeScriptCallback),
        reinterpret_cast<void*>(DestroyScriptCallback));
    state->done = std::move(done);
    g_liveScriptCallbacks++;
    // Edge owns it from here and frees it through DestroyScriptCallback.
    callbackSlot = state.release();
  }
  ExecuteScriptInFrame(frame, utf8Script, &callbackSlot);
  return true;
}

// Run `utf8Script` in the page's main frame, in isolated world `worldId`
// (ours unless named), and never wait for an answer: no callback is built, so
// this needs no BindStateBase.
inline bool RunScriptUnanswered(void* webContents, std::string_view utf8Script,
                                int worldId = edge155::off::kScriptWorldId) {
  const void* frame = LiveMainFrame(webContents);
  if (!frame) {
    return false;
  }
  const void* noCallback = nullptr;
  ExecuteScriptInFrame(frame, utf8Script, &noCallback, worldId);
  return true;
}

// Run `utf8Script` in the page's own (main) world of its main frame, never
// waiting for an answer: only where Edge allows it (a WebUI page), since
// ExecuteJavaScript CHECKs that and would bring Edge down. False when it is
// not run.
inline bool RunMainWorldScriptUnanswered(void* webContents,
                                         std::string_view utf8Script) {
  const void* frame = LiveMainFrame(webContents);
  if (!frame) {
    return false;
  }
  using CanExecuteFn = bool(__fastcall*)(const void*);
  const bool canExecute = Callable<CanExecuteFn>(
      edge155::rva::RenderFrameHostImpl_CanExecuteJavaScript)(frame);
  if (!canExecute) {
    return false;
  }
  const OwnedU16String script(utf8Script);
  const void* noCallback = nullptr;
  using ExecuteFn =
      void(__fastcall*)(const void*, const U16String*, const void**);
  Callable<ExecuteFn>(edge155::rva::RenderFrameHostImpl_ExecuteJavaScript)(
      frame, &script.text, &noCallback);
  return true;
}

inline bool RunScript(void* webContents, const std::string& utf8Script,
                      ScriptDone done) {
  return RunScriptInFrame(LiveMainFrame(webContents), utf8Script,
                          std::move(done));
}

}  // namespace edgeplus::edge
