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
inline constexpr int kVersionRevision = 62;
// TODO: the PE header's TimeDateStamp, as an extra guard (0 = not checked).
inline constexpr uint32_t kTimeDateStamp = 0;
// The preferred load address the PDB and the disassembly are written against.
inline constexpr uintptr_t kImageBase = 0x180000000;

// --- function RVAs (add to the msedge.dll base at runtime) ---
namespace rva {
inline constexpr uintptr_t MultiContentsView_ctor = 0x2d24796;
// cap: index < 2
inline constexpr uintptr_t MultiContentsView_SetWebContentsAtIndex = 0xc92c4e8;
// the `2` in cmp index,2
inline constexpr uintptr_t SetWebContentsAtIndex_guard_imm = 0xc92c4f2;
// loops the vector
inline constexpr uintptr_t MultiContentsView_ShowSplitView = 0xc92c5cc;
// CHECK(index < 2)
inline constexpr uintptr_t MultiContentsView_SetActiveIndex = 0xc92c94e;
// loops the vector
inline constexpr uintptr_t MultiContentsView_UpdateContentsBorderAndOverlay =
    0x2e41dda;
// 2 entries + 1 sep
inline constexpr uintptr_t MultiContentsView_CalculateProposedLayout =
    0x2b6f360;
// 2-element size array
inline constexpr uintptr_t MultiContentsView_GetViewSizes = 0x2b6f800;
inline constexpr uintptr_t MultiContentsView_CalculateSeparatorLayouts =
    0x2b6f906;
inline constexpr uintptr_t MultiContentsView_ClampToMinSize = 0x2b6f8c2;
// already N-ready
inline constexpr uintptr_t MultiContentsView_ExecuteOnEachVisible = 0x2bab654;
// FocusableViewsMap* GetFocusableViewsMapFor(this, ContentsContainerView*):
// a lookup in MCV+0x4c8, a flat_map with an entry per container Edge itself
// built (two), so null for ours. Its one caller,
// BrowserView::MaybeUpdateStoredFocusForWebContents (every tab switch),
// dereferences it.
inline constexpr uintptr_t MultiContentsView_GetFocusableViewsMapFor =
    0x2badad8;
inline constexpr uintptr_t EdgeMultiContentsResizeArea_ctor = 0x2d260b0;
inline constexpr uintptr_t ContentsContainerView_ctor = 0x2d26a32;
inline constexpr uintptr_t EdgeContentsContainerView_ctor = 0x2d261b0;
inline constexpr uintptr_t SplitWindowControllerImplV2_ctor = 0x31edb92;
inline constexpr uintptr_t IsEdgeSplitWindowEnabled = 0x0feda5d;
// The layout applier + the one append primitive the geometry uses. emplace_back
// grows the real std::vector<ChildLayout> with the real allocator, so calling it
// is the only safe way to add pane/separator entries from the injected DLL.
inline constexpr uintptr_t LayoutManagerBase_ApplyLayout = 0x100ca78;
// std::vector<ChildLayout>::emplace_back<View*, bool, const gfx::Rect&>
//   (rcx = &vector, rdx = View**, r8 = bool*, r9 = gfx::Rect*)
inline constexpr uintptr_t ChildLayouts_emplace_back = 0x27a8fda;

// --- pane construction (ctor.md) — all must run on the UI thread ---
// (size)->void*  View allocator
inline constexpr uintptr_t CheckedOperatorNew3 = 0x1058430;
// (parent, child, index)
inline constexpr uintptr_t AddChildViewAtImpl = 0x1053fda;
// (this, bool)  vtable+0x98
inline constexpr uintptr_t View_SetVisible = 0x10483f0;
inline constexpr uintptr_t View_GetVisible = 0x0fcc262;  // bool (this)
// (&vec@+0x368, raw_ptr*)
inline constexpr uintptr_t Containers_emplace_back = 0x2d25408;
// (p) BackupRef, gated
inline constexpr uintptr_t RawPtr_AcquireInternal = 0x0fc9ef0;
// (this) mark layout dirty
inline constexpr uintptr_t View_InvalidateLayout = 0x0feb7f0;
// (this) relayout now
inline constexpr uintptr_t View_LayoutImmediately = 0x100cc50;

// --- tab wiring (docs/re/tab-wiring.md): give a grown pane real web content ---
// (cwv, WebContents*)
inline constexpr uintptr_t ContentsWebView_SetWebContents = 0x2bac330;
// (WebContents*) force-paint a bg tab
inline constexpr uintptr_t WebContents_WasShown = 0x2a17d30;
// (tsm, int index) bounds-checked
inline constexpr uintptr_t TabStripModel_GetWebContentsAt = 0x105231e;

// --- the feed pipeline (controller.md) ---
// (BrowserView*) feeds panes
inline constexpr uintptr_t BrowserView_ShowSplitView = 0xc930f1a;
// void BrowserView::MaybeUpdateSplitView(this, WebContents* forContents): with
// the contents no longer in a split, runs MultiContentsView::CloseSplitView
// (clears pane 1, hides the resize area, moves content to container 0). The
// relayout a tab leaving a split would trigger, so a window is full-width right
// after a split is dissolved rather than on the user's next interaction.
inline constexpr uintptr_t BrowserView_MaybeUpdateSplitView = 0x2bace68;

// --- native split data-model grow (docs/re/split-insert.md, split-grow-impl.md) ---
// (size)->void*  (non-View new)
inline constexpr uintptr_t EdgeOperatorNew = 0x36ef380;
// operator delete(void*): jumps to Edge's free (the allocator shim).
inline constexpr uintptr_t EdgeOperatorDelete = 0x36ef2e0;
// split_tabs::LogSplitView{Created,Updated}UKM(TabStripModel*, const
// SplitTabId&): split metrics, each CHECK(ListTabs().size() == 2). Created runs
// from AddToSplitImpl, Updated from UpdateTabInSplitImpl and when a split
// member's page becomes primary (it navigates).
inline constexpr uintptr_t SplitTabs_LogSplitViewCreatedUKM = 0xa23e294;
inline constexpr uintptr_t SplitTabs_LogSplitViewUpdatedUKM = 0xa23e3e2;
// (out base::Token*) random 16B id
inline constexpr uintptr_t TokenCreateRandom = 0x1ed4ac0;
// ({layout,ratio=0.5})
inline constexpr uintptr_t SplitTabVisualData_ctor = 0x2d2521e;
// create 2-pane (caps at 2)
inline constexpr uintptr_t TabStripModel_AddToNewSplit = 0xa274686;
// N-generic add worker
inline constexpr uintptr_t TabStripModel_AddToSplitImpl = 0xa274b90;
// (tsm, anchorMember, newIdx, 0) add-to-split
inline constexpr uintptr_t TabStripModel_UpdateTabInSplit = 0xa273a2c;
inline constexpr uintptr_t TabStripModel_RemoveSplitImpl = 0xa26ecca;
inline constexpr uintptr_t GetTabsAndIndicesInSplit = 0xa26a7b0;
inline constexpr uintptr_t GetSplitForTab = 0x2b91c94;
// (tsm, &id)->SplitTabData*
inline constexpr uintptr_t GetSplitData = 0xa268504;
// (rootColl, int)->TabModel*
inline constexpr uintptr_t GetTabAtIndexRecursive = 0x0febd12;
// (rootColl, TabModel*)->int
inline constexpr uintptr_t GetIndexOfTabRecursive = 0x0feb5e8;
inline constexpr uintptr_t TabStripModel_GetActiveTab = 0x100c080;
inline constexpr uintptr_t TabStripModel_InsertDetachedTabAt = 0xa268916;
// (aura::Window*)->BrowserView*
inline constexpr uintptr_t BrowserView_ForNativeWindow = 0x1038160;
// (HWND)->aura::Window*
inline constexpr uintptr_t GetContentWindowForHWND = 0x26fd796;

// --- the split's 3-dot menu (extension point for directional "add pane") ---
inline constexpr uintptr_t SplitTabMenuModel_ctor = 0xa265cee;
inline constexpr uintptr_t SplitTabMenuModel_ExecuteCommand = 0xa266590;
inline constexpr uintptr_t SplitTabMenuModel_GetLabelForCommandId = 0xa266190;
// (model, cmd_id, &u16string)
inline constexpr uintptr_t SimpleMenuModel_AddItem = 0x10585d4;
// diag: the ⋯ split menu
inline constexpr uintptr_t WebUISplitTabsControl_HandleContextMenu = 0xa2d3d40;
inline constexpr uintptr_t UTF8ToUTF16 = 0x10591b0;  // (&out_u16, &string_view)

// --- collapse (docs/re/collapse.md): split button while split = keep active ---
// SplitWindowControllerImplV2::DoActionToSplitWindow(controller, WebContents*,
// int action), `action` one of off::SplitAction.
inline constexpr uintptr_t SplitWindowController_DoAction = 0xa021ab0;

// --- per-pane hover controls (the X / ⋯ bar, old-toolbar mode) ---
// EnsureControlButtons builds a ControlButtons bar for containers 0 and 1 only,
// and ShowSplitView registers hover on the FIRST and LAST member's WebContents
// only, so a middle pane has a bar but no hover and pane 3+ has hover but no bar.
// (this, BrowserView*, bool not_first)
inline constexpr uintptr_t ControlButtons_ctor = 0xa0114fa;
// ControlButtons::Button(this, const gfx::VectorIcon&): one round icon button
// of the bar (AddButton allocates it with ControlButtons_button_size bytes).
inline constexpr uintptr_t ControlButtons_Button_ctor = 0xa011b2a;
// Static: (WebContents* a, WebContents* b) -> CreateOrGet + StartObserving each
// (idempotent; a no-op under the new toolbar).
inline constexpr uintptr_t SplitWindow_ListenWebContentsInputEvent = 0xa0208de;
// What a pane's hover runs: show the bar of the container showing the given
// WebContents, hide the rest. (controller, WebContents*) -> bool. Controller
// from the Browser.
inline constexpr uintptr_t SplitWindowController_ShowControlButton = 0xa0218d0;
// (Browser*)
inline constexpr uintptr_t SplitWindowController_FromBrowser = 0x2e9d892;

// --- the drag grip in the hover bar (docs/re/drag-dock.md) ---
// AddButton(bar, int tooltip_string_id, const gfx::VectorIcon*, bool clickable)
// -> ControlButtons::Button*; clickable=false wires no click handler.
inline constexpr uintptr_t ControlButtons_AddButton = 0xa012246;
// InitLayout(bar): BoxLayout + preferred size {32 * visible children, 32}.
inline constexpr uintptr_t ControlButtons_InitLayout = 0xa01171c;
// (parent, child, size_t index)
inline constexpr uintptr_t View_ReorderChildView = 0x26ecf32;
// (parent, child) non-deleting
inline constexpr uintptr_t View_RemoveChildView = 0x26f00f8;
// static (src, dst, gfx::Point*) in place
inline constexpr uintptr_t View_ConvertPointToTarget = 0x1012b26;
// (view, const std::u16string&)
inline constexpr uintptr_t View_SetTooltipText = 0x100df50;
// views::Button's mouse handlers (vtable +0x118..+0x130 of every button,
// ControlButtons::Button included). A real press on a pane bar never reaches
// the window procedure as WM_LBUTTONDOWN, so the grip is driven from here.
// bool (button, const ui::MouseEvent&)
inline constexpr uintptr_t Button_OnMousePressed = 0xb2822b0;
// bool (button, const ui::MouseEvent&)
inline constexpr uintptr_t Button_OnMouseDragged = 0xb282340;
// void (button, const ui::MouseEvent&)
inline constexpr uintptr_t Button_OnMouseReleased = 0xb2824b0;
// void (button)
inline constexpr uintptr_t Button_OnMouseCaptureLost = 0xb2824d0;
// int ui::MouseEvent::GetClickCount(this): 2 on the second click of a double
// click (EdgeTwoDimensionalResizeArea resets its panes on that release).
inline constexpr uintptr_t MouseEvent_GetClickCount = 0xb62b9a8;
// bool views::View::IsMouseHovered(this): the cursor is over the view now.
inline constexpr uintptr_t View_IsMouseHovered = 0x28a9eb4;

// --- closing one pane of a grown split (docs/re/close-pane.md) ---
// Edge's X: ControlButtons::HandleButtonClick(bar, Button*). For pane 2+ it
// calls SplitWindowController::HideSplitView(true), which closes the split's
// LAST tab (not the clicked one), and closing any member dissolves the split.
inline constexpr uintptr_t ControlButtons_HandleButtonClick = 0xa012570;
// ControlButtons::ToggleContextMenu(bar, ...): the ⋯ of a pane's hover bar. It
// builds a SplitTabMenuModel that only knows "first pane or not", so the pane
// whose ⋯ was clicked is remembered here.
inline constexpr uintptr_t ControlButtons_ToggleContextMenu = 0xa012790;
// The bar's ⋯ menu: SplitWindowControlMenu. BuildMenuModel(this) builds a
// ui::SimpleMenuModel (kept at this+0x10, delegate this+8) and RETURNS it, and
// Run passes the return value to PopulateMenu(this, MenuItemView* root, model),
// which creates one item view per model item, so an item removed from the
// model first never shows. ExecuteCommand(this, int id, int flags) switches on
// off::ControlMenuCommand; IsCommandIdEnabled returns true above 5.
inline constexpr uintptr_t SplitWindowControlMenu_BuildMenuModel = 0xa018a4c;
inline constexpr uintptr_t SplitWindowControlMenu_PopulateMenu = 0xa0189b4;
inline constexpr uintptr_t SplitWindowControlMenu_ExecuteCommand = 0xa019150;
// ShouldExecuteCommandWithoutClosingMenu(this, int id, const ui::Event&) ->
// bool, `return id == 6`. Identical-code-folded: the SAME function serves
// other menus (e.g. EdgeDownloadsBtrItemContextMenu::IsItemForCommandIdDynamic),
// so a detour must pass every id but its own straight through.
inline constexpr uintptr_t SplitWindowControlMenu_ShouldExecuteWithoutClosing =
    0xa019540;
// gfx::VectorIcon data the ⋯ row's arrows reuse (Edge's own arrow icons).
inline constexpr uintptr_t Icon_ArrowUp = 0x120b2f20;    // kArrowUpIcon
inline constexpr uintptr_t Icon_ArrowDown = 0x120b2ee8;  // kArrowDownIcon
// ← and → are kArrowUpIcon turned (icons.h); these are the fallback.
inline constexpr uintptr_t Icon_ArrowBack = 0x12a55558;     // kArrowBackIcon
inline constexpr uintptr_t Icon_ArrowForward = 0x12a55620;  // kArrowForwardIcon
// The ⋯ row's own leading icon (Chromium's split-view icon).
inline constexpr uintptr_t Icon_SplitScene = 0x120b17d8;  // kSplitSceneIcon
// static ui::ImageModel FromVectorIcon(out, const VectorIcon&, ColorVariant
// (by value, 8 bytes), int size (0 = default), const VectorIcon* badge)
inline constexpr uintptr_t ImageModel_FromVectorIcon = 0x0ffd85c;
inline constexpr uintptr_t ImageModel_dtor = 0x0ffd702;
// ui::ColorVariant::ColorVariant(this, ui::ColorId)
inline constexpr uintptr_t ColorVariant_ctor = 0x109d690;
// (model, int id, const std::u16string& label, const ui::ImageModel& icon)
inline constexpr uintptr_t SimpleMenuModel_AddItemWithIcon = 0x2bb3890;
// views::View::View(this): a plain view (no layout manager).
inline constexpr uintptr_t View_ctor = 0x105a54a;
// (view, const std::optional<gfx::Size>*): {int width, int height, bool}.
inline constexpr uintptr_t View_SetPreferredSize = 0x2bd10f6;
// (view, const gfx::Rect&)
inline constexpr uintptr_t View_SetBoundsRect = 0x10121b4;
// gfx::Rect View::GetBoundsInScreen(this) (through the hidden out pointer),
// in screen DIPs.
inline constexpr uintptr_t View_GetBoundsInScreen = 0x28d0ef4;
// static View::ConvertPointFromScreen(const View* to, gfx::Point* in out):
// screen DIPs -> `to`'s coordinates.
inline constexpr uintptr_t View_ConvertPointFromScreen = 0x28aa022;
// gfx::Size View::GetPreferredSize(this, const SizeBounds&) (returned through
// the hidden out pointer); zeroed SizeBounds = unbounded.
inline constexpr uintptr_t View_GetPreferredSize = 0x0fdca00;
// ProposedLayout* MenuItemView::CalculateProposedLayout(LayoutDelegate* this,
// ProposedLayout* out, const SizeBounds&): every child's bounds in the row.
// `this` is the item's LayoutDelegate base (MENUITEM_layout_delegate). Edge
// centres the icon on a STANDARD row's height and pins other children to the
// row's top.
inline constexpr uintptr_t MenuItemView_CalculateProposedLayout = 0xb259920;
// MenuController* MenuController::GetActiveInstance(): the open menu's, or null.
inline constexpr uintptr_t MenuController_GetActiveInstance = 0xb25e34e;
// MenuController::Cancel(this, ExitType, int): as MenuRunnerImpl::Cancel calls
// it, with (MENU_exit_all, 0).
inline constexpr uintptr_t MenuController_Cancel = 0xb25e3a0;
// MenuItemView::ChildPreferredSizeChanged(this, View* child): drops the
// item's cached dimensions so it is measured again.
inline constexpr uintptr_t MenuItemView_ChildPreferredSizeChanged = 0xb256ea0;
// MenuItemView* MenuItemView::GetMenuItemByID(this, int id)
inline constexpr uintptr_t MenuItemView_GetMenuItemByID = 0xb2585e2;
inline constexpr uintptr_t SimpleMenuModel_AddSeparator = 0x319ef8a;
// size_t GetItemCount(model)
inline constexpr uintptr_t SimpleMenuModel_GetItemCount = 0x28e8b70;
// int GetCommandIdAt(model, size_t index)
inline constexpr uintptr_t SimpleMenuModel_GetCommandIdAt = 0x3466b90;
// void RemoveItemAt(model, size_t index)
inline constexpr uintptr_t SimpleMenuModel_RemoveItemAt = 0xb657a10;
// void SetLabel(model, size_t index, const std::u16string& label)
inline constexpr uintptr_t SimpleMenuModel_SetLabel = 0xb657c5e;
// (tsm, int index, uint32 closeTypes); HideSplitView passes 1
inline constexpr uintptr_t TabStripModel_CloseWebContentsAt = 0xa270676;
// (tsm, int index, const TabStripUserGestureDetails*)
inline constexpr uintptr_t TabStripModel_ActivateTabAt = 0xa26d1e2;
// int TabStripModel::GetIndexOfWebContents(this, WebContents*): -1 if absent.
inline constexpr uintptr_t TabStripModel_GetIndexOfWebContents = 0x10db914;
// int TabStripModel::MoveWebContentsAt(this, int index, int to_position,
// bool select_after_move): `to_position` is the tab's index after the move.
inline constexpr uintptr_t TabStripModel_MoveWebContentsAt = 0xa26d4ec;
// A split pane's links (docs/re/split-links.md):
// bool BrowserWebContentsDelegate::ShouldOverrideNewWindowCreation(this,
// WebContents* source, RenderFrameHost* opener, WindowOpenDisposition, bool,
// mojom::CreateNewWindowParams*): asked (via
// WebContentsImpl::EdgeShouldCreateNewWindowAsync) once a page's new window is
// allowed and before it is made; true = handled, make no window.
inline constexpr uintptr_t
    BrowserWebContentsDelegate_ShouldOverrideNewWindowCreation = 0x9fb83f0;
// WebContents* BrowserWebContentsDelegate::AddNewContents(this, WebContents*
// source, unique_ptr<WebContents>* new_contents, const GURL& target_url,
// WindowOpenDisposition, const WindowFeatures&, bool user_gesture, bool*
// was_blocked): puts a page's new tab or window into the browser.
inline constexpr uintptr_t BrowserWebContentsDelegate_AddNewContents =
    0x9fb5ad0;
// unique_ptr<NavigationThrottle> SplitWindowLinkModeNavigationThrottle::
// MaybeCreateThrottleFor(out, ...): Edge's own use of the "open left links to
// the right" toggle (the first pane's same-tab links load in the last pane).
inline constexpr uintptr_t SplitWindowLinkModeThrottle_MaybeCreate = 0x2a06224;
// void SplitWindowControllerImplV2::UpdateLinkedModeState(this, bool on): after
// the toggle's bool is stored (SetLinkedModeStateForTab, 0xa021310), tells the
// split's observers, which wire Edge's two-pane link mode (readying a target
// pane: it navigated pane 1 of three to a new tab page).
inline constexpr uintptr_t SplitWindowControllerV2_UpdateLinkedModeState =
    0xa020a50;
// NavigationController::LoadURLParams(this, const GURL&) and its dtor.
inline constexpr uintptr_t LoadURLParams_ctor = 0x2e03860;
inline constexpr uintptr_t LoadURLParams_dtor = 0x2e0359e;
// NavigationControllerImpl::LoadURLWithParams(this, WeakPtr<NavigationHandle>*
// out, const LoadURLParams&).
inline constexpr uintptr_t NavigationControllerImpl_LoadURLWithParams =
    0x2e03490;
// NavigationControllerImpl* WebContentsImpl::GetController(this).
inline constexpr uintptr_t WebContentsImpl_GetController = 0x113cad0;
// bool WebContentsImpl::HasOpener(this).
inline constexpr uintptr_t WebContentsImpl_HasOpener = 0x3454480;
// const GURL& WebContentsImpl::GetLastCommittedURL(this) (WebContents vtable
// +0x80).
inline constexpr uintptr_t WebContentsImpl_GetLastCommittedURL = 0x1134160;
// base::internal::WeakReference::~WeakReference(this): a WeakPtr's first field.
inline constexpr uintptr_t WeakReference_dtor = 0x0fcdbf0;

// A new pane's tab offer (docs/re/split-offer.md):
// WebContents* chrome::AddAndReturnTabAt(BrowserWindowInterface*, const GURL&,
// uint32 extra transition flags, int index, bool foreground,
// optional<TabGroupId>*, bool pinned, optional<int>): a new tab at `index`,
// loading the URL; returns its contents.
inline constexpr uintptr_t Chrome_AddAndReturnTabAt = 0xc94c6d0;
// GURL::GURL(this, const std::string_view*) and its dtor.
inline constexpr uintptr_t GURL_ctor_string = 0x1005a42;
inline constexpr uintptr_t GURL_dtor = 0x0fca40a;
// chrome::kEdgeUISplitWindowStartingPageURL ("edge://split-window-starting-page"):
// the page Edge's split button shows in the new pane, offering the window's
// other tabs.
inline constexpr uintptr_t EdgeUISplitWindowStartingPageURL = 0x121664e0;
// bool SplitWindowController::IsStartingPage(WebContents*, bool by_url).
inline constexpr uintptr_t SplitWindowController_IsStartingPage = 0xa023ac4;
// void SplitWindowControllerImplV2::OpenTabInCurrentSplitWindow(this,
// WebContents* chosen): the starting page's pick. It puts the chosen tab in
// place of the active split's LAST member (UpdateTabInSplit), where a
// two-pane split always has its starting page.
inline constexpr uintptr_t SplitWindowControllerV2_OpenTabInCurrentSplitWindow =
    0xa021e40;
// void SplitWindowStartingUIHandler::GetTabs(this, GetTabsCallback*): the
// page's tab list, every tab of its window but those
// SplitWindowController::IsSplitWindowWebContents(WebContents*) claims (the
// contents of Edge's older split, marked by their user data).
inline constexpr uintptr_t SplitWindowStartingUIHandler_GetTabs = 0x9ab5390;
inline constexpr uintptr_t SplitWindowController_IsSplitWindowWebContents =
    0x2a092f2;
// void base::internal::Invoker<lambda at split_window_controller_impl_v2.cc:1313,
// WeakPtr<WebContents>>::RunOnce(BindStateBase*): the task OnSplitTabRemoved
// (0xa022b96) posts when the removed split's last member is a starting page;
// it loads chrome::kChromeUINewTabURL there, replacing the entry.
inline constexpr uintptr_t StartingPageLeftSplitTask_RunOnce = 0xa0238d0;
// void StartingPageNotifierImpl::OnTabClosing(this, WebContents*): tells every
// registered starting page that the tab is gone (Page::OnTabRemoved(tab id)).
inline constexpr uintptr_t StartingPageNotifierImpl_OnTabClosing = 0xa029b7c;
// void SplitWindowStartingUIHandler::CloseSplitWindow(this): the starting
// page's X. Edge then closes the split's LAST member too
// (SplitWindowControllerImplV2::HideSplitView(true), 0xa020c80).
inline constexpr uintptr_t SplitWindowStartingUIHandler_CloseSplitWindow =
    0x9ab6190;

// A tab drag (docs/re/tab-drag.md):
// int TabDragController::ContinueDragging(this, const gfx::Point* screen DIPs):
// every pointer move of a started drag; it reorders the SELECTED tabs, or tears
// them off into a new window ~15 DIP below the strip. 0 = the drag lives on.
inline constexpr uintptr_t TabDragController_ContinueDragging = 0xa618b82;
// void TabDragController::EndDrag(this, EndDragReason); frees the controller.
inline constexpr uintptr_t TabDragController_EndDrag = 0xa616db0;
// void EventTracker::OnEvent(this, const ui::Event&): the drag's key watcher.
// Escape cancels the drag; ANY other key press or release ends it in place.
inline constexpr uintptr_t EventTracker_OnEvent = 0xa61faf0;

// --- the dividers between panes (docs/re/close-pane.md) ---
// EdgeMultiContentsResizeArea(this, MultiContentsView*): the 2-pane split's
// resize handle (8x8 preferred, hover pill; vertical when taller than wide).
// MultiContentsView's ctor allocates it with 0x358 bytes.
// views::EdgeTwoDimensionalResizeArea's own mouse handlers (overriding
// views::ResizeArea's): bool (view, const ui::MouseEvent&) for press / drag,
// void (view, const ui::MouseEvent&) for release, void (view) for capture lost.
inline constexpr uintptr_t ResizeArea2D_OnMousePressed = 0xb1f4f80;
inline constexpr uintptr_t ResizeArea2D_OnMouseDragged = 0xb1f5010;
inline constexpr uintptr_t ResizeArea2D_OnMouseReleased = 0xb1f5050;
inline constexpr uintptr_t ResizeArea2D_OnMouseCaptureLost = 0xb1f50b0;

// --- 2-pane-only CHECK sites that int3 when a split grows past 2 members ---
// Accessible-label builder: maps a split tab's pane index to a string id
// (0x3e62 pane0 / 0x3e63 pane1) and `cmp eax,1; jne <int3>` crashes at index>=2.
// `jne 0x42fbb16` (0F 85 rel32, 6 bytes); NOP it so index>=2 falls through to
// the pane-1 label instead of crashing. a11y text for pane 3+ is cosmetic.
inline constexpr uintptr_t SplitA11yLabel_paneidx_jne = 0x42fb7da;
// MultiContentsView::SetActiveIndex's `cmp edx,2; jae <int3>` (0F 83 rel32, 6
// bytes). The detour replaces the function while loaded; NOPing the CHECK too
// means a click on pane 3+ after unload only mis-flags the primary pane.
inline constexpr uintptr_t SetActiveIndex_index_jae = 0xc92c958;

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
    0x1090a66;

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
inline constexpr uintptr_t ThemeService_DoSetTheme = 0x87dffa0;
// extensions::ExtensionRegistry* ExtensionRegistry::Get(BrowserContext*) and
// the Extension* lookup on either of its sets (off::REGISTRY_*_set):
// const Extension* ExtensionSet::GetByID(this, const std::string& id).
inline constexpr uintptr_t ExtensionRegistry_Get = 0x10b9a92;
inline constexpr uintptr_t ExtensionSet_GetByID = 0x10a276c;
// A theme the panel installs through the external-extension loader arrives
// DISABLED (disable reason "external extension") until the user accepts Edge's
// "extension added" prompt, and Edge also switches a theme off when another
// becomes active; a disabled theme never paints. Before a theme is applied the
// DLL turns it on with the call the prompt's Enable button makes.
// ExtensionRegistrar* ExtensionRegistrar::Get(BrowserContext*)
inline constexpr uintptr_t ExtensionRegistrar_Get = 0x2bf4380;
// void ExtensionRegistrar::EnableExtension(this, const std::string& id): moves
// the extension from the disabled set to the enabled one. It CHECKs that the
// extension is in the disabled set, so only call it for one found there.
inline constexpr uintptr_t ExtensionRegistrar_EnableExtension = 0x29cde5e;
// A theme once removed from Edge's extensions page is recorded in the profile's
// "external uninstalls" and the external-extension loader never installs it
// again, however often its registry entry is read. ExtensionPrefs::Get takes the
// BrowserContext; ClearExternalExtensionUninstalled(this, const std::string& id)
// drops the record. ExternalProviderManager::Get(BrowserContext*) reaches the
// manager whose CheckForExternalUpdates(this) revisits every registered external
// extension, now past the cleared record, and installs it.
inline constexpr uintptr_t ExtensionPrefs_Get = 0x282aa16;
inline constexpr uintptr_t ExtensionPrefs_ClearExternalExtensionUninstalled =
    0x7e9a3f8;
inline constexpr uintptr_t ExternalProviderManager_Get = 0x2bfdba8;
inline constexpr uintptr_t ExternalProviderManager_CheckForExternalUpdates =
    0x2bf306e;
// void ThemeService::UseDefaultTheme(this): drop any extension theme. Used when
// a mode has no theme assigned.
inline constexpr uintptr_t ThemeService_UseDefaultTheme = 0x319a4d0;
// void ThemeService::RemoveUnusedThemes(this): Chromium keeps only the one
// active theme extension and unloads every other theme from the profile, so the
// two themes a user assigns to light and dark modes cannot both stay installed —
// the moment one is applied the other is garbage-collected, and the next flip
// finds it gone. The detour no-ops it while theming is active (hooks.h), so both
// assigned themes remain installed and either mode's flip switches instantly.
// Takes the primary `this` (rcx), reads large member offsets (+0x728, +0x6b8).
inline constexpr uintptr_t ThemeService_RemoveUnusedThemes = 0x33507f0;
// ThemeService* ThemeServiceFactory::GetForProfile(Profile*): the per-profile
// ThemeService (creates it if needed). Profile is a content::BrowserContext.
inline constexpr uintptr_t ThemeServiceFactory_GetForProfile = 0xfcf172;
// content::BrowserContext* WebContentsImpl::GetBrowserContext(this): the Profile
// behind any WebContents, our bridge from a pane to its ThemeService.
inline constexpr uintptr_t WebContentsImpl_GetBrowserContext = 0x113cae0;

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
inline constexpr uintptr_t EdgeComputePreferredColorScheme = 0x548333b;
// void WebContentsImpl::OnColorProviderChanged(this): `this` is the contents'
// ColorProviderSourceObserver subobject (off::WEBCONTENTS_color_observer). It
// tells the contents' observers, which makes a WebUI page fetch its colors.css
// again, then recomputes the web preferences (prefers-color-scheme).
inline constexpr uintptr_t WebContentsImpl_OnColorProviderChanged = 0x2f44c20;
// void NavigationControllerImpl::Reload(this, ReloadType, bool
// check_for_repost): the controller is embedded in the WebContentsImpl at
// off::WEBCONTENTS_controller.
inline constexpr uintptr_t NavigationControllerImpl_Reload = 0x78c1a80;
// ColorProvider* WebContentsImpl::GetColorProvider(this): the provider the
// WebUI's colors.css is generated from (ThemeSource::SendColorsCss reads it
// through the contents' vtable). It asks the contents' color provider source,
// the browser window, which builds its key through ThemeService below on every
// call, so a detour on that key decides the mode for the one call.
inline constexpr uintptr_t WebContentsImpl_GetColorProvider = 0x33c2dc0;
// ColorProviderKey* ThemeService::GetColorProviderKey(this, ColorProviderKey*
// out, const ColorProviderKey& nativeKey, Profile*): fills the key the browser's
// color provider is built from and returns `out`. Its mode starts as the OS's
// and is replaced only when the Appearance setting says Light or Dark.
inline constexpr uintptr_t ThemeService_GetColorProviderKey = 0xfcf25c;
// bool BrowserThemePack::GetColor(this, int id, SkColor* out): a theme
// extension's color for `id`, false when the theme did not set it. Its colors
// are a table of 8-byte {int id, SkColor} entries.
inline constexpr uintptr_t BrowserThemePack_GetColor = 0x87d5030;
// SkColor ui::ColorProvider::GetColor(this, ui::ColorId id): the browser-wide
// colour read every view resolves an id through (a std::map lookup keyed on the
// id, returning the mixer-computed SkColor). The vertical-tab SELECTED outline
// (the "spine") is a native ui::ColorId computed by a tab-strip colour mixer --
// NOT a theme-pack id, so BrowserThemePack::GetColor cannot reach it -- and is
// read here at paint. Signature: SkColor __fastcall(void* this, int id).
inline constexpr uintptr_t ColorProvider_GetColor = 0xffd9ce;

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
inline constexpr uintptr_t BasicStringChar_assignFill = 0x1ff42f2;
inline constexpr uintptr_t BasicStringChar_append = 0x27f9326;

// --- web protocol handlers (docs/re/protocol-handlers.md) ---
// A site's registerProtocolHandler without a user gesture lights the omnibox
// handler icon; its bubble (Allow / Deny / Ignore, Ignore preselected) commits
// the selected radio from ContentSettingBubbleContents::WindowClosing, so ANY
// close - Esc, the X, a click elsewhere - commits Ignore and writes the ignored
// list. void ContentSettingRPHBubbleModel::CommitChanges(this): performs the
// selected radio's action, then clears the pending handler (hides the icon).
inline constexpr uintptr_t ContentSettingRPHBubbleModel_CommitChanges =
    0xa209f70;
// The bubble model's vftable. Its Done button is the dialog's accept callback,
// bound to the model's virtual at off::BubbleModel_OnDoneButtonClicked_slot
// (a `vcall'{56}` thunk); for this model that slot holds the base class's empty
// virtual below, so it is the one place that sees Done as distinct from every
// other way the bubble closes.
inline constexpr uintptr_t ContentSettingRPHBubbleModel_vftable = 0x11234b90;
// The shared empty virtual (`ret`) the slot holds before we point it at ours.
inline constexpr uintptr_t ContentSettingBubbleModel_EmptyVirtual = 0x5469a10;
// void RegisterProtocolHandlerPermissionRequest::PermissionDecided(this,
// const Decision* decision): Edge's permission prompt (the user-gesture path).
// With a decision, 0 (Allow) accepts and EVERY other value ignores - including
// Edge's "Just this time", which Chromium's prompt never offers for handlers.
// No decision (the prompt dismissed) does nothing. Decision layout:
// off::PERMISSION_DECISION_*.
inline constexpr uintptr_t
    RegisterProtocolHandlerPermissionRequest_PermissionDecided = 0xbda8300;
// Lifting the blocks: once a handler is ignored, Edge drops every later request
// from it, so the icon never returns. While the feature turns on, the DLL
// lifts each one with Edge's own call (the bubble's Allow path makes it too).
// ProtocolHandlerRegistry* ProtocolHandlerRegistryFactory::GetForBrowserContext(
// BrowserContext*): the profile's registry (GetServiceForBrowserContext, create).
inline constexpr uintptr_t ProtocolHandlerRegistryFactory_GetForBrowserContext =
    0x29f62b6;
// void ProtocolHandlerRegistry::RemoveIgnoredHandler(this, const
// ProtocolHandler& handler, bool save): erases the handler from the user's
// ignored list and, unless policy also ignores it, from the live ignored list
// (off::RPH_REGISTRY_ignored_*), then notifies. `save` (Edge's addition; the
// bubble passes 1) writes the prefs. Its second erase compares against
// `handler` while shifting the very vector it erases from, so `handler` must
// be a copy, never an element of that list.
inline constexpr uintptr_t ProtocolHandlerRegistry_RemoveIgnoredHandler =
    0xbdaa87c;
// ProtocolHandler::ProtocolHandler(this, const ProtocolHandler& other): the
// copy constructor (what vector<ProtocolHandler>'s copy, used by
// GetIgnoredHandlers, calls per element). The other ProtocolHandler ctor,
// 0xbdabf18, is the by-field one.
inline constexpr uintptr_t ProtocolHandler_CopyConstructor = 0x2cb1804;
// ProtocolHandler::~ProtocolHandler(this): frees its strings and GURL.
inline constexpr uintptr_t ProtocolHandler_Destructor = 0x2d97898;

// --- the browser assistant's toolbar button ---
// The button's vftables (one per base; EdgeCopilotToolbarLabelButton has
// multiple inheritance). A view is a chat button when its object+0 vptr is any
// of these, so the recognition does not depend on which base is primary.
inline constexpr uintptr_t CopilotButton_vftables[] = {
    0x113634e0, 0x11363500, 0x11363520, 0x11363540, 0x11363580, 0x113635d0,
    0x113635e8, 0x11363600, 0x11363630, 0x113636e0, 0x11363700, 0x11363740,
    0x11363758, 0x11363770, 0x113637a0, 0x113637b8, 0x115b6220, 0x1173fd10};
// EdgeCopilotDropdownMenuButton: the chevron beside the chat pill (a sibling in
// EdgeCopilotToolbarSplitButtonContainer). Its constructor sets both its tooltip
// and its accessible name from the formatted string "$1 menu"; the vftables
// are matched like the chat button's.
inline constexpr uintptr_t CopilotDropdownButton_vftables[] = {
    0x113c0d60, 0x113c0d80, 0x113c0da0, 0x113c0dc0, 0x113c0e00,
    0x113c0e50, 0x113c0e68, 0x113c0e80, 0x113c0eb0, 0x113c0f60,
    0x113c0f80, 0x113c0fc0, 0x113c0fd8, 0x1176de70};
// bool IsConsumerCopilotLabelButtonWithDropdownEnabled(bool): the one question
// EdgeToolbarView::CreateDiscoverButtonIfNecessary asks (through
// IsCopilotLabeledButtonWithDropdownEnabled) before building the chevron's
// split-button container for a personal profile; false builds the bare chat
// button. See docs/re/removed-gemini-integration.md section 12.
inline constexpr uintptr_t Copilot_IsConsumerLabelButtonWithDropdownEnabled =
    0x3221438;
// void EdgeToolbarView::CreateDiscoverButtonIfNecessary(this = the toolbar's
// secondary base, EDGETOOLBAR_discoverBase into the toolbar): builds the chat
// button, inside a split-button container with its chevron when
// IsConsumerCopilotLabelButtonWithDropdownEnabled says so. It does not check
// for a button already built, and stores the new one in EDGETOOLBAR_chatButton.
inline constexpr uintptr_t EdgeToolbarView_CreateDiscoverButtonIfNecessary =
    0x3221240;
// views::WebView: (this, BrowserContext*); LoadInitialURL(this, const GURL&,
// HttpsUpgradePolicy, base::Location* by value); the scalar deleting dtor
// (this, 1).
inline constexpr uintptr_t WebView_ctor = 0x2d27f6e;
inline constexpr uintptr_t WebView_LoadInitialURL = 0xc3fe140;
inline constexpr uintptr_t WebView_ScalarDeletingDtor = 0x34208b0;
// --- running a script in a page, and reading a tab ---
// RenderFrameHost* WebContentsImpl::GetPrimaryMainFrame(this).
inline constexpr uintptr_t WebContentsImpl_GetPrimaryMainFrame = 0x113caf0;
// WebContentsImpl::ForEachRenderFrameHost(this, FunctionRef<void(RenderFrameHost*)>*):
// every frame of a page, main frame first. A FunctionRef is {object, invoker}:
// the invoker is called (object, frame). RenderFrameHostImpl::GetLastCommittedURL
// (frame) is the frame's GURL.
inline constexpr uintptr_t WebContentsImpl_ForEachRenderFrameHost = 0x274b980;
inline constexpr uintptr_t RenderFrameHostImpl_GetLastCommittedURL = 0x113c6c0;
// const std::string& GURL::spec(this).
inline constexpr uintptr_t GURL_spec = 0xfd4aca;
// bool RenderFrameHostImpl::IsRenderFrameLive(this): false for a discarded tab.
inline constexpr uintptr_t RenderFrameHostImpl_IsRenderFrameLive = 0x1135050;
// void RenderFrameHostImpl::ExecuteJavaScriptInIsolatedWorld(this,
// const std::u16string*, JavaScriptResultCallback* (a OnceCallback<void(Value)>,
// moved from), int worldId). The main-world ExecuteJavaScript CHECKs
// CanExecuteJavaScript, which a web page fails, so this is the one to call.
inline constexpr uintptr_t
    RenderFrameHostImpl_ExecuteJavaScriptInIsolatedWorld = 0x78daf80;
// BindStateBase::BindStateBase(this, polymorphic_invoke, destructor): refcount 1
// at +0, invoke at +8, destructor at +0x10, the default cancellation query at
// +0x18; a BindState's bound state starts at BINDSTATE_first_bound_argument.
inline constexpr uintptr_t BindStateBase_ctor = 0xfca4b0;
// The native find bar (docs/re/find-bar.md). FindBarView is the bar's content
// view; its text field's edits end in Find(this, const u16string_view*), and
// Enter and the arrow buttons in FindNext(this, bool reverse), which reads the
// text field itself. Both go on to FindTabHelper::StartFinding.
inline constexpr uintptr_t FindBarView_Find = 0xa372f80;
inline constexpr uintptr_t FindBarView_FindNext = 0xa371930;
// bool HandleKeyEvent(this = the FindBarView's TextfieldController subobject,
// Textfield*, const ui::KeyEvent&): Enter starts the search itself (FindNext is
// inlined into it), so Enter needs its own hook.
inline constexpr uintptr_t FindBarView_HandleKeyEvent = 0xa372a90;
// void UpdateForResult(this, const FindNotificationDetails&, const
// std::u16string& findText): the "n/m" label and the buttons' enabled state.
inline constexpr uintptr_t FindBarView_UpdateForResult = 0xa371cd4;
// void EndFindSession(this, ?, bool): the bar is closing (Esc, the X, a click
// into the page); FindBarView::EndFindSession ends in it.
inline constexpr uintptr_t FindBarController_EndFindSession = 0xa068e66;
// Copilot's side panel, the path the page's "Ask Copilot" menu uses.
// edge_copilot::utils::SendPageContextualPromptToChat(BrowserWindowInterface*
// browser, const std::string& pageText, <unused>, const std::string& prompt,
// bool): builds an AskCopilotRequest (the prompt, plus the page text as a "text"
// attachment) and dispatches it to the side panel, opening it. pageText must not
// be empty or it takes another path.
inline constexpr uintptr_t CopilotUtils_SendPageContextualPromptToChat =
    0x93dc691;
// static content::WebContentsImpl::GetAllWebContents(out): hidden return pointer
// in rcx. What it fills is not a begin/end vector: its first word points at the
// WebContents pointers and its second is their count. The panel is the one whose
// URL is edge://discover-chat-v2/ (the "iframeless" panel has no host manager:
// CopilotSidePanelController::GetHostManagerForBrowser answers null for it).
inline constexpr uintptr_t WebContentsImpl_GetAllWebContents = 0x28282c6;
// views::Textfield::GetText(this, std::u16string_view* out): fills `out`.
inline constexpr uintptr_t Textfield_GetText = 0xfec056;
// The bar's options (the "..." button): an inline FindOptionsControlView, a
// column of FindInPageOptionToggleViews (Match case, Whole word, Match
// diacritics), each a title label and an EdgeToggleButton. See
// docs/re/find-bar.md.
// void EdgeMaybeAddFindOptionsView(this = FindBarView, bool): builds the options
// view the first time the bar needs it.
inline constexpr uintptr_t FindBarView_EdgeMaybeAddFindOptionsView = 0xa3749ce;
// void EdgeToggleFindOptions(this = FindBarView, ...): shows or hides the view.
inline constexpr uintptr_t FindBarView_EdgeToggleFindOptions = 0xa37472c;
// void PreferredSizeChanged(this = FindBarView): the bar re-measures itself.
inline constexpr uintptr_t FindBarView_PreferredSizeChanged = 0xa373c60;
// void AddFindInPageOptionChildView(this = FindOptionsControlView, int id, int
// width, FindBarHost*): builds toggle `id` at the end of the column (one per
// id; Edge builds 0..2 and its title text for any other id is empty).
inline constexpr uintptr_t FindOptionsControlView_AddFindInPageOptionChildView =
    0xc8b4ee6;
// void OnButtonToggled(this = FindInPageOptionToggleView): the toggle button's
// click callback; it flips Edge's own state for the id and re-runs the search.
inline constexpr uintptr_t FindInPageOptionToggleView_OnButtonToggled =
    0xc8b5a10;
// bool views::EdgeToggleButton::GetIsOn(this) and void SetIsOn(this, bool).
inline constexpr uintptr_t EdgeToggleButton_GetIsOn = 0xb1f5c16;
inline constexpr uintptr_t EdgeToggleButton_SetIsOn = 0xb1f5c44;
// void views::View::SetEnabled(this, bool).
inline constexpr uintptr_t View_SetEnabled = 0x1070896;
// void views::Label::SetText(this, const std::u16string_view*).
inline constexpr uintptr_t Label_SetText = 0x1058090;
}  // namespace rva

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

