using Microsoft.UI.Xaml;

namespace EdgePlus;

// Each must match an x:Key in App.xaml.
internal static class StyleKeys {
  public const string Card = "FeatureCardStyle";
  public const string CardTitle = "CardTitleStyle";
  public const string CardDescription = "CardDescriptionStyle";
  public const string CardNote = "CardNoteStyle";
  public const string CardError = "CardErrorStyle";
  public const string Badge = "BadgeStyle";
  public const string BadgeText = "BadgeTextStyle";
  public const string ThemeListItem = "ThemeListItemStyle";
  public const string DropVeil = "DropVeilStyle";
  public const string DropVeilFrame = "DropVeilFrameStyle";
  // WinUI's own.
  public const string AccentButton = "AccentButtonStyle";
  // WinUI's own, as Windows Settings sets a section header.
  public const string SectionHeader = "BodyStrongTextBlockStyle";

  public static Style Get(string key) => (Style)Application.Current.Resources[key];
}
