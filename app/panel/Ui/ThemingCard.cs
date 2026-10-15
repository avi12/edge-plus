using System;
using System.Collections.Generic;
using System.Linq;
using CommunityToolkit.WinUI.Controls;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Windows.Storage.Pickers;

namespace FixEdge;

// The theming feature's UI: a switch, a library of themes the user adds (from a
// store URL or a local .crx), and two dropdowns that pick which theme to use in
// Windows' light mode and which in dark. Every change persists (ThemeLibrary),
// installs through Edge's loader (ThemeInstaller) and is pushed to the running
// Edge (the onChanged callback), so switching a theme needs no restart.
internal sealed class ThemingCard {
  private const string DefaultThemeLabel = "Default theme";
  private const string Glyph = "";  // the personalization "paint" glyph

  private readonly IntPtr _windowHandle;
  private readonly Action _onChanged;
  private ThemingConfig _config = ThemeLibrary.Load();

  private readonly SettingsExpander _expander = new();
  private readonly ComboBox _lightChoice = new() { MinWidth = 180 };
  private readonly ComboBox _darkChoice = new() { MinWidth = 180 };
  private readonly TextBox _storeUrl = new() {
    PlaceholderText = "Paste a Chrome Web Store or Edge Add-ons link",
    MinWidth = 240,
  };

  public ThemingCard(IntPtr windowHandle, Action onChanged) {
    _windowHandle = windowHandle;
    _onChanged = onChanged;
  }

  public FrameworkElement Build() {
    bool isOn = FeatureSettings.IsEnabled(FeatureId.Theming, isEnabledByDefault: false);
    var toggle = new ToggleSwitch { OnContent = "", OffContent = "", MinWidth = 0, IsOn = isOn };
    toggle.Toggled += (_, _) => {
      FeatureSettings.SetEnabled(FeatureId.Theming, toggle.IsOn);
      _onChanged();
    };

    _expander.Header = "Theme by Windows light and dark";
    _expander.Description =
        "Use one theme when Windows is light and another when it is dark, switched live. "
        + "Add themes from the Chrome Web Store, Edge Add-ons or a local file.";
    _expander.HeaderIcon = new FontIcon { Glyph = Glyph };
    _expander.Content = toggle;

    _lightChoice.SelectionChanged += (_, _) => OnAssignmentChanged();
    _darkChoice.SelectionChanged += (_, _) => OnAssignmentChanged();
    RebuildItems();
    return _expander;
  }

  // Rebuild the expanded rows from the current library: add-a-theme, one row per
  // theme, and the two assignment dropdowns.
  private void RebuildItems() {
    _expander.Items.Clear();
    _expander.Items.Add(BuildAddRow());
    foreach (ThemeEntry theme in _config.Themes) {
      _expander.Items.Add(BuildThemeRow(theme));
    }
    _expander.Items.Add(BuildChoiceRow("When Windows is light", _lightChoice, _config.LightThemeId));
    _expander.Items.Add(BuildChoiceRow("When Windows is dark", _darkChoice, _config.DarkThemeId));
  }

  private SettingsCard BuildAddRow() {
    var add = new Button { Content = "Add" };
    add.Click += (_, _) => AddFromStore();
    var browse = new Button { Content = "Local .crx…" };
    browse.Click += (_, _) => AddFromLocalFile();
    var row = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 8 };
    row.Children.Add(_storeUrl);
    row.Children.Add(add);
    row.Children.Add(browse);
    return new SettingsCard { Header = "Add a theme", Content = row };
  }

  private SettingsCard BuildThemeRow(ThemeEntry theme) {
    var remove = new Button { Content = "Remove" };
    remove.Click += (_, _) => RemoveTheme(theme);
    return new SettingsCard {
      Header = theme.Name,
      Description = theme.Source == ThemeSource.Local ? "Local file" : "Store",
      Content = remove,
    };
  }

  private SettingsCard BuildChoiceRow(string header, ComboBox choice, string selectedId) {
    choice.Items.Clear();
    choice.Items.Add(DefaultThemeLabel);
    foreach (ThemeEntry theme in _config.Themes) {
      choice.Items.Add(theme.Name);
    }
    choice.SelectedIndex = IndexOfTheme(selectedId);
    return new SettingsCard { Header = header, Content = choice };
  }

  private int IndexOfTheme(string themeId) {
    int index = _config.Themes.FindIndex(theme => theme.Id == themeId);
    return index < 0 ? 0 : index + 1;  // 0 is the default-theme row
  }

  private string ThemeIdAt(int choiceIndex) =>
      choiceIndex <= 0 ? "" : _config.Themes[choiceIndex - 1].Id;

  private void OnAssignmentChanged() {
    _config.LightThemeId = ThemeIdAt(_lightChoice.SelectedIndex);
    _config.DarkThemeId = ThemeIdAt(_darkChoice.SelectedIndex);
    ThemeLibrary.Save(_config);
    _onChanged();
  }

  private void AddFromStore() {
    if (!ThemeInstaller.TryParseStoreUrl(_storeUrl.Text, out var parsed)) {
      return;
    }
    ThemeInstaller.InstallStore(parsed.Id, parsed.Store);
    AddToLibrary(new ThemeEntry(parsed.Id, parsed.Name, ThemeSource.Store, _storeUrl.Text.Trim()));
    _storeUrl.Text = "";
  }

  private async void AddFromLocalFile() {
    var picker = new FileOpenPicker();
    WinRT.Interop.InitializeWithWindow.Initialize(picker, _windowHandle);
    picker.FileTypeFilter.Add(".crx");
    Windows.Storage.StorageFile? file = await picker.PickSingleFileAsync();
    if (file is null) {
      return;
    }
    try {
      ThemeEntry theme = ThemeInstaller.InstallLocalCrx(file.Path);
      AddToLibrary(theme);
    } catch (Exception) {
      // A file that is not a readable .crx is simply not added.
    }
  }

  private void AddToLibrary(ThemeEntry theme) {
    bool isAlreadyListed = _config.Themes.Any(existing => existing.Id == theme.Id);
    if (!isAlreadyListed) {
      _config.Themes.Add(theme);
    }
    ThemeLibrary.Save(_config);
    RebuildItems();
    _onChanged();
  }

  private void RemoveTheme(ThemeEntry theme) {
    ThemeInstaller.Uninstall(theme.Id);
    _config.Themes.RemoveAll(existing => existing.Id == theme.Id);
    if (_config.LightThemeId == theme.Id) {
      _config.LightThemeId = "";
    }
    if (_config.DarkThemeId == theme.Id) {
      _config.DarkThemeId = "";
    }
    ThemeLibrary.Save(_config);
    RebuildItems();
    _onChanged();
  }
}
