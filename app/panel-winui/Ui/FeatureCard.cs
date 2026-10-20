using System;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;

namespace EdgePlus;

// One feature's card: icon, title, description, an optional note saying why its
// switch is locked, and the switch — the Windows Settings card, built by hand
// from WinUI's own controls rather than a toolkit's SettingsCard (no UI
// library, to keep the supply chain small). Its look is App.xaml's card
// styles. A card may carry a body below the header (the theming card's
// library).
internal sealed class FeatureCard {
  private const double IconSize = 20;
  private const double IconGap = 16;
  private const double BodyGap = 12;

  private readonly ToggleSwitch _toggle;
  private readonly TextBlock _note;
  private readonly Grid _layout;
  // Set while Update writes the switch, so the write is not taken for a click.
  private bool _isUpdating;

  public FeatureCard(CardWording wording) {
    _toggle = new ToggleSwitch {
      OnContent = "",
      OffContent = "",
      MinWidth = 0,
      VerticalAlignment = VerticalAlignment.Center,
    };
    AutomationProperties.SetName(_toggle, wording.Title);
    _toggle.Toggled += (_, _) => {
      if (!_isUpdating) {
        Toggled?.Invoke(_toggle.IsOn);
      }
    };

    _note = new TextBlock {
      Style = StyleKeys.Get(StyleKeys.CardNote),
      Visibility = Visibility.Collapsed,
    };
    var text = new StackPanel { VerticalAlignment = VerticalAlignment.Center };
    text.Children.Add(new TextBlock {
      Text = wording.Title,
      Style = StyleKeys.Get(StyleKeys.CardTitle),
    });
    text.Children.Add(new TextBlock {
      Text = wording.Description,
      Style = StyleKeys.Get(StyleKeys.CardDescription),
    });
    text.Children.Add(_note);

    var icon = new FontIcon {
      Glyph = wording.Glyph,
      FontSize = IconSize,
      Margin = new Thickness(0, 0, IconGap, 0),
      VerticalAlignment = VerticalAlignment.Center,
    };

    _layout = new Grid();
    _layout.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
    _layout.ColumnDefinitions.Add(new ColumnDefinition {
      Width = new GridLength(1, GridUnitType.Star),
    });
    _layout.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
    _layout.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
    Grid.SetColumn(text, 1);
    Grid.SetColumn(_toggle, 2);
    _layout.Children.Add(icon);
    _layout.Children.Add(text);
    _layout.Children.Add(_toggle);

    View = new Border { Style = StyleKeys.Get(StyleKeys.Card), Child = _layout };
  }

  // Raised with the switch's new state when the user flips it.
  public event Action<bool>? Toggled;

  public FrameworkElement View {
    get;
  }

  // Show the stored state. A locked switch says why in `note`.
  public void Update(bool isOn, bool isLocked, string note) {
    _isUpdating = true;
    _toggle.IsOn = isOn;
    _isUpdating = false;
    _toggle.IsEnabled = !isLocked;
    _note.Text = note;
    _note.Visibility = note.Length > 0 ? Visibility.Visible : Visibility.Collapsed;
  }

  // A body under the header, spanning the text and the switch.
  public void SetBody(FrameworkElement body) {
    _layout.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
    Grid.SetRow(body, 1);
    Grid.SetColumn(body, 1);
    Grid.SetColumnSpan(body, 2);
    body.Margin = new Thickness(0, BodyGap, 0, 0);
    _layout.Children.Add(body);
  }
}
