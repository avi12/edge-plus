using Microsoft.UI.Xaml;

namespace EdgePlus;

// The keys of App.xaml's styles, as the code-built cards ask for them; each
// must match an x:Key there.
internal static class StyleKeys {
  public const string Card = "FeatureCardStyle";
  public const string CardTitle = "CardTitleStyle";
  public const string CardDescription = "CardDescriptionStyle";
  public const string CardNote = "CardNoteStyle";
  public const string CardError = "CardErrorStyle";
  public const string Badge = "BadgeStyle";
  public const string BadgeText = "BadgeTextStyle";
  // WinUI's own.
  public const string AccentButton = "AccentButtonStyle";

  public static Style Get(string key) => (Style)Application.Current.Resources[key];
}
