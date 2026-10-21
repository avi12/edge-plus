using System;
using System.Buffers.Binary;
using System.Text;

namespace EdgePlus;

// What the DLL should do: which of its features are on, and the theme for each
// Windows mode (an extension id, or empty for the default theme).
internal readonly record struct DllConfig(
    bool IsSplitActive,
    bool IsThemingActive,
    bool IsProtocolHandlersActive,
    string LightId,
    string DarkId,
    bool IsEnhancedFindActive,
    bool IsWebUiFollowsThemeActive,
    bool IsLivePreviewActive,
    bool IsTabSpineActive,
    bool IsReadingThemeActive,
    ReadingTheme ReadingThemeLight,
    ReadingTheme ReadingThemeDark) {
  public static DllConfig From(Settings settings) => From(settings, settings.Features);

  // With the switches as they apply on the current Edge build (a paused
  // feature off), rather than as the user chose them.
  public static DllConfig From(Settings settings, Features features) => new(
      IsSplitActive: features.NativeSplit,
      IsThemingActive: features.Theming,
      IsProtocolHandlersActive: features.ProtocolHandlers,
      LightId: settings.Theming.Light,
      DarkId: settings.Theming.Dark,
      IsEnhancedFindActive: features.EnhancedFind,
      IsWebUiFollowsThemeActive: features.WebUiFollowsTheme,
      IsLivePreviewActive: features.LivePreview,
      IsTabSpineActive: features.TabSpine,
      IsReadingThemeActive: features.ReadingTheme,
      ReadingThemeLight: settings.ReadingMode.Light,
      ReadingThemeDark: settings.ReadingMode.Dark);
}

// The bytes of the config file the DLL reads. The layout mirrors struct Wire in
// app/native-split/src/config.h field for field: five u32s, the two ids, then a
// reserved u32 (once the chosen assistant; written as zero, never read), the
// find switch, the WebUI-follows-theme switch, the live-preview switch, the
// tab-spine switch, and the reading-theme switch with its light and dark themes
// (Edge's own numbers, ReadingTheme's values). Every u32 is little-endian, as
// the DLL reads it on x64.
internal static class ConfigWire {
  private const int FieldBytes = sizeof(uint);

  public const int SplitActiveOffset = 0;
  public const int ThemingActiveOffset = SplitActiveOffset + FieldBytes;
  public const int ProtocolHandlersActiveOffset = ThemingActiveOffset + FieldBytes;
  public const int LightLengthOffset = ProtocolHandlersActiveOffset + FieldBytes;
  public const int DarkLengthOffset = LightLengthOffset + FieldBytes;
  public const int LightIdOffset = DarkLengthOffset + FieldBytes;
  public const int DarkIdOffset = LightIdOffset + Shared.WireIdCapacity;
  public const int ReservedOffset = DarkIdOffset + Shared.WireIdCapacity;
  public const int EnhancedFindActiveOffset = ReservedOffset + FieldBytes;
  public const int WebUiFollowsThemeActiveOffset = EnhancedFindActiveOffset + FieldBytes;
  public const int LivePreviewActiveOffset = WebUiFollowsThemeActiveOffset + FieldBytes;
  public const int TabSpineActiveOffset = LivePreviewActiveOffset + FieldBytes;
  public const int ReadingThemeActiveOffset = TabSpineActiveOffset + FieldBytes;
  public const int ReadingThemeLightOffset = ReadingThemeActiveOffset + FieldBytes;
  public const int ReadingThemeDarkOffset = ReadingThemeLightOffset + FieldBytes;
  public const int Size = ReadingThemeDarkOffset + FieldBytes;

  // The size config.h static_asserts, from its own counts.
  public const int SizeConfigHExpects =
      Shared.WireFieldCount * FieldBytes + Shared.WireIdCount * Shared.WireIdCapacity;

  public static byte[] Encode(DllConfig config) {
    byte[] wire = new byte[Size];
    WriteFlag(wire, SplitActiveOffset, config.IsSplitActive);
    WriteFlag(wire, ThemingActiveOffset, config.IsThemingActive);
    WriteFlag(wire, ProtocolHandlersActiveOffset, config.IsProtocolHandlersActive);
    WriteId(wire, LightLengthOffset, LightIdOffset, config.LightId);
    WriteId(wire, DarkLengthOffset, DarkIdOffset, config.DarkId);
    WriteFlag(wire, EnhancedFindActiveOffset, config.IsEnhancedFindActive);
    WriteFlag(wire, WebUiFollowsThemeActiveOffset, config.IsWebUiFollowsThemeActive);
    WriteFlag(wire, LivePreviewActiveOffset, config.IsLivePreviewActive);
    WriteFlag(wire, TabSpineActiveOffset, config.IsTabSpineActive);
    WriteFlag(wire, ReadingThemeActiveOffset, config.IsReadingThemeActive);
    WriteU32(wire, ReadingThemeLightOffset, (uint)config.ReadingThemeLight);
    WriteU32(wire, ReadingThemeDarkOffset, (uint)config.ReadingThemeDark);
    return wire;
  }

  private static void WriteU32(byte[] wire, int offset, uint value) =>
      BinaryPrimitives.WriteUInt32LittleEndian(wire.AsSpan(offset), value);

  private static void WriteFlag(byte[] wire, int offset, bool isOn) =>
      BinaryPrimitives.WriteUInt32LittleEndian(wire.AsSpan(offset), isOn ? 1u : 0u);

  // An id longer than the field is cut to it, as the Tauri panel does.
  private static void WriteId(byte[] wire, int lengthOffset, int idOffset, string id) {
    byte[] bytes = Encoding.UTF8.GetBytes(id);
    int length = Math.Min(bytes.Length, Shared.WireIdCapacity);
    BinaryPrimitives.WriteUInt32LittleEndian(wire.AsSpan(lengthOffset), (uint)length);
    bytes.AsSpan(0, length).CopyTo(wire.AsSpan(idOffset));
  }
}
