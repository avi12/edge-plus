using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Windows.ApplicationModel.DataTransfer;
using Windows.Graphics;
using Windows.Storage;
using Windows.Storage.Pickers;

namespace EdgePlus;

// The panel's one window: a card per feature, in the Tauri panel's order, and
// the notices above them. It renders whatever PanelState the last action
// returned and never touches settings, Edge or the DLL itself (PanelActions
// does). The Tauri panel's App.svelte.
public sealed partial class MainWindow : Window {
  // In device-independent pixels, the Tauri window's sizes: the width is fixed
  // and only the height is the user's.
  private const int WindowWidth = 560;
  private const int WindowHeight = 640;
  private const int MinimumHeight = 420;
  private const double BaseDpi = 96.0;
  private const double VeilShown = 1;
  private const double VeilHidden = 0;
  private const string AnyFile = "*";
  private static readonly string[] s_themeFileTypes = [".crx", ".zip"];

  // The plain switches, top to bottom; the theming card sits second and the
  // start-at-sign-in card last.
  private static readonly Feature[] s_switchCardsAfterTheming = [
    Feature.WebUiFollowsTheme, Feature.TabSpine, Feature.EnhancedFind, Feature.LivePreview,
    Feature.QuietDevMode, Feature.ProtocolHandlers,
  ];

  private readonly Dictionary<Feature, FeatureCard> _switchCards = [];
  private readonly ThemingCard _themingCard;
  private readonly FeatureCard _startupCard = new(Wording.StartWithWindows);
  private readonly EdgeWatch _edgeWatch = new();
  private readonly ThemeWatch _themeWatch = new();
  private PanelState? _state;
  // What the last Edge update switched off; replaces the standing held-off
  // sentence it would only repeat.
  private string _updateNotice = "";

  public MainWindow() {
    InitializeComponent();
    WindowChrome.Apply(this, AppTitleBar);
    FixWidth();
    WindowChrome.CenterOnScreen(AppWindow);
    OpenEdgeHint.Text = Wording.OpenEdgeHint;
    DropHint.Text = Wording.DropHint;

    _themingCard = new ThemingCard(RunAsync, PickLocalThemeAsync);
    AddSwitchCard(Feature.NativeSplit);
    CardList.Children.Add(_themingCard.View);
    foreach (Feature feature in s_switchCardsAfterTheming) {
      AddSwitchCard(feature);
    }
    _startupCard.Toggled += isOn => _ = RunAndReportAsync(
        () => PanelActions.SetStartWithWindows(isOn));
    CardList.Children.Add(_startupCard.View);
    // The cards under the theming card glide as its list grows or shrinks.
    Motion.Animate(CardList, Motion.Reposition);
    // Turning Windows' "Animation effects" off or on applies at once.
    WindowSubclass.SettingChanged += Motion.Refresh;

    // Coming back to the panel picks up themes installed in Edge meanwhile.
    Activated += (_, e) => {
      if (e.WindowActivationState != WindowActivationState.Deactivated) {
        _ = RunAsync(PanelActions.Load);
      }
    };
    // Edge came up on a build not seen before; features that depend on the old
    // msedge.dll may have been switched off.
    _edgeWatch.BuildChanged += change => DispatcherQueue.TryEnqueue(() => {
      _updateNotice = Wording.DescribeUpdate(change);
      _ = RunAsync(PanelActions.Load);
    });
    // A local theme edited on disk was re-checked and re-installed (or refused).
    _themeWatch.Refreshed += refresh => DispatcherQueue.TryEnqueue(
        () => ShowActionNotice(refresh.Error ?? ""));
    _edgeWatch.Start();
    _themeWatch.Start();
    _ = RunAsync(PanelActions.Load);
  }

  // Launching Edge+ again brings the running panel back.
  public void BringBack() {
    if (AppWindow.Presenter is OverlappedPresenter presenter) {
      presenter.Restore();
    }
    Activate();
  }

  public void Minimize() {
    if (AppWindow.Presenter is OverlappedPresenter presenter) {
      presenter.Minimize();
    }
  }

  private void AddSwitchCard(Feature feature) {
    var card = new FeatureCard(Wording.For(feature));
    card.Toggled += isOn => _ = RunAndReportAsync(() => PanelActions.SetFeature(feature, isOn));
    _switchCards[feature] = card;
    CardList.Children.Add(card.View);
  }

