using System;
using System.Collections.Generic;
using System.Linq;

namespace EdgePlus;

// The glyph (Segoe Fluent Icons) is held as its code point: a private-use
// character in a string literal is invisible in an editor, and dotnet format
// rewrites a \u escape into exactly that.
internal sealed record CardWording(string Title, string Description, int GlyphCodePoint) {
  public string Glyph => char.ConvertFromUtf32(GlyphCodePoint);
}

internal static class Wording {
  // Must match the DLL's copy (chat_target.h).
  private const string AssistantName = "Copilot";
  // Must match the DLL's kRegExpName.
  private const string RegExpName = "RegExp";

  public const string OpenEdgeHint = "Open Edge to apply changes.";

  public static string SectionName(Section section) => section switch {
    Section.TabsAndWindows => "Tabs and windows",
    Section.LookAndThemes => "Look and themes",
    Section.Browsing => "Browsing",
    Section.EdgePlusItself => "Edge+ itself",
    _ => throw new ArgumentOutOfRangeException(nameof(section)),
  };

  public static string NeedsFeature(Feature needed) => $"Turn on {For(needed).Title} to use this.";
  // The footer's "by Avi".
  public const string By = "by";
  // The footer version's tooltip.
  public const string WhatsNew = "What's new";
  public const string DropHint = "Drop a theme folder, .zip or .crx to add it";

  public const string NotAStoreLink =
      "That is not a theme link from the Chrome Web Store or Edge Add-ons.";
  public const string StoreLinkPlaceholder = "Paste a Chrome Web Store or Edge Add-ons link";
  public const string AddTheme = "Add";
  public const string AddLocalTheme = "Or add a local theme";
  public const string PickFile = "File…";
  public const string PickFolder = "Folder…";
  public const string PickFileFilterName = "Edge theme";
  public const string WhenLight = "When Windows is light";
  public const string WhenDark = "When Windows is dark";
  public const string DefaultTheme = "Default theme";
  public const string StoreBadge = "Store";
  public const string LocalBadge = "Local";
  public const string InUseBadge = "In use";
  public const string InstalledInEdgeBadge = "Installed in Edge";
  public const string NoThemes =
      "No themes yet — paste a store link or add a local theme (.crx, .zip or folder).";

  public static string RemoveTheme(string name) => $"Remove {name}";

  public const string ActionFailed =
      "That did not work. Edge+ wrote the details to its log (panel.log in %LOCALAPPDATA%\\EdgePlus).";
  public const string ThemeFilesUnreadable =
      "Edge+ could not read or write that theme's files. Check the folder still exists and is not open elsewhere.";
  public const string ManifestNotJson = "manifest.json is not valid JSON";

  public const string ThemeListName = "Your themes";
  public const string ReorderHelp =
      "Drag a theme, or press Alt+Up or Alt+Down, to move it in this list and in the pickers.";

  public const string InjectButton = "Inject into Edge";

  public static CardWording InjectReady {
    get;
  } = new(
      "Edge is open without Edge+",
      "Inject automatically is off, so this Edge started without Edge+. Inject it to turn " +
          "your switches on.",
      0xE768);

  public static CardWording Injected {
    get;
  } = new(
      "Edge+ is in Edge",
      "Injected. When Edge next starts, Edge+ offers this button again.",
      0xE73E);

  public static CardWording For(OwnCard card) => card switch {
    OwnCard.StartWithWindows => new(
        "Start Edge+ when Windows starts",
        "Runs Edge+ in the background, with no window, from when you sign in to Windows: your " +
            "settings apply and an Edge update that needs a newer Edge+ is caught. Launch " +
            "Edge+ to open its window.",
        0xE7E8),
    OwnCard.AutoInject => new(
        "Inject automatically",
        "Puts Edge+ into Edge each time Edge starts. Off, Edge starts without it and Edge+ " +
            "offers an Inject button.",
        0xE945),
    _ => throw new ArgumentOutOfRangeException(nameof(card)),
  };

