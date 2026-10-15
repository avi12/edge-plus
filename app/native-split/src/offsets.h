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
// FocusableViewsMap* GetFocusableViewsMapFor(this, ContentsContainerView*):
// a lookup in MCV+0x4c8, a flat_map with an entry per container Edge itself
// built (two), so null for ours. Its one caller,
// BrowserView::MaybeUpdateStoredFocusForWebContents (every tab switch),
// dereferences it.
inline constexpr uintptr_t MultiContentsView_GetFocusableViewsMapFor =
    0x2b58cc8;
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
// void BrowserView::MaybeUpdateSplitView(this, WebContents* forContents): with
// the contents no longer in a split, runs MultiContentsView::CloseSplitView
// (clears pane 1, hides the resize area, moves content to container 0). The
// relayout a tab leaving a split would trigger, so a window is full-width right
// after a split is dissolved rather than on the user's next interaction.
inline constexpr uintptr_t BrowserView_MaybeUpdateSplitView = 0x2b58058;

// --- native split data-model grow (docs/re/split-insert.md, split-grow-impl.md) ---
// (size)->void*  (non-View new)
inline constexpr uintptr_t EdgeOperatorNew = 0x36b3c20;
// operator delete(void*): jumps to Edge's free (the allocator shim).
inline constexpr uintptr_t EdgeOperatorDelete = 0x36b2e50;
// split_tabs::LogSplitView{Created,Updated}UKM(TabStripModel*, const
// SplitTabId&): split metrics, each CHECK(ListTabs().size() == 2). Created runs
// from AddToSplitImpl, Updated from UpdateTabInSplitImpl and when a split
// member's page becomes primary (it navigates).
inline constexpr uintptr_t SplitTabs_LogSplitViewCreatedUKM = 0xa23d3c4;
inline constexpr uintptr_t SplitTabs_LogSplitViewUpdatedUKM = 0xa23d512;
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
// SplitWindowControllerImplV2::DoActionToSplitWindow(controller, WebContents*,
// int action), `action` one of off::SplitAction.
inline constexpr uintptr_t SplitWindowController_DoAction = 0xa021020;

// --- per-pane hover controls (the X / ⋯ bar, old-toolbar mode) ---
// EnsureControlButtons builds a ControlButtons bar for containers 0 and 1 only,
// and ShowSplitView registers hover on the FIRST and LAST member's WebContents
// only, so a middle pane has a bar but no hover and pane 3+ has hover but no bar.
// (this, BrowserView*, bool not_first)
inline constexpr uintptr_t ControlButtons_ctor = 0xa010a6a;
// ControlButtons::Button(this, const gfx::VectorIcon&): one round icon button
// of the bar (AddButton allocates it with ControlButtons_button_size bytes).
inline constexpr uintptr_t ControlButtons_Button_ctor = 0xa01109a;
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
// int ui::MouseEvent::GetClickCount(this): 2 on the second click of a double
// click (EdgeTwoDimensionalResizeArea resets its panes on that release).
inline constexpr uintptr_t MouseEvent_GetClickCount = 0xb631a18;
// bool views::View::IsMouseHovered(this): the cursor is over the view now.
inline constexpr uintptr_t View_IsMouseHovered = 0x2893f54;

// --- closing one pane of a grown split (docs/re/close-pane.md) ---
// Edge's X: ControlButtons::HandleButtonClick(bar, Button*). For pane 2+ it
// calls SplitWindowController::HideSplitView(true), which closes the split's
// LAST tab (not the clicked one), and closing any member dissolves the split.
inline constexpr uintptr_t ControlButtons_HandleButtonClick = 0xa011ae0;
// ControlButtons::ToggleContextMenu(bar, ...): the ⋯ of a pane's hover bar. It
// builds a SplitTabMenuModel that only knows "first pane or not", so the pane
// whose ⋯ was clicked is remembered here.
inline constexpr uintptr_t ControlButtons_ToggleContextMenu = 0xa011d00;
// The bar's ⋯ menu: SplitWindowControlMenu. BuildMenuModel(this) builds a
// ui::SimpleMenuModel (kept at this+0x10, delegate this+8) and RETURNS it, and
// Run passes the return value to PopulateMenu(this, MenuItemView* root, model),
// which creates one item view per model item, so an item removed from the
// model first never shows. ExecuteCommand(this, int id, int flags) switches on
// off::ControlMenuCommand; IsCommandIdEnabled returns true above 5.
inline constexpr uintptr_t SplitWindowControlMenu_BuildMenuModel = 0xa017fbc;
inline constexpr uintptr_t SplitWindowControlMenu_PopulateMenu = 0xa017f24;
inline constexpr uintptr_t SplitWindowControlMenu_ExecuteCommand = 0xa0186c0;
// ShouldExecuteCommandWithoutClosingMenu(this, int id, const ui::Event&) ->
// bool, `return id == 6`. Identical-code-folded: the SAME function serves
// other menus (e.g. EdgeDownloadsBtrItemContextMenu::IsItemForCommandIdDynamic),
// so a detour must pass every id but its own straight through.
inline constexpr uintptr_t SplitWindowControlMenu_ShouldExecuteWithoutClosing =
    0xa018ab0;