// --- ThemeService ---
// The active theme's supplier, a scoped_refptr to a BrowserThemePack for an
// extension or colored theme and null for the default theme. SwapThemeSupplier
// (primary `this`) is what writes it; ThemeService::GetThemeSupplier reads the
// same field as +0x760 because it is a method of the base at +8.
inline constexpr uintptr_t THEMESERVICE_supplier = 0x768;
// BrowserThemePack colour-table ids: the id space GetColor is keyed on. The
// manifest-key -> id map is a {const char* key; int id} array in msedge.dll.
// Edge keeps a SEPARATE *tints* table with its own enum (frame is 0x1c there),
// so a tint id handed to GetColor always misses — do not cross the two.
inline constexpr int THEMECOLOR_frame = 0x02;  // frame colour
inline constexpr int THEMECOLOR_tab_text =
    0x12;  // active-tab text (cyan accent)
inline constexpr int THEMECOLOR_toolbar = 0x17;  // toolbar colour (dark ground)
// Edge's vertical-tab strip colour ids. Edge has no manifest key for them:
// BrowserThemePack::EdgeSetVTabAndRelatedColor (rva 0x87d42fe) DERIVES all of
// them from the toolbar colour (it reads THEMECOLOR_toolbar/0x17, then stores a
// contiguous run 0x23-0x2c) and GetColor reads them back when the colour mixer
// builds the vertical-tab views' ColorProvider. That is why a theme can tint the
// rail (it follows the toolbar) but cannot give the SELECTED tab its own colour -
// every one is just a shade of the toolbar. The ten ids pair as active-window /
// inactive-window; the DLL overrides only the selected tab so it adopts the
// theme's own accent (docs/re/theming.md, hooks.h h_getcolor). Verified from the
// store sequence in EdgeSetVTabAndRelatedColor.
inline constexpr int VTABCOLOR_background = 0x23;  // rail bg (+0x24 inactive)
inline constexpr int VTABCOLOR_background_alt =
    0x27;  // idle fill (+0x28 inact.)