  public static CardWording For(Feature feature) => feature switch {
    Feature.NativeSplit => new(
        "Split into any grid",
        "Grows Edge's own split past two panes: add panes on any side, drag the dividers, " +
            "move with Shift+Alt+arrow. Live, no restart. Open Edge first.",
        0xF0E2),
    Feature.Theming => new(
        "Theme by Windows light and dark",
        "Use one theme when Windows is light and another when it is dark, switched live. Add " +
            "themes from the Chrome Web Store, Edge Add-ons or a local file.",
        0xE790),
    Feature.WebUiFollowsTheme => new(
        "Settings and extensions pages follow the theme",
        "Makes edge://settings and edge://extensions light or dark to match your Edge theme, " +
            "not Windows. Only those two pages change; websites are untouched. Live, no " +
            "restart. Open Edge first.",
        0xE706),
    Feature.EnhancedFind => new(
        $"Enhanced find: {RegExpName} and Ask {AssistantName}",
        $"Adds {RegExpName} (with its flags) and Ask {AssistantName} to the options of Edge's " +
            $"Ctrl+F bar. {RegExpName} highlights every match; Ask sends your question to " +
            $"{AssistantName}, which opens in its side panel. Live, no restart. Open Edge first.",
        0xE721),
    Feature.LivePreview => new(
        "Live tab preview on hover",
        "Hover a tab and its thumbnail plays live instead of a frozen snapshot, by keeping " +
            "Edge's own tab capture running while the preview is open. Live, no restart. Open " +
            "Edge first.",
        0xE890),
    Feature.TabShape => new(
        "Cut-corner tabs",
        "Gives every tab, vertical and horizontal, a straight leading edge and a cut trailing " +
            "corner, with the selected tab filled in the theme's neon. Works with Theming on. " +
            "Live, no restart.",
        0xE7C3),
    Feature.ReadingTheme => new(
        "Reading mode follows Windows light and dark",
        "Reading mode uses one page theme while Windows is light and another while it is dark, " +
            "switched as Windows switches. Pick both in reading mode: Appearance settings, " +
            "Themes, then Light mode or Dark mode. Live, no restart.",
        0xE736),
    Feature.QuietDevMode => new(
        "Hide the developer-mode prompt",
        "Stops Edge asking to turn off developer-mode extensions when it starts. Takes effect " +
            "from Edge's next start.",
        0xE7BA),
    Feature.ProtocolHandlers => new(
        "Ask to handle links, like Chrome",
        "Like Chrome: when a site such as Gmail offers to open email or calendar links, closing " +
            "the bubble no longer blocks it for good; only Ignore or Block does. Turning this on " +
            "lifts earlier blocks. Live, no restart.",
        0xE71B),
    _ => throw new ArgumentOutOfRangeException(nameof(feature)),
  };

  public static string EdgeLead(FileVersion edge, FileVersion supported) =>
      edge > supported ? $"Edge updated to {edge}." : $"Edge is on version {edge}.";

  public static string DescribeFeature(PanelState state, Feature feature) {
    FeatureState featureState = state.StateOf(feature);
    bool isHeld = featureState is FeatureState.Paused or FeatureState.Unavailable;
    if (!isHeld || state.EdgeBuild is not FileVersion edge) {
      return "";
    }
    string verb = featureState == FeatureState.Paused ? "paused" : "unavailable";
    return $"{EdgeLead(edge, state.SupportedEdgeBuild)} {For(feature).Title} is {verb} until " +
           "Edge+ supports this version.";
  }

  // The panel's notice and the toast's body.
  public static string DescribePaused(FileVersion edge, FileVersion supported,
                                      IReadOnlyList<Feature> paused) {
    if (paused.Count == 0) {
      return "";
    }
    return $"{EdgeLead(edge, supported)} Paused until Edge+ supports this version: " +
           $"{JoinTitles(paused)}.";
  }

  public const string PausedToastTitle = "Edge+ paused features for this Edge version";

  // Not commas: some titles carry their own ("Ask to handle links, like Chrome").
  private const string TitleSeparator = "; ";

  private static string JoinTitles(IReadOnlyList<Feature> features) =>
      string.Join(TitleSeparator, features.Select(feature => For(feature).Title));
}
