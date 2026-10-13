// fixedge-split.dll — injected into Edge's browser process to grow the native
// split beyond two panes. Authorized personal customization of the user's own
// Edge (the fix-edge repo). Pinned to msedge.dll 155.0.4283.45 via offsets.h.
//
// Strategy (see docs/re/*): everything that FEEDS the split already loops over
// pane count; only MultiContentsView caps at two. So we
//   1. grow the container vector to N real EdgeContentsContainerViews (ctor.md),
//   2. replace the layout geometry with one that tiles N panes (split-view-abi.md).
// Both run on the UI thread, driven from a hook on BrowserView::ShowSplitView.
// Everything is gated on count > 2, so a stock two-pane split is byte-for-byte
// untouched and cannot regress.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
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
// How many panes a shown split should grow to. 1 = leave stock (off). The panel
// will drive this later; for now the DLL reads it from an env var at load so a
// test can ask for 3, 4, ... without a rebuild.
static int g_targetPanes = 1;

static constexpr wchar_t kUnloadEvent[] = L"Local\\fixedge-split-unload";
static inline void* At(uintptr_t rva) { return reinterpret_cast<void*>(g_base + rva); }
template <class T> static inline T Rd(void* obj, uintptr_t off) {
  return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(obj) + off);
}
template <class T> static inline void Wr(void* obj, uintptr_t off, T v) {
  *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(obj) + off) = v;
}

// ---- resolved function pointers (bound in Init) ----
using fn_new_t      = void*(__fastcall*)(size_t);
using fn_ctor1_t    = void*(__fastcall*)(void*, void*);
using fn_addchild_t = void*(__fastcall*)(void*, void*, size_t);
using fn_emplaceC_t = void*(__fastcall*)(void*, void*);          // containers vector
using fn_setvis_t   = void (__fastcall*)(void*, bool);
using fn_inval_t    = void (__fastcall*)(void*);
using fn_emplaceL_t = void*(__fastcall*)(void*, void*, void*, void*); // ChildLayout emplace

static fn_new_t      p_new      = nullptr;
static fn_ctor1_t    p_eccvCtor = nullptr;
static fn_addchild_t p_addChild = nullptr;
static fn_emplaceC_t p_emplaceC = nullptr;
static fn_setvis_t   p_setVis   = nullptr;
static fn_inval_t    p_inval    = nullptr;
static fn_emplaceL_t p_emplaceL = nullptr;

// gfx::Rect is four ints {x, y, w, h}.
struct Rect { int32_t x, y, w, h; };

// Build one more EdgeContentsContainerView and append it to the MCV's container
// vector, exactly as the ctor builds panes 0 and 1 (ctor.md §6). UI thread only.
static bool AddContainer(void* mcv) {
  void* bv = Rd<void*>(mcv, off::MCV_browser_view);
  void* p = p_new(0x558);                                   // sizeof(EdgeContentsContainerView)
  if (!p) { Log("[grow] alloc failed"); return false; }
  p_eccvCtor(p, bv);                                        // builds child chain + vtables
  if (Rd<uint8_t>(p, 0x179) == 1) { Log("[grow] poisoned alloc"); return false; }
  size_t idx = Rd<size_t>(mcv, off::MCV_child_insert_idx);  // append at end
  p_addChild(mcv, p, idx);
  void* tmp = p;                                            // raw_ptr temp for emplace_back
  p_emplaceC(reinterpret_cast<uint8_t*>(mcv) + off::MCV_containers_data, &tmp);
  p_setVis(p, true);                                        // show the new pane
  return true;
}

// Grow to exactly n panes (no-op if already >= n). Returns the resulting count.
static size_t GrowToPanes(void* mcv, int n) {
  size_t count = Rd<size_t>(mcv, off::MCV_containers_count);
  while ((int)count < n) {
    if (!AddContainer(mcv)) break;
    count = Rd<size_t>(mcv, off::MCV_containers_count);
  }
  return count;
}

// ---- hook: BrowserView::ShowSplitView — the one pane feeder ----
// Runs on the UI thread. Let the original feed the real tabs into panes 0..k-1,
// then grow to the target pane count and force a relayout so the N-pane geometry
// detour lays them out.
using ShowSplit_t = void*(__fastcall*)(void* bv);
static ShowSplit_t o_ShowSplit = nullptr;
static void* __fastcall h_ShowSplit(void* bv) {
  void* r = o_ShowSplit(bv);
  if (g_targetPanes > 2) {
    void* mcv = Rd<void*>(bv, off::BV_multi_contents_view);
    if (mcv) {
      size_t before = Rd<size_t>(mcv, off::MCV_containers_count);
      if (before >= 1 && before < (size_t)g_targetPanes) {
        size_t after = GrowToPanes(mcv, g_targetPanes);
        Log("[grow] ShowSplitView mcv=%p panes %llu -> %llu", mcv,
            (unsigned long long)before, (unsigned long long)after);
        if (p_inval) p_inval(mcv);                 // mark dirty; next layout tiles N
      }
    }
  }
  return r;
}

// ---- hook: MultiContentsView::SetWebContentsAtIndex (index < 2 guard) ----
// Observational for now + a safety net: the real feeder only sends indices for
// the tabs that exist, so with blank grown panes it never calls index >= 2. If a
// real N-tab split is driven later, dropping the guard happens here.
using SetWC_t = void*(__fastcall*)(void*, void*, uint32_t);
static SetWC_t o_SetWC = nullptr;
static void* __fastcall h_SetWC(void* self, void* wc, uint32_t index) {
  Log("[hook] SetWebContentsAtIndex self=%p index=%u paneCount=%llu",
      self, index, (unsigned long long)Rd<size_t>(self, off::MCV_containers_count));
  return o_SetWC(self, wc, index);
}

