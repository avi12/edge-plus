using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Markup;
using Microsoft.UI.Xaml.Media;
using Windows.System;

namespace EdgePlus;

// A .crx or .zip file, or an unpacked folder.
internal enum LocalThemeKind {
  File,
  Folder,
}

internal sealed class ThemingCard {
  private const double RowSpacing = 8;
  private const double ColumnGap = 8;
  private const double DimmedOpacity = 0.5;
  private const int RemoveGlyph = 0xE711;
  private const double RemoveGlyphSize = 12;
  private const int GripGlyph = 0xE76F;
  private const double GripGlyphSize = 12;
  private const double GripWidth = 16;
  // Inside a row, so the list's rows and Edge's rows line up.
  private const double RowInset = 4;
  // Parsed at run time, not compiled XAML, so nothing of it registers with
  // WinUI for the life of the process (the panel is swapped live).
  private const string RowSlotTemplate =
      "<DataTemplate xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\">" +
      "<ContentControl IsTabStop=\"False\" HorizontalContentAlignment=\"Stretch\" " +
      "HorizontalAlignment=\"Stretch\"/></DataTemplate>";

  // Runs a panel action off the UI thread and renders the state it returns;
  // returns why it failed, or null.
  private readonly Func<Func<PanelState>, Task<string?>> _run;
  // Null when the user cancels.
  private readonly Func<LocalThemeKind, Task<string?>> _pick;

  private readonly FeatureCard _card = new(Wording.For(Feature.Theming));
  private readonly StackPanel _body = new() { Spacing = RowSpacing };
  private readonly TextBox _storeLink = new() { PlaceholderText = Wording.StoreLinkPlaceholder };
  private readonly Button _add;
  private readonly Button _pickFile;
  private readonly Button _pickFolder;
  private readonly TextBlock _error = new() {
    Style = StyleKeys.Get(StyleKeys.CardError),
    Visibility = Visibility.Collapsed,
  };
  private readonly ComboBox _lightPicker = new();
  private readonly ComboBox _darkPicker = new();
  // No selection: with it on, each dragged row kept the accent bar, so two
  // drags looked like two selected themes. Alt+Up / Alt+Down act on the
  // focused row instead.
  private readonly ListView _ownThemes = new() {
    SelectionMode = ListViewSelectionMode.None,
    IsItemClickEnabled = false,
    CanReorderItems = true,
    CanDragItems = true,
    AllowDrop = true,
    ItemContainerStyle = StyleKeys.Get(StyleKeys.ThemeListItem),
  };
  // The list's items are the rows' keys, not the rows: ListView's built-in
  // reorder only commits a dragged move to a list of plain items (with
  // UIElements as items the lifted row came back on drop). Each container shown
  // gets a freshly built row (OnOwnRowContainerChanging), so a row is never
  // moved between recycled containers.
  private readonly ObservableCollection<string> _ownKeys = [];
  private Dictionary<string, Func<FrameworkElement>> _ownRowFactories = [];
  private readonly StackPanel _edgeThemes = new() { Spacing = RowSpacing / 2 };
  // So a refill of the pickers is not taken for a choice.
  private bool _isUpdating;

