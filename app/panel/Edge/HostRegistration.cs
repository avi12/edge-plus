using System;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Win32;

namespace FixEdge;

// Tells Edge where its native-messaging host lives: a manifest naming this
// exe and the one extension allowed to start it, and a per-user registry key
// pointing at that manifest. Per-user, so no elevation is ever needed.
internal static class HostRegistration {
  private const string EdgeHostsKey = @"Software\Microsoft\Edge\NativeMessagingHosts\";

  private static readonly JsonSerializerOptions s_indented = new() { WriteIndented = true };

  public static void Register() {
    var manifest = new JsonObject {
      ["name"] = Catalog.HostName,
      ["description"] = Product.Summary,
      ["path"] = Paths.HostExecutable,
      ["type"] = "stdio",
      ["allowed_origins"] = new JsonArray($"chrome-extension://{ExtensionPayload.Id}/"),
    };
    Paths.WriteAtomically(Paths.HostManifest, manifest.ToJsonString(s_indented));
    using RegistryKey key = Registry.CurrentUser.CreateSubKey(EdgeHostsKey + Catalog.HostName);
    key.SetValue(string.Empty, Paths.HostManifest);
  }
}