// ---- hook: LayoutManagerBase::CalculateProposedLayout ----
// self is the LayoutManagerBase subobject; mcv = self - 0x340. arg3 is a
// const SizeBounds& giving the content box {width @+0, height @+8} plus a
// has-value byte per axis (@+4, @+0xc). Stock when count <= 2.
using CalcLayout_t = void*(__fastcall*)(void* self, void* out, void* avail);
static CalcLayout_t o_CalcLayout = nullptr;
static void* __fastcall h_CalcLayout(void* self, void* out, void* avail) {
  void* mcv = reinterpret_cast<uint8_t*>(self) - off::MCV_layout_base;
  size_t count = Rd<size_t>(mcv, off::MCV_containers_count);
  bool bothBounded = Rd<uint8_t>(avail, 0x4) == 1 && Rd<uint8_t>(avail, 0xc) == 1;
  if (count <= 2 || !bothBounded) {
    return o_CalcLayout(self, out, avail);          // stock path, untouched
  }

  // --- N-pane geometry ---
  int width  = Rd<int32_t>(avail, 0x0);
  int height = Rd<int32_t>(avail, 0x8);
  bool vertical = Rd<uint32_t>(mcv, off::MCV_orientation) != 0;  // match GetViewSizes axis pick

  // Zero the out ProposedLayout (its ctor only zeroes 0x20 bytes).
  std::memset(out, 0, 0x20);
  Wr<int32_t>(out, off::PL_host_size + 0, width);
  Wr<int32_t>(out, off::PL_host_size + 4, height);

  void* vec = reinterpret_cast<uint8_t*>(out) + off::PL_child_layouts;
  void** data = Rd<void**>(mcv, off::MCV_containers_data);
  int n = (int)count;
  int total = vertical ? height : width;
  int base = total / n;
  bool visible = true;

  for (int i = 0; i < n; ++i) {
    void* container = data[i];
    int start = i * base;
    int len = (i == n - 1) ? (total - start) : base;   // last pane takes the remainder
    Rect rc = vertical ? Rect{0, start, width, len}
                       : Rect{start, 0, len, height};
    p_emplaceL(vec, &container, &visible, &rc);
  }
  Log("[layout] tiled %d panes in %dx%d (%s)", n, width, height,
      vertical ? "rows" : "cols");
  return out;
}

template <class T>
static bool Hook(uintptr_t rva, void* detour, T** orig, const char* name) {
  if (MH_CreateHook(At(rva), detour, reinterpret_cast<void**>(orig)) != MH_OK ||
      MH_EnableHook(At(rva)) != MH_OK) {
    Log("[err] hook %s failed", name);
    return false;
  }
  Log("[ok] hooked %s at %p", name, At(rva));
  return true;
}

static void BindFns() {
  p_new      = reinterpret_cast<fn_new_t>(At(rva::CheckedOperatorNew3));
  p_eccvCtor = reinterpret_cast<fn_ctor1_t>(At(rva::EdgeContentsContainerView_ctor));
  p_addChild = reinterpret_cast<fn_addchild_t>(At(rva::AddChildViewAtImpl));
  p_emplaceC = reinterpret_cast<fn_emplaceC_t>(At(rva::Containers_emplace_back));
  p_setVis   = reinterpret_cast<fn_setvis_t>(At(rva::View_SetVisible));
  p_inval    = reinterpret_cast<fn_inval_t>(At(rva::View_InvalidateLayout));
  p_emplaceL = reinterpret_cast<fn_emplaceL_t>(At(rva::ChildLayouts_emplace_back));
}

static DWORD WINAPI Init(LPVOID) {
  HMODULE edge = nullptr;
  for (int i = 0; i < 100 && !edge; ++i) {
    edge = GetModuleHandleW(kModule);
    if (!edge) Sleep(50);
  }
  if (!edge) { Log("[err] %ls not found", kModule); return 0; }
  g_base = reinterpret_cast<uintptr_t>(edge);
  Log("[init] fixedge-split loaded. %ls base=%p", kModule, (void*)g_base);

  // Target pane count from the environment (the panel sets this before injecting;
  // a test exports FIXEDGE_PANES=3). Absent/invalid -> stay stock.
  wchar_t buf[16];
  if (GetEnvironmentVariableW(L"FIXEDGE_PANES", buf, 16)) {
    g_targetPanes = (int)wcstol(buf, nullptr, 10);
  }
  Log("[init] target panes = %d", g_targetPanes);

  BindFns();
  if (MH_Initialize() != MH_OK) { Log("[err] MH_Initialize failed"); return 0; }
  Hook(rva::BrowserView_ShowSplitView, (void*)h_ShowSplit, &o_ShowSplit, "ShowSplitView");
  Hook(rva::MultiContentsView_SetWebContentsAtIndex, (void*)h_SetWC, &o_SetWC, "SetWebContentsAtIndex");
  Hook(rva::MultiContentsView_CalculateProposedLayout, (void*)h_CalcLayout, &o_CalcLayout, "CalculateProposedLayout");
  Log("[init] hooks installed; split into %d panes armed", g_targetPanes);

  HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, kUnloadEvent);
  ResetEvent(ev);
  WaitForSingleObject(ev, INFINITE);
  Log("[unload] signalled; removing hooks");
  MH_DisableHook(MH_ALL_HOOKS);
  MH_Uninitialize();
  if (ev) CloseHandle(ev);
  Log("[unload] fixedge-split detached");
  FreeLibraryAndExitThread(g_self, 0);
  return 0;
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_self = mod;
    DisableThreadLibraryCalls(mod);
    CreateThread(nullptr, 0, Init, nullptr, 0, nullptr);
  }
  return TRUE;
}