  // Run a panel action off the UI thread (an injection can wait on Edge) and
  // render the state it returns. Returns why it failed, or null — the
  // action's own sentence, as the Tauri panel hands its errors to the UI.
  private async Task<string?> RunAsync(Func<PanelState> action) {
    try {
      Render(await Task.Run(action));
      return null;
    } catch (Exception error) {
      if (_state is not null) {
        Render(_state);
      }
      return error.Message;
    }
  }

  private async Task RunAndReportAsync(Func<PanelState> action) {
    string? error = await RunAsync(action);
    if (error is not null) {
      ShowActionNotice(error);
    }
  }

  private void Render(PanelState state) {
    _state = state;
    OpenEdgeHint.Visibility = state.IsEdgeRunning ? Visibility.Collapsed : Visibility.Visible;
    string buildNotice = _updateNotice.Length > 0
        ? _updateNotice
        : Wording.DescribeUnsupported(state);
    BuildNotice.Message = buildNotice;
    BuildNotice.IsOpen = buildNotice.Length > 0;

    foreach ((Feature feature, FeatureCard card) in _switchCards) {
      bool isHeldOff = state.IsHeldOff(feature);
      card.Update(state.Features.IsOn(feature), isHeldOff, HeldOffNote(state, isHeldOff));
    }
    bool isThemingHeldOff = state.IsHeldOff(Feature.Theming);
    _themingCard.Update(state, isThemingHeldOff, HeldOffNote(state, isThemingHeldOff));
    _startupCard.Update(state.IsStartWithWindows, isLocked: false, note: "");
  }

  // What a locked card says about why, on the card itself.
  private static string HeldOffNote(PanelState state, bool isHeldOff) =>
      isHeldOff ? Wording.DescribeHeldOff(state) : "";

  private void ShowActionNotice(string message) {
    ActionNotice.Message = message;
    ActionNotice.IsOpen = message.Length > 0;
  }

  private async Task<string?> PickLocalThemeAsync(LocalThemeKind kind) {
    IntPtr window = WinRT.Interop.WindowNative.GetWindowHandle(this);
    if (kind == LocalThemeKind.Folder) {
      var folderPicker = new FolderPicker();
      WinRT.Interop.InitializeWithWindow.Initialize(folderPicker, window);
      folderPicker.FileTypeFilter.Add(AnyFile);
      StorageFolder? folder = await folderPicker.PickSingleFolderAsync();
      return folder?.Path;
    }
    var filePicker = new FileOpenPicker();
    WinRT.Interop.InitializeWithWindow.Initialize(filePicker, window);
    foreach (string fileType in s_themeFileTypes) {
      filePicker.FileTypeFilter.Add(fileType);
    }
    StorageFile? file = await filePicker.PickSingleFileAsync();
    return file?.Path;
  }

  private void OnDragOver(object sender, DragEventArgs e) {
    bool isFiles = e.DataView.Contains(StandardDataFormats.StorageItems);
    if (!isFiles) {
      return;
    }
    e.AcceptedOperation = DataPackageOperation.Copy;
    DropVeil.Opacity = VeilShown;
  }

  private void OnDragLeave(object sender, DragEventArgs e) => DropVeil.Opacity = VeilHidden;

  // Every dropped folder, .zip or .crx becomes a theme (ThemeLibrary tells
  // them apart); the last failure is the one shown.
  private async void OnDrop(object sender, DragEventArgs e) {
    DropVeil.Opacity = VeilHidden;
    bool isFiles = e.DataView.Contains(StandardDataFormats.StorageItems);
    if (!isFiles) {
      return;
    }
    ShowActionNotice("");
    IReadOnlyList<IStorageItem> items = await e.DataView.GetStorageItemsAsync();
    foreach (IStorageItem item in items) {
      string path = item.Path;
      string? error = await RunAsync(() => PanelActions.AddLocalTheme(path));
      if (error is not null) {
        ShowActionNotice(error);
      }
    }
  }

  private void FixWidth() {
    double scale = WindowDpi.Of(this) / BaseDpi;
    int width = (int)Math.Round(WindowWidth * scale);
    AppWindow.Resize(new SizeInt32(width, (int)Math.Round(WindowHeight * scale)));
    if (AppWindow.Presenter is OverlappedPresenter presenter) {
      presenter.IsMaximizable = false;
      presenter.PreferredMinimumWidth = width;
      presenter.PreferredMaximumWidth = width;
      presenter.PreferredMinimumHeight = (int)Math.Round(MinimumHeight * scale);
    }
    // The width is fixed, so offer no horizontal resize handle either (and
    // hear about Windows setting changes).
    _ = WindowSubclass.Attach(this);
  }
}