// gfx::VectorIcon data the ⋯ row's arrows reuse (Edge's own arrow icons).
inline constexpr uintptr_t Icon_ArrowUp = 0x120bb7b0;    // kArrowUpIcon
inline constexpr uintptr_t Icon_ArrowDown = 0x120bb778;  // kArrowDownIcon
// ← and → are kArrowUpIcon turned (icons.h); these are the fallback.
inline constexpr uintptr_t Icon_ArrowBack = 0x12a5cd58;     // kArrowBackIcon
inline constexpr uintptr_t Icon_ArrowForward = 0x12a5ce20;  // kArrowForwardIcon
// The ⋯ row's own leading icon (Chromium's split-view icon).
inline constexpr uintptr_t Icon_SplitScene = 0x120ba068;  // kSplitSceneIcon
// static ui::ImageModel FromVectorIcon(out, const VectorIcon&, ColorVariant
// (by value, 8 bytes), int size (0 = default), const VectorIcon* badge)
inline constexpr uintptr_t ImageModel_FromVectorIcon = 0x0fc8dfc;
inline constexpr uintptr_t ImageModel_dtor = 0x0fc8ca2;
// ui::ColorVariant::ColorVariant(this, ui::ColorId)
inline constexpr uintptr_t ColorVariant_ctor = 0x10693d0;
// (model, int id, const std::u16string& label, const ui::ImageModel& icon)
inline constexpr uintptr_t SimpleMenuModel_AddItemWithIcon = 0x2b5ea80;
// views::View::View(this): a plain view (no layout manager).
inline constexpr uintptr_t View_ctor = 0x101de7a;
// (view, const std::optional<gfx::Size>*): {int width, int height, bool}.
inline constexpr uintptr_t View_SetPreferredSize = 0x2b89b16;
// (view, const gfx::Rect&)
inline constexpr uintptr_t View_SetBoundsRect = 0x0fd8234;
// gfx::Rect View::GetBoundsInScreen(this) (through the hidden out pointer),
// in screen DIPs.
inline constexpr uintptr_t View_GetBoundsInScreen = 0x28ae474;
// static View::ConvertPointFromScreen(const View* to, gfx::Point* in out):
// screen DIPs -> `to`'s coordinates.
inline constexpr uintptr_t View_ConvertPointFromScreen = 0x28940c2;
// gfx::Size View::GetPreferredSize(this, const SizeBounds&) (returned through
// the hidden out pointer); zeroed SizeBounds = unbounded.
inline constexpr uintptr_t View_GetPreferredSize = 0x0faf994;
// ProposedLayout* MenuItemView::CalculateProposedLayout(LayoutDelegate* this,
// ProposedLayout* out, const SizeBounds&): every child's bounds in the row.
// `this` is the item's LayoutDelegate base (MENUITEM_layout_delegate). Edge
// centres the icon on a STANDARD row's height and pins other children to the
// row's top.
inline constexpr uintptr_t MenuItemView_CalculateProposedLayout = 0xb25f350;
// MenuController* MenuController::GetActiveInstance(): the open menu's, or null.
inline constexpr uintptr_t MenuController_GetActiveInstance = 0xb263d7e;
// MenuController::Cancel(this, ExitType, int): as MenuRunnerImpl::Cancel calls
// it, with (MENU_exit_all, 0).
inline constexpr uintptr_t MenuController_Cancel = 0xb263dd0;
// MenuItemView::ChildPreferredSizeChanged(this, View* child): drops the
// item's cached dimensions so it is measured again.
inline constexpr uintptr_t MenuItemView_ChildPreferredSizeChanged = 0xb25c8d0;
// MenuItemView* MenuItemView::GetMenuItemByID(this, int id)
inline constexpr uintptr_t MenuItemView_GetMenuItemByID = 0xb25e012;
inline constexpr uintptr_t SimpleMenuModel_AddSeparator = 0x317665a;
// size_t GetItemCount(model)
inline constexpr uintptr_t SimpleMenuModel_GetItemCount = 0x28cf330;
// int GetCommandIdAt(model, size_t index)
inline constexpr uintptr_t SimpleMenuModel_GetCommandIdAt = 0x342b5b0;
// void RemoveItemAt(model, size_t index)
inline constexpr uintptr_t SimpleMenuModel_RemoveItemAt = 0xb65da80;
// void SetLabel(model, size_t index, const std::u16string& label)
inline constexpr uintptr_t SimpleMenuModel_SetLabel = 0xb65dcce;
// (tsm, int index, uint32 closeTypes); HideSplitView passes 1
inline constexpr uintptr_t TabStripModel_CloseWebContentsAt = 0xa26f916;
// (tsm, int index, const TabStripUserGestureDetails*)
inline constexpr uintptr_t TabStripModel_ActivateTabAt = 0xa26c482;
// int TabStripModel::GetIndexOfWebContents(this, WebContents*): -1 if absent.
inline constexpr uintptr_t TabStripModel_GetIndexOfWebContents = 0x10a7d34;
// int TabStripModel::MoveWebContentsAt(this, int index, int to_position,
// bool select_after_move): `to_position` is the tab's index after the move.
inline constexpr uintptr_t TabStripModel_MoveWebContentsAt = 0xa26c78c;
// A split pane's links (docs/re/split-links.md):
// bool BrowserWebContentsDelegate::ShouldOverrideNewWindowCreation(this,
// WebContents* source, RenderFrameHost* opener, WindowOpenDisposition, bool,
// mojom::CreateNewWindowParams*): asked (via
// WebContentsImpl::EdgeShouldCreateNewWindowAsync) once a page's new window is
// allowed and before it is made; true = handled, make no window.
inline constexpr uintptr_t
    BrowserWebContentsDelegate_ShouldOverrideNewWindowCreation = 0x9fb7960;
