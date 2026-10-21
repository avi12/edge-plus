using System;
using System.Collections.Generic;
using System.Linq;

namespace EdgePlus;

// What a card says: its title, its description, and its icon (a Segoe Fluent
// Icons glyph, Windows' own icon font — no image assets). The glyph is held as
// its code point: a private-use character in a string literal is invisible in
// an editor, and dotnet format rewrites a \u escape into exactly that.
internal sealed record CardWording(string Title, string Description, int GlyphCodePoint) {
  public string Glyph => char.ConvertFromUtf32(GlyphCodePoint);
}

// Every sentence the window shows, in one place: each card's wording, and what
// the panel says when Edge runs a build the split DLL was not made for. The
// Tauri panel's App.svelte constants, assistantNames.ts and edgeBuild.ts.
internal static class Wording {
  // The browser assistant's name; the DLL has its own copy (chat_target.h),
  // which must match.
  private const string AssistantName = "Copilot";
  // The regular-expression mode, as the user reads it; the DLL's kRegExpName
  // is the same word.
  private const string RegExpName = "RegExp";

  public const string OpenEdgeHint = "Open Edge to apply changes.";
  // The footer's "by Avi".
  public const string By = "by";
  public const string DropHint = "Drop a theme folder, .zip or .crx to add it";

  // The theming card's own words.
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

  // A reading mode picker's name for a screen reader.
  public static string ReadingPickerName(ColorMode mode) =>
      mode == ColorMode.Light
          ? "Reading mode theme when Windows is light"
          : "Reading mode theme when Windows is dark";

  // A reading theme as the user reads it: Edge's name, its words spaced out
  // ("LightBlue" -> "Light blue").
  public static string ReadingThemeLabel(ReadingTheme theme) {
    string name = ReadingThemes.EdgeName(theme);
    var label = new System.Text.StringBuilder();
    for (int i = 0; i < name.Length; i++) {
      bool isNewWord = i > 0 && char.IsUpper(name[i]);
      if (isNewWord) {
        label.Append(' ').Append(char.ToLowerInvariant(name[i]));
      } else {
        label.Append(name[i]);
      }
    }
    return label.ToString();
  }

  // What a failure says. The details go to the panel's log (PanelLog), never
  // into the window.
  public const string ActionFailed =
      "That did not work. Edge+ wrote the details to its log (panel.log in %LOCALAPPDATA%\\FixEdge).";
  public const string ThemeFilesUnreadable =
      "Edge+ could not read or write that theme's files. Check the folder still exists and is not open elsewhere.";
  public const string ManifestNotJson = "manifest.json is not valid JSON";

  public const string ThemeListName = "Your themes";
  public const string ReorderHelp =
      "Drag a theme, or press Alt+Up or Alt+Down, to move it in this list and in the pickers.";

  public static CardWording StartWithWindows {
    get;
  } = new(
      "Start Edge+ when Windows starts",
      "Opens Edge+ minimized when you sign in to Windows, so your settings apply without " +
          "opening it. Launch Edge+ again to bring its window back.",
      0xE7E8);

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
    Feature.TabSpine => new(
        "Square selected tab with neon spine",
        "Gives the selected vertical tab a straight leading edge with a neon magenta stripe (on " +
            "the right when Edge runs right to left). Works with Theming on. Live, no restart.",
        0xE7C3),
    Feature.ReadingTheme => new(
        "Reading mode follows Windows light and dark",
        "Reading mode uses one page theme while Windows is light and another while it is dark, " +
            "set as Windows switches and kept for every page you open in reading mode. Live, no " +
            "restart.",
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

  // How a build other than the supported one is introduced: newer is an
  // update, older a version Edge+ does not know.
  public static string EdgeLead(FileVersion edge, FileVersion supported) =>
      edge > supported ? $"Edge updated to {edge}." : $"Edge is on version {edge}.";

  // What one card says about its feature on this build; empty when the build
  // holds nothing back. "Edge updated to 156.0.1.0. Split into any grid is
  // paused until Edge+ supports this version."
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

  // The features a build pauses, as one sentence; empty when it pauses none.
  // The panel's notice and the toast's body.
  public static string DescribePaused(FileVersion edge, FileVersion supported,
                                      IReadOnlyList<Feature> paused) {
    if (paused.Count == 0) {
      return "";
    }
    return $"{EdgeLead(edge, supported)} Paused until Edge+ supports this version: " +
           $"{JoinTitles(paused)}.";
  }

  // The toast's title.
  public const string PausedToastTitle = "Edge+ paused features for this Edge version";

  // By card title, apart by semicolons: some titles carry commas of their own
  // ("Ask to handle links, like Chrome").
  private const string TitleSeparator = "; ";

  private static string JoinTitles(IReadOnlyList<Feature> features) =>
      string.Join(TitleSeparator, features.Select(feature => For(feature).Title));
}
