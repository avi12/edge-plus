using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Shapes;
using Windows.ApplicationModel.DataTransfer;
using Windows.Storage;
using Windows.Storage.Pickers;

namespace EdgePlus;

// The window's content: a card per feature, in the Tauri panel's order, the
// notices above them and the footer under them. It renders whatever
// PanelState the last action returned and never touches settings, Edge or the
// DLL itself (PanelActions does). The Tauri panel's App.svelte.
//
// Built in C#, not XAML, and composed from WinUI's controls rather than
// subclassing them: this assembly is swapped live in a collectible load
// context (the host's PanelLoader), and compiled XAML or a WinRT subclass
// would register with WinUI for the life of the process. Dispose undoes every
// tie to the host's window so the context can unload.
internal sealed class PanelView : IDisposable {
  private const double VeilShown = 1;
  private const double VeilHidden = 0;
  private const double ContentPadding = 16;
  private const double CardSpacing = 4;
  private const double VeilInset = 12;
  private const double VeilCornerRadius = 8;
  private const string AnyFile = "*";
  private static readonly string[] s_themeFileTypes = [".crx", ".zip"];
  private static readonly DoubleCollection s_veilDashes = [4, 3];
  private static readonly TimeSpan s_veilFade = TimeSpan.FromMilliseconds(120);

  // The plain switches, top to bottom; the theming card sits second and the
  // start-at-sign-in card last.
  private static readonly Feature[] s_switchCardsAfterTheming = [
    Feature.WebUiFollowsTheme, Feature.TabSpine, Feature.EnhancedFind, Feature.LivePreview,
    Feature.QuietDevMode, Feature.ProtocolHandlers,
  ];

  private readonly Window _window;
  private readonly ContentControl _slot;
  private readonly Grid _root = new() { AllowDrop = true };
  private readonly StackPanel _cardList = new() { Spacing = CardSpacing };
  private readonly TextBlock _openEdgeHint;
  private readonly InfoBar _buildNotice = new() {
    IsClosable = false,
    Severity = InfoBarSeverity.Warning,
  };
  private readonly InfoBar _actionNotice = new() {
    IsClosable = true,
    Severity = InfoBarSeverity.Error,
  };
  private readonly Grid _dropVeil;
  private readonly Dictionary<Feature, FeatureCard> _switchCards = [];
  private readonly ThemingCard _themingCard;
  private readonly FeatureCard _startupCard = new(Wording.StartWithWindows);
  private readonly EdgeWatch _edgeWatch = new();
  private readonly ThemeWatch _themeWatch = new();
  private readonly List<IDisposable> _hostSubscriptions = [];
  private PanelState? _state;
  // What the last Edge update switched off; replaces the standing held-off
  // sentence it would only repeat.
  private string _updateNotice = "";
  private bool _isDisposed;

  public PanelView(Window window, ContentControl slot, IObservable<EventArgs> settingChanged,
                   IObservable<EventArgs> broughtBack) {
    _window = window;
    _slot = slot;
    _openEdgeHint = new TextBlock {
      Text = Wording.OpenEdgeHint,
      Style = StyleKeys.Get(StyleKeys.CardDescription),
      Margin = new Thickness(4, 0, 4, 4),
      Visibility = Visibility.Collapsed,
    };
    _themingCard = new ThemingCard(RunAsync, PickLocalThemeAsync);
    AddSwitchCard(Feature.NativeSplit);
    _cardList.Children.Add(_themingCard.View);
    foreach (Feature feature in s_switchCardsAfterTheming) {
      AddSwitchCard(feature);
    }
    _startupCard.Toggled += isOn => _ = RunAndReportAsync(
        () => PanelActions.SetStartWithWindows(isOn));
    _cardList.Children.Add(_startupCard.View);
    // The cards under the theming card glide as its list grows or shrinks.
    Motion.Animate(_cardList, Motion.Reposition);

    var page = new StackPanel {
      Padding = new Thickness(ContentPadding, CardSpacing, ContentPadding, ContentPadding),
      Spacing = CardSpacing,
    };
    page.Children.Add(_openEdgeHint);
    page.Children.Add(_buildNotice);
    page.Children.Add(_actionNotice);
    page.Children.Add(_cardList);
    page.Children.Add(Footer());
    _root.Children.Add(new ScrollViewer {
      Content = page,
      VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
    });
    _dropVeil = DropVeil();
    _root.Children.Add(_dropVeil);
    _root.DragOver += OnDragOver;
    _root.DragLeave += (_, _) => _dropVeil.Opacity = VeilHidden;
    _root.Drop += OnDrop;
    _slot.Content = _root;

    // Turning Windows' "Animation effects" off or on applies at once.
    _hostSubscriptions.Add(settingChanged.Subscribe(new Observer(Motion.Refresh)));
    // Launching Edge+ again brings the panel back, re-read: a window that was
    // already active gets no new Activated to refresh it.
    _hostSubscriptions.Add(broughtBack.Subscribe(new Observer(Refresh)));
    // Coming back to the panel picks up themes installed in Edge meanwhile.
    _window.Activated += OnActivated;
    // Edge came up on a build not seen before; features that depend on the old
    // msedge.dll may have been switched off.
    _edgeWatch.BuildChanged += change => OnUiThread(() => {
      _updateNotice = Wording.DescribeUpdate(change);
      Refresh();
    });
    // A local theme edited on disk was re-checked and re-installed (or refused).
    _themeWatch.Refreshed += refresh => OnUiThread(() => ShowActionNotice(refresh.Error ?? ""));
    _edgeWatch.Start();
    _themeWatch.Start();
    Refresh();
  }