  public ThemingCard(Func<Func<PanelState>, Task<string?>> run,
                     Func<LocalThemeKind, Task<string?>> pick) {
    _run = run;
    _pick = pick;
    _card.Toggled += isOn => _ = RunAsync(() => PanelActions.SetFeature(Feature.Theming, isOn));

    _add = new Button { Content = Wording.AddTheme, Style = StyleKeys.Get(StyleKeys.AccentButton) };
    _add.Click += (_, _) => _ = AddStoreAsync();
    _storeLink.KeyDown += (_, e) => {
      if (e.Key == VirtualKey.Enter) {
        _ = AddStoreAsync();
      }
    };
    _pickFile = new Button { Content = Wording.PickFile };
    _pickFile.Click += (_, _) => _ = AddLocalAsync(LocalThemeKind.File);
    _pickFolder = new Button { Content = Wording.PickFolder };
    _pickFolder.Click += (_, _) => _ = AddLocalAsync(LocalThemeKind.Folder);
    AutomationProperties.SetName(_lightPicker, Wording.WhenLight);
    AutomationProperties.SetName(_darkPicker, Wording.WhenDark);
    _lightPicker.SelectionChanged += (_, _) => OnPicked(ColorMode.Light, _lightPicker);
    _darkPicker.SelectionChanged += (_, _) => OnPicked(ColorMode.Dark, _darkPicker);

    _body.Children.Add(StoreRow());
    _body.Children.Add(LocalRow());
    _body.Children.Add(_error);
    _body.Children.Add(CardRows.Picker(Wording.WhenLight, _lightPicker));
    _body.Children.Add(CardRows.Picker(Wording.WhenDark, _darkPicker));
    _ownThemes.ItemTemplate = (DataTemplate)XamlReader.Load(RowSlotTemplate);
    _ownThemes.ItemsSource = _ownKeys;
    _ownThemes.ContainerContentChanging += OnOwnRowContainerChanging;
    AutomationProperties.SetName(_ownThemes, Wording.ThemeListName);
    AutomationProperties.SetHelpText(_ownThemes, Wording.ReorderHelp);
    _ownThemes.DragItemsCompleted += (_, _) => {
      // WinUI can leave the dragged (or last pressed) row in its Pressed or
      // PointerOver look after a drop; every row goes back to Normal.
      ResetRowStates();
      _ = ReorderAsync(CurrentOrder());
    };
    // Preview: the list's items take arrow keys for moving focus before a
    // bubbling KeyDown would see them.
    _ownThemes.PreviewKeyDown += OnThemeListKeyDown;
    _body.Children.Add(_ownThemes);
    _body.Children.Add(_edgeThemes);
    _card.SetBody(_body);
    Motion.AnimateItems(_ownThemes, Motion.ListItems);
    Motion.Animate(_edgeThemes, Motion.AddDelete);
    Motion.Animate(_body, Motion.Reposition);
  }

  public FrameworkElement View => _card.View;

  public void Update(PanelState state, bool isLocked, string note) {
    bool isOn = state.Features.Theming;
    _card.Update(isOn, isLocked, note);
    _body.Opacity = isOn ? 1 : DimmedOpacity;

    var options = new List<(string Id, string Label)> { ("", Wording.DefaultTheme) };
    options.AddRange(state.Theming.Themes.Select(theme => (theme.Id, theme.Name)));
    options.AddRange(state.EdgeThemes.Select(theme => (theme.Id, theme.Name)));
    _isUpdating = true;
    Fill(_lightPicker, options, state.Theming.Light);
    Fill(_darkPicker, options, state.Theming.Dark);
    _isUpdating = false;

    // Keyed by everything a row shows, so an unchanged row is left alone (and
    // does not re-animate).
    var own = new RowSet();
    foreach (ThemeEntry theme in state.Theming.Themes) {
      string badge = theme.Source == ThemeSource.Store ? Wording.StoreBadge : Wording.LocalBadge;
      own.Want(RowKey(theme.Id, theme.Name, [badge]),
               () => ThemeRow(theme.Id, theme.Name, [badge], isOwn: true));
    }
    var edge = new RowSet();
    foreach (InstalledTheme theme in state.EdgeThemes) {
      string[] badges = theme.IsActive
          ? [Wording.InUseBadge, Wording.InstalledInEdgeBadge]
          : [Wording.InstalledInEdgeBadge];
      edge.Want(RowKey(theme.Id, theme.Name, badges),
                () => ThemeRow(theme.Id, theme.Name, badges, isOwn: false));
    }
    bool hasNoThemes = own.Order.Count == 0 && edge.Order.Count == 0;
    if (hasNoThemes) {
      edge.Want(EmptyRowKey, () => new TextBlock {
        Text = Wording.NoThemes,
        Style = StyleKeys.Get(StyleKeys.CardDescription),
      });
    }
    _ownRowFactories = own.Factories;
    KeyedList.Sync(_ownKeys, own.Order, key => key, key => key, _ownKeys.Move);
    edge.SyncInto(_edgeThemes.Children);
    ResetRowStates();
    _ownThemes.Visibility = own.Order.Count > 0 ? Visibility.Visible : Visibility.Collapsed;
  }

  private const string EmptyRowKey = "empty";
  private const char RowKeySeparator = '\n';

  private static string RowKey(string id, string name, string[] badges) =>
      string.Join(RowKeySeparator, [id, name, string.Join(',', badges)]);

  private static string IdOf(string rowKey) => rowKey.Split(RowKeySeparator)[0];

  private static string NameOf(string rowKey) => rowKey.Split(RowKeySeparator)[1];

  private List<string> CurrentOrder() => _ownKeys.Select(IdOf).ToList();

