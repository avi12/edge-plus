using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace EdgePlus;

// The Windows Settings card, built by hand from WinUI's own controls rather
// than a toolkit's SettingsCard (no UI library, to keep the supply chain small):
// an icon, a title over a description, and one control at the end. FeatureCard
// puts a switch there, InjectCard a button.
internal sealed class CardFrame {
  private const double IconSize = 20;
  private const double IconGap = 16;
  private const double BodyGap = 12;
  // Between the words and the control, as in Windows Settings: without it a
  // description wraps only at the control's own edge and runs under it.
  private const double ControlGap = 16;

  private readonly FontIcon _icon;
  private readonly TextBlock _title;
  private readonly TextBlock _description;
  private readonly TextBlock _note;
  private readonly Grid _layout;

  public CardFrame(CardWording wording, FrameworkElement control) {
    _note = new TextBlock {
      Style = StyleKeys.Get(StyleKeys.CardNote),
      Visibility = Visibility.Collapsed,
    };
    _title = new TextBlock { Style = StyleKeys.Get(StyleKeys.CardTitle) };
    _description = new TextBlock { Style = StyleKeys.Get(StyleKeys.CardDescription) };
    var text = new StackPanel {
      VerticalAlignment = VerticalAlignment.Center,
      Margin = new Thickness(0, 0, ControlGap, 0),
    };
    text.Children.Add(_title);
    text.Children.Add(_description);
    text.Children.Add(_note);

    _icon = new FontIcon {
      FontSize = IconSize,
      Margin = new Thickness(0, 0, IconGap, 0),
      VerticalAlignment = VerticalAlignment.Center,
    };
    control.VerticalAlignment = VerticalAlignment.Center;

    _layout = new Grid();
    _layout.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
    _layout.ColumnDefinitions.Add(new ColumnDefinition {
      Width = new GridLength(1, GridUnitType.Star),
    });
    _layout.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
    _layout.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
    Grid.SetColumn(text, 1);
    Grid.SetColumn(control, 2);
    _layout.Children.Add(_icon);
    _layout.Children.Add(text);
    _layout.Children.Add(control);

    View = new Border { Style = StyleKeys.Get(StyleKeys.Card), Child = _layout };
    SetWording(wording);
  }

  public FrameworkElement View {
    get;
  }

  public void SetWording(CardWording wording) {
    _icon.Glyph = wording.Glyph;
    _title.Text = wording.Title;
    _description.Text = wording.Description;
  }

  public void SetNote(string note) {
    _note.Text = note;
    _note.Visibility = note.Length > 0 ? Visibility.Visible : Visibility.Collapsed;
  }

  public void SetBody(FrameworkElement body) {
    _layout.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
    Grid.SetRow(body, 1);
    Grid.SetColumn(body, 1);
    Grid.SetColumnSpan(body, 2);
    body.Margin = new Thickness(0, BodyGap, 0, 0);
    _layout.Children.Add(body);
  }
}