// WebContents* BrowserWebContentsDelegate::AddNewContents(this, WebContents*
// source, unique_ptr<WebContents>* new_contents, const GURL& target_url,
// WindowOpenDisposition, const WindowFeatures&, bool user_gesture, bool*
// was_blocked): puts a page's new tab or window into the browser.
inline constexpr uintptr_t BrowserWebContentsDelegate_AddNewContents =
    0x9fb5040;
// unique_ptr<NavigationThrottle> SplitWindowLinkModeNavigationThrottle::
// MaybeCreateThrottleFor(out, ...): Edge's own use of the "open left links to
// the right" toggle (the first pane's same-tab links load in the last pane).
inline constexpr uintptr_t SplitWindowLinkModeThrottle_MaybeCreate = 0x29d2d54;
// void SplitWindowControllerImplV2::UpdateLinkedModeState(this, bool on): after
// the toggle's bool is stored (SetLinkedModeStateForTab, 0xa020880), tells the
// split's observers, which wire Edge's two-pane link mode (readying a target
// pane: it navigated pane 1 of three to a new tab page).
inline constexpr uintptr_t SplitWindowControllerV2_UpdateLinkedModeState =
    0xa01ffc0;
// NavigationController::LoadURLParams(this, const GURL&) and its dtor.
inline constexpr uintptr_t LoadURLParams_ctor = 0x2e54bc0;
inline constexpr uintptr_t LoadURLParams_dtor = 0x2e548fe;
// NavigationControllerImpl::LoadURLWithParams(this, WeakPtr<NavigationHandle>*
// out, const LoadURLParams&).
inline constexpr uintptr_t NavigationControllerImpl_LoadURLWithParams =
    0x2e547f0;
// NavigationControllerImpl* WebContentsImpl::GetController(this).
inline constexpr uintptr_t WebContentsImpl_GetController = 0x1107b90;
// bool WebContentsImpl::HasOpener(this).
inline constexpr uintptr_t WebContentsImpl_HasOpener = 0x341d580;
// const GURL& WebContentsImpl::GetLastCommittedURL(this) (WebContents vtable
// +0x80).
inline constexpr uintptr_t WebContentsImpl_GetLastCommittedURL = 0x10ff1a0;
// base::internal::WeakReference::~WeakReference(this): a WeakPtr's first field.
inline constexpr uintptr_t WeakReference_dtor = 0x0f99f60;

