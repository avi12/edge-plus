using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.Json.Nodes;

namespace FixEdge;

// Glyph is the feature's Segoe Fluent Icons code point, for its row in the app.
internal sealed record Feature(
    string Id, string Title, string Description, string Glyph, bool IsEnabledByDefault);

// The features the app's own code names. Every other feature is known only by
// its row in catalog.json, which stays the one list of the features there are.
internal enum FeatureId {
  SplitPanes,
  QuietDevMode,
  // Not a catalog feature: the native split DLL, whose switch the app owns.
  NativeSplit,
}

// The features the extension implements, read from its own catalog.json, so a
// feature's name, wording and defaults are written once for both sides.
//
// Linked into the host as well; each exe embeds catalog.json under this name.
internal static class Catalog {
  private const string Resource = "catalog.json";

  private static readonly JsonNode s_catalog = JsonNode.Parse(
      typeof(Catalog).Assembly.GetManifestResourceStream(Resource)
      ?? throw new InvalidOperationException($"This exe carries no {Resource}"))!;

  // The name Edge looks the native host up by.
  public static string HostName { get; } = s_catalog["hostName"]!.GetValue<string>();

  public static IReadOnlyList<Feature> Features {
    get;
  } = s_catalog["features"]!.AsArray()
      .Select(node => new Feature(
          node!["id"]!.GetValue<string>(),
          node["title"]!.GetValue<string>(),
          node["description"]!.GetValue<string>(),
          node["glyph"]!.GetValue<string>(),
          node["enabledByDefault"]!.GetValue<bool>()))
      .ToList();

  public static Feature Get(string id) => Features.Single(feature => feature.Id == id);

  // The id as catalog.json and the extension's FeatureId spell it, and as the
  // settings and status files key it.
  public static string WireName(this FeatureId feature) => feature switch {
    FeatureId.SplitPanes => "splitPanes",
    FeatureId.QuietDevMode => "quietDevMode",
    FeatureId.NativeSplit => "nativeSplit",
    _ => throw new ArgumentOutOfRangeException(nameof(feature)),
  };
}