  // The row goes in the slot its template made. Setting the container's own
  // Content instead does not hold: the list writes the item over it (or, told
  // the content is handled, leaves it hidden).
  private void OnOwnRowContainerChanging(ListViewBase sender,
                                         ContainerContentChangingEventArgs args) {
    bool hasRow = !args.InRecycleQueue && args.Item is string key
        && _ownRowFactories.ContainsKey(key);
    if (!hasRow || args.ItemContainer.ContentTemplateRoot is not ContentControl slot) {
      return;
    }
    string rowKey = (string)args.Item;
    slot.Content = _ownRowFactories[rowKey]();
    AutomationProperties.SetName(args.ItemContainer, NameOf(rowKey));
  }

  // Alt+Up / Alt+Down: a reorder for whoever does not drag.
  private void OnThemeListKeyDown(object sender, KeyRoutedEventArgs e) {
    bool isMoveKey = e.Key == VirtualKey.Up || e.Key == VirtualKey.Down;
    bool isMove = isMoveKey && e.KeyStatus.IsMenuKeyDown;
    if (!isMove || FocusedRowKey(e.OriginalSource) is not string rowKey) {
      return;
    }
    e.Handled = true;
    string id = IdOf(rowKey);
    int offset = e.Key == VirtualKey.Up ? -1 : 1;
    IReadOnlyList<string> order = ThemeOrder.Moved(CurrentOrder(), id, offset);
    _ = MoveKeepingFocusAsync(order, id);
  }

  private const string NormalState = "Normal";

  private void ResetRowStates() {
    foreach (string key in _ownKeys) {
      if (_ownThemes.ContainerFromItem(key) is ListViewItem container) {
        VisualStateManager.GoToState(container, NormalState, useTransitions: false);
      }
    }
  }

  // Focus is on the list item itself or on its remove button.
  private string? FocusedRowKey(object? focused) {
    var element = focused as DependencyObject;
    while (element is not null && element is not ListViewItem) {
      element = VisualTreeHelper.GetParent(element);
    }
    return element is ListViewItem container
        ? _ownThemes.ItemFromContainer(container) as string
        : null;
  }

  private async Task MoveKeepingFocusAsync(IReadOnlyList<string> order, string id) {
    await ReorderAsync(order);
    string? moved = _ownKeys.FirstOrDefault(key => IdOf(key) == id);
    if (moved is null) {
      return;
    }
    if (_ownThemes.ContainerFromItem(moved) is ListViewItem container) {
      container.Focus(FocusState.Keyboard);
    }
  }

  // The rows one list should show, in order, each built only if missing.
  private sealed class RowSet {
    private readonly Dictionary<string, Func<FrameworkElement>> _create =
        new(StringComparer.Ordinal);

    public List<string> Order { get; } = [];

    public Dictionary<string, Func<FrameworkElement>> Factories => _create;

    public void Want(string key, Func<FrameworkElement> create) {
      Order.Add(key);
      _create[key] = create;
    }

    public void SyncInto(IList<UIElement> rows) =>
        KeyedList.Sync(rows, Order, row => (string)((FrameworkElement)row).Tag, key => {
          FrameworkElement row = _create[key]();
          row.Tag = key;
          return row;
        });
  }

  private Grid StoreRow() {
    var row = new Grid { ColumnSpacing = ColumnGap };
    row.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
    row.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
    Grid.SetColumn(_add, 1);
    row.Children.Add(_storeLink);
    row.Children.Add(_add);
    return row;
  }

