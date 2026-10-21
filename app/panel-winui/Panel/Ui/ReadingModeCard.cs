using System;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Shapes;

namespace EdgePlus;

// The reading mode card: its switch, then a page theme for Windows' light mode
// and one for dark, each picked from the twelve of Edge's Themes grid, in the
// grid's order, by name with its colour chip. The Tauri panel's
// ReadingModeCard.svelte.
internal sealed class ReadingModeCard {
  private const double RowSpacing = 8;
  private const double ChipGap = 8;
  private const double DimmedOpacity = 0.5;

  // Runs a panel action off the UI thread and renders the state it returns;
  // returns why it failed (a plain sentence), or null.
  private readonly Func<Func<PanelState>, Task<string?>> _run;
  private readonly FeatureCard _card = new(Wording.For(Feature.ReadingTheme));
  private readonly StackPanel _body = new() { Spacing = RowSpacing };
  private readonly ComboBox _lightPicker = Picker(Wording.ReadingPickerName(ColorMode.Light));
  private readonly ComboBox _darkPicker = Picker(Wording.ReadingPickerName(ColorMode.Dark));
  private readonly TextBlock _error = new() {
    Style = StyleKeys.Get(StyleKeys.CardError),
    Visibility = Visibility.Collapsed,
  };
  // Set while the pickers show the stored choice, so that is not taken for a
  // new one.
  private bool _isUpdating;

  public ReadingModeCard(Func<Func<PanelState>, Task<string?>> run) {
    _run = run;
    _card.Toggled += isOn =>
        _ = RunAsync(() => PanelActions.SetFeature(Feature.ReadingTheme, isOn));
    _lightPicker.SelectionChanged += (_, _) => OnPicked(ColorMode.Light, _lightPicker);
    _darkPicker.SelectionChanged += (_, _) => OnPicked(ColorMode.Dark, _darkPicker);
    _body.Children.Add(CardRows.Picker(Wording.WhenLight, _lightPicker));
    _body.Children.Add(CardRows.Picker(Wording.WhenDark, _darkPicker));
    _body.Children.Add(_error);
    _card.SetBody(_body);
  }

  public FrameworkElement View => _card.View;

  public void Update(PanelState state, bool isLocked, string note) {
    bool isOn = state.Features.ReadingTheme;
    _card.Update(isOn, isLocked, note);
    _body.Opacity = isOn ? 1 : DimmedOpacity;
    _isUpdating = true;
    Show(_lightPicker, state.ReadingMode.Light);
    Show(_darkPicker, state.ReadingMode.Dark);
    _isUpdating = false;
  }

  // A picker of the twelve, built once: a theme's item is tagged with Edge's
  // name for it (a string, so nothing of this assembly is handed to WinUI).
  private static ComboBox Picker(string name) {
    var picker = new ComboBox();
    Microsoft.UI.Xaml.Automation.AutomationProperties.SetName(picker, name);
    foreach (ReadingTheme theme in ReadingThemes.Grid) {
      var chip = new Ellipse {
        Style = StyleKeys.Get(StyleKeys.Swatch),
        Fill = new SolidColorBrush(ReadingSwatches.Of(theme)),
      };
      var content = new StackPanel { Orientation = Orientation.Horizontal, Spacing = ChipGap };
      content.Children.Add(chip);
      content.Children.Add(new TextBlock {
        Text = Wording.ReadingThemeLabel(theme),
        VerticalAlignment = VerticalAlignment.Center,
      });
      var item = new ComboBoxItem { Content = content, Tag = ReadingThemes.EdgeName(theme) };
      Microsoft.UI.Xaml.Automation.AutomationProperties.SetName(item,
                                                                Wording.ReadingThemeLabel(theme));
      picker.Items.Add(item);
    }
    return picker;
  }

  // Select the stored theme, touching the picker only when it shows another.
  private static void Show(ComboBox picker, ReadingTheme theme) {
    string tag = ReadingThemes.EdgeName(theme);
    object? wanted = picker.Items.Cast<ComboBoxItem>().FirstOrDefault(item => (string)item.Tag == tag);
    if (!ReferenceEquals(picker.SelectedItem, wanted)) {
      picker.SelectedItem = wanted;
    }
  }

  private void OnPicked(ColorMode mode, ComboBox picker) {
    bool isUserChoice = !_isUpdating && picker.SelectedItem is ComboBoxItem;
    if (!isUserChoice) {
      return;
    }
    string tag = (string)((ComboBoxItem)picker.SelectedItem!).Tag;
    ReadingTheme theme = ReadingThemes.Grid.First(each => ReadingThemes.EdgeName(each) == tag);
    _ = RunAsync(() => PanelActions.SetReadingTheme(mode, theme));
  }

  private async Task RunAsync(Func<PanelState> action) {
    string? error = await _run(action);
    _error.Text = error ?? "";
    _error.Visibility = error is null ? Visibility.Collapsed : Visibility.Visible;
  }
}
