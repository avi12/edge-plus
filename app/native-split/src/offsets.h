// Edge split-view ABI, pinned to msedge.dll 155.0.4283.45 (image base 0x180000000).
// Single source of truth in code; the prose is docs/re/split-view-abi.md.
// Every value is a module-relative RVA or a struct field offset verified from the
// public PDB + disassembly. Re-verify after an Edge update (tools/re/pdbid.py).
#pragma once
#include <cstdint>

namespace fixedge::edge155 {

// The build these offsets belong to. DllMain refuses to hook anything else.
inline constexpr wchar_t kModule[] = L"msedge.dll";
inline constexpr char kBuild[] = "155.0.4283.45";
inline constexpr uint32_t kTimeDateStamp = 0;  // TODO: fill from PE header as an extra guard

// --- function RVAs (add to the msedge.dll base at runtime) ---
namespace rva {
inline constexpr uintptr_t MultiContentsView_ctor              = 0x2cdf896;
inline constexpr uintptr_t MultiContentsView_SetWebContentsAtIndex = 0xc930f18; // cap: index < 2
inline constexpr uintptr_t SetWebContentsAtIndex_guard_imm = 0xc930f22;         // the `2` in cmp index,2
inline constexpr uintptr_t MultiContentsView_ShowSplitView      = 0xc930ffc;    // loops the vector
inline constexpr uintptr_t MultiContentsView_CalculateProposedLayout = 0x2b22540; // 2 entries + 1 sep
inline constexpr uintptr_t MultiContentsView_GetViewSizes       = 0x2b229e0;     // 2-element size array
inline constexpr uintptr_t MultiContentsView_CalculateSeparatorLayouts = 0x2b22ae6;
inline constexpr uintptr_t MultiContentsView_ClampToMinSize     = 0x2b22aa2;
inline constexpr uintptr_t MultiContentsView_ExecuteOnEachVisible = 0x2b56844;   // already N-ready
inline constexpr uintptr_t EdgeMultiContentsResizeArea_ctor     = 0x2ce11b0;
inline constexpr uintptr_t ContentsContainerView_ctor           = 0x2ce1b32;
inline constexpr uintptr_t EdgeContentsContainerView_ctor       = 0x2ce12b0;
inline constexpr uintptr_t SplitWindowControllerImplV2_ctor     = 0x2ccfac2;
inline constexpr uintptr_t IsEdgeSplitWindowEnabled             = 0x0fb9f2d;
// The layout applier + the one append primitive the geometry uses. emplace_back
// grows the real std::vector<ChildLayout> with the real allocator, so calling it
// is the only safe way to add pane/separator entries from the injected DLL.
inline constexpr uintptr_t LayoutManagerBase_ApplyLayout        = 0x0fd3ed8;
// std::vector<ChildLayout>::emplace_back<View*, bool, const gfx::Rect&>
//   (rcx = &vector, rdx = View**, r8 = bool*, r9 = gfx::Rect*)
inline constexpr uintptr_t ChildLayouts_emplace_back            = 0x278099a;

// --- pane construction (ctor.md) — all must run on the UI thread ---
inline constexpr uintptr_t CheckedOperatorNew3   = 0x101bd60;  // (size)->void*  View allocator
inline constexpr uintptr_t AddChildViewAtImpl    = 0x1017902;  // (parent, child, index)
inline constexpr uintptr_t View_SetVisible       = 0x100b6b0;  // (this, bool)  vtable+0x98
inline constexpr uintptr_t Containers_emplace_back = 0x2ce0508; // (&vec@+0x368, raw_ptr*)
inline constexpr uintptr_t RawPtr_AcquireInternal = 0x0f946b0; // (p) BackupRef, gated
inline constexpr uintptr_t View_InvalidateLayout = 0x0fb5540;   // (this) mark layout dirty
inline constexpr uintptr_t View_LayoutImmediately = 0x0fd40b0;  // (this) relayout now
// --- tab wiring (docs/re/tab-wiring.md): give a grown pane real web content ---
inline constexpr uintptr_t ContentsWebView_SetWebContents = 0x2b57520; // (cwv, WebContents*)
inline constexpr uintptr_t WebContents_WasShown           = 0x29f2b30; // (WebContents*) force-paint a bg tab
inline constexpr uintptr_t TabStripModel_GetWebContentsAt = 0x101530e; // (tsm, int index) bounds-checked
// --- the feed pipeline (controller.md) ---
inline constexpr uintptr_t BrowserView_ShowSplitView = 0xc93594a; // (BrowserView*) feeds panes
// --- the split's 3-dot menu (extension point for directional "add pane") ---
inline constexpr uintptr_t SplitTabMenuModel_ctor          = 0xa264f8e;
inline constexpr uintptr_t SplitTabMenuModel_ExecuteCommand = 0xa265830;
inline constexpr uintptr_t SplitTabMenuModel_GetLabelForCommandId = 0xa265430;
inline constexpr uintptr_t SimpleMenuModel_AddItem         = 0x101bf04;  // (model, cmd_id, &u16string)
inline constexpr uintptr_t WebUISplitTabsControl_HandleContextMenu = 0xa2d2fe0; // diag: the ⋯ split menu
inline constexpr uintptr_t UTF8ToUTF16                     = 0x101cae0;  // (&out_u16, &string_view)
// --- collapse (docs/re/collapse.md): split button while split = keep active ---
// SplitWindowControllerImplV2::DoActionToSplitWindow(controller, WebContents*, int action).
// action 2 = dissolve keeping tabs + activate the active tab (fills window);
// action 3 = same but keeps the LAST tab. Clamp 3->2 to always keep the active.
inline constexpr uintptr_t SplitWindowController_DoAction  = 0xa021020;
}  // namespace rva

// Linear addresses (not RVAs): partition-alloc BRP-pool gate for raw_ptr acquire.
namespace addr {
inline constexpr uintptr_t PA_setup_plus8 = 0x180000000 + 0x140f5b48; // [here] = BRP pool base mask cmp
}

// --- struct field offsets ---
namespace off {
// MultiContentsView : public views::View, public views::LayoutManagerBase.
// The View base is at +0; the LayoutManagerBase subobject is embedded at +0x340
// (verified: CalculateProposedLayout's `this` is that subobject, and it recovers
// the MCV with `lea rax,[this-0x340]` before calling the MCV-relative helpers).
inline constexpr uintptr_t MCV_layout_base      = 0x340; // LayoutManagerBase subobject within MCV
inline constexpr uintptr_t MCV_containers_data = 0x368;  // ContentsContainerView** (vector begin)
inline constexpr uintptr_t MCV_containers_count = 0x370; // size_t
inline constexpr uintptr_t MCV_resize_area      = 0x3f0; // EdgeMultiContentsResizeArea* (single)
inline constexpr uintptr_t MCV_orientation      = 0x428; // SplitTabLayout enum (0 = one axis, != 0 the other)
inline constexpr uintptr_t MCV_split_ratio      = 0x430; // double (default 0.5)
inline constexpr uintptr_t MCV_browser_view     = 0x350; // BrowserView* (arg to each container ctor)
inline constexpr uintptr_t MCV_child_insert_idx = 0x1a8; // size_t (View's child count; AddChildViewAt index)
inline constexpr uintptr_t MCV_drop_target_view = 0x0b0; // views::View* (relative to the View base)
inline constexpr uintptr_t BV_multi_contents_view = 0x6e0; // MultiContentsView* within BrowserView
inline constexpr uintptr_t BV_browser           = 0x600; // Browser* within BrowserView
inline constexpr uintptr_t CWV_primary_flag     = 0x450; // bool on ContentsWebView (pane-0 fixup)
// Browser / TabStripModel (docs/re/tab-wiring.md)
inline constexpr uintptr_t BROWSER_tab_strip_model = 0x30;  // TabStripModel* within Browser
inline constexpr uintptr_t TSM_collection       = 0x10;  // TabCollection* within TabStripModel
inline constexpr uintptr_t TABCOLL_count        = 0x18;  // int tab count within TabCollection
// ContentsContainerView
inline constexpr uintptr_t CCV_contents_web_view = 0x3a8; // ContentsWebView*
inline constexpr uintptr_t CCV_is_active         = 0x38d; // bool

// views::ProposedLayout (the out-param CalculateProposedLayout fills; ctor zeroes 0x20)
inline constexpr uintptr_t PL_host_size     = 0x00;  // gfx::Size {int w, int h}
inline constexpr uintptr_t PL_child_layouts = 0x08;  // std::vector<ChildLayout> {begin, end, cap}

// views::ChildLayout (element of PL_child_layouts; stride 0x30, from ApplyLayout)
inline constexpr uintptr_t CL_stride   = 0x30;
inline constexpr uintptr_t CL_child    = 0x00;  // views::View*
inline constexpr uintptr_t CL_visible  = 0x08;  // bool
inline constexpr uintptr_t CL_bounds   = 0x0c;  // gfx::Rect {int x, y, w, h}
inline constexpr uintptr_t CL_avail    = 0x1c;  // views::SizeBounds {SizeBound w{val,has}, h{val,has}}

// gfx::Rect field offsets within CL_bounds
inline constexpr uintptr_t RECT_x = 0x0, RECT_y = 0x4, RECT_w = 0x8, RECT_h = 0xc;
}  // namespace off

}  // namespace fixedge::edge155
