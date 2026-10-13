using System;
using System.Buffers.Binary;
using System.IO;
using System.Text;
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
  // A save raises several file events in a row; send once they stop.
  private static readonly TimeSpan s_settleDelay = TimeSpan.FromMilliseconds(100);

  private static readonly object s_writeLock = new();
  private static Stream s_output = Stream.Null;

  public static void Run() {
    Directory.CreateDirectory(Paths.Root);
    using Stream input = Console.OpenStandardInput();
    s_output = Console.OpenStandardOutput();

    using var settingsSettled = new Timer(_ => SendSettings());
    using var stampSettled = new Timer(_ => Send(new JsonObject { ["type"] = "reload" }));
    using var watcher = new FileSystemWatcher(Paths.Root) {
      NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.FileName,
    };
    void OnFileEvent(string path) {
      if (string.Equals(path, Paths.Settings, StringComparison.OrdinalIgnoreCase)) {
        settingsSettled.Change(s_settleDelay, Timeout.InfiniteTimeSpan);
      } else if (string.Equals(path, Paths.ExtensionStamp, StringComparison.OrdinalIgnoreCase)) {
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
    switch (message["type"]?.GetValue<string>()) {
      case "hello":
        SendSettings();
        break;
      case "status":
        message["pid"] = Environment.ProcessId;
        Paths.WriteAtomically(Paths.Status, message.ToJsonString());
        break;
    }
  }

  private static void SendSettings() {
    JsonNode? settings;
    try {
      settings = JsonNode.Parse(Paths.ReadSettings());
    } catch (Exception exception) when (exception is IOException or System.Text.Json.JsonException) {
      return;
    }
    Send(new JsonObject { ["type"] = "settings", ["settings"] = settings });
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
}
