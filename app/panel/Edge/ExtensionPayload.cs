using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text.Json.Nodes;

namespace FixEdge;

// The extension's files, carried inside the exe (see the csproj) and unpacked
// into the folder Edge loads them from.
internal static class ExtensionPayload {
  private const string ResourcePrefix = "extension/";
  private const string ManifestFile = "manifest.json";
  private const string CatalogResource = "catalog.json";
  private const int ExtensionIdBytes = 16;

  private static readonly Assembly s_assembly = typeof(ExtensionPayload).Assembly;

  // Extension-relative path, forward slashes -> the resource's real name.
  // MSBuild's RecursiveDir writes the folder part with backslashes.
  private static readonly Dictionary<string, string> s_resources = s_assembly
      .GetManifestResourceNames()
      .Where(name => name.StartsWith(ResourcePrefix, StringComparison.Ordinal))
      .ToDictionary(name => name[ResourcePrefix.Length..].Replace('\\', '/'));

  // Edge derives an unpacked extension's id from the manifest's public key:
  // the first 16 bytes of its SHA-256, each nibble written as a letter a-p.
  // Computing it here means the key in manifest.json is its only source.
  public static string Id { get; } = ComputeId();

  public static string ReadCatalog() => System.Text.Encoding.UTF8.GetString(ReadBytes(CatalogResource));

  private static string Read(string relativePath) {
    if (!s_resources.TryGetValue(relativePath, out string? resource)) {
      throw new InvalidOperationException($"fixedge.exe carries no extension/{relativePath}");
    }
    return System.Text.Encoding.UTF8.GetString(ReadBytes(resource));
  }

  // Writes only what changed, the manifest last so Edge never reloads half a
  // set of files, then touches the stamp the host is watching. A forced
  // reload is for a new host: the extension reconnecting is what starts it.
  public static void Unpack(bool isReloadForced) {
    var files = s_resources.OrderBy(file => file.Key == ManifestFile);
    bool isChanged = DeleteStaleFiles() || isReloadForced;
    foreach ((string relativePath, string resource) in files) {
      string target = Path.Combine(Paths.Extension, relativePath);
      byte[] contents = ReadBytes(resource);
      bool isCurrent = File.Exists(target) && File.ReadAllBytes(target).AsSpan().SequenceEqual(contents);
      if (isCurrent) {
        continue;
      }
      Directory.CreateDirectory(Path.GetDirectoryName(target)!);
      File.WriteAllBytes(target, contents);
      isChanged = true;
    }
    if (isChanged) {
      Paths.WriteAtomically(Paths.ExtensionStamp, DateTime.UtcNow.ToString("O"));
    }
  }

  // WXT names its chunks by content hash, so every build leaves the last
  // build's files behind under names this one no longer carries.
  private static bool DeleteStaleFiles() {
    if (!Directory.Exists(Paths.Extension)) {
      return false;
    }
    bool isAnyDeleted = false;
    foreach (string file in Directory.EnumerateFiles(Paths.Extension, "*", SearchOption.AllDirectories).ToList()) {
      string relativePath = Path.GetRelativePath(Paths.Extension, file).Replace('\\', '/');
      if (s_resources.ContainsKey(relativePath)) {
        continue;
      }
      File.Delete(file);
      isAnyDeleted = true;
    }
    return isAnyDeleted;
  }

  private static byte[] ReadBytes(string resource) {
    using var stream = s_assembly.GetManifestResourceStream(resource)!;
    using var buffer = new MemoryStream();
    stream.CopyTo(buffer);
    return buffer.ToArray();
  }

  private static string ComputeId() {
    string key = JsonNode.Parse(Read(ManifestFile))!["key"]!.GetValue<string>();
    byte[] hash = SHA256.HashData(Convert.FromBase64String(key));
    return string.Concat(hash.Take(ExtensionIdBytes).Select(
        b => $"{(char)('a' + (b >> 4))}{(char)('a' + (b & 0xF))}"));
  }
}
