using System;
using System.Buffers.Binary;
using System.IO;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Threading;

namespace FixEdge;

// The relay Edge starts when the extension connects: settings from the panel
// go to Edge the moment they are saved, the extension's status comes back to
// a file the panel reads. Lives as long as the extension's port is open.
//
// Native messaging frames every message as a 4-byte little-endian length and
// that many bytes of UTF-8 JSON, in both directions.
internal static class NativeHost {
  private const int LengthPrefixBytes = 4;
  private const string TypeKey = "type";
  // A save raises several file events in a row; send once they stop.
  private static readonly TimeSpan s_settleDelay = TimeSpan.FromMilliseconds(100);

  private static readonly object s_writeLock = new();
  private static Stream s_output = Stream.Null;

  // The message types on the wire, named the same in extension/src/lib/host.ts.
  private enum HostMessageType {
    Settings,
    Reload,
  }

  private enum ExtensionMessageType {
    Hello,
    Status,
  }

  public static void Run() {
    Directory.CreateDirectory(Paths.Root);
    using Stream input = Console.OpenStandardInput();
    s_output = Console.OpenStandardOutput();

    using var settingsSettled = new Timer(_ => SendSettings());
    using var stampSettled = new Timer(
        _ => Send(new JsonObject { [TypeKey] = WireName(HostMessageType.Reload) }));
    using var watcher = new FileSystemWatcher(Paths.Root) {
      NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.FileName,
    };
    void OnFileEvent(string path) {
      if (IsSamePath(path, Paths.Settings)) {
        settingsSettled.Change(s_settleDelay, Timeout.InfiniteTimeSpan);
      } else if (IsSamePath(path, Paths.ExtensionStamp)) {
        stampSettled.Change(s_settleDelay, Timeout.InfiniteTimeSpan);
      }
    }
    watcher.Changed += (_, e) => OnFileEvent(e.FullPath);
    watcher.Created += (_, e) => OnFileEvent(e.FullPath);
    watcher.Renamed += (_, e) => OnFileEvent(e.FullPath);
    // A deleted settings file means "all defaults", which Edge must hear too.
    watcher.Deleted += (_, e) => OnFileEvent(e.FullPath);
    watcher.EnableRaisingEvents = true;

    try {
      while (ReadMessage(input) is JsonObject message) {
        Handle(message);
      }
    } finally {
      ClearStatus();
    }
  }

  private static void Handle(JsonObject message) {
    switch (ParseExtensionMessageType(message[TypeKey]?.GetValue<string>())) {
      case ExtensionMessageType.Hello:
        SendSettings();
        break;
      case ExtensionMessageType.Status:
        message[Paths.StatusHostProcessIdKey] = Environment.ProcessId;
        Paths.WriteAtomically(Paths.Status, message.ToJsonString());
        break;
    }
  }

  private static void SendSettings() {
    JsonNode? settings;
    try {
      settings = JsonNode.Parse(Paths.ReadSettings());
    } catch (Exception exception) when (exception is IOException or JsonException) {
      return;
    }
    Send(new JsonObject {
      [TypeKey] = WireName(HostMessageType.Settings),
      ["settings"] = settings,
    });
  }

  private static void Send(JsonObject message) {
    byte[] body = Encoding.UTF8.GetBytes(message.ToJsonString());
    Span<byte> length = stackalloc byte[LengthPrefixBytes];
    BinaryPrimitives.WriteInt32LittleEndian(length, body.Length);
    lock (s_writeLock) {
      s_output.Write(length);
      s_output.Write(body);
      s_output.Flush();
    }
  }

  // Null once Edge closes the pipe, which is how it says the port is gone.
  private static JsonObject? ReadMessage(Stream input) {
    byte[] length = new byte[LengthPrefixBytes];
    if (!ReadExactly(input, length)) {
      return null;
    }
    byte[] body = new byte[BinaryPrimitives.ReadInt32LittleEndian(length)];
    if (!ReadExactly(input, body)) {
      return null;
    }
    return JsonNode.Parse(body) as JsonObject;
  }

  private static bool ReadExactly(Stream input, byte[] buffer) {
    try {
      input.ReadExactly(buffer);
      return true;
    } catch (EndOfStreamException) {
      return false;
    }
  }

  private static void ClearStatus() {
    try {
      File.Delete(Paths.Status);
    } catch (IOException) {
    }
  }

  private static bool IsSamePath(string path, string other) =>
      string.Equals(path, other, StringComparison.OrdinalIgnoreCase);

  private static string WireName(HostMessageType type) => type switch {
    HostMessageType.Settings => "settings",
    HostMessageType.Reload => "reload",
    _ => throw new ArgumentOutOfRangeException(nameof(type)),
  };

  // Null for a type this host does not know, which it ignores.
  private static ExtensionMessageType? ParseExtensionMessageType(string? type) => type switch {
    "hello" => ExtensionMessageType.Hello,
    "status" => ExtensionMessageType.Status,
    _ => null,
  };
}
