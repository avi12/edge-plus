using System;
using System.Globalization;
using System.Linq;

namespace EdgePlus;

// Which store a theme came from, so the matching update URL is used.
internal enum ThemeStore {
  Chrome,
  Edge,
}

// A theme parsed from a store URL. `Name` is the link's slug as a title, or
// null when the link has no slug (the caller names it otherwise).
internal sealed record ParsedStoreTheme(string Id, string? Name, ThemeStore Store);

// Pulling a theme's identity out of a pasted store URL. Only the real stores
// are recognised, each by its exact host and the path before the theme
// (StoreSite): <prefix>/<slug>/<id> or <prefix>/<id>. Pure string logic, no
// network; the Tauri panel's store_url.rs mirrors it.
internal static class StoreUrl {
  private const string HttpsPrefix = "https://";
  private const string SchemeSeparator = "://";
  private const string WwwPrefix = "www.";
  private static readonly char[] s_slugSeparators = ['-', '_'];

  // A store's host and the path segments in front of a theme's slug and id.
  private sealed record StoreSite(string Host, string[] Prefix, ThemeStore Store);

  // The one table of the sites a theme link may come from.
  private static readonly StoreSite[] s_sites = [
    new("chromewebstore.google.com", ["detail"], ThemeStore.Chrome),
    // The Chrome Web Store's old address, still in links people keep.
    new("chrome.google.com", ["webstore", "detail"], ThemeStore.Chrome),
    new("microsoftedge.microsoft.com", ["addons", "detail"], ThemeStore.Edge),
  ];

  // A theme link from the Chrome Web Store or Edge Add-ons, or null for any
  // other text. Surrounding space, http, a missing scheme, "www.", a trailing
  // slash, a query (such as ?hl=en) and a fragment are all tolerated.
  public static ParsedStoreTheme? Parse(string url) {
    string trimmed = url.Trim();
    string absolute = trimmed.Contains(SchemeSeparator, StringComparison.Ordinal)
        ? trimmed
        : HttpsPrefix + trimmed;
    if (!Uri.TryCreate(absolute, UriKind.Absolute, out Uri? uri)) {
      return null;
    }
    bool isWebLink = uri.Scheme == Uri.UriSchemeHttps || uri.Scheme == Uri.UriSchemeHttp;
    if (!isWebLink) {
      return null;
    }
    string host = uri.Host.StartsWith(WwwPrefix, StringComparison.OrdinalIgnoreCase)
        ? uri.Host[WwwPrefix.Length..]
        : uri.Host;
    StoreSite? site = s_sites.FirstOrDefault(
        each => string.Equals(each.Host, host, StringComparison.OrdinalIgnoreCase));
    if (site is null) {
      return null;
    }
    string[] segments = uri.AbsolutePath.Split('/', StringSplitOptions.RemoveEmptyEntries);
    bool hasPrefix = segments.Length > site.Prefix.Length
        && segments.Take(site.Prefix.Length)
               .SequenceEqual(site.Prefix, StringComparer.OrdinalIgnoreCase);
    if (!hasPrefix) {
      return null;
    }
    string[] rest = segments[site.Prefix.Length..];
    // /<id>, or /<slug>/<id>; a tab after it (/reviews) is ignored.
    if (ExtensionId.IsValid(rest[0])) {
      return new ParsedStoreTheme(rest[0], null, site.Store);
    }
    bool hasSlugAndId = rest.Length > 1 && ExtensionId.IsValid(rest[1]);
    if (!hasSlugAndId) {
      return null;
    }
    return new ParsedStoreTheme(rest[1], TitleFromSlug(rest[0]), site.Store);
  }

  // "dusk-at-sea" -> "Dusk At Sea".
  private static string TitleFromSlug(string slug) {
    string spaced = s_slugSeparators.Aggregate(slug, (text, separator) => text.Replace(separator, ' '))
                        .Trim();
    if (spaced.Length == 0) {
      return slug;
    }
    return string.Join(' ', spaced.Split(' ', StringSplitOptions.RemoveEmptyEntries)
                                .Select(Capitalize));
  }

  private static string Capitalize(string word) =>
      char.ToUpper(word[0], CultureInfo.InvariantCulture) + word[1..];
}