inline constexpr int VTABCOLOR_selected_background = 0x25;  // selected tab bg
inline constexpr int VTABCOLOR_selected_background_inactive = 0x26;
inline constexpr int VTABCOLOR_foreground = 0x29;  // idle text (+0x2a inactive)
inline constexpr int VTABCOLOR_selected_foreground = 0x2b;  // selected text
inline constexpr int VTABCOLOR_selected_foreground_inactive = 0x2c;
// Native ui::ColorId values (ui::ColorProvider's id space, DISTINCT from the
// BrowserThemePack colour-table ids above). The SELECTED vertical tab's OUTLINE
// ("spine"), active- and inactive-window variants -- read through
// ColorProvider::GetColor at paint, not from the theme pack. Verified from Edge's
// colour-id name table (kColorEdgeVTabOutlineSelected{Active,Inactive}).
inline constexpr int VTABUICOLOR_outline_selected_active = 0xc19;    // 3097
inline constexpr int VTABUICOLOR_outline_selected_inactive = 0xc1a;  // 3098
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
// ui::ColorProviderKey: its first field is the ColorMode (read first by
// ColorProviderSource::GetColorMode), 0 = light, 1 = dark.
// The pair EdgeComputePreferredColorScheme fills.
inline constexpr uintptr_t PREFERREDSCHEME_color_scheme = 0x0;
inline constexpr uintptr_t PREFERREDSCHEME_scrollbar_scheme = 0x4;
inline constexpr uintptr_t COLORPROVIDERKEY_color_mode = 0x0;
inline constexpr int32_t COLORMODE_light = 0;
inline constexpr int32_t COLORMODE_dark = 1;

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
inline constexpr int ControlButtons_more_string_id = 0x4659;
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
// The out-param CalculateProposedLayout fills; its ctor zeroes 0x20 bytes.
inline constexpr uintptr_t PL_host_size = 0x00;  // gfx::Size {int w, int h}
// std::vector<ChildLayout> in libc++'s size-based layout {data, size,
// capacity}: emplace_back (0x8ae18da) writes at data + size * CL_stride.
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
// sizeof(views::WebView): every `new views::WebView` asks for 0x540.
inline constexpr size_t WebView_size = 0x540;
// sizeof(base::Location) {function, file, line, program counter}, which
// LoadInitialURL takes by value (by address under the x64 ABI).
inline constexpr size_t Location_size = 0x20;
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
// 1-based ordinal (-1 = unknown) and whether this is the final update, then a ref-counted pointer (null here) that the copy constructor
// reads, so the whole 0x28 must be ours.
inline constexpr size_t FINDDETAILS_size = 0x28;
inline constexpr uintptr_t FINDDETAILS_request_id = 0x00;
inline constexpr uintptr_t FINDDETAILS_matches = 0x04;
inline constexpr uintptr_t FINDDETAILS_active_ordinal = 0x18;
inline constexpr uintptr_t FINDDETAILS_is_final = 0x1c;
}  // namespace off

}  // namespace fixedge::edge155
