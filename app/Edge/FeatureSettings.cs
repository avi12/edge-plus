using System.Text.Json;
using System.Text.Json.Nodes;

namespace FixEdge;

// The user's switches: { "features": { "<id>": { "enabled": … } } }. Only what
// the user changed is stored; everything else falls through to the catalog's
// defaults, on this side and in the extension alike.
internal static class FeatureSettings {
  private const string FeaturesKey = "features";
  private const string EnabledKey = "enabled";

  private static readonly JsonSerializerOptions s_indented = new() { WriteIndented = true };

  public static bool IsEnabled(string featureId) =>
      Stored(featureId, EnabledKey)?.GetValue<bool>() ?? Catalog.Get(featureId).IsEnabledByDefault;

  public static void SetEnabled(string featureId, bool isEnabled) =>
      Set(featureId, EnabledKey, JsonValue.Create(isEnabled));

  private static JsonNode? Stored(string featureId, string key) =>
      Load()[FeaturesKey]?[featureId]?[key];

  private static void Set(string featureId, string key, JsonNode? value) {
    JsonObject settings = Load();
    var features = settings[FeaturesKey] as JsonObject ?? new JsonObject();
    settings[FeaturesKey] = features;
    var feature = features[featureId] as JsonObject ?? new JsonObject();
    features[featureId] = feature;
    feature[key] = value;
    Paths.WriteAtomically(Paths.Settings, settings.ToJsonString(s_indented));
  }

  private static JsonObject Load() {
    try {
      return JsonNode.Parse(Paths.ReadSettings()) as JsonObject ?? new JsonObject();
    } catch (JsonException) {
      return new JsonObject();
    }
  }
}
