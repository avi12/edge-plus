using System;
using System.Buffers.Binary;
using System.Text;

namespace EdgePlus;

// A theme id is an extension id, or empty for the default theme.
internal readonly record struct DllConfig(
    bool IsSplitActive,
    bool IsThemingActive,
    bool IsProtocolHandlersActive,
    string LightId,
    string DarkId,
    bool IsEnhancedFindActive,
    bool IsWebUiFollowsThemeActive,
    bool IsLivePreviewActive,
    bool IsTabShapeActive,
    bool IsReadingThemeActive) {
  public static DllConfig From(Settings settings) => From(settings, settings.Features);

  // `features` as they apply on the current Edge build (a paused feature off).
  public static DllConfig From(Settings settings, Features features) => new(
      IsSplitActive: features.NativeSplit,
      IsThemingActive: features.Theming,
      IsProtocolHandlersActive: features.ProtocolHandlers,
      LightId: settings.Theming.Light,
      DarkId: settings.Theming.Dark,
      IsEnhancedFindActive: features.EnhancedFind,
      IsWebUiFollowsThemeActive: features.WebUiFollowsTheme,
      IsLivePreviewActive: features.LivePreview,
      IsTabShapeActive: features.TabShape,
      IsReadingThemeActive: features.ReadingTheme);
}

// Mirrors struct Wire in app/native-split/src/config.h field for field, every
// u32 little-endian as the DLL reads it on x64. The reserved u32 was once the
// chosen assistant: written as zero, never read.
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
  public const int TabShapeActiveOffset = LivePreviewActiveOffset + FieldBytes;
  public const int ReadingThemeActiveOffset = TabShapeActiveOffset + FieldBytes;
  public const int Size = ReadingThemeActiveOffset + FieldBytes;

  // What config.h static_asserts, from its own counts.
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
    WriteFlag(wire, TabShapeActiveOffset, config.IsTabShapeActive);
    WriteFlag(wire, ReadingThemeActiveOffset, config.IsReadingThemeActive);
    return wire;
  }

  private static void WriteFlag(byte[] wire, int offset, bool isOn) =>
      BinaryPrimitives.WriteUInt32LittleEndian(wire.AsSpan(offset), isOn ? 1u : 0u);

  private static void WriteId(byte[] wire, int lengthOffset, int idOffset, string id) {
    byte[] bytes = Encoding.UTF8.GetBytes(id);
    int length = Math.Min(bytes.Length, Shared.WireIdCapacity);
    BinaryPrimitives.WriteUInt32LittleEndian(wire.AsSpan(lengthOffset), (uint)length);
    bytes.AsSpan(0, length).CopyTo(wire.AsSpan(idOffset));
  }
}
