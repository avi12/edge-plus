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

// The window's content. It renders the PanelState the last action returned and
// never touches settings, Edge or the DLL itself (PanelActions does).
//
// Built in C#, not XAML, and composed from WinUI's controls rather than
// subclassing them: this assembly is swapped live in a collectible load
// context, and compiled XAML or a WinRT subclass would register with WinUI for
// the life of the process. Dispose undoes every tie to the host's window so
// the context can unload.
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

  private const double SectionSpacing = 20;
  // Lines the header up with the cards' text, as Windows Settings does.
  private static readonly Thickness s_sectionHeaderMargin = new(4, 0, 0, 4);

  private readonly Window _window;
  private readonly ContentControl _slot;
  private readonly Grid _root = new() { AllowDrop = true };
  private readonly StackPanel _sectionList = new() { Spacing = SectionSpacing };
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
  private readonly Dictionary<OwnCard, FeatureCard> _ownCards = [];
  private readonly InjectCard _injectCard = new();
  // The Inject automatically card, with the Inject card under it while offered;
  // its add/delete transition slides the Inject card in and out.
  private readonly StackPanel _autoInjectGroup = new() { Spacing = CardSpacing };
  private readonly BackgroundService? _service;
  private readonly List<IDisposable> _hostSubscriptions = [];
  private PanelState? _state;
  private InjectOffer _injectOffer = InjectOffer.Hidden;
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
    _injectCard.Clicked += () => _ = InjectAsync();
    Motion.Animate(_autoInjectGroup, Motion.AddDelete);
    foreach ((Section section, CardSlot[] cards) in PanelLayout.Sections) {
      _sectionList.Children.Add(SectionView(section, cards));
    }
    Motion.Animate(_sectionList, Motion.Reposition);

    var page = new StackPanel {
      Padding = new Thickness(ContentPadding, CardSpacing, ContentPadding, ContentPadding),
      Spacing = CardSpacing,
    };
    page.Children.Add(_openEdgeHint);
    page.Children.Add(_buildNotice);
    page.Children.Add(_actionNotice);
    page.Children.Add(_sectionList);
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

    _hostSubscriptions.Add(settingChanged.Subscribe(new Observer(Motion.Refresh)));
    // A window that was already active gets no new Activated to refresh it.
    _hostSubscriptions.Add(broughtBack.Subscribe(new Observer(Refresh)));
    // Picks up themes installed in Edge meanwhile.
    _window.Activated += OnActivated;
    _service = BackgroundService.Current;
    if (_service is not null) {
      // The service raises no toast while the window is open.
      _service.IsWindowOpen = true;
      _service.BuildChanged += OnBuildChanged;
      _service.ThemeRefreshed += OnThemeRefreshed;
      _service.BrowsersChanged += OnBrowsersChanged;
    }
    Refresh();
  }

  public void Dispose() {
    if (_isDisposed) {
      return;
    }
    _isDisposed = true;
    _window.Activated -= OnActivated;
    foreach (IDisposable subscription in _hostSubscriptions) {
      subscription.Dispose();
    }
    if (_service is not null) {
      _service.BuildChanged -= OnBuildChanged;
      _service.ThemeRefreshed -= OnThemeRefreshed;
      _service.BrowsersChanged -= OnBrowsersChanged;
      _service.IsWindowOpen = false;
    }
    Motion.Forget();
    _slot.Content = null;
  }

  private void OnBuildChanged(BuildChange change) => OnUiThread(Refresh);

  // Edge restarted: with auto-inject off, it may want the Inject button again.
  private void OnBrowsersChanged() => OnUiThread(Refresh);

  private void OnThemeRefreshed(ThemeRefresh refresh) =>
      OnUiThread(() => ShowActionNotice(refresh.Error ?? ""));

  private void Refresh() => _ = RunAsync(PanelActions.Load);

  private void OnActivated(object sender, WindowActivatedEventArgs e) {
    if (e.WindowActivationState != WindowActivationState.Deactivated) {
      Refresh();
    }
  }

  private void OnUiThread(Action action) =>
      _window.DispatcherQueue.TryEnqueue(() => {
        if (!_isDisposed) {
          action();
        }
      });

  private StackPanel SectionView(Section section, CardSlot[] cards) {
    var stack = new StackPanel { Spacing = CardSpacing };
    stack.Children.Add(new TextBlock {
      Text = Wording.SectionName(section),
      Style = StyleKeys.Get(StyleKeys.SectionHeader),
      Margin = s_sectionHeaderMargin,
    });
    foreach (CardSlot slot in cards) {
      stack.Children.Add(CardView(slot));
    }
    Motion.Animate(stack, Motion.Reposition);
    return stack;
  }

  private FrameworkElement CardView(CardSlot slot) {
    if (slot.Own is OwnCard own) {
      return OwnCardView(own);
    }
    Feature feature = slot.Feature!.Value;
    if (feature == Feature.Theming) {
      return _themingCard.View;
    }
    var card = new FeatureCard(Wording.For(feature));
    card.Toggled += isOn => _ = RunAndReportAsync(() => PanelActions.SetFeature(feature, isOn));
    _switchCards[feature] = card;
    return card.View;
  }

  private FrameworkElement OwnCardView(OwnCard own) {
    var card = new FeatureCard(Wording.For(own));
    card.Toggled += isOn => _ = RunAndReportAsync(() => SetOwnCard(own, isOn));
    _ownCards[own] = card;
    if (own != OwnCard.AutoInject) {
      return card.View;
    }
    _autoInjectGroup.Children.Add(card.View);
    return _autoInjectGroup;
  }

  private async Task InjectAsync() {
    await RunAndReportAsync(PanelActions.Inject);
    // A failed injection renders no new state; the button the click disabled
    // comes back as the last offer shown.
    if (!_isDisposed) {
      ShowInjectOffer(_injectOffer);
    }
  }

  private void ShowInjectOffer(InjectOffer offer) {
    FrameworkElement view = _injectCard.View;
    bool isShown = _autoInjectGroup.Children.Contains(view);
    if (offer == InjectOffer.Hidden) {
      if (isShown) {
        _autoInjectGroup.Children.Remove(view);
      }
      return;
    }
    _injectCard.Update(offer);
    if (!isShown) {
      _autoInjectGroup.Children.Add(view);
    }
  }

  private static PanelState SetOwnCard(OwnCard own, bool isOn) => own switch {
    OwnCard.StartWithWindows => PanelActions.SetStartWithWindows(isOn),
    OwnCard.AutoInject => PanelActions.SetAutoInject(isOn),
    _ => throw new ArgumentOutOfRangeException(nameof(own)),
  };

  private static bool IsOn(OwnCard own, PanelState state) => own switch {
    OwnCard.StartWithWindows => state.IsStartWithWindows,
    OwnCard.AutoInject => state.IsAutoInject,
    _ => throw new ArgumentOutOfRangeException(nameof(own)),
  };

  // Off the UI thread: an injection can wait on Edge. Returns why it failed,
  // or null. Only a ThemeException carries a sentence written for the user;
  // anything else is logged, never shown as a raw exception message.
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
    string buildNotice = state.EdgeBuild is FileVersion edge
        ? Wording.DescribePaused(edge, state.SupportedEdgeBuild, state.Paused)
        : "";
    _buildNotice.Message = buildNotice;
    _buildNotice.IsOpen = buildNotice.Length > 0;

    // A paused switch shows the user's choice (on), locked, with why; so does
    // one whose needed feature is off.
    foreach (CardSlot slot in PanelLayout.AllCards()) {
      if (slot.Feature is not Feature feature) {
        continue;
      }
      // The theming card is not in _switchCards; it renders itself below.
      if (!_switchCards.TryGetValue(feature, out FeatureCard? card)) {
        continue;
      }
      string note = Wording.DescribeFeature(state, feature);
      bool isNeededOff = slot.Needs is Feature needed && !state.Features.IsOn(needed);
      if (note.Length == 0 && isNeededOff) {
        note = Wording.NeedsFeature(slot.Needs!.Value);
      }
      card.Update(state.Features.IsOn(feature), state.IsLocked(feature) || isNeededOff, note);
    }
    _themingCard.Update(state, state.IsLocked(Feature.Theming),
                        Wording.DescribeFeature(state, Feature.Theming));
    foreach ((OwnCard own, FeatureCard card) in _ownCards) {
      card.Update(IsOn(own, state), isLocked: false, note: "");
    }
    _injectOffer = InjectOffers.Next(_injectOffer, state);
    ShowInjectOffer(_injectOffer);
  }

  private void ShowActionNotice(string message) {
    _actionNotice.Message = message;
    _actionNotice.IsOpen = message.Length > 0;
  }

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
    var version = new HyperlinkButton {
      Content = Product.Version,
      NavigateUri = Product.ChangelogUri,
      Padding = new Thickness(2, 0, 2, 0),
    };
    ToolTipService.SetToolTip(version, Wording.WhatsNew);
    Grid.SetColumn(version, 1);
    footer.Children.Add(byline);
    footer.Children.Add(version);
    return footer;
  }

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

  // ThemeLibrary tells a folder, .zip and .crx apart; the last failure is the
  // one shown.
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

  private sealed class Observer(Action onNext) : IObserver<EventArgs> {
    public void OnNext(EventArgs value) => onNext();

    public void OnCompleted() {
    }

    public void OnError(Exception error) {
    }
  }
}