// A new pane's tab offer (docs/re/split-offer.md):
// WebContents* chrome::AddAndReturnTabAt(BrowserWindowInterface*, const GURL&,
// uint32 extra transition flags, int index, bool foreground,
// optional<TabGroupId>*, bool pinned, optional<int>): a new tab at `index`,
// loading the URL; returns its contents.
inline constexpr uintptr_t Chrome_AddAndReturnTabAt = 0xc951100;
// GURL::GURL(this, const std::string_view*) and its dtor.
inline constexpr uintptr_t GURL_ctor_string = 0x0fcc3c2;
inline constexpr uintptr_t GURL_dtor = 0x0f94bca;
// chrome::kEdgeUISplitWindowStartingPageURL ("edge://split-window-starting-page"):
// the page Edge's split button shows in the new pane, offering the window's
// other tabs.
inline constexpr uintptr_t EdgeUISplitWindowStartingPageURL = 0x1216ed60;
// bool SplitWindowController::IsStartingPage(WebContents*, bool by_url).
inline constexpr uintptr_t SplitWindowController_IsStartingPage = 0xa023034;
// void SplitWindowControllerImplV2::OpenTabInCurrentSplitWindow(this,
// WebContents* chosen): the starting page's pick. It puts the chosen tab in
// place of the active split's LAST member (UpdateTabInSplit), where a
// two-pane split always has its starting page.
inline constexpr uintptr_t SplitWindowControllerV2_OpenTabInCurrentSplitWindow =
    0xa0213b0;
// void SplitWindowStartingUIHandler::GetTabs(this, GetTabsCallback*): the
// page's tab list, every tab of its window but those
// SplitWindowController::IsSplitWindowWebContents(WebContents*) claims (the
// contents of Edge's older split, marked by their user data).
inline constexpr uintptr_t SplitWindowStartingUIHandler_GetTabs = 0x9ab4950;
inline constexpr uintptr_t SplitWindowController_IsSplitWindowWebContents =
    0x29d5e22;
// void base::internal::Invoker<lambda at split_window_controller_impl_v2.cc:1313,
// WeakPtr<WebContents>>::RunOnce(BindStateBase*): the task OnSplitTabRemoved
// (0xa022106) posts when the removed split's last member is a starting page;
// it loads chrome::kChromeUINewTabURL there, replacing the entry.
inline constexpr uintptr_t StartingPageLeftSplitTask_RunOnce = 0xa022e40;
// void StartingPageNotifierImpl::OnTabClosing(this, WebContents*): tells every
// registered starting page that the tab is gone (Page::OnTabRemoved(tab id)).
inline constexpr uintptr_t StartingPageNotifierImpl_OnTabClosing = 0xa0290ec;
// void SplitWindowStartingUIHandler::CloseSplitWindow(this): the starting
// page's X. Edge then closes the split's LAST member too
// (SplitWindowControllerImplV2::HideSplitView(true), 0xa0201f0).
inline constexpr uintptr_t SplitWindowStartingUIHandler_CloseSplitWindow =
    0x9ab5750;

// A tab drag (docs/re/tab-drag.md):
// int TabDragController::ContinueDragging(this, const gfx::Point* screen DIPs):
// every pointer move of a started drag; it reorders the SELECTED tabs, or tears
// them off into a new window ~15 DIP below the strip. 0 = the drag lives on.
inline constexpr uintptr_t TabDragController_ContinueDragging = 0xa617de2;
// void TabDragController::EndDrag(this, EndDragReason); frees the controller.
inline constexpr uintptr_t TabDragController_EndDrag = 0xa616010;
// void EventTracker::OnEvent(this, const ui::Event&): the drag's key watcher.
// Escape cancels the drag; ANY other key press or release ends it in place.
inline constexpr uintptr_t EventTracker_OnEvent = 0xa61ed50;

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
inline constexpr uintptr_t ThemeService_DoSetTheme = 0x87d7f60;
// extensions::ExtensionRegistry* ExtensionRegistry::Get(BrowserContext*) and
// the Extension* lookup on its enabled set (ExtensionSet at registry + 0x28):
// const Extension* ExtensionSet::GetByID(this, const std::string& id).
inline constexpr uintptr_t ExtensionRegistry_Get = 0x108dae2;
inline constexpr uintptr_t ExtensionSet_GetByID = 0x106e7ce;
// void ThemeService::UseDefaultTheme(this): drop any extension theme. Used when
// a mode has no theme assigned.
inline constexpr uintptr_t ThemeService_UseDefaultTheme = 0x3171ba0;
// ThemeService* ThemeServiceFactory::GetForProfile(Profile*): the per-profile
// ThemeService (creates it if needed). Profile is a content::BrowserContext.
inline constexpr uintptr_t ThemeServiceFactory_GetForProfile = 0xf9b4e2;
// content::BrowserContext* WebContentsImpl::GetBrowserContext(this): the Profile
// behind any WebContents, our bridge from a pane to its ThemeService.
inline constexpr uintptr_t WebContentsImpl_GetBrowserContext = 0x1107ba0;