  private StackPanel LocalRow() {
    var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = ColumnGap };
    row.Children.Add(new TextBlock {
      Text = Wording.AddLocalTheme,
      Style = StyleKeys.Get(StyleKeys.CardDescription),
      VerticalAlignment = VerticalAlignment.Center,
    });
    row.Children.Add(_pickFile);
    row.Children.Add(_pickFolder);
    return row;
  }

  // Edge's own themes get an invisible grip and remove button, so every row's
  // columns line up.
  private Grid ThemeRow(string id, string name, string[] badges, bool isOwn) {
    var row = new Grid { ColumnSpacing = ColumnGap, Padding = new Thickness(RowInset, 0, RowInset, 0) };
    row.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(GripWidth) });
    row.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
    row.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
    row.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
    AutomationProperties.SetName(row, name);
    var grip = new FontIcon {
      Glyph = char.ConvertFromUtf32(GripGlyph),
      FontSize = GripGlyphSize,
      Opacity = isOwn ? 1 : 0,
    };
    row.Children.Add(grip);
    var title = new TextBlock {
      Text = name,
      Style = StyleKeys.Get(StyleKeys.CardTitle),
      VerticalAlignment = VerticalAlignment.Center,
      TextTrimming = TextTrimming.CharacterEllipsis,
    };
    Grid.SetColumn(title, 1);
    row.Children.Add(title);
    bool isRemovable = isOwn;
    var badgeList = new StackPanel { Orientation = Orientation.Horizontal, Spacing = ColumnGap / 2 };
    foreach (string badge in badges) {
      badgeList.Children.Add(new Border {
        Style = StyleKeys.Get(StyleKeys.Badge),
        Child = new TextBlock { Text = badge, Style = StyleKeys.Get(StyleKeys.BadgeText) },
      });
    }
    Grid.SetColumn(badgeList, 2);
    row.Children.Add(badgeList);

    var remove = new Button {
      Content = new FontIcon {
        Glyph = char.ConvertFromUtf32(RemoveGlyph),
        FontSize = RemoveGlyphSize,
      },
      IsEnabled = isRemovable,
      Opacity = isRemovable ? 1 : 0,
    };
    AutomationProperties.SetName(remove, Wording.RemoveTheme(name));
    remove.Click += (_, _) => _ = RunAsync(() => PanelActions.RemoveTheme(id));
    Grid.SetColumn(remove, 3);
    row.Children.Add(remove);
    return row;
  }

  // Rebuilt only when the options changed, and then with new items: moving a
  // ComboBoxItem by removing and re-inserting the same element throws
  // (E_INVALIDARG), which put an error line on the card on every reorder.
  // Runs under _isUpdating, so the re-selection re-applies no theme.
  private static void Fill(ComboBox picker, List<(string Id, string Label)> options,
                           string selectedId) {
    var shown = picker.Items.Cast<ComboBoxItem>()
                    .Select(item => ((string)item.Tag, item.Content as string))
                    .ToList();
    bool isCurrent = shown.SequenceEqual(options.Select(option => (option.Id, (string?)option.Label)));
    if (!isCurrent) {
      picker.Items.Clear();
      foreach ((string id, string label) in options) {
        picker.Items.Add(new ComboBoxItem { Content = label, Tag = id });
      }
    }
    // A theme removed meanwhile falls back to the default theme, the first option.
    object? wanted = picker.Items.Cast<ComboBoxItem>()
                         .FirstOrDefault(item => (string)item.Tag == selectedId)
        ?? picker.Items.FirstOrDefault();
    if (!ReferenceEquals(picker.SelectedItem, wanted)) {
      picker.SelectedItem = wanted;
    }
  }

  private void OnPicked(ColorMode mode, ComboBox picker) {
    bool isUserChoice = !_isUpdating && picker.SelectedItem is ComboBoxItem;
    if (!isUserChoice) {
      return;
    }
    string id = (string)((ComboBoxItem)picker.SelectedItem!).Tag;
    _ = RunAsync(() => PanelActions.AssignTheme(mode, id));
  }

  private async Task AddStoreAsync() {
    string link = _storeLink.Text.Trim();
    if (link.Length == 0) {
      return;
    }
    bool isAdded = await RunAsync(() => PanelActions.AddStoreTheme(link));
    if (isAdded) {
      _storeLink.Text = "";
    }
  }

  private async Task AddLocalAsync(LocalThemeKind kind) {
    string? path = await _pick(kind);
    if (path is not null) {
      await RunAsync(() => PanelActions.AddLocalTheme(path));
    }
  }

  // None of RunAsync's busy state: holding the add buttons for the save
  // flickered File… and Folder… on every drop.
  private async Task ReorderAsync(IReadOnlyList<string> order) {
    string? error = await _run(() => PanelActions.ReorderThemes(order));
    if (error is not null) {
      ShowError(error);
    }
  }

  private async Task<bool> RunAsync(Func<PanelState> action) {
    ShowError("");
    SetBusy(true);
    string? error = await _run(action);
    SetBusy(false);
    ShowError(error ?? "");
    return error is null;
  }

  private void SetBusy(bool isBusy) {
    _add.IsEnabled = !isBusy;
    _pickFile.IsEnabled = !isBusy;
    _pickFolder.IsEnabled = !isBusy;
  }

  private void ShowError(string error) {
    _error.Text = error;
    _error.Visibility = error.Length > 0 ? Visibility.Visible : Visibility.Collapsed;
  }
}
