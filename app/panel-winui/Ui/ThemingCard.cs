using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;
using Windows.System;

namespace EdgePlus;

// What a local theme is picked from: a .crx or .zip file, or an unpacked folder.
internal enum LocalThemeKind {
  File,
  Folder,
}

// The theming feature's card: its switch, then the library — add a theme from a
// store link or a local file or folder, pick one for Windows' light mode and one
// for dark, and the list of themes (the panel's own, removable, then those Edge
// already had). Every change goes through PanelActions, so switching a theme
// needs no restart. The Tauri panel's ThemingCard.svelte and ThemeList.svelte.
internal sealed class ThemingCard {
  private const double RowSpacing = 8;
  private const double ColumnGap = 8;
  private const double PickerMinWidth = 200;
  private const double DimmedOpacity = 0.5;
  private const int RemoveGlyph = 0xE711;
  private const double RemoveGlyphSize = 12;

  // Runs a panel action off the UI thread and renders the state it returns;
  // returns why it failed, or null.
  private readonly Func<Func<PanelState>, Task<string?>> _run;
  // Asks the user for a local theme; null when they cancel.
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
  private readonly ComboBox _lightPicker = new() { MinWidth = PickerMinWidth };
  private readonly ComboBox _darkPicker = new() { MinWidth = PickerMinWidth };
  private readonly StackPanel _themeList = new() { Spacing = RowSpacing / 2 };
  // Set while the pickers are refilled, so a refill is not taken for a choice.
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
    _body.Children.Add(PickerRow(Wording.WhenLight, _lightPicker));
    _body.Children.Add(PickerRow(Wording.WhenDark, _darkPicker));
    _body.Children.Add(_themeList);
    _card.SetBody(_body);
    // A theme added or removed slides in or out, and the rows after it move to
    // make room; the card grows or shrinks with it.
    Motion.Animate(_themeList, Motion.AddDelete);
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

    // Each row keyed by everything it shows, so a row that changed is replaced
    // and one that did not is left alone (and does not re-animate).
    var rows = new Dictionary<string, Func<FrameworkElement>>(StringComparer.Ordinal);
    var order = new List<string>();
    void Want(string key, Func<FrameworkElement> create) {
      order.Add(key);
      rows[key] = create;
    }
    foreach (ThemeEntry theme in state.Theming.Themes) {
      string badge = theme.Source == ThemeSource.Store ? Wording.StoreBadge : Wording.LocalBadge;
      Want(RowKey(theme.Id, theme.Name, [badge], isRemovable: true),
           () => ThemeRow(theme.Id, theme.Name, [badge], isRemovable: true));
    }
    foreach (InstalledTheme theme in state.EdgeThemes) {
      string[] badges = theme.IsActive
          ? [Wording.InUseBadge, Wording.InstalledInEdgeBadge]
          : [Wording.InstalledInEdgeBadge];
      Want(RowKey(theme.Id, theme.Name, badges, isRemovable: false),
           () => ThemeRow(theme.Id, theme.Name, badges, isRemovable: false));
    }
    bool hasNoThemes = order.Count == 0;
    if (hasNoThemes) {
      Want(EmptyRowKey, () => new TextBlock {
        Text = Wording.NoThemes,
        Style = StyleKeys.Get(StyleKeys.CardDescription),
      });
    }
    KeyedList.Sync(_themeList.Children, order, child => (string)((FrameworkElement)child).Tag,
                   key => {
                     FrameworkElement row = rows[key]();
                     row.Tag = key;
                     return row;
                   });
  }

  private const string EmptyRowKey = "empty";

  private static string RowKey(string id, string name, string[] badges, bool isRemovable) =>
      string.Join('\n', [id, name, string.Join(',', badges), isRemovable.ToString()]);

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

  private static Grid PickerRow(string label, ComboBox picker) {
    var row = new Grid { ColumnSpacing = ColumnGap };
    row.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
    row.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
    row.Children.Add(new TextBlock {
      Text = label,
      Style = StyleKeys.Get(StyleKeys.CardTitle),
      VerticalAlignment = VerticalAlignment.Center,
    });
    Grid.SetColumn(picker, 1);
    row.Children.Add(picker);
    return row;
  }

  private Grid ThemeRow(string id, string name, string[] badges, bool isRemovable) {
    var row = new Grid { ColumnSpacing = ColumnGap };
    row.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
    row.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
    row.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
    row.Children.Add(new TextBlock {
      Text = name,
      Style = StyleKeys.Get(StyleKeys.CardTitle),
      VerticalAlignment = VerticalAlignment.Center,
      TextTrimming = TextTrimming.CharacterEllipsis,
    });
    var badgeList = new StackPanel { Orientation = Orientation.Horizontal, Spacing = ColumnGap / 2 };
    foreach (string badge in badges) {
      badgeList.Children.Add(new Border {
        Style = StyleKeys.Get(StyleKeys.Badge),
        Child = new TextBlock { Text = badge, Style = StyleKeys.Get(StyleKeys.BadgeText) },
      });
    }
    Grid.SetColumn(badgeList, 1);
    row.Children.Add(badgeList);

    // Edge's own themes keep the column, so every row's badges line up.
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
    Grid.SetColumn(remove, 2);
    row.Children.Add(remove);
    return row;
  }

  private static void Fill(ComboBox picker, List<(string Id, string Label)> options,
                           string selectedId) {
    picker.Items.Clear();
    foreach ((string id, string label) in options) {
      picker.Items.Add(new ComboBoxItem { Content = label, Tag = id });
    }
    int selected = options.FindIndex(option => option.Id == selectedId);
    picker.SelectedIndex = selected < 0 ? 0 : selected;
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

  // An action with the add buttons held while it runs, and its failure shown
  // on the card. Returns whether it succeeded.
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