// The OS light/dark signal does NOT come through Edge. ui::NativeThemeObserver
// is a base::CheckedObserver (the observer list uses CheckedObserverAdapter), so
// a hand-rolled observer object would have to fake a weak-ptr factory. Instead
// the DLL already subclasses the browser window, so it watches that window's
// WM_SETTINGCHANGE with lParam == "ImmersiveColorSet" and reads the current mode
// straight from the OS (HKCU Themes\Personalize\AppsUseLightTheme, 0 = dark).
// No Edge theme-observer ABI is touched.

// Building the libc++ std::string id we hand RevertToExtensionTheme, into one
// reused DLL-lifetime buffer (so we never pair an operator delete ourselves):
// first clear it with the fill assign(size_type n, char c) at n = 0 (keeps any
// heap buffer), then append(const char* s) the NUL-terminated id. The *string*
// overloads of assign/append read a source string's own fields, not a raw
// pointer, so these two char-pointer entries are the ones to call.
inline constexpr uintptr_t BasicStringChar_assignFill = 0x1fb4b02;
inline constexpr uintptr_t BasicStringChar_append = 0x27d1ae6;
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
inline constexpr size_t ImageModel_size_bound = 256;
// MenuItemView's icon view. GetChildPreferredSize takes a row's height from
// this view ALONE (extra children only add width), so a taller row comes from
// a taller icon view.
inline constexpr uintptr_t MENUITEM_icon_view = 0x418;
// MenuItemView's views::LayoutDelegate base, right after its views::View base.
inline constexpr size_t MENUITEM_layout_delegate = View_size;
inline constexpr size_t EdgeResizeArea_size = 0x358;
// a valid id (⋯'s); grip overrides the tooltip
inline constexpr int ControlButtons_more_string_id = 0x4676;
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
// TabDragContext virtuals: Tab* GetTabForContents(this, WebContents*) and
// TabStripModel* GetTabStripModel(this).
inline constexpr uintptr_t DRAGCONTEXT_vtbl_GetTabForContents = 0x3a0;
inline constexpr uintptr_t DRAGCONTEXT_vtbl_GetTabStripModel = 0x3c0;

// --- views::View ---
// View* parent_ (null at the widget's root view)
inline constexpr uintptr_t VIEW_parent = 0x198;
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
// The out-param CalculateProposedLayout fills; its ctor zeroes 0x20 bytes.
inline constexpr uintptr_t PL_host_size = 0x00;  // gfx::Size {int w, int h}
// std::vector<ChildLayout> in libc++'s size-based layout {data, size,
// capacity}: emplace_back (0x8ad999a) writes at data + size * CL_stride.
inline constexpr uintptr_t PL_child_layouts = 0x08;
inline constexpr uintptr_t PL_child_count = 0x10;

// --- views::ChildLayout ---
// The element of PL_child_layouts; stride 0x30, from ApplyLayout.
inline constexpr uintptr_t CL_stride = 0x30;
inline constexpr uintptr_t CL_child = 0x00;    // views::View*
inline constexpr uintptr_t CL_visible = 0x08;  // bool
inline constexpr uintptr_t CL_bounds = 0x0c;   // gfx::Rect {int x, y, w, h}
// views::SizeBounds {SizeBound w{val,has}, h{val,has}}
inline constexpr uintptr_t CL_avail = 0x1c;

// extensions::ExtensionRegistry::enabled_extensions_, an ExtensionSet.
inline constexpr uintptr_t REGISTRY_enabled_set = 0x28;

// --- gfx::Rect field offsets within CL_bounds ---
inline constexpr uintptr_t RECT_x = 0x0;
inline constexpr uintptr_t RECT_y = 0x4;
inline constexpr uintptr_t RECT_w = 0x8;
inline constexpr uintptr_t RECT_h = 0xc;
}  // namespace off

}  // namespace fixedge::edge155
