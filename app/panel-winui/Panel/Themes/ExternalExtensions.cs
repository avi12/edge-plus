using Microsoft.Win32;

namespace EdgePlus;

// A theme is added without touching Edge's ABI: an entry under Edge's per-user
// external-extensions key, and Edge's loader installs it. A store theme points
// at the store's update URL, so Edge trusts it without developer mode.
internal static class ExternalExtensions {
  private const string ExtensionsKey = @"Software\Microsoft\Edge\Extensions";
  private const string ChromeStoreUpdateUrl = "https://clients2.google.com/service/update2/crx";
  private const string EdgeStoreUpdateUrl =
      "https://edge.microsoft.com/extensionwebstorebase/v1/crx";
  // The value names Edge's loader reads under each id.
  private const string UpdateUrlValue = "update_url";
  private const string PathValue = "path";
  private const string VersionValue = "version";

  public static void InstallStore(string extensionId, ThemeStore store) {
    using RegistryKey key = CreateKey(extensionId);
    key.SetValue(UpdateUrlValue,
                 store == ThemeStore.Edge ? EdgeStoreUpdateUrl : ChromeStoreUpdateUrl);
  }

  public static void InstallLocal(string extensionId, string crxPath, string version) {
    using RegistryKey key = CreateKey(extensionId);
    key.SetValue(PathValue, crxPath);
    key.SetValue(VersionValue, version);
  }

  // Edge removes the extension on its next start.
  public static void Uninstall(string extensionId) {
    try {
      Registry.CurrentUser.DeleteSubKeyTree(KeyPath(extensionId), throwOnMissingSubKey: false);
    } catch (System.UnauthorizedAccessException) {
    }
  }

  private static string KeyPath(string extensionId) => $@"{ExtensionsKey}\{extensionId}";

  private static RegistryKey CreateKey(string extensionId) =>
      Registry.CurrentUser.CreateSubKey(KeyPath(extensionId));
}
