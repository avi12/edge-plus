// Edge split-view ABI, pinned to the msedge.dll build named by kVersion* below.
// Single source of truth in code; the prose is docs/re/split-view-abi.md.
// Every value is a module-relative RVA or a struct field offset verified from the
// public PDB + disassembly. Re-verify after an Edge update (tools/re/pdbid.py).
#pragma once
#include <cstdint>

namespace fixedge::edge155 {

// The build these offsets belong to (the module's file version,
// major.minor.build.revision). verify.h refuses to hook anything else.
inline constexpr wchar_t kModule[] = L"msedge.dll";
inline constexpr int kVersionMajor = 155;
inline constexpr int kVersionMinor = 0;
inline constexpr int kVersionBuild = 4283;
inline constexpr int kVersionRevision = 45;
// TODO: the PE header's TimeDateStamp, as an extra guard (0 = not checked).
inline constexpr uint32_t kTimeDateStamp = 0;
// The preferred load address the PDB and the disassembly are written against.
inline constexpr uintptr_t kImageBase = 0x180000000;

// --- function RVAs (add to the msedge.dll base at runtime) ---
namespace rva {
inline constexpr uintptr_t MultiContentsView_ctor = 0x2cdf896;
// cap: index < 2
inline constexpr uintptr_t MultiContentsView_SetWebContentsAtIndex = 0xc930f18;
// the `2` in cmp index,2
inline constexpr uintptr_t SetWebContentsAtIndex_guard_imm = 0xc930f22;
// loops the vector
inline constexpr uintptr_t MultiContentsView_ShowSplitView = 0xc930ffc;
// CHECK(index < 2)
inline constexpr uintptr_t MultiContentsView_SetActiveIndex = 0xc93137e;
// loops the vector
inline constexpr uintptr_t MultiContentsView_UpdateContentsBorderAndOverlay =
    0x2e0c13a;
// 2 entries + 1 sep
inline constexpr uintptr_t MultiContentsView_CalculateProposedLayout =
    0x2b22540;
// 2-element size array
inline constexpr uintptr_t MultiContentsView_GetViewSizes = 0x2b229e0;
inline constexpr uintptr_t MultiContentsView_CalculateSeparatorLayouts =
    0x2b22ae6;
inline constexpr uintptr_t MultiContentsView_ClampToMinSize = 0x2b22aa2;
// already N-ready
inline constexpr uintptr_t MultiContentsView_ExecuteOnEachVisible = 0x2b56844;
inline constexpr uintptr_t EdgeMultiContentsResizeArea_ctor = 0x2ce11b0;
inline constexpr uintptr_t ContentsContainerView_ctor = 0x2ce1b32;
inline constexpr uintptr_t EdgeContentsContainerView_ctor = 0x2ce12b0;
inline constexpr uintptr_t SplitWindowControllerImplV2_ctor = 0x2ccfac2;
inline constexpr uintptr_t IsEdgeSplitWindowEnabled = 0x0fb9f2d;
// The layout applier + the one append primitive the geometry uses. emplace_back
// grows the real std::vector<ChildLayout> with the real allocator, so calling it
// is the only safe way to add pane/separator entries from the injected DLL.
inline constexpr uintptr_t LayoutManagerBase_ApplyLayout = 0x0fd3ed8;
// std::vector<ChildLayout>::emplace_back<View*, bool, const gfx::Rect&>
//   (rcx = &vector, rdx = View**, r8 = bool*, r9 = gfx::Rect*)
inline constexpr uintptr_t ChildLayouts_emplace_back = 0x278099a;

// --- pane construction (ctor.md) — all must run on the UI thread ---
// (size)->void*  View allocator
inline constexpr uintptr_t CheckedOperatorNew3 = 0x101bd60;
// (parent, child, index)
inline constexpr uintptr_t AddChildViewAtImpl = 0x1017902;
// (this, bool)  vtable+0x98
inline constexpr uintptr_t View_SetVisible = 0x100b6b0;
inline constexpr uintptr_t View_GetVisible = 0x0f96a22;  // bool (this)
// (&vec@+0x368, raw_ptr*)
inline constexpr uintptr_t Containers_emplace_back = 0x2ce0508;
// (p) BackupRef, gated
inline constexpr uintptr_t RawPtr_AcquireInternal = 0x0f946b0;
// (this) mark layout dirty
inline constexpr uintptr_t View_InvalidateLayout = 0x0fb5540;
// (this) relayout now
inline constexpr uintptr_t View_LayoutImmediately = 0x0fd40b0;

// --- tab wiring (docs/re/tab-wiring.md): give a grown pane real web content ---
// (cwv, WebContents*)
inline constexpr uintptr_t ContentsWebView_SetWebContents = 0x2b57520;
// (WebContents*) force-paint a bg tab
inline constexpr uintptr_t WebContents_WasShown = 0x29f2b30;
// (tsm, int index) bounds-checked
inline constexpr uintptr_t TabStripModel_GetWebContentsAt = 0x101530e;

// --- the feed pipeline (controller.md) ---
// (BrowserView*) feeds panes
inline constexpr uintptr_t BrowserView_ShowSplitView = 0xc93594a;

// --- native split data-model grow (docs/re/split-insert.md, split-grow-impl.md) ---
// (size)->void*  (non-View new)
inline constexpr uintptr_t EdgeOperatorNew = 0x36b3c20;
// (out base::Token*) random 16B id
inline constexpr uintptr_t TokenCreateRandom = 0x1e98440;
// ({layout,ratio=0.5})
inline constexpr uintptr_t SplitTabVisualData_ctor = 0x2ce031e;
// create 2-pane (caps at 2)
inline constexpr uintptr_t TabStripModel_AddToNewSplit = 0xa273926;
// N-generic add worker
inline constexpr uintptr_t TabStripModel_AddToSplitImpl = 0xa273e30;
// (tsm, anchorMember, newIdx, 0) add-to-split
inline constexpr uintptr_t TabStripModel_UpdateTabInSplit = 0xa272ccc;
inline constexpr uintptr_t TabStripModel_RemoveSplitImpl = 0xa26df6a;
inline constexpr uintptr_t GetTabsAndIndicesInSplit = 0xa269a50;
inline constexpr uintptr_t GetSplitForTab = 0x2b2a682;
// (tsm, &id)->SplitTabData*
inline constexpr uintptr_t GetSplitData = 0xa2677a4;
// (rootColl, int)->TabModel*
inline constexpr uintptr_t GetTabAtIndexRecursive = 0x0fb86b2;
// (rootColl, TabModel*)->int
inline constexpr uintptr_t GetIndexOfTabRecursive = 0x0fb8408;
inline constexpr uintptr_t TabStripModel_GetActiveTab = 0x0fd2e60;
inline constexpr uintptr_t TabStripModel_InsertDetachedTabAt = 0xa267bb6;
// (aura::Window*)->BrowserView*
inline constexpr uintptr_t BrowserView_ForNativeWindow = 0x107d3e0;
// (HWND)->aura::Window*
inline constexpr uintptr_t GetContentWindowForHWND = 0x26af8e6;

// --- the split's 3-dot menu (extension point for directional "add pane") ---
inline constexpr uintptr_t SplitTabMenuModel_ctor = 0xa264f8e;
inline constexpr uintptr_t SplitTabMenuModel_ExecuteCommand = 0xa265830;
inline constexpr uintptr_t SplitTabMenuModel_GetLabelForCommandId = 0xa265430;
// (model, cmd_id, &u16string)
inline constexpr uintptr_t SimpleMenuModel_AddItem = 0x101bf04;
// diag: the ⋯ split menu
inline constexpr uintptr_t WebUISplitTabsControl_HandleContextMenu = 0xa2d2fe0;
inline constexpr uintptr_t UTF8ToUTF16 = 0x101cae0;  // (&out_u16, &string_view)

// --- collapse (docs/re/collapse.md): split button while split = keep active ---
// SplitWindowControllerImplV2::DoActionToSplitWindow(controller, WebContents*, int action).
// action 2 = dissolve keeping tabs + activate the active tab (fills window);
// action 3 = same but keeps the LAST tab. Clamp 3->2 to always keep the active.
inline constexpr uintptr_t SplitWindowController_DoAction = 0xa021020;

// --- per-pane hover controls (the X / ⋯ bar, old-toolbar mode) ---
// EnsureControlButtons builds a ControlButtons bar for containers 0 and 1 only,
// and ShowSplitView registers hover on the FIRST and LAST member's WebContents
// only, so a middle pane has a bar but no hover and pane 3+ has hover but no bar.
// (this, BrowserView*, bool not_first)
inline constexpr uintptr_t ControlButtons_ctor = 0xa010a6a;
// Static: (WebContents* a, WebContents* b) -> CreateOrGet + StartObserving each
// (idempotent; a no-op under the new toolbar).
inline constexpr uintptr_t SplitWindow_ListenWebContentsInputEvent = 0xa01fe4e;
// What a pane's hover runs: show the bar of the container showing the given
// WebContents, hide the rest. (controller, WebContents*) -> bool. Controller
// from the Browser.
inline constexpr uintptr_t SplitWindowController_ShowControlButton = 0xa020e40;
// (Browser*)
inline constexpr uintptr_t SplitWindowController_FromBrowser = 0x2e70b32;

// --- the drag grip in the hover bar (docs/re/drag-dock.md) ---
// AddButton(bar, int tooltip_string_id, const gfx::VectorIcon*, bool clickable)
// -> ControlButtons::Button*; clickable=false wires no click handler.
inline constexpr uintptr_t ControlButtons_AddButton = 0xa0117b6;
// InitLayout(bar): BoxLayout + preferred size {32 * visible children, 32}.
inline constexpr uintptr_t ControlButtons_InitLayout = 0xa010c8c;
// (parent, child, size_t index)
inline constexpr uintptr_t View_ReorderChildView = 0x269fa62;
// (parent, child) non-deleting
inline constexpr uintptr_t View_RemoveChildView = 0x26a2c28;
// static (src, dst, gfx::Point*) in place
inline constexpr uintptr_t View_ConvertPointToTarget = 0x0fd8b86;
// (view, const std::u16string&)
inline constexpr uintptr_t View_SetTooltipText = 0x0fd48f0;
// views::Button's mouse handlers (vtable +0x118..+0x130 of every button,
// ControlButtons::Button included). A real press on a pane bar never reaches
// the window procedure as WM_LBUTTONDOWN, so the grip is driven from here.
// bool (button, const ui::MouseEvent&)
inline constexpr uintptr_t Button_OnMousePressed = 0xb287ce0;
// bool (button, const ui::MouseEvent&)
inline constexpr uintptr_t Button_OnMouseDragged = 0xb287d70;
// void (button, const ui::MouseEvent&)
inline constexpr uintptr_t Button_OnMouseReleased = 0xb287ee0;
// void (button)
inline constexpr uintptr_t Button_OnMouseCaptureLost = 0xb287f00;

// --- closing one pane of a grown split (docs/re/close-pane.md) ---
// Edge's X: ControlButtons::HandleButtonClick(bar, Button*). For pane 2+ it
// calls SplitWindowController::HideSplitView(true), which closes the split's
// LAST tab (not the clicked one), and closing any member dissolves the split.
inline constexpr uintptr_t ControlButtons_HandleButtonClick = 0xa011ae0;
// ControlButtons::ToggleContextMenu(bar, ...): the ⋯ of a pane's hover bar. It
// builds a SplitTabMenuModel that only knows "first pane or not", so the pane
// whose ⋯ was clicked is remembered here.
inline constexpr uintptr_t ControlButtons_ToggleContextMenu = 0xa011d00;
// (tsm, int index, uint32 closeTypes); HideSplitView passes 1
inline constexpr uintptr_t TabStripModel_CloseWebContentsAt = 0xa26f916;
// (tsm, int index, const TabStripUserGestureDetails*)
inline constexpr uintptr_t TabStripModel_ActivateTabAt = 0xa26c482;

// --- the dividers between panes (docs/re/close-pane.md) ---
// EdgeMultiContentsResizeArea(this, MultiContentsView*): the 2-pane split's
// resize handle (8x8 preferred, hover pill; vertical when taller than wide).
// MultiContentsView's ctor allocates it with 0x358 bytes.
// views::EdgeTwoDimensionalResizeArea's own mouse handlers (overriding
// views::ResizeArea's): bool (view, const ui::MouseEvent&) for press / drag,
// void (view, const ui::MouseEvent&) for release, void (view) for capture lost.
inline constexpr uintptr_t ResizeArea2D_OnMousePressed = 0xb1fac20;
inline constexpr uintptr_t ResizeArea2D_OnMouseDragged = 0xb1facb0;
inline constexpr uintptr_t ResizeArea2D_OnMouseReleased = 0xb1facf0;
inline constexpr uintptr_t ResizeArea2D_OnMouseCaptureLost = 0xb1fad50;

// --- 2-pane-only CHECK sites that int3 when a split grows past 2 members ---
// Accessible-label builder: maps a split tab's pane index to a string id
// (0x3e7f pane0 / 0x3e80 pane1) and `cmp eax,1; jne <int3>` crashes at index>=2.
// `jne 0x42e72eb` (0F 85 rel32, 6 bytes); NOP it so index>=2 falls through to
// the pane-1 label instead of crashing. a11y text for pane 3+ is cosmetic.
inline constexpr uintptr_t SplitA11yLabel_paneidx_jne = 0x42e6faf;
// MultiContentsView::SetActiveIndex's `cmp edx,2; jae <int3>` (0F 83 rel32, 6
// bytes). The detour replaces the function while loaded; NOPing the CHECK too
// means a click on pane 3+ after unload only mis-flags the primary pane.
inline constexpr uintptr_t SetActiveIndex_index_jae = 0xc931388;
}  // namespace rva

// Linear addresses (not RVAs): partition-alloc BRP-pool gate for raw_ptr acquire.
namespace addr {
// [here] = BRP pool base mask cmp
inline constexpr uintptr_t PA_setup_plus8 = kImageBase + 0x140f5b48;
}  // namespace addr

// --- struct field offsets ---
namespace off {

// --- MultiContentsView : public views::View, public views::LayoutManagerBase ---
// The View base is at +0; the LayoutManagerBase subobject is embedded at +0x340
// (verified: CalculateProposedLayout's `this` is that subobject, and it recovers
// the MCV with `lea rax,[this-0x340]` before calling the MCV-relative helpers).
inline constexpr uintptr_t MCV_layout_base = 0x340;
// ContentsContainerView** (vector begin)
inline constexpr uintptr_t MCV_containers_data = 0x368;
inline constexpr uintptr_t MCV_containers_count = 0x370;  // size_t
// EdgeMultiContentsResizeArea* (single)
inline constexpr uintptr_t MCV_resize_area = 0x3f0;
// SplitTabLayout enum (0 = one axis, != 0 the other)
inline constexpr uintptr_t MCV_orientation = 0x428;
inline constexpr uintptr_t MCV_split_ratio = 0x430;  // double (default 0.5)
// int (SetActiveIndex writes it)
inline constexpr uintptr_t MCV_active_index = 0x420;
// BrowserView* (arg to each container ctor)
inline constexpr uintptr_t MCV_browser_view = 0x350;
// size_t (View's child count; AddChildViewAt index)
inline constexpr uintptr_t MCV_child_insert_idx = 0x1a8;
// views::View* (relative to the View base)
inline constexpr uintptr_t MCV_drop_target_view = 0x0b0;

// --- BrowserView ---
// MultiContentsView* within BrowserView
inline constexpr uintptr_t BV_multi_contents_view = 0x6e0;
inline constexpr uintptr_t BV_browser = 0x600;  // Browser* within BrowserView

// --- ContentsWebView ---
// bool on ContentsWebView (pane-0 fixup)
inline constexpr uintptr_t CWV_primary_flag = 0x450;
// WebContents* (WebView::web_contents())
inline constexpr uintptr_t CWV_web_contents = 0x390;

// --- Browser / TabStripModel (docs/re/tab-wiring.md) ---
// TabStripModel* within Browser
inline constexpr uintptr_t BROWSER_tab_strip_model = 0x30;
// root TabCollection* within TabStripModel
inline constexpr uintptr_t TSM_collection = 0x10;
// active TabModel* within TabStripModel
inline constexpr uintptr_t TSM_active_tab = 0x98;
// int tab count within TabCollection
inline constexpr uintptr_t TABCOLL_count = 0x18;

// --- TabModel virtual-table slots (docs/re/split-grow-impl.md) ---
// GetSplit(this, out optional<SplitTabId>)
inline constexpr uintptr_t TABMODEL_vtbl_GetSplit = 0x118;
inline constexpr uintptr_t TABMODEL_vtbl_IsSplit = 0x108;  // bool IsSplit(this)

// --- ContentsContainerView ---
inline constexpr uintptr_t CCV_contents_web_view = 0x3a8;  // ContentsWebView*
inline constexpr uintptr_t CCV_is_active = 0x38d;          // bool

// --- EdgeContentsContainerView and its ControlButtons bar ---
// The control-buttons interface subobject: its vtable holds
// SetControlButtons(unique_ptr<ControlButtons>*) @+0x10 and
// SetControlButtonsVisible(bool) @+0x18; the bar pointer lives at iface+0x1f8.
inline constexpr uintptr_t ECCV_buttons_iface = 0x348;
// ControlButtons* (iface + 0x1f8)
inline constexpr uintptr_t ECCV_control_buttons = 0x540;
inline constexpr uintptr_t ECCV_vtbl_SetControlButtons = 0x10;
inline constexpr size_t ControlButtons_size = 0x380;
inline constexpr size_t EdgeResizeArea_size = 0x358;
// a valid id (⋯'s); grip overrides the tooltip
inline constexpr int ControlButtons_more_string_id = 0x4676;
// The bar's buttons (InitButtons): ⋯ @+0x340, X @+0x350.
inline constexpr uintptr_t ControlButtons_close_button = 0x350;

// --- TabStripModel close / activate arguments ---
// TabStripModel::CloseTypes: CLOSE_USER_GESTURE, what Edge's own X passes.
inline constexpr uint32_t TSM_close_user_gesture = 1;
// sizeof(TabStripUserGestureDetails) {GestureType, base::TimeTicks}
inline constexpr size_t TSM_gesture_details_size = 16;

// --- gfx::VectorIcon path command ids in this build ---
// Decoded from kEdgeSplitWindowMoreIcon: CANVAS_DIMENSIONS 16, MOVE_TO x y,
// R_ARC_TO rx ry angle large sweep dx dy, CLOSE.
inline constexpr int ICON_MOVE_TO = 7;
inline constexpr int ICON_R_ARC_TO = 10;
inline constexpr int ICON_CLOSE = 27;
inline constexpr int ICON_CANVAS_DIMENSIONS = 28;

// --- views::View ---
// View* parent_ (null at the widget's root view)
inline constexpr uintptr_t VIEW_parent = 0x198;
// gfx::Rect bounds_ in parent coords (GetLocalBounds reads size @+0x1d0)
inline constexpr uintptr_t VIEW_bounds = 0x1c8;

// --- ui::LocatedEvent ---
// (LocatedEvent ctor): location_ @+0xb0, root_location_ @+0xb8, each a
// gfx::PointF; the root location is in the window's DIPs.
inline constexpr uintptr_t EVENT_root_location = 0xb8;

// --- views::ProposedLayout ---
// The out-param CalculateProposedLayout fills; its ctor zeroes 0x20 bytes.
inline constexpr uintptr_t PL_host_size = 0x00;  // gfx::Size {int w, int h}
// std::vector<ChildLayout> {begin, end, cap}
inline constexpr uintptr_t PL_child_layouts = 0x08;

// --- views::ChildLayout ---
// The element of PL_child_layouts; stride 0x30, from ApplyLayout.
inline constexpr uintptr_t CL_stride = 0x30;
inline constexpr uintptr_t CL_child = 0x00;    // views::View*
inline constexpr uintptr_t CL_visible = 0x08;  // bool
inline constexpr uintptr_t CL_bounds = 0x0c;   // gfx::Rect {int x, y, w, h}
// views::SizeBounds {SizeBound w{val,has}, h{val,has}}
inline constexpr uintptr_t CL_avail = 0x1c;

// --- gfx::Rect field offsets within CL_bounds ---
inline constexpr uintptr_t RECT_x = 0x0;
inline constexpr uintptr_t RECT_y = 0x4;
inline constexpr uintptr_t RECT_w = 0x8;
inline constexpr uintptr_t RECT_h = 0xc;
}  // namespace off

}  // namespace fixedge::edge155