  // Undo every tie to the host's window and stop every thread, so nothing of
  // this assembly is left referenced once the host unloads it.
  public void Dispose() {
    if (_isDisposed) {
      return;
    }
    _isDisposed = true;
    _window.Activated -= OnActivated;
    foreach (IDisposable subscription in _hostSubscriptions) {
      subscription.Dispose();
    }
    _edgeWatch.Stop();
    _themeWatch.Stop();
    Motion.Forget();
    _slot.Content = null;
  }

  private void Refresh() => _ = RunAsync(PanelActions.Load);

  private void OnActivated(object sender, WindowActivatedEventArgs e) {
    if (e.WindowActivationState != WindowActivationState.Deactivated) {
      Refresh();
    }
  }

  // A watcher's news, handled on the UI thread unless the view is gone.
  private void OnUiThread(Action action) =>
      _window.DispatcherQueue.TryEnqueue(() => {
        if (!_isDisposed) {
          action();
        }
      });

  private void AddSwitchCard(Feature feature) {
    var card = new FeatureCard(Wording.For(feature));
    card.Toggled += isOn => _ = RunAndReportAsync(() => PanelActions.SetFeature(feature, isOn));
    _switchCards[feature] = card;
    _cardList.Children.Add(card.View);
  }

  // Run a panel action off the UI thread (an injection can wait on Edge) and
  // render the state it returns. Returns why it failed, or null. Only a
  // ThemeException carries a sentence written for the user (a theme that
  // fails its checks says why); anything else is logged and said plainly,
  // never as a raw exception message.
  private async Task<string?> RunAsync(Func<PanelState> action) {
    try {
      PanelState state = await Task.Run(action);
      if (!_isDisposed) {
        Render(state);
      }
      return null;
    } catch (ThemeException error) {
      return error.Message;
    } catch (Exception error) {
      PanelLog.Write("panel action failed", error);
      return Wording.ActionFailed;
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
    _openEdgeHint.Visibility = state.IsEdgeRunning ? Visibility.Collapsed : Visibility.Visible;
    string buildNotice = _updateNotice.Length > 0
        ? _updateNotice
        : Wording.DescribeUnsupported(state);
    _buildNotice.Message = buildNotice;
    _buildNotice.IsOpen = buildNotice.Length > 0;

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
    _actionNotice.Message = message;
    _actionNotice.IsOpen = message.Length > 0;
  }

  // "by Avi" on the left, the version on the right.
  private static Grid Footer() {
    var footer = new Grid { Margin = new Thickness(4, 14, 4, 0) };
    footer.ColumnDefinitions.Add(new ColumnDefinition {
      Width = new GridLength(1, GridUnitType.Star),
    });
    footer.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
    var byline = new StackPanel { Orientation = Orientation.Horizontal, Spacing = CardSpacing };
    byline.Children.Add(new TextBlock {
      Text = Wording.By,
      Style = StyleKeys.Get(StyleKeys.CardDescription),
      VerticalAlignment = VerticalAlignment.Center,
    });
    byline.Children.Add(new HyperlinkButton {
      Content = Product.Author,
      NavigateUri = Product.AuthorUri,
      Padding = new Thickness(2, 0, 2, 0),
    });
    var version = new TextBlock {
      Text = Product.Version,
      Style = StyleKeys.Get(StyleKeys.CardDescription),
      VerticalAlignment = VerticalAlignment.Center,
    };
    Grid.SetColumn(version, 1);
    footer.Children.Add(byline);
    footer.Children.Add(version);
    return footer;
  }

  // Shown while a theme is dragged over the window.
  private static Grid DropVeil() {
    var veil = new Grid {
      Style = StyleKeys.Get(StyleKeys.DropVeil),
      Margin = new Thickness(VeilInset),
      Opacity = VeilHidden,
      IsHitTestVisible = false,
      OpacityTransition = new ScalarTransition { Duration = s_veilFade },
    };
    veil.Children.Add(new Rectangle {
      Style = StyleKeys.Get(StyleKeys.DropVeilFrame),
      StrokeDashArray = s_veilDashes,
      RadiusX = VeilCornerRadius,
      RadiusY = VeilCornerRadius,
    });
    veil.Children.Add(new TextBlock {
      Text = Wording.DropHint,
      Style = StyleKeys.Get(StyleKeys.CardTitle),
      HorizontalAlignment = HorizontalAlignment.Center,
      VerticalAlignment = VerticalAlignment.Center,
    });
    return veil;
  }

  private async Task<string?> PickLocalThemeAsync(LocalThemeKind kind) {
    IntPtr window = WinRT.Interop.WindowNative.GetWindowHandle(_window);
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
    _dropVeil.Opacity = VeilShown;
  }

  // Every dropped folder, .zip or .crx becomes a theme (ThemeLibrary tells
  // them apart); the last failure is the one shown.
  private async void OnDrop(object sender, DragEventArgs e) {
    _dropVeil.Opacity = VeilHidden;
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

  // A host signal handled by one action.
  private sealed class Observer(Action onNext) : IObserver<EventArgs> {
    public void OnNext(EventArgs value) => onNext();

    public void OnCompleted() {
    }

    public void OnError(Exception error) {
    }
  }
}
