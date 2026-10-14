using System;
using System.Diagnostics;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace FixEdge;

internal sealed record SplitStatus(int Splits, int Panes);

// Whether the extension is running in Edge right now, as its host last wrote
// down. A status file whose host process is gone is a stale one: Edge closed.
internal static class EdgeConnection {
  public static bool IsConnected => Read() is not null;

  public static SplitStatus? Split() {
    JsonNode? split = Read()?["features"]?[FeatureId.SplitPanes.WireName()];
    if (split is null) {
      return null;
    }
    return new SplitStatus(split["splits"]!.GetValue<int>(), split["panes"]!.GetValue<int>());
  }

  private static JsonNode? Read() {
    try {
      JsonNode? status = JsonNode.Parse(File.ReadAllText(Paths.Status));
      int? hostProcessId = status?[Paths.StatusHostProcessIdKey]?.GetValue<int>();
      bool isHostRunning = hostProcessId is int processId && IsHostProcess(processId);
      return isHostRunning ? status : null;
    } catch (Exception exception) when (exception is IOException or JsonException) {
      return null;
    }
  }

  // By name as well as id: once the host exits, Windows can hand its id to
  // another process.
  private static bool IsHostProcess(int processId) {
    try {
      using var process = Process.GetProcessById(processId);
      return process.ProcessName == Path.GetFileNameWithoutExtension(Paths.HostExecutable);
    } catch (ArgumentException) {
      return false;
    }
  }
}
