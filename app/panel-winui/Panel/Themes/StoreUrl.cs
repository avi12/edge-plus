using System;
using System.Globalization;
using System.Linq;

namespace EdgePlus;

// Which store a theme came from, so the matching update URL is used.
internal enum ThemeStore {
  Chrome,
  Edge,
}

// A theme parsed from a store URL.
internal sealed record ParsedStoreTheme(string Id, string Name, ThemeStore Store);

// Pulling a theme's identity out of a pasted store URL.
//
// Edge Add-ons and the Chrome Web Store both end /detail/<slug>/<id>, where
// <id> is an extension id. Pure string logic, no network — the Tauri panel's
// store_url.rs.
internal static class StoreUrl {
  private static readonly string[] s_schemes = ["https://", "http://"];
  private static readonly char[] s_queryStarts = ['?', '#'];
  private static readonly char[] s_slugSeparators = ['-', '_'];
  // Edge Add-ons is microsoftedge.microsoft.com; every other host is taken as
  // the Chrome Web Store.
  private const string EdgeStoreHostPart = "microsoft";

  // A Chrome Web Store / Edge Add-ons URL, or null if it carries no extension
  // id.
  public static ParsedStoreTheme? Parse(string url) {
    string trimmed = url.Trim();
    string? scheme = s_schemes.FirstOrDefault(
        prefix => trimmed.StartsWith(prefix, StringComparison.Ordinal));
    if (scheme is null) {
      return null;
    }
    string afterScheme = trimmed[scheme.Length..];
    int slash = afterScheme.IndexOf('/');
    string host = slash < 0 ? afterScheme : afterScheme[..slash];
    string path = slash < 0 ? "" : afterScheme[(slash + 1)..];
    int queryStart = path.IndexOfAny(s_queryStarts);
    if (queryStart >= 0) {
      path = path[..queryStart];
    }
    string[] segments = path.Split('/', StringSplitOptions.RemoveEmptyEntries);

    int idIndex = Array.FindLastIndex(segments, ExtensionId.IsValid);
    if (idIndex < 0) {
      return null;
    }
    string id = segments[idIndex];
    string slug = idIndex > 0 ? segments[idIndex - 1] : id;
    bool isEdgeStore = host.ToLowerInvariant().Contains(EdgeStoreHostPart, StringComparison.Ordinal);
    return new ParsedStoreTheme(id, TitleFromSlug(slug),
                                isEdgeStore ? ThemeStore.Edge : ThemeStore.Chrome);
  }

  // "dusk-at-sea" -> "Dusk At Sea".
  private static string TitleFromSlug(string slug) {
    string spaced = s_slugSeparators.Aggregate(slug, (text, separator) => text.Replace(separator, ' '))
                        .Trim();
    if (spaced.Length == 0) {
      return slug;
    }
    return string.Join(' ', spaced.Split(' ').Select(Capitalize));
  }

  private static string Capitalize(string word) =>
      word.Length == 0
          ? word
          : char.ToUpper(word[0], CultureInfo.InvariantCulture) + word[1..];
}
