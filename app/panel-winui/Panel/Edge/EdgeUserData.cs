using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace EdgePlus;

internal static class EdgeUserData {
  public static string Default {
    get;
  } = Path.Combine(
      Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
      "Microsoft", "Edge", "User Data");

  private const char ByteOrderMark = (char)0xFEFF;

  // Edge writes some of these with a UTF-8 byte-order mark.
  public static JsonNode? ReadJson(string path) {
    try {
      return JsonNode.Parse(File.ReadAllText(path).TrimStart(ByteOrderMark));
    } catch (IOException) {
      return null;
    } catch (UnauthorizedAccessException) {
      return null;
    } catch (JsonException) {
      return null;
    }
  }
}
