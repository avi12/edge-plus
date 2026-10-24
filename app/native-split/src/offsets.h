// Edge split-view ABI, pinned to the msedge.dll build named by kVersion* below.
// Single source of truth in code; the prose is docs/re/split-view-abi.md.
// Every value is a module-relative RVA or a struct field offset verified from the
// public PDB + disassembly. Re-verify after an Edge update (tools/re/pdbid.py).
#pragma once
#include <cstdint>

namespace fixedge::edge155 {

// The build these offsets belong to (the module's file version,
// major.minor.build.revision). verify.h refuses to hook anything else.
// The @share markers hand the build to the WinUI panel, which holds the DLL
// back from any other (tools/Export-SharedConstants.ps1).
// @share EdgeModule
inline constexpr wchar_t kModule[] = L"msedge.dll";
// @share EdgeVersionMajor
inline constexpr int kVersionMajor = 156;
// @share EdgeVersionMinor
inline constexpr int kVersionMinor = 0;
// @share EdgeVersionBuild
inline constexpr int kVersionBuild = 4314;
// @share EdgeVersionRevision
inline constexpr int kVersionRevision = 42;
// The preferred load address the PDB and the disassembly are written against.
inline constexpr uintptr_t kImageBase = 0x180000000;

// --- function RVAs (add to the msedge.dll base at runtime) ---
namespace rva {
// cap: index < 2
inline constexpr uintptr_t MultiContentsView_SetWebContentsAtIndex = 0xa66e296;
// the `2` in cmp index,2
inline constexpr uintptr_t SetWebContentsAtIndex_guard_imm = 0xa66e2a0;
// CHECK(index < 2)
inline constexpr uintptr_t MultiContentsView_SetActiveIndex = 0xa66e6fc;
// void CloseSplitView(this): the view leaves split mode (BrowserView::
// MaybeUpdateSplitView, once the active tab is in no split). Returns at once
// unless the resize area (+0x3f0) is visible; else swaps the active pane's
// page into pane 0, empties pane 1 (ContentsWebView::SetWebContents(null)) and
// hides it and the resize area. Panes 2+ it never touches.
inline constexpr uintptr_t MultiContentsView_CloseSplitView = 0xa66e4f8;
// loops the vector
inline constexpr uintptr_t MultiContentsView_UpdateContentsBorderAndOverlay =
    0x2e2654a;
// 2 entries + 1 sep
inline constexpr uintptr_t MultiContentsView_CalculateProposedLayout =
    0x2c01930;
// FocusableViewsMap* GetFocusableViewsMapFor(this, ContentsContainerView*):
// a lookup in MCV+0x4c8, a flat_map with an entry per container Edge itself
// built (two), so null for ours. Its one caller,
// BrowserView::MaybeUpdateStoredFocusForWebContents (every tab switch),
// dereferences it.
inline constexpr uintptr_t MultiContentsView_GetFocusableViewsMapFor =
    0x2a4aa80;
inline constexpr uintptr_t EdgeMultiContentsResizeArea_ctor = 0x2b0d4c2;
inline constexpr uintptr_t EdgeContentsContainerView_ctor = 0x2b0d5c2;
// The one append primitive the geometry uses. EmplaceBack grows the real
// absl::InlinedVector<ChildLayout, 8> (onto the heap past 8 entries) with the
// real allocator, so calling it is the only safe way to add pane/separator
// entries from the injected DLL.
// absl::inlined_vector_internal::Storage<ChildLayout, 8>::EmplaceBack<View*,
// bool, const gfx::Rect&>
//   (rcx = &vector, rdx = View**, r8 = bool*, r9 = gfx::Rect*)
inline constexpr uintptr_t ChildLayouts_emplace_back = 0x284310c;

// --- pane construction (ctor.md) — all must run on the UI thread ---
// (size)->void*  View allocator
inline constexpr uintptr_t CheckedOperatorNew3 = 0x10df260;
// (parent, child, index)
inline constexpr uintptr_t AddChildViewAtImpl = 0x10dabee;
// (this, bool)  vtable+0x98
inline constexpr uintptr_t View_SetVisible = 0x1055e20;
inline constexpr uintptr_t View_GetVisible = 0x0fe0542;  // bool (this)
// vector<raw_ptr<ContentsContainerView,1>>::emplace_back<raw_ptr<...>>
// (&vec@+0x368, raw_ptr*): it MOVES the raw_ptr in, taking no BackupRefPtr
// reference, so the entry must hold one already (edge::AcquireRawPtrRef);
// the vector's destructor gives one back. A plain pointer passed as the
// raw_ptr left the count one short: closing the window freed our pane while
// its cached ChildLayout still pointed at it, and ReleaseInternal CHECKed
// (seen: a window with a grown split closed or torn off crashed Edge).
inline constexpr uintptr_t Containers_emplace_back = 0x2b0c7f8;
// base::internal::RawPtrBackupRefImpl<0>::AcquireInternal(uintptr_t), and the
// test raw_ptr's constructor makes before calling it: the address masked by
// BRP_pool_mask equals the BRP pool's base, a uint64 global at
// BRP_pool_base_global (read at the call in ChildLayouts_emplace_back).
inline constexpr uintptr_t RawPtrBackupRef_AcquireInternal = 0xfddd20;
inline constexpr uintptr_t BRP_pool_base_global = 0x142da588;
inline constexpr uint64_t BRP_pool_mask = 0xfffffffc00000000;
// (this) mark layout dirty
inline constexpr uintptr_t View_InvalidateLayout = 0x0ffa2d0;
// (view): queue a repaint of the whole view.
inline constexpr uintptr_t View_SchedulePaint = 0x1003340;
// (this) relayout now
inline constexpr uintptr_t View_LayoutImmediately = 0x10335ce;

// --- tab wiring (docs/re/tab-wiring.md): give a grown pane real web content ---
// (cwv, WebContents*)
inline constexpr uintptr_t ContentsWebView_SetWebContents = 0x2a49c50;
// (WebContents*) force-paint a bg tab
inline constexpr uintptr_t WebContents_WasShown = 0x29ff760;
// (tsm, int index) bounds-checked
inline constexpr uintptr_t TabStripModel_GetWebContentsAt = 0x105df8e;

// --- the feed pipeline (controller.md) ---
// void BrowserView::MaybeUpdateSplitView(this, WebContents* forContents): with
// the contents no longer in a split, runs MultiContentsView::CloseSplitView
// (clears pane 1, hides the resize area, moves content to container 0). The
// relayout a tab leaving a split would trigger, so a window is full-width right
// after a split is dissolved rather than on the user's next interaction.
inline constexpr uintptr_t BrowserView_MaybeUpdateSplitView = 0x2e0fae0;

// --- native split data-model grow (docs/re/split-insert.md, split-grow-impl.md) ---
// (size)->void*  (non-View new)
inline constexpr uintptr_t EdgeOperatorNew = 0x36cdec0;
// operator delete(void*): jumps to Edge's free (the allocator shim).
inline constexpr uintptr_t EdgeOperatorDelete = 0x36cdf10;
// split_tabs::LogSplitView{Created,Updated}UKM(TabStripModel*, const
// SplitTabId&): split metrics, each CHECK(ListTabs().size() == 2). Created runs
// from AddToSplitImpl, Updated from UpdateTabInSplitImpl and when a split
// member's page becomes primary (it navigates).
inline constexpr uintptr_t SplitTabs_LogSplitViewCreatedUKM = 0xa3cd407;
inline constexpr uintptr_t SplitTabs_LogSplitViewUpdatedUKM = 0xa3cd555;
// (out base::Token*) random 16B id
inline constexpr uintptr_t TokenCreateRandom = 0x1eb7f80;
// create 2-pane (caps at 2)
inline constexpr uintptr_t TabStripModel_AddToNewSplit = 0xa40af46;
// N-generic add worker
inline constexpr uintptr_t TabStripModel_AddToSplitImpl = 0xa40b450;
// (tsm, anchorMember, newIdx, 0) add-to-split
inline constexpr uintptr_t TabStripModel_UpdateTabInSplit = 0xa40a2f6;
inline constexpr uintptr_t TabStripModel_RemoveSplitImpl = 0xa40566e;
inline constexpr uintptr_t GetTabsAndIndicesInSplit = 0xa40116e;
// (tsm, &id)->SplitTabData*
inline constexpr uintptr_t GetSplitData = 0xa401d78;
// (rootColl, int)->TabModel*
inline constexpr uintptr_t GetTabAtIndexRecursive = 0x0ffc3c2;
// (rootColl, TabModel*)->int
inline constexpr uintptr_t GetIndexOfTabRecursive = 0x0ffc118;
// (aura::Window*)->BrowserView*
inline constexpr uintptr_t BrowserView_ForNativeWindow = 0x10cd6e8;
// (HWND)->aura::Window*
inline constexpr uintptr_t GetContentWindowForHWND = 0x26a4c46;

// --- the split's 3-dot menu (extension point for directional "add pane") ---
inline constexpr uintptr_t SplitTabMenuModel_ctor = 0xa3fca4e;
inline constexpr uintptr_t SplitTabMenuModel_ExecuteCommand = 0xa3fd2f0;
// (model, cmd_id, &u16string)
inline constexpr uintptr_t SimpleMenuModel_AddItem = 0x10df73e;
// diag: the ⋯ split menu
inline constexpr uintptr_t WebUISplitTabsControl_HandleContextMenu = 0xa470f70;
inline constexpr uintptr_t UTF8ToUTF16 = 0x10e03d0;  // (&out_u16, &string_view)

// --- collapse (docs/re/collapse.md): split button while split = keep active ---
// SplitWindowControllerImplV2::DoActionToSplitWindow(controller, WebContents*,
// int action), `action` one of off::SplitAction.
inline constexpr uintptr_t SplitWindowController_DoAction = 0xa12b680;

// --- per-pane hover controls (the X / ⋯ bar, old-toolbar mode) ---
// EnsureControlButtons builds a ControlButtons bar for containers 0 and 1 only,
// and ShowSplitView registers hover on the FIRST and LAST member's WebContents
// only, so a middle pane has a bar but no hover and pane 3+ has hover but no bar.
// (this, BrowserView*, bool not_first)
inline constexpr uintptr_t ControlButtons_ctor = 0xa11b0fa;
// ControlButtons::Button(this, const gfx::VectorIcon&): one round icon button
// of the bar (AddButton allocates it with ControlButtons_button_size bytes).
inline constexpr uintptr_t ControlButtons_Button_ctor = 0xa11b72a;
// Static: (WebContents* a, WebContents* b) -> CreateOrGet + StartObserving each
// (idempotent; a no-op under the new toolbar).
inline constexpr uintptr_t SplitWindow_ListenWebContentsInputEvent = 0xa12a4ae;
// What a pane's hover runs: show the bar of the container showing the given
// WebContents, hide the rest. (controller, WebContents*) -> bool. Controller
// from the Browser.
inline constexpr uintptr_t SplitWindowController_ShowControlButton = 0xa12b4a0;
// (Browser*)
inline constexpr uintptr_t SplitWindowController_FromBrowser = 0x2e7ea8a;

// --- the drag grip in the hover bar (docs/re/drag-dock.md) ---
// AddButton(bar, int tooltip_string_id, const gfx::VectorIcon*, bool clickable)
// -> ControlButtons::Button*; clickable=false wires no click handler.
inline constexpr uintptr_t ControlButtons_AddButton = 0xa11be46;
// InitLayout(bar): BoxLayout + preferred size {32 * visible children, 32}.
inline constexpr uintptr_t ControlButtons_InitLayout = 0xa11b31c;
// (parent, child, size_t index)
inline constexpr uintptr_t View_ReorderChildView = 0x2784138;
// (parent, child) non-deleting
inline constexpr uintptr_t View_RemoveChildView = 0x2787392;
// static (src, dst, gfx::Point*) in place
inline constexpr uintptr_t View_ConvertPointToTarget = 0x1020386;
// (view, const std::u16string&)
inline constexpr uintptr_t View_SetTooltipText = 0x1015040;
// views::Button's mouse handlers (vtable +0x118..+0x130 of every button,
// ControlButtons::Button included). A real press on a pane bar never reaches
// the window procedure as WM_LBUTTONDOWN, so the grip is driven from here.
// bool (button, const ui::MouseEvent&)
inline constexpr uintptr_t Button_OnMousePressed = 0xb3fa3a0;
// bool (button, const ui::MouseEvent&)
inline constexpr uintptr_t Button_OnMouseDragged = 0xb3fa430;
// void (button, const ui::MouseEvent&)
inline constexpr uintptr_t Button_OnMouseReleased = 0xb3fa5a0;
// void (button)
inline constexpr uintptr_t Button_OnMouseCaptureLost = 0xb3fa5c0;
// int ui::MouseEvent::GetClickCount(this): 2 on the second click of a double
// click (EdgeTwoDimensionalResizeArea resets its panes on that release).
inline constexpr uintptr_t MouseEvent_GetClickCount = 0xb7a1f68;
// bool views::View::IsMouseHovered(this): the cursor is over the view now.
inline constexpr uintptr_t View_IsMouseHovered = 0x28800e4;

// --- closing one pane of a grown split (docs/re/close-pane.md) ---
// Edge's X: ControlButtons::HandleButtonClick(bar, Button*). For pane 2+ it
// calls SplitWindowController::HideSplitView(true), which closes the split's
// LAST tab (not the clicked one), and closing any member dissolves the split.
inline constexpr uintptr_t ControlButtons_HandleButtonClick = 0xa11c170;
// ControlButtons::ToggleContextMenu(bar, ...): the ⋯ of a pane's hover bar. It
// builds a SplitTabMenuModel that only knows "first pane or not", so the pane
// whose ⋯ was clicked is remembered here.
inline constexpr uintptr_t ControlButtons_ToggleContextMenu = 0xa11c390;
// The bar's ⋯ menu: SplitWindowControlMenu. BuildMenuModel(this) builds a
// ui::SimpleMenuModel (kept at this+0x10, delegate this+8) and RETURNS it, and
// Run passes the return value to PopulateMenu(this, MenuItemView* root, model),
// which creates one item view per model item, so an item removed from the
// model first never shows. ExecuteCommand(this, int id, int flags) switches on
// off::ControlMenuCommand; IsCommandIdEnabled returns true above 5.
inline constexpr uintptr_t SplitWindowControlMenu_BuildMenuModel = 0xa12269c;
inline constexpr uintptr_t SplitWindowControlMenu_PopulateMenu = 0xa122604;
inline constexpr uintptr_t SplitWindowControlMenu_ExecuteCommand = 0xa122da0;
// ShouldExecuteCommandWithoutClosingMenu(this, int id, const ui::Event&) ->
// bool, `return id == 6`. Identical-code-folded: the SAME function serves
// other menus (e.g. EdgeDownloadsBtrItemContextMenu::IsItemForCommandIdDynamic),
// so a detour must pass every id but its own straight through.
inline constexpr uintptr_t SplitWindowControlMenu_ShouldExecuteWithoutClosing =
    0xa123190;
// gfx::VectorIcon data the ⋯ row's arrows reuse (Edge's own arrow icons).
inline constexpr uintptr_t Icon_ArrowUp = 0x122eef18;    // kArrowUpIcon
inline constexpr uintptr_t Icon_ArrowDown = 0x122eeee0;  // kArrowDownIcon
// ← and → are kArrowUpIcon turned (icons.h); these are the fallback.
inline constexpr uintptr_t Icon_ArrowBack = 0x12c5f138;     // kArrowBackIcon
inline constexpr uintptr_t Icon_ArrowForward = 0x12c5f200;  // kArrowForwardIcon
// The ⋯ row's own leading icon (Chromium's split-view icon).
inline constexpr uintptr_t Icon_SplitScene = 0x122ed698;  // kSplitSceneIcon
// static ui::ImageModel FromVectorIcon(out, const VectorIcon&, ColorVariant
// (by value, 8 bytes), int size (0 = default), const VectorIcon* badge)
inline constexpr uintptr_t ImageModel_FromVectorIcon = 0x10050ca;
inline constexpr uintptr_t ImageModel_dtor = 0x1004f70;
// ui::ColorVariant::ColorVariant(this, ui::ColorId)
inline constexpr uintptr_t ColorVariant_ctor = 0x10a1080;
// (model, int id, const std::u16string& label, const ui::ImageModel& icon)
inline constexpr uintptr_t SimpleMenuModel_AddItemWithIcon = 0x2e14f80;
// views::View::View(this): a plain view (no layout manager).
inline constexpr uintptr_t View_ctor = 0x10e179a;
// The outline of every tab, vertical ones included, in which it draws its fill,
// hover and press: SkPath EdgeTabStyleViews::GetPath(this, SkPath* out, int
// pathType, float scale, const int* isUnscaled). A vertical tab's is one rounded
// rectangle, built by the SkRRect::setRectXY call whose return address is
// EdgeTabStyleViews_GetPath_setRectXY_return, so the tab's shape is decided there.
inline constexpr uintptr_t EdgeTabStyleViews_GetPath = 0x27bb9c0;
inline constexpr uintptr_t EdgeTabStyleViews_GetPath_setRectXY_return =
    0x27bc0b0;
// SkPath& SkPath::arcTo(this, SkPoint radii, float rotation, ArcSize, Direction,
// SkPoint end): how GetPath rounds each corner of a horizontal tab's outline. It
// makes four arcs, in order: the left foot (a concave tail from the bottom, one
// radius out from the left side), the top-left corner, the top-right corner and
// the right foot. Their return addresses are below.
inline constexpr uintptr_t SkPath_arcTo = 0x10f6eb0;
inline constexpr uintptr_t EdgeTabStyleViews_GetPath_leftFoot_return =
    0x27bbeab;
inline constexpr uintptr_t EdgeTabStyleViews_GetPath_topLeftArc_return =
    0x27bbf10;
inline constexpr uintptr_t EdgeTabStyleViews_GetPath_topRightArc_return =
    0x27bbf7d;
inline constexpr uintptr_t EdgeTabStyleViews_GetPath_rightFoot_return =
    0x27bbff9;
// A vertical tab's outline (pathTypes 0 and 1) is built by the same four arcs,
// except that its fourth, the bottom-right corner, is a call at its own return
// address: it ends one radius in from the right side, at the tab's bottom.
inline constexpr uintptr_t EdgeTabStyleViews_GetPath_bottomRightArc_return =
    0x474e763;
// A vertical tab's outline: GetPath builds an SkRRect (setRectXY, above) and adds
// it to the path with SkPath::addRRect(this, const SkRRect&, direction,
// startIndex), whose call returns to EdgeTabStyleViews_GetPath_addRRect_return.
inline constexpr uintptr_t EdgeTabStyleViews_GetPath_addRRect_return =
    0x27bc0c9;
inline constexpr uintptr_t SkPath_addRRect = 0x27bc310;
// SkPath& SkPath::moveTo(this, SkPoint).
inline constexpr uintptr_t SkPath_moveTo = 0x12073f0;
// SkPath& SkPath::lineTo(this, SkPoint): the call in GetPath, returning to
// EdgeTabStyleViews_GetPath_topEdge_return, draws the top edge from the top-left
// corner to where the top-right arc begins.
inline constexpr uintptr_t SkPath_lineTo = 0x1209670;
inline constexpr uintptr_t EdgeTabStyleViews_GetPath_topEdge_return = 0x27bbf4d;
// EdgeTabStrip::UpdateDropShadow (rva 0x2954850) shadows the horizontal strip's
// active tab, outlined by GetPath (pathType 1) through the call returning to
// EdgeTabStrip_UpdateDropShadow_activeTabPath_return. When that tab is in a
// split, it extends the outline by every other tab of TabStrip::GetTabsInSplit
// (it and one neighbour), each through the call returning to
// EdgeTabStrip_UpdateDropShadow_splitMemberPath_return; with none, it closes
// the outline.
inline constexpr uintptr_t EdgeTabStrip_UpdateDropShadow_activeTabPath_return =
    0x2954bfe;
inline constexpr uintptr_t
    EdgeTabStrip_UpdateDropShadow_splitMemberPath_return = 0x47503b0;
// bool EdgeTabStyleViews::IsLeftSplitTab / IsRightSplitTab(Tab*): the tab is
// the first of its TabStrip::GetTabsInSplit / the last. GetPath asks after the
// top-left arc and again after the top-right one; for a first member it draws
// the top edge on to a square right side by lineTo calls of its own, skipping
// the top edge and top-right arc above, and leaves a later one without its
// left foot and top-left arc. Edge pairs a split's tabs two by two, so in a
// grown split the first member alone came out square.
inline constexpr uintptr_t EdgeTabStyleViews_IsLeftSplitTab = 0x10f654c;
inline constexpr uintptr_t EdgeTabStyleViews_IsRightSplitTab = 0x10f6594;
// SkPath::SkPath(this, SkPathFillType): an empty path.
inline constexpr uintptr_t SkPath_ctor = 0x1021180;
// SkRRect::setRectXY(this, const SkRect*, float rx, float ry): zeroes the rest
// of `this` itself when the rect is valid; one radius on every corner.
inline constexpr uintptr_t SkRRect_setRectXY = 0x1021e70;
// The vftable (offset 0) of SplitTabView, the view that holds a split's member
// tabs side by side in one row of the vertical list and paints their shared
// rounded group fill in its OnPaint.
inline constexpr uintptr_t SplitTabView_vftable = 0x11842398;
// The return address of SplitTabView::OnPaint's call to Canvas::DrawRoundRect
// (rva 0x99ff1d0 +0x1cf): the call that paints the group's rounded fill.
inline constexpr uintptr_t SplitTabView_OnPaint_DrawRoundRect_return =
    0x99ff39f;
// void gfx::Canvas::DrawRoundRect(this, const gfx::Rect*, int radius,
// const cc::PaintFlags*), a rounded rectangle of one radius on every corner.
inline constexpr uintptr_t Canvas_DrawRoundRect = 0x5406b30;
// void gfx::Canvas::DrawRect(this, const gfx::Rect*, const cc::PaintFlags*).
inline constexpr uintptr_t Canvas_DrawRect = 0x27bb1f4;
// SkRRect::setRectRadii(this, const SkRect*, const SkVector radii[4]): the
// radii run upper-left, upper-right, lower-right, lower-left.
inline constexpr uintptr_t SkRRect_setRectRadii = 0x850fe0;
// (view, const std::optional<gfx::Size>*): {int width, int height, bool}.
inline constexpr uintptr_t View_SetPreferredSize = 0x2c4ad9c;
// (view, const gfx::Rect&)
inline constexpr uintptr_t View_SetBoundsRect = 0x10218c8;
// gfx::Rect View::GetBoundsInScreen(this) (through the hidden out pointer),
// in screen DIPs.
inline constexpr uintptr_t View_GetBoundsInScreen = 0x28c8bc0;
// static View::ConvertPointFromScreen(const View* to, gfx::Point* in out):
// screen DIPs -> `to`'s coordinates.
inline constexpr uintptr_t View_ConvertPointFromScreen = 0x2880252;
// static View::ConvertPointToScreen(const View* from, gfx::Point* in out):
// `from`'s coordinates -> screen DIPs.
inline constexpr uintptr_t View_ConvertPointToScreen = 0x28c8ca6;
// gfx::Size View::GetPreferredSize(this, const SizeBounds&) (returned through
// the hidden out pointer); zeroed SizeBounds = unbounded.
inline constexpr uintptr_t View_GetPreferredSize = 0x0ff4dc4;
// ProposedLayout* MenuItemView::CalculateProposedLayout(LayoutDelegate* this,
// ProposedLayout* out, const SizeBounds&): every child's bounds in the row.
// `this` is the item's LayoutDelegate base (MENUITEM_layout_delegate). Edge
// centres the icon on a STANDARD row's height and pins other children to the
// row's top.
inline constexpr uintptr_t MenuItemView_CalculateProposedLayout = 0xb3d0b20;
// MenuController* MenuController::GetActiveInstance(): the open menu's, or null.
inline constexpr uintptr_t MenuController_GetActiveInstance = 0xb3d55be;
// MenuController::Cancel(this, ExitType, int): as MenuRunnerImpl::Cancel calls
// it, with (MENU_exit_all, 0).
inline constexpr uintptr_t MenuController_Cancel = 0xb3d5610;
// MenuItemView::ChildPreferredSizeChanged(this, View* child): drops the
// item's cached dimensions so it is measured again.
inline constexpr uintptr_t MenuItemView_ChildPreferredSizeChanged = 0xb3ce1d0;
// MenuItemView* MenuItemView::GetMenuItemByID(this, int id)
inline constexpr uintptr_t MenuItemView_GetMenuItemByID = 0xb3cf806;
inline constexpr uintptr_t SimpleMenuModel_AddSeparator = 0x2790998;
// size_t GetItemCount(model)
inline constexpr uintptr_t SimpleMenuModel_GetItemCount = 0x28d55e0;
// int GetCommandIdAt(model, size_t index)
inline constexpr uintptr_t SimpleMenuModel_GetCommandIdAt = 0x342d380;
// void RemoveItemAt(model, size_t index)
inline constexpr uintptr_t SimpleMenuModel_RemoveItemAt = 0xb7cdf20;
// void SetLabel(model, size_t index, const std::u16string& label)
inline constexpr uintptr_t SimpleMenuModel_SetLabel = 0xb7ce16e;
// (tsm, int index, uint32 closeTypes); HideSplitView passes 1
inline constexpr uintptr_t TabStripModel_CloseWebContentsAt = 0xa406ffa;
// (tsm, int index, const TabStripUserGestureDetails*)
inline constexpr uintptr_t TabStripModel_ActivateTabAt = 0xa403ba2;
// int TabStripModel::GetIndexOfWebContents(this, WebContents*): -1 if absent.
inline constexpr uintptr_t TabStripModel_GetIndexOfWebContents = 0x10f55ea;
// int TabStripModel::MoveWebContentsAt(this, int index, int to_position,
// bool select_after_move): `to_position` is the tab's index after the move.
inline constexpr uintptr_t TabStripModel_MoveWebContentsAt = 0xa403e2c;
// A split pane's links (docs/re/split-links.md):
// bool BrowserWebContentsDelegate::ShouldOverrideNewWindowCreation(this,
// WebContents* source, RenderFrameHost* opener, WindowOpenDisposition, bool,
// mojom::CreateNewWindowParams*): asked (via
// WebContentsImpl::EdgeShouldCreateNewWindowAsync) once a page's new window is
// allowed and before it is made; true = handled, make no window.
inline constexpr uintptr_t
    BrowserWebContentsDelegate_ShouldOverrideNewWindowCreation = 0xa0bc740;
// WebContents* BrowserWebContentsDelegate::AddNewContents(this, WebContents*
// source, unique_ptr<WebContents>* new_contents, const GURL& target_url,
// WindowOpenDisposition, const WindowFeatures&, bool user_gesture, bool*
// was_blocked): puts a page's new tab or window into the browser.
inline constexpr uintptr_t BrowserWebContentsDelegate_AddNewContents =
    0xa0b9d70;
// unique_ptr<NavigationThrottle> SplitWindowLinkModeNavigationThrottle::
// MaybeCreateThrottleFor(out, ...): Edge's own use of the "open left links to
// the right" toggle (the first pane's same-tab links load in the last pane).
inline constexpr uintptr_t SplitWindowLinkModeThrottle_MaybeCreate = 0x29ef3d4;
// void SplitWindowControllerImplV2::UpdateLinkedModeState(this, bool on): after
// the toggle's bool is stored (SetLinkedModeStateForTab, 0xa12aee0), tells the
// split's observers, which wire Edge's two-pane link mode (readying a target
// pane: it navigated pane 1 of three to a new tab page).
inline constexpr uintptr_t SplitWindowControllerV2_UpdateLinkedModeState =
    0xa12a620;
// NavigationController::LoadURLParams(this, const GURL&) and its dtor.
inline constexpr uintptr_t LoadURLParams_ctor = 0x2d5ef10;
inline constexpr uintptr_t LoadURLParams_dtor = 0x2d5ec4e;
// NavigationController::LoadURLWithParams(this, WeakPtr<NavigationHandle>*
// out, const LoadURLParams&): since 156 a wrapper around the Impl's
// LoadURLWithParamsForResult, which writes a base::expected instead.
inline constexpr uintptr_t NavigationControllerImpl_LoadURLWithParams =
    0x2d5ea76;
// NavigationControllerImpl* WebContentsImpl::GetController(this).
inline constexpr uintptr_t WebContentsImpl_GetController = 0x1164090;
// bool WebContentsImpl::HasOpener(this).
inline constexpr uintptr_t WebContentsImpl_HasOpener = 0x3420120;
// const GURL& WebContentsImpl::GetLastCommittedURL(this) (WebContents vtable
// +0x80).
inline constexpr uintptr_t WebContentsImpl_GetLastCommittedURL = 0x115b550;
// base::internal::WeakReference::~WeakReference(this): a WeakPtr's first field.
inline constexpr uintptr_t WeakReference_dtor = 0x0fe4310;

// A new pane's tab offer (docs/re/split-offer.md):
// WebContents* chrome::AddAndReturnTabAt(BrowserWindowInterface*, const GURL&,
// uint32 extra transition flags, int index, bool foreground,
// optional<TabGroupId>*, bool pinned, optional<int>): a new tab at `index`,
// loading the URL; returns its contents.
inline constexpr uintptr_t Chrome_AddAndReturnTabAt = 0xa86512c;
// GURL::GURL(this, const std::string_view*) and its dtor.
inline constexpr uintptr_t GURL_ctor_string = 0x10162a2;
inline constexpr uintptr_t GURL_dtor = 0x0fde6fa;
// chrome::kEdgeUISplitWindowStartingPageURL ("edge://split-window-starting-page"):
// the page Edge's split button shows in the new pane, offering the window's
// other tabs.
inline constexpr uintptr_t EdgeUISplitWindowStartingPageURL = 0x123a5c90;
// bool SplitWindowController::IsStartingPage(WebContents*, bool by_url).
inline constexpr uintptr_t SplitWindowController_IsStartingPage = 0xa12d6a2;
// void SplitWindowControllerImplV2::OpenTabInCurrentSplitWindow(this,
// WebContents* chosen): the starting page's pick. It puts the chosen tab in
// place of the active split's LAST member (UpdateTabInSplit), where a
// two-pane split always has its starting page.
inline constexpr uintptr_t SplitWindowControllerV2_OpenTabInCurrentSplitWindow =
    0xa12ba10;
// void SplitWindowStartingUIHandler::GetTabs(this, GetTabsCallback*): the
// page's tab list, every tab of its window but those
// SplitWindowController::IsSplitWindowWebContents(WebContents*) claims (the
// contents of Edge's older split, marked by their user data).
inline constexpr uintptr_t SplitWindowStartingUIHandler_GetTabs = 0x9b9f4d0;
inline constexpr uintptr_t SplitWindowController_IsSplitWindowWebContents =
    0x29f2498;
// void base::internal::Invoker<lambda at split_window_controller_impl_v2.cc:1331,
// WeakPtr<WebContents>>::RunOnce(BindStateBase*): the task OnSplitTabRemoved
// (0xa12c756) posts when the removed split's last member is a starting page;
// it loads chrome::kChromeUINewTabURL there, replacing the entry.
inline constexpr uintptr_t StartingPageLeftSplitTask_RunOnce = 0xa12d480;
// void StartingPageNotifierImpl::OnTabClosing(this, WebContents*): tells every
// registered starting page that the tab is gone (Page::OnTabRemoved(tab id)).
inline constexpr uintptr_t StartingPageNotifierImpl_OnTabClosing = 0xa13373c;
// void SplitWindowStartingUIHandler::CloseSplitWindow(this): the starting
// page's X. Edge then closes the split's LAST member too
// (SplitWindowControllerImplV2::HideSplitView(true), 0xa12a850).
inline constexpr uintptr_t SplitWindowStartingUIHandler_CloseSplitWindow =
    0x9ba02d0;

// A tab drag (docs/re/tab-drag.md):
// int TabDragController::ContinueDragging(this, const gfx::Point* screen DIPs):
// every pointer move of a started drag; it reorders the SELECTED tabs, or tears
// them off into a new window ~15 DIP below the strip. 0 = the drag lives on.
inline constexpr uintptr_t TabDragController_ContinueDragging = 0xa784d58;
// void TabDragController::EndDrag(this, EndDragReason); frees the controller.
inline constexpr uintptr_t TabDragController_EndDrag = 0xa782ec0;
// void EventTracker::OnEvent(this, const ui::Event&): the drag's key watcher.
// Escape cancels the drag; ANY other key press or release ends it in place.
inline constexpr uintptr_t EventTracker_OnEvent = 0xa78bc10;
// int TabDragController::Drag(this, const gfx::Point* screen DIPs): what the
// strip's OnMouseDragged reaches (TabDragContextImpl::ContinueDrag converts the
// event to screen DIPs and calls it); starts the drag past the drag threshold,
// then runs ContinueDragging.
inline constexpr uintptr_t TabDragController_Drag = 0xa783486;
// bool TabDragController::SetCapture(this, TabDragContext*): hands the mouse
// capture to that strip (its widget's Widget::SetCapture(context)), as the
// attach of a tab dragged in from another window does. From then on views
// sends the strip the drags and the release.
inline constexpr uintptr_t TabDragController_SetCapture = 0xa783232;
// TabStrip* BrowserView::tabstrip(this): the strip the window's tab list
// draws, vertical tabs included.
inline constexpr uintptr_t BrowserView_tabstrip = 0x2930564;
// TabDragContext* TabStrip::GetDragContext(this).
inline constexpr uintptr_t TabStrip_GetDragContext = 0x30ba8ee;
// void TabStrip::StopAnimating(this): its TabContainerImpl's
// CompleteAnimationAndLayout. TabStrip::MaybeStartDrag starts nothing while the
// strip animates (TabStrip::IsAnimatingInTabStrip), as it does right after a
// tab moved or was activated.
inline constexpr uintptr_t TabStrip_StopAnimating = 0x2afe282;
// ui::MouseEvent::MouseEvent(this, EventType, const gfx::Point* location,
// const gfx::Point* root_location, base::TimeTicks, int flags,
// int changed_button_flags, const PointerDetails*).
inline constexpr uintptr_t MouseEvent_ctor = 0x2c312e2;
// ui::PointerDetails::PointerDetails(this): a mouse's.
inline constexpr uintptr_t PointerDetails_ctor = 0xb78e19a;
// ui::Event::~Event(this): all a MouseEvent needs (Tab::OnMousePressed ends
// its own synthesized one so).
inline constexpr uintptr_t Event_dtor = 0x109ad5a;
// static base::TimeTicks base::TimeTicks::Now() (through the hidden out
// pointer).
inline constexpr uintptr_t TimeTicks_Now = 0xfe1850;

// --- the dividers between panes (docs/re/close-pane.md) ---
// EdgeMultiContentsResizeArea(this, MultiContentsView*): the 2-pane split's
// resize handle (8x8 preferred, hover pill; vertical when taller than wide).
// MultiContentsView's ctor allocates it with 0x358 bytes.
// views::EdgeTwoDimensionalResizeArea's own mouse handlers (overriding
// views::ResizeArea's): bool (view, const ui::MouseEvent&) for press / drag,
// void (view, const ui::MouseEvent&) for release, void (view) for capture lost.
inline constexpr uintptr_t ResizeArea2D_OnMousePressed = 0xb367d80;
inline constexpr uintptr_t ResizeArea2D_OnMouseDragged = 0xb367e10;
inline constexpr uintptr_t ResizeArea2D_OnMouseReleased = 0xb367e50;
inline constexpr uintptr_t ResizeArea2D_OnMouseCaptureLost = 0xb367eb0;

// --- 2-pane-only CHECK sites that int3 when a split grows past 2 members ---
// Accessible-label builder: maps a split tab's pane index to a string id
// (0x3e89 pane0 / 0x3e8a pane1) and `cmp eax,1; jne <int3>` crashes at index>=2.
// `jne 0x431cbd3` (0F 85 rel32, 6 bytes); NOP it so index>=2 falls through to
// the pane-1 label instead of crashing. a11y text for pane 3+ is cosmetic.
inline constexpr uintptr_t SplitA11yLabel_paneidx_jne = 0x431c837;
// MultiContentsView::SetActiveIndex's `cmp edx,2; jae <int3>` (0F 83 rel32, 6
// bytes). The detour replaces the function while loaded; NOPing the CHECK too
// means a click on pane 3+ after unload only mis-flags the primary pane.
inline constexpr uintptr_t SetActiveIndex_index_jae = 0xa66e706;

// --- a split's tabs in the tab list (tabrows.h) ---
// The horizontal strip and the vertical list lay tabs out through
// TabStripLayoutHelper, which draws a split as one row of two tabs. With 3+
// members every member after the first pair is given the second half's rect, so
// they pile up in one slot (overlapping titles, a hover showing another pane).
// Result* TabStripLayoutHelper::CalculateIdealBounds(this, Result* out,
// std::optional<int> availableWidth): one Rect per slot, {Rect* data, size_t
// count} at the start of the result. UpdateIdealBounds stores it and
// EdgeTabStripLayoutHelper::CalculatePreferredHeight sizes the container from it.
inline constexpr uintptr_t TabStripLayoutHelper_CalculateIdealBounds =
    0x109c1aa;

// --- theming (docs/re/theming.md) ---
// The whole feature rides one Edge entry point. ThemeService applies a theme
// that is already INSTALLED in the profile; the panel installs each theme (via
// Edge's own external-extension loader), and the DLL only switches between
// installed themes when Windows flips light <-> dark.
//
// Setting a theme = find the installed theme extension, then hand it to
// DoSetTheme. void ThemeService::DoSetTheme(this, const Extension*, bool
// suppress_notification): builds a fresh BrowserThemePack and (suppress=false)
// notifies, which repaints the browser in place — Edge's own path, live, no
// restart. (RevertToExtensionTheme only re-enables an already-enabled theme
// extension, so it does not switch the active theme.)
inline constexpr uintptr_t ThemeService_DoSetTheme = 0x8833a10;
// extensions::ExtensionRegistry* ExtensionRegistry::Get(BrowserContext*) and
// the Extension* lookup on either of its sets (off::REGISTRY_*_set):
// const Extension* ExtensionSet::GetByID(this, const std::string& id).
inline constexpr uintptr_t ExtensionRegistry_Get = 0x10a5c52;
inline constexpr uintptr_t ExtensionSet_GetByID = 0x10d087a;
// A theme the panel installs through the external-extension loader arrives
// DISABLED (disable reason "external extension") until the user accepts Edge's
// "extension added" prompt, and Edge also switches a theme off when another
// becomes active; a disabled theme never paints. Before a theme is applied the
// DLL turns it on with the call the prompt's Enable button makes.
// ExtensionRegistrar* ExtensionRegistrar::Get(BrowserContext*)
inline constexpr uintptr_t ExtensionRegistrar_Get = 0x2c55b80;
// void ExtensionRegistrar::EnableExtension(this, const std::string& id): moves
// the extension from the disabled set to the enabled one. It CHECKs that the
// extension is in the disabled set, so only call it for one found there.
inline constexpr uintptr_t ExtensionRegistrar_EnableExtension = 0x7f332b6;
// A theme once removed from Edge's extensions page is recorded in the profile's
// "external uninstalls" and the external-extension loader never installs it
// again, however often its registry entry is read. ExtensionPrefs::Get takes the
// BrowserContext; ClearExternalExtensionUninstalled(this, const std::string& id)
// drops the record. ExternalProviderManager::Get(BrowserContext*) reaches the
// manager whose CheckForExternalUpdates(this) revisits every registered external
// extension, now past the cleared record, and installs it.
inline constexpr uintptr_t ExtensionPrefs_Get = 0x280c6a6;
inline constexpr uintptr_t ExtensionPrefs_ClearExternalExtensionUninstalled =
    0x7f2ebfe;
inline constexpr uintptr_t ExternalProviderManager_Get = 0x2c5f018;
inline constexpr uintptr_t ExternalProviderManager_CheckForExternalUpdates =
    0x2c5485e;
// void ThemeService::UseDefaultTheme(this): drop any extension theme. Used when
// a mode has no theme assigned.
inline constexpr uintptr_t ThemeService_UseDefaultTheme = 0x2fc7140;
// void ThemeService::RemoveUnusedThemes(this): Chromium keeps only the one
// active theme extension and unloads every other theme from the profile, so the
// two themes a user assigns to light and dark modes cannot both stay installed —
// the moment one is applied the other is garbage-collected, and the next flip
// finds it gone. The detour no-ops it while theming is active (hooks.h), so both
// assigned themes remain installed and either mode's flip switches instantly.
// Takes the primary `this` (rcx), reads large member offsets (+0x728, +0x6b8).
inline constexpr uintptr_t ThemeService_RemoveUnusedThemes = 0x331bde0;
// ThemeService* ThemeServiceFactory::GetForProfile(Profile*): the per-profile
// ThemeService (creates it if needed). Profile is a content::BrowserContext.
inline constexpr uintptr_t ThemeServiceFactory_GetForProfile = 0xfe57e8;
// content::BrowserContext* WebContentsImpl::GetBrowserContext(this): the Profile
// behind any WebContents, our bridge from a pane to its ThemeService.
inline constexpr uintptr_t WebContentsImpl_GetBrowserContext = 0x11640a0;

// --- reading mode's page theme (docs/re/reading-mode-theme.md) ---
// DomDistillerContextKeyedService* DomDistillerServiceFactory::
// GetForBrowserContext(content::BrowserContext*): the profile's distiller
// service (its DomDistillerService base at DOMDISTILLERKEYED_service).
inline constexpr uintptr_t DomDistillerServiceFactory_GetForBrowserContext =
    0x2e3d270;
// void DistilledPagePrefs::SetTheme(this, dom_distiller::Theme, std::string
// source): writes the reading_view.theme pref and posts NotifyOnChangeTheme, so
// every open reading view runs readingViewController.setPreferences({theme}).
// `source` (the viewer that asked, which is skipped) is passed by value and the
// callee frees it; an empty one notifies every viewer.
inline constexpr uintptr_t DistilledPagePrefs_SetTheme = 0xc118fd0;

// --- WebUI pages follow the theme (docs/re/webui-color-scheme.md) ---
// Two pieces make a page light or dark, and Edge feeds neither from the theme:
// the page's prefers-color-scheme (computed per WebContents) and the colors
// the WebUI draws with (chrome://theme/colors.css, built from the contents'
// color provider, whose mode is the OS's or the Appearance setting's).
// void* EdgeComputePreferredColorScheme(out, const SiteInfo*, WebContents*):
// the pair of answers behind one page's web preferences, called from
// ChromeContentBrowserClient::OverrideWebPreferences (and its after-navigation
// and need-update siblings); out->+0 is the preferred color scheme as blink
// stores it (0 = dark, 1 = light), out->+4 the root scrollbar scheme (1 = dark).
inline constexpr uintptr_t EdgeComputePreferredColorScheme = 0x54a1129;
// void WebContentsImpl::OnColorProviderChanged(this): `this` is the contents'
// ColorProviderSourceObserver subobject (off::WEBCONTENTS_color_observer). It
// tells the contents' observers, which makes a WebUI page fetch its colors.css
// again, then recomputes the web preferences (prefers-color-scheme).
inline constexpr uintptr_t WebContentsImpl_OnColorProviderChanged = 0x2f294b0;
// void NavigationControllerImpl::Reload(this, ReloadType, bool
// check_for_repost): the controller is embedded in the WebContentsImpl at
// off::WEBCONTENTS_controller.
inline constexpr uintptr_t NavigationControllerImpl_Reload = 0x7940fd0;
// ColorProvider* WebContentsImpl::GetColorProvider(this): the provider the
// WebUI's colors.css is generated from (ThemeSource::SendColorsCss reads it
// through the contents' vtable). It asks the contents' color provider source,
// the browser window, which builds its key through ThemeService below on every
// call, so a detour on that key decides the mode for the one call.
inline constexpr uintptr_t WebContentsImpl_GetColorProvider = 0x115d030;
// ColorProviderKey* ThemeService::GetColorProviderKey(this, ColorProviderKey*
// out, const ColorProviderKey& nativeKey, Profile*): fills the key the browser's
// color provider is built from and returns `out`. Its mode starts as the OS's
// and is replaced only when the Appearance setting says Light or Dark.
inline constexpr uintptr_t ThemeService_GetColorProviderKey = 0xfe58d2;
// bool BrowserThemePack::GetColor(this, int id, SkColor* out): a theme
// extension's color for `id`, false when the theme did not set it. Its colors
// are a table of 8-byte {int id, SkColor} entries.
inline constexpr uintptr_t BrowserThemePack_GetColor = 0x8839020;
// SkColor ui::ColorProvider::GetColor(this, ui::ColorId id): the browser-wide
// colour read every view resolves an id through. Signature:
// SkColor __fastcall(void* this, int id).
inline constexpr uintptr_t ColorProvider_GetColor = 0x1002aee;
// void EdgeTab::UpdateForegroundColors(this): recomputes a tab's title, close
// and icon colours (through EdgeTabStyleViews::CalculateTargetColors and
// TabStyle::GetTabForegroundColor) whenever its state, hover, theme or window
// focus changes, and hands them to its views.
inline constexpr uintptr_t EdgeTab_UpdateForegroundColors = 0x2a5a990;
// bool Tab::IsActive(this): the tab is the strip's active one. EdgeTab's Tab
// base is at +0, so an EdgeTab* passes as is.
inline constexpr uintptr_t Tab_IsActive = 0x1055540;
// EdgeTabStyleViews::PaintInactiveTabBackground (rva 0x27ba6f8) +0x75: the return
// address of its call to PaintTabBackground, on the stack whenever a tab other
// than the active one paints its fill (a hovered one).
inline constexpr uintptr_t
    EdgeTabStyleViews_PaintInactiveTabBackground_PaintTabBackground_return =
        0x27ba76d;
// EdgeTabStyleViews::CalculateTargetColors (rva 0x28790d0) +0xbd / +0xcf: the
// return addresses of its two EdgeGetTabBackgroundColor calls (the active and
// the inactive fill), which it blends into a tab's hover target.
inline constexpr uintptr_t
    EdgeTabStyleViews_CalculateTargetColors_active_return = 0x287918d;
inline constexpr uintptr_t
    EdgeTabStyleViews_CalculateTargetColors_inactive_return = 0x287919f;
// EdgeTabStyleViews::PaintTab's cold part: after a SaveLayerAlpha at the tab's
// hover (throb) value it paints the tab as active (PaintTabBackground, active =
// true); this is that call's return address.
inline constexpr uintptr_t EdgeTabStyleViews_PaintTab_hover_layer_return =
    0x474eee3;
// The OS light/dark signal does NOT come through Edge. ui::NativeThemeObserver
// is a base::CheckedObserver (the observer list uses CheckedObserverAdapter), so
// a hand-rolled observer object would have to fake a weak-ptr factory. Instead
// the DLL already subclasses the browser window, so it watches that window's
// WM_SETTINGCHANGE with lParam == "ImmersiveColorSet" and reads the current mode
// straight from the OS (HKCU Themes\Personalize\AppsUseLightTheme, 0 = dark).
// No Edge theme-observer ABI is touched.

// The libc++ std::string a theme id is built in (edge::ThemeIdString): the fill
// assign(size_type n, char c) at n = 0 clears it, keeping any heap buffer, then
// append(const char* s). The *string* overloads of assign/append read a source
// string's own fields, not a raw pointer, so these char-pointer ones are used.
inline constexpr uintptr_t BasicStringChar_assignFill = 0x2023032;
inline constexpr uintptr_t BasicStringChar_append = 0x27d5f14;

// --- web protocol handlers (docs/re/protocol-handlers.md) ---
// A site's registerProtocolHandler without a user gesture lights the omnibox
// handler icon; its bubble (Allow / Deny / Ignore, Ignore preselected) commits
// the selected radio from ContentSettingBubbleContents::WindowClosing, so ANY
// close - Esc, the X, a click elsewhere - commits Ignore and writes the ignored
// list. void ContentSettingRPHBubbleModel::CommitChanges(this): performs the
// selected radio's action, then clears the pending handler (hides the icon).
inline constexpr uintptr_t ContentSettingRPHBubbleModel_CommitChanges =
    0xa398e60;
// The bubble model's vftable. Its Done button is the dialog's accept callback,
// bound to the model's virtual at off::BubbleModel_OnDoneButtonClicked_slot
// (a `vcall'{56}` thunk); for this model that slot holds the base class's empty
// virtual below, so it is the one place that sees Done as distinct from every
// other way the bubble closes.
inline constexpr uintptr_t ContentSettingRPHBubbleModel_vftable = 0x11408690;
// The shared empty virtual (`ret`) the slot holds before we point it at ours.
inline constexpr uintptr_t ContentSettingBubbleModel_EmptyVirtual = 0x548aac0;
// void RegisterProtocolHandlerPermissionRequest::PermissionDecided(this,
// const Decision* decision): Edge's permission prompt (the user-gesture path).
// With a decision, 0 (Allow) accepts and EVERY other value ignores - including
// Edge's "Just this time", which Chromium's prompt never offers for handlers.
// No decision (the prompt dismissed) does nothing. Decision layout:
// off::PERMISSION_DECISION_*.
inline constexpr uintptr_t
    RegisterProtocolHandlerPermissionRequest_PermissionDecided = 0xbf334c0;
// Lifting the blocks: once a handler is ignored, Edge drops every later request
// from it, so the icon never returns. While the feature turns on, the DLL
// lifts each one with Edge's own call (the bubble's Allow path makes it too).
// ProtocolHandlerRegistry* ProtocolHandlerRegistryFactory::GetForBrowserContext(
// BrowserContext*): the profile's registry (GetServiceForBrowserContext, create).
inline constexpr uintptr_t ProtocolHandlerRegistryFactory_GetForBrowserContext =
    0x29c77fc;
// void ProtocolHandlerRegistry::RemoveIgnoredHandler(this, const
// ProtocolHandler& handler, bool save): erases the handler from the user's
// ignored list and, unless policy also ignores it, from the live ignored list
// (off::RPH_REGISTRY_ignored_*), then notifies. `save` (Edge's addition; the
// bubble passes 1) writes the prefs. Its second erase compares against
// `handler` while shifting the very vector it erases from, so `handler` must
// be a copy, never an element of that list.
inline constexpr uintptr_t ProtocolHandlerRegistry_RemoveIgnoredHandler =
    0xbf35c1e;
// ProtocolHandler::ProtocolHandler(this, const ProtocolHandler& other): the
// copy constructor (what vector<ProtocolHandler>'s copy, used by
// GetIgnoredHandlers, calls per element). The other ProtocolHandler ctor,
// 0xbf372b8, is the by-field one.
inline constexpr uintptr_t ProtocolHandler_CopyConstructor = 0x2d70964;
// ProtocolHandler::~ProtocolHandler(this): frees its strings and GURL.
inline constexpr uintptr_t ProtocolHandler_Destructor = 0x2d521d8;

// --- the browser assistant's toolbar button ---
// The button's vftables (one per base; EdgeCopilotToolbarLabelButton has
// multiple inheritance). A view is a chat button when its object+0 vptr is any
// of these, so the recognition does not depend on which base is primary.
inline constexpr uintptr_t CopilotButton_vftables[] = {
    0x1153ec50, 0x1153ec70, 0x1153ec90, 0x1153ecb0, 0x1153ecf0, 0x1153ed40,
    0x1153ed58, 0x1153ed70, 0x1153eda0, 0x1153ee50, 0x1153ee70, 0x1153eeb0,
    0x1153eec8, 0x1153eee0, 0x1153ef10, 0x1153ef28, 0x117886a8, 0x1191e5b8};

// The vftable of an EdgeTab's view. One class serves both layouts: a row of the
// vertical tab list and a tab of the horizontal strip share it (found by
// tabrows::Dump), so only the tab's layout mode (EDGETAB_layout_controller)
// tells them apart.
inline constexpr uintptr_t EdgeTab_vftable = 0x11844e98;
// EdgeCopilotDropdownMenuButton: the chevron beside the chat pill (a sibling in
// EdgeCopilotToolbarSplitButtonContainer). Its constructor sets both its tooltip
// and its accessible name from the formatted string "$1 menu"; the vftables
// are matched like the chat button's.
inline constexpr uintptr_t CopilotDropdownButton_vftables[] = {
    0x1159ea60, 0x1159ea80, 0x1159eaa0, 0x1159eac0, 0x1159eb00,
    0x1159eb50, 0x1159eb68, 0x1159eb80, 0x1159ebb0, 0x1159ec60,
    0x1159ec80, 0x1159ecc0, 0x1159ecd8, 0x119488f8};
// bool IsConsumerCopilotLabelButtonWithDropdownEnabled(bool): the one question
// EdgeToolbarView::CreateDiscoverButtonIfNecessary asks (through
// IsCopilotLabeledButtonWithDropdownEnabled) before building the chevron's
// split-button container for a personal profile; false builds the bare chat
// button. See docs/re/removed-gemini-integration.md section 12.
inline constexpr uintptr_t Copilot_IsConsumerLabelButtonWithDropdownEnabled =
    0x31ecc2b;
// void EdgeToolbarView::CreateDiscoverButtonIfNecessary(this = the toolbar's
// secondary base, EDGETOOLBAR_discoverBase into the toolbar): builds the chat
// button, inside a split-button container with its chevron when
// IsConsumerCopilotLabelButtonWithDropdownEnabled says so. It does not check
// for a button already built, and stores the new one in EDGETOOLBAR_chatButton.
inline constexpr uintptr_t EdgeToolbarView_CreateDiscoverButtonIfNecessary =
    0x31eca50;
// --- running a script in a page, and reading a tab ---
// RenderFrameHost* WebContentsImpl::GetPrimaryMainFrame(this).
inline constexpr uintptr_t WebContentsImpl_GetPrimaryMainFrame = 0x11640b0;
// WebContentsImpl::ForEachRenderFrameHost(this, FunctionRef<void(RenderFrameHost*)>*):
// every frame of a page, main frame first. A FunctionRef is {object, invoker}:
// the invoker is called (object, frame). RenderFrameHostImpl::GetLastCommittedURL
// (frame) is the frame's GURL.
inline constexpr uintptr_t WebContentsImpl_ForEachRenderFrameHost = 0x2791750;
inline constexpr uintptr_t RenderFrameHostImpl_GetLastCommittedURL = 0x1163b30;
// const std::string& GURL::spec(this).
inline constexpr uintptr_t GURL_spec = 0xfecc5a;
// bool RenderFrameHostImpl::IsRenderFrameLive(this): false for a discarded tab.
inline constexpr uintptr_t RenderFrameHostImpl_IsRenderFrameLive = 0x115c690;
// void RenderFrameHostImpl::ExecuteJavaScriptInIsolatedWorld(this,
// const std::u16string*, JavaScriptResultCallback* (a OnceCallback<void(Value)>,
// moved from), int worldId). The main-world ExecuteJavaScript CHECKs
// CanExecuteJavaScript, which a web page fails, so this is the one to call.
inline constexpr uintptr_t
    RenderFrameHostImpl_ExecuteJavaScriptInIsolatedWorld = 0x795a860;
// BindStateBase::BindStateBase(this, polymorphic_invoke, destructor): refcount 1
// at +0, invoke at +8, destructor at +0x10, the default cancellation query at
// +0x18; a BindState's bound state starts at BINDSTATE_first_bound_argument.
inline constexpr uintptr_t BindStateBase_ctor = 0xfde8b0;
// The native find bar (docs/re/find-bar.md). FindBarView is the bar's content
// view; its text field's edits end in Find(this, const u16string_view*), and
// Enter and the arrow buttons in FindNext(this, bool reverse), which reads the
// text field itself. Both go on to FindTabHelper::StartFinding.
inline constexpr uintptr_t FindBarView_Find = 0xa4ff910;
inline constexpr uintptr_t FindBarView_FindNext = 0xa4fe2c0;
// bool HandleKeyEvent(this = the FindBarView's TextfieldController subobject,
// Textfield*, const ui::KeyEvent&): Enter starts the search itself (FindNext is
// inlined into it), so Enter needs its own hook.
inline constexpr uintptr_t FindBarView_HandleKeyEvent = 0xa4ff420;
// void UpdateForResult(this, const FindNotificationDetails&, const
// std::u16string& findText): the "n/m" label and the buttons' enabled state.
inline constexpr uintptr_t FindBarView_UpdateForResult = 0xa4fe664;
// void EndFindSession(this, ?, bool): the bar is closing (Esc, the X, a click
// into the page); FindBarView::EndFindSession ends in it.
inline constexpr uintptr_t FindBarController_EndFindSession = 0xa171e00;
// Copilot's side panel, the path the page's "Ask Copilot" menu uses.
// edge_copilot::utils::SendPageContextualPromptToChatImpl(
// BrowserWindowInterface* browser, const std::string& pageText, <unused>,
// const std::string& prompt, const std::string& account, bool): builds an
// AskCopilotRequest (the prompt, plus the page text as a "text" attachment) and
// dispatches it to the side panel, opening it. pageText must not be empty or it
// takes another path; an empty account skips the work-account branch. Not the
// public SendPageContextualPromptToChat: since 156 it empties a prompt that is
// not a short run of letters and digits (Edge's own menu passes a code there).
inline constexpr uintptr_t CopilotUtils_SendPageContextualPromptToChatImpl =
    0x945311a;
// bool edge_copilot::utils::IsUndersideChatVisible(BrowserWindowInterface*):
// whether Copilot's side panel is open in that window.
inline constexpr uintptr_t CopilotUtils_IsUndersideChatVisible = 0x273b00d;
// void edge_copilot::utils::CloseUndersideChat(BrowserWindowInterface*, bool):
// closes the side panel, as the panel page's own close message does (it passes
// true: OnPostMessageClosePane).
inline constexpr uintptr_t CopilotUtils_CloseUndersideChat = 0x944f9f4;
// The request the side panel's chat page is sent, and the function that delivers
// it with no panel UI: IframelessPromptDispatcher::DispatchAskCopilotToDocument(
// RenderFrameHost* chatFrame, const AskCopilotRequest&) checks the frame is
// live and active and the request has a prompt and a type, then raises the
// extension event OnAskCopilot in that document. SendPageContextualPromptToChat
// builds the request with exactly these calls. [infer from its disassembly; run]
// AskCopilotRequest::AskCopilotRequest(this) / ~AskCopilotRequest(this)
inline constexpr uintptr_t AskCopilotRequest_ctor = 0xc360e90;
inline constexpr uintptr_t AskCopilotRequest_dtor = 0xc360f0c;
// std::string::operator=(std::string& dest, const std::string& source)
inline constexpr uintptr_t StdString_assign = 0x1056f26;
// std::optional<std::string>::operator=(optional&, const std::string&)
inline constexpr uintptr_t OptionalString_assignString = 0x11318b8;
// std::optional<std::string>::operator=(optional&, const char (&)[1])
inline constexpr uintptr_t OptionalString_assignEmpty = 0x626e95e;
inline constexpr uintptr_t
    IframelessPromptDispatcher_DispatchAskCopilotToDocument = 0x945b030;
// static content::WebContentsImpl::GetAllWebContents(out): hidden return pointer
// in rcx. What it fills is not a begin/end vector: its first word points at the
// WebContents pointers and its second is their count. The panel is the one whose
// URL is edge://discover-chat-v2/ (the "iframeless" panel has no host manager:
// CopilotSidePanelController::GetHostManagerForBrowser answers null for it).
inline constexpr uintptr_t WebContentsImpl_GetAllWebContents = 0x2808116;
// views::Textfield::GetText(this, std::u16string_view* out): fills `out`.
inline constexpr uintptr_t Textfield_GetText = 0xffc6f6;
// The bar's options (the "..." button): an inline FindOptionsControlView, a
// column of FindInPageOptionToggleViews (Match case, Whole word, Match
// diacritics), each a title label and an EdgeToggleButton. See
// docs/re/find-bar.md.
// void EdgeMaybeAddFindOptionsView(this = FindBarView, bool): builds the options
// view the first time the bar needs it.
inline constexpr uintptr_t FindBarView_EdgeMaybeAddFindOptionsView = 0xa50135e;
// void EdgeToggleFindOptions(this = FindBarView, ...): shows or hides the view.
inline constexpr uintptr_t FindBarView_EdgeToggleFindOptions = 0xa5010bc;
// void PreferredSizeChanged(this = FindBarView): the bar re-measures itself.
inline constexpr uintptr_t FindBarView_PreferredSizeChanged = 0xa5005f0;
// void AddFindInPageOptionChildView(this = FindOptionsControlView, int id, int
// width, FindBarHost*): builds toggle `id` at the end of the column (one per
// id; Edge builds 0..2 and its title text for any other id is empty).
inline constexpr uintptr_t FindOptionsControlView_AddFindInPageOptionChildView =
    0xca6b0a6;
// void OnButtonToggled(this = FindInPageOptionToggleView): the toggle button's
// click callback; it flips Edge's own state for the id and re-runs the search.
inline constexpr uintptr_t FindInPageOptionToggleView_OnButtonToggled =
    0xca6bbd0;
// void EdgeOnActiveWebContentsChanged(this = FindInPageOptionToggleView): run
// for each switch when the bar is shown again (and on a tab change); it sets the
// switch from edge::GetFindInPageOptionToggleValue(the tab's find helper, id),
// which is false for any id but Edge's own, with no OnButtonToggled callback.
inline constexpr uintptr_t
    FindInPageOptionToggleView_EdgeOnActiveWebContentsChanged = 0xca6c1b0;
// bool views::EdgeToggleButton::GetIsOn(this) and void SetIsOn(this, bool).
inline constexpr uintptr_t EdgeToggleButton_GetIsOn = 0xb368a16;
inline constexpr uintptr_t EdgeToggleButton_SetIsOn = 0xb368a44;
// void views::View::SetEnabled(this, bool).
inline constexpr uintptr_t View_SetEnabled = 0x105e576;
// void views::Label::SetText(this, const std::u16string_view*).
inline constexpr uintptr_t Label_SetText = 0x10df2b0;
// views::Label::Label(this, const std::u16string_view* text, int text_context,
// int text_style, gfx::DirectionalityMode (5th, on the stack)): the overload
// FindBarMatchCountLabel's constructor calls (context 5, its own style, mode 0).
inline constexpr uintptr_t Label_ctor = 0x2784870;
// The vtable a plain views::Label has at +0 once constructed (read live off one
// Label_ctor built; the PDB names several Label vftables); a subclass such as
// FindBarMatchCountLabel has its own, so this names a bare Label.
inline constexpr uintptr_t Label_vftable = 0x11906398;
// void views::Label::SetEnabledColor(this, ui::ColorVariant): the variant is 8
// bytes passed by value (a colour or a colour id, its index byte at +4); it
// lands in LABEL_requested_enabled_color.
inline constexpr uintptr_t Label_SetEnabledColor = 0x1002e40;
// void views::Label::SetFontList(this, const gfx::FontList&) and
// const gfx::FontList& views::Textfield::GetFontList(this) (its RenderText's).
inline constexpr uintptr_t Label_SetFontList = 0x2d98f80;
inline constexpr uintptr_t Textfield_GetFontList = 0x1033ea0;

// --- live tab hover preview (docs/re/live-preview.md) ---
// A hovered tab's card already shows a frame from Edge's own continuous tab
// capturer (BackgroundThumbnailVideoCapturer), and the card repaints every frame
// it receives; it looks frozen only because Edge stops the capturer after one
// good frame. Keeping the capturer alive while a hover card is open turns the
// still into live video, reusing Edge's pipeline (live_preview.h).
// void ThumbnailTabHelper::TabStateTracker::StopCapture(this): the driver's
// end-of-capture-session call (-> BackgroundThumbnailVideoCapturer::Stop). Not
// per-frame, idempotent; suppressing it keeps the already-running capturer going.
inline constexpr uintptr_t ThumbnailTabHelper_TabStateTracker_StopCapture =
    0x2bab5f0;
// void TabHoverCardThumbnailObserver::Observe(this, ThumbnailImage*): a hover
// card's thumbnail subscription begins (once per hover / tab retarget) — the
// moment a preview is on screen and frames are wanted.
inline constexpr uintptr_t TabHoverCardThumbnailObserver_Observe = 0x8fc2b64;
// void TabHoverCardController::OnCardClosing(this): the hover card's definitive
// teardown (once per close).
inline constexpr uintptr_t TabHoverCardController_OnCardClosing = 0xa7986d0;
// bool base::i18n::IsRTL(): Edge's UI runs right to left (its locale, or the
// --force-ui-direction switch). A bare `jmp` to ICUIsRTL, which asks
// GetForcedTextDirection and then the default ICU locale; Views mirrors every
// tab by it.
inline constexpr uintptr_t base_i18n_IsRTL = 0xfe2210;
}  // namespace rva

// --- struct field offsets ---
namespace off {
// AskCopilotRequest (0x160 bytes): the prompt string at +0, the type string
// ("text") at +0x18, a flag byte at +0x30 (SendPageContextualPromptToChat's last
// argument), the page text as an optional<string> at +0x38, an optional
// empty string at +0xd8 and (since 156) another optional<string> at +0x140.
inline constexpr uintptr_t ASKREQUEST_size = 0x160;
inline constexpr uintptr_t ASKREQUEST_type = 0x18;
inline constexpr uintptr_t ASKREQUEST_flag = 0x30;
inline constexpr uintptr_t ASKREQUEST_text = 0x38;
inline constexpr uintptr_t ASKREQUEST_trailing = 0xd8;
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
// views::View* (relative to the View base)
inline constexpr uintptr_t MCV_drop_target_view = 0x0b0;

// --- ThemeService ---
// The active theme's supplier, a scoped_refptr to a BrowserThemePack for an
// extension or colored theme and null for the default theme. SwapThemeSupplier
// (primary `this`) is what writes it; ThemeService::GetThemeSupplier reads the
// same field as +0x760 because it is a method of the base at +8.
inline constexpr uintptr_t THEMESERVICE_supplier = 0x768;
// ThemeService::UseDefaultTheme takes its secondary base at +0x10 (it adjusts
// this - 0x10 itself); the primary `this` there faults.
inline constexpr uintptr_t THEMESERVICE_use_default_this = 0x10;
// BrowserThemePack colour-table ids: the id space GetColor is keyed on. The
// manifest-key -> id map is a {const char* key; int id} array in msedge.dll.
// Edge keeps a SEPARATE *tints* table with its own enum (frame is 0x1c there),
// so a tint id handed to GetColor always misses — do not cross the two.
inline constexpr int THEMECOLOR_frame = 0x02;
inline constexpr int THEMECOLOR_tab_text = 0x12;  // the active tab's text
inline constexpr int THEMECOLOR_toolbar = 0x17;
// Edge's vertical-tab strip colour ids. Edge has no manifest key for them:
// BrowserThemePack::EdgeSetVTabAndRelatedColor (rva 0x8838352) DERIVES all of
// them from the toolbar colour (it reads THEMECOLOR_toolbar/0x17, then stores a
// contiguous run 0x23-0x2c) and GetColor reads them back when the colour mixer
// builds the vertical-tab views' ColorProvider. That is why a theme can tint the
// rail (it follows the toolbar) but cannot give the SELECTED tab its own colour -
// every one is just a shade of the toolbar. The ten ids pair as active-window /
// inactive-window; the DLL overrides only the selected tab so it adopts the
// theme's own accent (docs/re/theming.md, hooks.h h_getcolor). Verified from the
// store sequence in EdgeSetVTabAndRelatedColor.
inline constexpr int VTABCOLOR_background = 0x23;  // rail bg
inline constexpr int VTABCOLOR_background_inactive =
    0x24;  // rail, unfocused window
inline constexpr int VTABCOLOR_background_alt =
    0x27;  // idle fill (+0x28 inact.)
inline constexpr int VTABCOLOR_selected_background = 0x25;  // selected tab bg
inline constexpr int VTABCOLOR_selected_background_inactive = 0x26;
inline constexpr int VTABCOLOR_foreground = 0x29;  // idle text (+0x2a inactive)
inline constexpr int VTABCOLOR_selected_foreground = 0x2b;  // selected text
inline constexpr int VTABCOLOR_selected_foreground_inactive = 0x2c;
// Native ui::ColorIds (ColorProvider's id space, distinct from the pack ids
// above). A tab's title and close colour: TabStyle::GetTabForegroundColor picks
// one of four from a [state][window focus] table at rva 0x12888228; a vertical
// tab always uses the active-state pair, so every title shares it.
// The active horizontal tab's fill, for a focused and an unfocused window:
// EdgeTabStyle::GetTabBackgroundColor (rva 0x2879530) reads 0xaca + 2 * !focused
// for the active tab (an inactive tab reads 0xacd / 0xace). A vertical tab
// reads the theme pack's VTABCOLOR ids instead.
inline constexpr int UICOLOR_active_tab_background_focused = 0xaca;
inline constexpr int UICOLOR_active_tab_background_unfocused = 0xacc;
inline constexpr int UICOLOR_tab_foreground_first = 0xadb;
inline constexpr int UICOLOR_tab_foreground_last = 0xade;
// The audio/alert icon of the ACTIVE tab: tabs::GetAlertIndicatorColor (rva
// 0x2a5b5da) indexes [alert kind][active][window focus] tables (rva 0x1288c4a0,
// 0x1288c4e0); these are their active-tab entries. Seen live (155): a playing
// tab's speaker was the last one.
inline constexpr int UICOLOR_active_tab_alert[] = {
    0xabf, 0xac0, 0xac3, 0xac4, 0xabb, 0xabc, 0xc06, 0xc08, 0xc04};
// WebContentsImpl::GetController is a bare `lea rax, [this + 0x280]`.
inline constexpr uintptr_t WEBCONTENTS_controller = 0x280;
// content::ReloadType::NORMAL.
inline constexpr int32_t RELOADTYPE_normal = 1;
// WebContentsImpl: the ColorProviderSourceObserver subobject.
inline constexpr uintptr_t WEBCONTENTS_color_observer = 0x80;
// WebContents: the virtual at +0x68 is GetVisibleURL, the URL the address bar
// shows, which is the pending one while a navigation has not committed yet (the
// last committed URL is still empty or the previous page's then). Edge's own
// EdgeComputePreferredColorScheme asks it.
inline constexpr uintptr_t WEBCONTENTS_get_visible_url_slot = 0x68;
// The pair EdgeComputePreferredColorScheme fills.
inline constexpr uintptr_t PREFERREDSCHEME_color_scheme = 0x0;
inline constexpr uintptr_t PREFERREDSCHEME_scrollbar_scheme = 0x4;
// ui::ColorProviderKey: its first field is the ColorMode (read first by
// ColorProviderSource::GetColorMode), 0 = light, 1 = dark.
inline constexpr uintptr_t COLORPROVIDERKEY_color_mode = 0x0;
inline constexpr int32_t COLORMODE_light = 0;
inline constexpr int32_t COLORMODE_dark = 1;

// --- BrowserView ---
// MultiContentsView* within BrowserView
inline constexpr uintptr_t BV_multi_contents_view = 0x6f8;
inline constexpr uintptr_t BV_browser = 0x618;  // Browser* within BrowserView

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
inline constexpr uintptr_t TABCOLL_count = 0x20;

// --- TabModel virtual-table slots (docs/re/split-grow-impl.md) ---
// WebContents* TabInterface::GetContents(this) (as GetCurrentMainWebContents
// calls it on a split's first member).
inline constexpr uintptr_t TABMODEL_vtbl_GetContents = 0x18;
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
inline constexpr size_t ControlButtons_button_size = 0x640;
// sizeof(views::View): its subclasses' own fields start here (ControlButtons
// @+0x338 its BrowserView, EdgeMultiContentsResizeArea @+0x338 its press point).
inline constexpr size_t View_size = 0x338;
// The tab a style serves: a pointer to the Tab base of the EdgeTab, whose views::
// View subobject (the tab view, a VerticalTab for a row of the vertical list)
// starts EDGETAB_view_offset further on.
inline constexpr uintptr_t STYLEVIEWS_tab = 0x10;
inline constexpr uintptr_t EDGETAB_view_offset = 0x18;
// EdgeTab + 0x548: the tab's layout controller. Its virtual at LAYOUT_mode_slot
// returns the tab layout mode; EdgeTabStyleViews::UseVerticalStyle (rva
// 0x1166370) calls the tab vertical for modes LAYOUT_vertical_first through
// LAYOUT_vertical_first + LAYOUT_vertical_count - 1.
inline constexpr uintptr_t EDGETAB_layout_controller = 0x548;
inline constexpr size_t LAYOUT_mode_slot = 7;
inline constexpr uint32_t LAYOUT_vertical_first = 2;
inline constexpr uint32_t LAYOUT_vertical_count = 3;
// sizeof(a FocusableViewsMap): a base::flat_map, i.e. a vector {data, size,
// capacity}; all zero is empty.
inline constexpr size_t FocusableViewsMap_size = 0x18;
// The ui::ColorId Edge's split ⋯ items draw their icons in.
inline constexpr int MENU_icon_color_id = 0x172;
// views::MenuController::ExitType::kAll: close the whole menu.
inline constexpr int MENU_exit_all = 1;
// Stack room for value types built only to be handed to Edge: ui::ColorVariant
// (passed on by value as its first 8 bytes), views::SizeBounds (zeroed =
// unbounded), and ui::ImageModel (an upper bound, with room to spare).
inline constexpr size_t ColorVariant_size = 16;
inline constexpr size_t SizeBounds_size = 16;
// views::SizeBounds: {width, has width, height, has height}, each a
// std::optional<int>'s value then its engaged byte, as CalculateProposedLayout
// reads it.
inline constexpr uintptr_t SIZEBOUNDS_width = 0x0;
inline constexpr uintptr_t SIZEBOUNDS_has_width = 0x4;
inline constexpr uintptr_t SIZEBOUNDS_height = 0x8;
inline constexpr uintptr_t SIZEBOUNDS_has_height = 0xc;
// sizeof(EdgeContentsContainerView), the bytes NewView allocates before
// EdgeContentsContainerView_ctor builds one.
inline constexpr size_t EdgeContentsContainerView_size = 0x558;
// A byte NewView's allocator leaves at 1 in a block it handed out poisoned.
inline constexpr uintptr_t CONTAINER_allocation_poisoned = 0x179;
inline constexpr size_t ImageModel_size_bound = 256;
// MenuItemView's icon view. GetChildPreferredSize takes a row's height from
// this view ALONE (extra children only add width), so a taller row comes from
// a taller icon view.
inline constexpr uintptr_t MENUITEM_icon_view = 0x418;
// MenuItemView's views::LayoutDelegate base, right after its views::View base.
inline constexpr size_t MENUITEM_layout_delegate = View_size;
inline constexpr size_t EdgeResizeArea_size = 0x358;
// a valid id (⋯'s); grip overrides the tooltip
inline constexpr int ControlButtons_more_string_id = 0x4692;
// ui::MenuSeparatorType NORMAL_SEPARATOR.
inline constexpr int MENU_normal_separator = 0;
// The bar's buttons (InitButtons): ⋯ @+0x340, X @+0x350.
inline constexpr uintptr_t ControlButtons_close_button = 0x350;
// The ⋯ menu's items (SplitWindowControlMenu). BuildMenuModel adds them in
// this order: 2, 3, 5, then 0 or 1 (the switch to the other orientation), 6
// (with the link-mode feature), 4.
enum class ControlMenuCommand : int {
  kSwitchToVertical = 0,    // SetVerticalSplitStateForTab(active, true)
  kSwitchToHorizontal = 1,  // SetVerticalSplitStateForTab(active, false)
  kOpenInNewTab = 2,        // SplitAction kCopyFirstToNewTab from the first
                            // pane's ⋯, else kCopyLastToNewTab
  kSeparate = 3,            // SplitAction kDissolveKeepFirst from the first
                            // pane's ⋯, else kDissolveKeepLast
  kSendFeedback = 4,        // Edge's feedback dialog
  kSwitchSides = 5,         // SplitAction kReverseSides
  kLinkMode = 6,            // the "open left links to the right" toggle
};

// --- TabStripModel close / activate arguments ---
// TabStripModel::CloseTypes: CLOSE_USER_GESTURE, what Edge's own X passes.
inline constexpr uint32_t TSM_close_user_gesture = 1;
// sizeof(TabStripUserGestureDetails) {GestureType, base::TimeTicks}
inline constexpr size_t TSM_gesture_details_size = 16;

// --- SplitWindowController::DoActionToSplitWindow's `action` ---
// Its 6-way jump table, mapped in docs/re/collapse.md §5. Every action works on
// the split's first tab, or its last (0 and 3); the WebContents it is handed is
// only checked for null. The names are ours: Edge's own enum was not located.
enum class SplitAction : int {
  kCopyLastToNewTab = 0,   // the last tab's page, into a new background tab
  kCopyFirstToNewTab = 1,  // the first tab's page, into a new background tab
  kDissolveKeepFirst = 2,  // dissolve; the first tab comes to the front
  kDissolveKeepLast = 3,   // dissolve; the last tab comes to the front
  kCloseFirst = 4,         // CLOSES the split's first tab
  kReverseSides = 5,       // ReverseTabsInSplit: the first tab one step on
};

// --- gfx::VectorIcon path command ids in this build ---
// Decoded from kEdgeSplitWindowMoreIcon: CANVAS_DIMENSIONS 16, MOVE_TO x y,
// R_ARC_TO rx ry angle large sweep dx dy, CLOSE.
inline constexpr int ICON_NEW_PATH = 0;
inline constexpr int ICON_MOVE_TO = 7;
inline constexpr int ICON_LINE_TO = 11;    // from kArrowUpIcon
inline constexpr int ICON_R_LINE_TO = 12;  // from kArrowUpIcon
inline constexpr int ICON_V_LINE_TO = 15;  // from kArrowUpIcon
inline constexpr int ICON_R_ARC_TO = 10;
inline constexpr int ICON_CLOSE = 27;
inline constexpr int ICON_CANVAS_DIMENSIONS = 28;

// --- a split pane's links (docs/re/split-links.md) ---
// mojom::CreateNewWindowParams: opener_suppressed (bool) and target_url (GURL).
inline constexpr uintptr_t NEWWINDOW_opener_suppressed = 0x50;
inline constexpr uintptr_t NEWWINDOW_target_url = 0x80;
// WindowOpenDisposition, with Edge's own split foreground tab (12).
inline constexpr int DISPOSITION_new_foreground_tab = 3;
inline constexpr int DISPOSITION_new_background_tab = 4;
inline constexpr int DISPOSITION_split_foreground_tab = 12;
// sizeof(NavigationController::LoadURLParams) (CreateNewWindow allocates it
// with this), and of a base::WeakPtr {WeakReference, T*}.
inline constexpr size_t LoadURLParams_size = 0x478;
inline constexpr size_t WeakPtr_size = 0x10;
// SplitTabData's "open left links to the right" toggle (bool, one per split;
// a new split takes it from the edge_split_window.default_link_mode pref).
inline constexpr uintptr_t SPLITDATA_link_mode = 0x28;
// GetTabsAndIndicesInSplit's entries: {TabModel*, int strip index}.
inline constexpr size_t SPLITMEMBER_size = 0x10;
// A new pane's tab offer: sizeof(GURL), sizeof(optional<TabGroupId>), and
// SplitWindowControllerImplV2's TabStripModel*.
inline constexpr size_t GURL_size = 0x78;
inline constexpr size_t OptionalTabGroupId_size = 0x18;
inline constexpr uintptr_t SPLITCONTROLLER_tab_strip_model = 0x48;
// SplitWindowControllerImplV2's StartingPageNotifierImpl*, made by the first
// GetStartingPageNotifier (a starting page registering).
inline constexpr uintptr_t SPLITCONTROLLER_starting_page_notifier = 0x78;
// A base::BindState's first bound argument (after BindStateBase), here a
// base::WeakPtr {WeakReference::Flag*, T*}, and the flag's invalidated byte.
inline constexpr uintptr_t BINDSTATE_first_bound_argument = 0x28;
inline constexpr uintptr_t WEAKFLAG_invalidated = 4;
// SplitWindowStartingUIHandler's raw_ptr<content::WebUI>, and that WebUI's
// WebContents: the starting page's own.
inline constexpr uintptr_t STARTINGHANDLER_web_ui = 0x38;
inline constexpr uintptr_t WEBUI_web_contents = 0x68;

// --- a tab drag (docs/re/tab-drag.md) ---
// TabDragController fields: its state (TABDRAG_state_dragging_tabs once
// moving tabs), the strip it is attached to (a TabDragContext, itself a
// views::View), and vector<TabDragData> {data, size} of the dragged tabs.
inline constexpr uintptr_t TABDRAG_state = 0x38;
inline constexpr int TABDRAG_state_dragging_tabs = 1;
inline constexpr uintptr_t TABDRAG_attached_context = 0x58;
inline constexpr uintptr_t TABDRAG_drag_data = 0xe0;
inline constexpr uintptr_t TABDRAG_drag_data_count = 0xe8;
inline constexpr size_t TABDRAGDATA_size = 0x70;
inline constexpr uintptr_t TABDRAGDATA_contents = 0x10;  // WebContents*
// EndDragReason::kComplete (the pointer was released).
inline constexpr int TABDRAG_end_complete = 0;
// EndDragReason::kCancel (Escape).
inline constexpr int TABDRAG_end_cancel = 1;
// TabDragContext virtuals: Tab* GetTabForContents(this, WebContents*) and
// TabStripModel* GetTabStripModel(this).
inline constexpr uintptr_t DRAGCONTEXT_vtbl_GetTabForContents = 0x3a0;
inline constexpr uintptr_t DRAGCONTEXT_vtbl_GetTabStripModel = 0x3c0;
// TabDragController* GetDragController(this), null when no drag runs, and
// void EndDrag(this, EndDragReason): what the strip's OnMouseReleased (reason
// complete) and OnMouseCaptureLost (capture lost) call.
inline constexpr uintptr_t DRAGCONTEXT_vtbl_GetDragController = 0x3c8;
inline constexpr uintptr_t DRAGCONTEXT_vtbl_EndDrag = 0x420;

// --- a press on a tab, as Tab::OnMousePressed makes it (docs/re/tab-drag.md) ---
// A tab view's TabSlotController* (its TabStrip, seen through that base).
inline constexpr uintptr_t TABVIEW_controller = 0x530;
// TabSlotController virtuals: ui::ListSelectionModel GetSelectionModel(this)
// (through the hidden out pointer) and void MaybeStartDrag(this, TabSlotView*,
// const ui::LocatedEvent&, ui::ListSelectionModel by value: the caller's copy,
// which the callee destroys). MaybeStartDrag drags the SELECTED tabs; the
// event's location is the grab point in the tab's coordinates.
inline constexpr uintptr_t SLOTCONTROLLER_vtbl_GetSelectionModel = 0x178;
inline constexpr uintptr_t SLOTCONTROLLER_vtbl_MaybeStartDrag = 0x220;
// bool IsActiveTab(this, const Tab* view): the strip's one active tab. The
// controller is the tab object's EDGETAB_layout_controller. Tab::IsActive
// (0x1055540) asks TabStrip::GetTabsInSplit (0x1164d70) for a split tab, which
// returns the tab and ONE neighbour of the same split (the previous, else the
// next, through GetAdjacentTab at vtable +0x240), and calls the tab active if
// either is: in a grown split a sliding pair lit up.
inline constexpr uintptr_t SLOTCONTROLLER_vtbl_IsActiveTab = 0x208;
// A tab object's optional<SplitTabId>: the 16-byte token @0x370, then this
// has_value byte.
inline constexpr uintptr_t TABOBJECT_has_split_id = 0x380;
inline constexpr size_t LISTSELECTIONMODEL_size = 0x38;
// ui::MouseEvent: 0x118 bytes, its gfx::PointerDetails (0x48) at its end.
inline constexpr size_t MOUSEEVENT_size = 0x118;
inline constexpr size_t POINTERDETAILS_size = 0x48;
inline constexpr int EVENT_type_mouse_pressed = 1;
// ui::EF_LEFT_MOUSE_BUTTON, in an event's flags (+0x50) and as the changed
// button.
inline constexpr int EVENT_flag_left_mouse_button = 0x800;

// --- the tab list's layout (tabrows.h) ---
// TabStripLayoutHelper's slots, {data, count}: TABSLOT_size bytes each, a type
// (TABSLOT_type_tab for a tab; the others are group headers) and the slot's
// TabSlotView*.
inline constexpr uintptr_t TABLAYOUT_slots = 0x38;
inline constexpr uintptr_t TABLAYOUT_slot_count = 0x40;
inline constexpr size_t TABSLOT_size = 0x38;
inline constexpr uintptr_t TABSLOT_type = 0x00;
inline constexpr uint32_t TABSLOT_type_tab = 0;
inline constexpr uintptr_t TABSLOT_view = 0x08;
// A tab view's split: std::optional of the split's 16-byte token, with the
// has-value flag after it.
inline constexpr uintptr_t TABVIEW_split_token = 0x358;
inline constexpr size_t TABVIEW_split_token_size = 16;
inline constexpr uintptr_t TABVIEW_split_has_value = 0x368;
// CalculateIdealBounds's result: {gfx::Rect* data, size_t count}, one Rect per
// slot.
inline constexpr uintptr_t IDEALBOUNDS_data = 0x00;
inline constexpr uintptr_t IDEALBOUNDS_count = 0x08;

// --- views::View ---
// View* parent_ (null at the widget's root view)
inline constexpr uintptr_t VIEW_parent = 0x198;
// View** children_ data, the size-based vector before VIEW_child_count
inline constexpr uintptr_t VIEW_children = 0x1a0;
// size_t: the number of children (AddChildViewAt's "at the end" index)
inline constexpr uintptr_t VIEW_child_count = 0x1a8;
// gfx::Rect bounds_ in parent coords (GetLocalBounds reads size @+0x1d0)
inline constexpr uintptr_t VIEW_bounds = 0x1c8;

// --- ui::Event / ui::KeyEvent ---
// ui::EventType @+0xc (key pressed / released); a KeyEvent's key_code
// (ui::KeyboardCode, the Windows VK) @+0xb0.
inline constexpr uintptr_t EVENT_type = 0xc;
inline constexpr int EVENT_type_key_pressed = 7;
inline constexpr int EVENT_type_key_released = 8;
inline constexpr uintptr_t KEYEVENT_key_code = 0xb0;

// --- ui::LocatedEvent ---
// (LocatedEvent ctor): location_ @+0xb0, root_location_ @+0xb8, each a
// gfx::PointF; the root location is in the window's DIPs.
inline constexpr uintptr_t EVENT_root_location = 0xb8;

// --- views::ProposedLayout ---
// The out-param CalculateProposedLayout fills (0x190 bytes, which it clears).
inline constexpr uintptr_t PL_host_size = 0x00;  // gfx::Size {int w, int h}
// absl::InlinedVector<ChildLayout, 8>, as ApplyLayout reads it: a size word
// (count << 1, bit 0 set once the entries moved to the heap), then either the
// 8 entries inline or {ChildLayout* data, size_t capacity}. PL_child_layouts is
// the vector itself, the `this` EmplaceBack takes.
inline constexpr uintptr_t PL_child_layouts = 0x08;
inline constexpr uintptr_t PL_child_size_word = 0x08;
inline constexpr uint64_t PL_child_on_heap = 1;
inline constexpr int PL_child_count_shift = 1;
inline constexpr uintptr_t PL_child_entries = 0x10;

// --- views::ChildLayout ---
// The element of PL_child_layouts; stride 0x30, from ApplyLayout.
inline constexpr uintptr_t CL_stride = 0x30;
inline constexpr uintptr_t CL_child = 0x00;    // views::View*
inline constexpr uintptr_t CL_visible = 0x08;  // bool
inline constexpr uintptr_t CL_bounds = 0x0c;   // gfx::Rect {int x, y, w, h}
// views::SizeBounds {SizeBound w{val,has}, h{val,has}}
inline constexpr uintptr_t CL_avail = 0x1c;

// extensions::ExtensionRegistry's ExtensionSets: enabled at +8 (AddEnabled),
// disabled at +0x28 (RemoveDisabled, EnableExtension).
inline constexpr uintptr_t REGISTRY_enabled_set = 0x08;
inline constexpr uintptr_t REGISTRY_disabled_set = 0x28;

// ContentSettingBubbleModel's vtable slot (byte offset) for OnDoneButtonClicked,
// the accept callback ContentSettingBubbleContents' ctor binds (`vcall'{56}`).
inline constexpr uintptr_t BubbleModel_OnDoneButtonClicked_slot = 0x38;
// The decision PermissionDecided reads: the value at +0 (0 = Allow) and, at +8,
// whether there is one (1) or the prompt was dismissed (0).
inline constexpr uintptr_t PERMISSION_DECISION_value = 0x0;
inline constexpr uintptr_t PERMISSION_DECISION_has_value = 0x8;
// ProtocolHandlerRegistry's live ignored list, a size-based libc++ vector
// {ProtocolHandler* data, size_t size, size_t capacity} at +0x28: read as
// `[rcx+0x28]` + `[rcx+0x30] * 0xe0` by RemoveIgnoredHandler,
// SilentlyHandleRegisterHandlerRequest and GetIgnoredHandlers.
inline constexpr uintptr_t RPH_REGISTRY_ignored_data = 0x28;
inline constexpr uintptr_t RPH_REGISTRY_ignored_size = 0x30;
// sizeof(custom_handlers::ProtocolHandler): the stride of every handler
// vector, and what the copy constructor fills (its last field is at +0xd0..0xe0).
inline constexpr size_t PROTOCOL_HANDLER_size = 0xe0;

// --- gfx::Rect field offsets within CL_bounds ---
inline constexpr uintptr_t RECT_x = 0x0;
inline constexpr uintptr_t RECT_y = 0x4;
inline constexpr uintptr_t RECT_w = 0x8;
inline constexpr uintptr_t RECT_h = 0xc;

// --- the browser assistant ---
// EdgeToolbarView: the base CreateDiscoverButtonIfNecessary is called on, and
// the raw_ptr to the chat button (the bare button, or the one inside the
// split-button container).
inline constexpr uintptr_t EDGETOOLBAR_discoverBase = 0x5a8;
inline constexpr uintptr_t EDGETOOLBAR_chatButton = 0x5d0;
// libc++ std::u16string: a short string keeps its length in the last byte,
// and its top bit marks the long form {data, size, capacity}.
inline constexpr uintptr_t U16STRING_short_size = 0x17;
inline constexpr uint8_t U16STRING_long_flag = 0x80;
inline constexpr uintptr_t U16STRING_long_size = 0x08;
// base::Value: a std::variant whose index byte sits after its 24 bytes of
// storage (a std::string for the string alternative, index 4).
inline constexpr uintptr_t VALUE_variant_index = 0x18;
inline constexpr uint8_t VALUE_string_index = 4;
// The isolated world our scripts run in. ExecuteJavaScriptInIsolatedWorld
// accepts 1..11 and an isolated world shares the page's DOM but not its
// globals, so a page cannot see or tamper with our script.
inline constexpr int kScriptWorldId = 11;
// FindBarView: its views::Textfield* and its FindBarMatchCountLabel* (the
// "n/m" label, a views::Label whose virtual at LABEL_set_text_slot takes a
// const std::u16string_view*).
inline constexpr uintptr_t FINDBARVIEW_textfield = 0x390;
inline constexpr uintptr_t FINDBARVIEW_match_count_label = 0x398;
inline constexpr uintptr_t FINDBARLABEL_set_text_vtable_offset = 0x3a8;
// views::Label: its text context and text style (Label::GetTextContext and
// GetTextStyle are bare reads of these), which pick its font and colour id.
inline constexpr uintptr_t LABEL_text_context = 0x388;
inline constexpr uintptr_t LABEL_text_style = 0x38c;
// DomDistillerContextKeyedService: its DomDistillerService base, and that
// service's DistilledPagePrefs* (ChromeDomDistillerClient::GetDistilledPagePrefs
// does exactly this: +8, then the field at +0x20).
inline constexpr uintptr_t DOMDISTILLERKEYED_service = 0x8;
inline constexpr uintptr_t DOMDISTILLERSERVICE_distilled_page_prefs = 0x20;
// views::Label: the colour asked for by SetEnabledColor, a
// std::optional<ui::ColorVariant> (the variant's 8 bytes, then the engaged
// flag), as Label::SetEnabledColor writes it.
inline constexpr uintptr_t LABEL_requested_enabled_color = 0x3b0;
inline constexpr uintptr_t LABEL_requested_enabled_color_is_set = 0x3b8;
// Bytes to allocate for a views::Label: FindBarMatchCountLabel's own fields start
// here, so a Label fits in it.
inline constexpr size_t LABEL_allocation_size = 0x4a8;
// The TextfieldController subobject HandleKeyEvent receives as `this` sits this
// far into the FindBarView (the text field is at +0x58 of it).
inline constexpr uintptr_t FINDBARVIEW_textfield_controller = 0x338;
// FindBarView: the FindBarHost* (the bar's widget) and the raw_ptr to the
// FindOptionsControlView (null until the options were first needed).
inline constexpr uintptr_t FINDBARVIEW_host = 0x370;
inline constexpr uintptr_t FINDBARVIEW_options_view = 0x418;
// FindOptionsControlView: a std::map<int, raw_ptr<FindInPageOptionToggleView>>
// (libc++): its header node's left child, the tree's root, sits at +0x340; a
// node is {left +0, right +8, parent +0x10, color +0x18, key +0x20, value +0x28}.
inline constexpr uintptr_t FINDOPTIONS_toggle_map_root = 0x340;
inline constexpr uintptr_t MAPNODE_left = 0x00;
inline constexpr uintptr_t MAPNODE_right = 0x08;
inline constexpr uintptr_t MAPNODE_key = 0x20;
inline constexpr uintptr_t MAPNODE_value = 0x28;
// FindInPageOptionToggleView: the FindBarHost* it belongs to, its id, the width
// it was built with, its title Label* and its EdgeToggleButton*; and the
// FindBarView* that FindBarHost keeps.
inline constexpr uintptr_t FINDTOGGLE_host = 0x340;
inline constexpr uintptr_t FINDBARHOST_view = 0x210;
inline constexpr uintptr_t FINDTOGGLE_id = 0x348;
inline constexpr uintptr_t FINDTOGGLE_width = 0x34c;
inline constexpr uintptr_t FINDTOGGLE_label = 0x358;
inline constexpr uintptr_t FINDTOGGLE_button = 0x360;
// The toggle ids. Edge builds the first three (each only where its feature
// is on); any other id has an empty title and no meaning to Edge's find, so the
// next two are ours.
inline constexpr int FINDTOGGLE_id_match_case = 0;
inline constexpr int FINDTOGGLE_id_whole_word = 1;
inline constexpr int FINDTOGGLE_id_diacritics = 2;
inline constexpr int FINDTOGGLE_id_regexp = 3;
inline constexpr int FINDTOGGLE_id_ask = 4;
// The RegExp flag switches (find_flags.h), shown under RegExp while it is on.
inline constexpr int FINDTOGGLE_id_flag_indices = 5;
inline constexpr int FINDTOGGLE_id_flag_ignore_case = 6;
inline constexpr int FINDTOGGLE_id_flag_multiline = 7;
inline constexpr int FINDTOGGLE_id_flag_dot_all = 8;
inline constexpr int FINDTOGGLE_id_flag_unicode = 9;
inline constexpr int FINDTOGGLE_id_flag_unicode_sets = 10;
inline constexpr int FINDTOGGLE_id_flag_sticky = 11;
// ui::KeyEvent: its type (7 = key pressed) and its key code (0x0d = Enter).
inline constexpr uintptr_t KEYEVENT_type = 0x0c;
inline constexpr int32_t KEYEVENT_type_pressed = 7;
inline constexpr uint8_t KEYEVENT_key_enter = 0x0d;
// find_in_page::FindNotificationDetails (0x28 bytes): request id, number of
// matches (-1 = unknown), the match's selection rect, the active match's
// 1-based ordinal (-1 = unknown) and whether this is the final update, then a
// ref-counted pointer (null here) that the copy constructor reads, so the whole
// 0x28 must be ours.
inline constexpr size_t FINDDETAILS_size = 0x28;
inline constexpr uintptr_t FINDDETAILS_request_id = 0x00;
inline constexpr uintptr_t FINDDETAILS_matches = 0x04;
inline constexpr uintptr_t FINDDETAILS_active_ordinal = 0x18;
inline constexpr uintptr_t FINDDETAILS_is_final = 0x1c;
}  // namespace off

}  // namespace fixedge::edge155
