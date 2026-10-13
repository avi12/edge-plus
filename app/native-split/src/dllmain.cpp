// fixedge-split.dll — injected into Edge's browser process to grow the native
// split beyond two panes. This file is the feasibility foothold: it proves the
// chain works end to end (inject -> locate msedge.dll -> MinHook -> resolve the
// verified RVAs -> intercept the split's own methods and read its live state)
// before any N-pane surgery. The N-pane geometry replacement builds on this.
//
// Authorized personal customization of the user's own Edge (the fix-edge repo).
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include "MinHook.h"
#include "offsets.h"

using namespace fixedge::edge155;

// ---- logging: a file the panel can read, plus the debugger ----
static void Log(const char* fmt, ...) {
  char line[512];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(line, sizeof(line) - 2, fmt, ap);
  va_end(ap);
  if (n < 0) return;
  line[n++] = '\n';
  line[n] = 0;
  OutputDebugStringA(line);
  wchar_t path[MAX_PATH];
  DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
  if (!len) return;
  wcscat_s(path, L"\\FixEdge\\split-native.log");
  HANDLE h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  DWORD wrote;
  WriteFile(h, line, (DWORD)n, &wrote, nullptr);
  CloseHandle(h);
}

static uintptr_t g_base = 0;
static HMODULE g_self = nullptr;
// The companion app toggles the feature off by signalling this; the DLL then
// removes its hooks and unloads itself, so Edge is back to stock without a restart.
static constexpr wchar_t kUnloadEvent[] = L"Local\\fixedge-split-unload";
static inline void* At(uintptr_t rva) { return reinterpret_cast<void*>(g_base + rva); }
static inline uint32_t U32(void* obj, uintptr_t off) { return *reinterpret_cast<uint32_t*>((uint8_t*)obj + off); }
static inline uint64_t U64(void* obj, uintptr_t off) { return *reinterpret_cast<uint64_t*>((uint8_t*)obj + off); }

// ---- hook: SetWebContentsAtIndex(self, WebContents*, index) ----
// Fires whenever Edge puts a tab's web contents into a pane. Safe 3-arg shape.
using SetWC_t = void*(__fastcall*)(void* self, void* wc, uint32_t index);
static SetWC_t o_SetWC = nullptr;
static void* __fastcall h_SetWC(void* self, void* wc, uint32_t index) {
  uint64_t count = U64(self, off::MCV_containers_count);
  Log("[hook] SetWebContentsAtIndex self=%p index=%u wc=%p paneCount=%llu",
      self, index, wc, (unsigned long long)count);
  return o_SetWC(self, wc, index);
}

// ---- hook: CalculateProposedLayout(self, out, bounds) ----
// Fires on every relayout of a split. Reads the live pane count + active index.
using CalcLayout_t = void*(__fastcall*)(void* self, void* out, void* bounds);
static CalcLayout_t o_CalcLayout = nullptr;
static void* __fastcall h_CalcLayout(void* self, void* out, void* bounds) {
  Log("[hook] CalculateProposedLayout self=%p paneCount=%llu active=%d",
      self, (unsigned long long)U64(self, off::MCV_containers_count),
      (int)U32(self, off::MCV_active_index));
  return o_CalcLayout(self, out, bounds);
}

template <class T>
static bool Hook(uintptr_t rva, void* detour, T** orig, const char* name) {
  MH_STATUS s = MH_CreateHook(At(rva), detour, reinterpret_cast<void**>(orig));
  if (s != MH_OK) { Log("[err] MH_CreateHook %s failed: %d", name, s); return false; }
  s = MH_EnableHook(At(rva));
  if (s != MH_OK) { Log("[err] MH_EnableHook %s failed: %d", name, s); return false; }
  Log("[ok] hooked %s at %p", name, At(rva));
  return true;
}

static DWORD WINAPI Init(LPVOID) {
  HMODULE edge = nullptr;
  for (int i = 0; i < 100 && !edge; ++i) {       // msedge.dll may still be loading
    edge = GetModuleHandleW(kModule);
    if (!edge) Sleep(50);
  }
  if (!edge) { Log("[err] %ls not found in process", kModule); return 0; }
  g_base = reinterpret_cast<uintptr_t>(edge);
  wchar_t modpath[MAX_PATH]; GetModuleFileNameW(edge, modpath, MAX_PATH);
  Log("[init] fixedge-split loaded. %ls base=%p (%ls)", kModule, (void*)g_base, modpath);

  if (MH_Initialize() != MH_OK) { Log("[err] MH_Initialize failed"); return 0; }
  Hook(rva::MultiContentsView_SetWebContentsAtIndex, (void*)h_SetWC, &o_SetWC, "SetWebContentsAtIndex");
  Hook(rva::MultiContentsView_CalculateProposedLayout, (void*)h_CalcLayout, &o_CalcLayout, "CalculateProposedLayout");
  Log("[init] hooks installed; waiting for a split to be created");

  // Toggle-off from the app: wait for the unload signal, then cleanly detach.
  // Manual-reset, so the panel's IsActive() sees it the instant we are loaded.
  HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, kUnloadEvent);
  // A prior load cycle can leave the named object alive and still signalled for
  // a moment after its DLL woke; clear it so a quick off->on re-toggle does not
  // make this fresh instance unload itself immediately.
  ResetEvent(ev);
  WaitForSingleObject(ev, INFINITE);
  Log("[unload] signalled; removing hooks");
  MH_DisableHook(MH_ALL_HOOKS);
  MH_Uninitialize();
  if (ev) CloseHandle(ev);
  Log("[unload] fixedge-split detached");
  FreeLibraryAndExitThread(g_self, 0);  // drop out of Edge without a restart
  return 0;
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_self = mod;
    DisableThreadLibraryCalls(mod);
    CreateThread(nullptr, 0, Init, nullptr, 0, nullptr);  // off the loader lock
  }
  return TRUE;
}
