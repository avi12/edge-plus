using System;
using System.Buffers.Binary;
using System.Text;

namespace EdgePlus;

internal static class ConfigWireTests {
  public static void Run() {
    Expect.Equal(180, ConfigWire.Size, "the wire is config.h's 180 bytes");
    Expect.Equal(ConfigWire.SizeConfigHExpects, ConfigWire.Size,
                 "the layout matches config.h's own field counts");

    byte[] wire = ConfigWire.Encode(new DllConfig(
        IsSplitActive: false,
        IsThemingActive: true,
        IsProtocolHandlersActive: true,
        LightId: "abc",
        DarkId: "",
        IsEnhancedFindActive: true,
        IsWebUiFollowsThemeActive: true,
        IsLivePreviewActive: true,
        IsTabSpineActive: true,
        IsReadingThemeActive: true,
        ReadingThemeLight: ReadingTheme.Lavender,
        ReadingThemeDark: ReadingTheme.DarkGrey));
    Expect.Equal(164, ConfigWire.TabSpineActiveOffset, "the tab spine at 164");
    Expect.Equal(1u, U32At(wire, 164), "tab spine on");
    Expect.Equal(168, ConfigWire.ReadingThemeActiveOffset, "the reading theme switch at 168");
    Expect.Equal(172, ConfigWire.ReadingThemeLightOffset, "the light reading theme at 172");
    Expect.Equal(176, ConfigWire.ReadingThemeDarkOffset, "the dark reading theme at 176");
    Expect.Equal(1u, U32At(wire, 168), "reading theme on");
    Expect.Equal(18u, U32At(wire, 172), "Lavender is Edge's 18");
    Expect.Equal(22u, U32At(wire, 176), "DarkGrey is Edge's 22");

    // Every theme goes on the wire as Edge's own number (reading_themes.h).
    (ReadingTheme Theme, uint Number)[] edgeNumbers = [
      (ReadingTheme.Sepia, 0), (ReadingTheme.Light, 1), (ReadingTheme.Grey, 2),
      (ReadingTheme.Black, 3), (ReadingTheme.Green, 4), (ReadingTheme.Blue, 5),
      (ReadingTheme.Yellow, 6), (ReadingTheme.LightGreen, 12), (ReadingTheme.SkyBlue, 16),
      (ReadingTheme.LightBlue, 17), (ReadingTheme.Lavender, 18), (ReadingTheme.DarkGrey, 22),
    ];
    foreach ((ReadingTheme theme, uint number) in edgeNumbers) {
      byte[] one = ConfigWire.Encode(new DllConfig(
          false, false, false, "", "", false, false, false, false, true, theme, theme));
      Expect.Equal(number, U32At(one, 172), $"{theme} is Edge's {number}");
    }
    byte[] defaults = ConfigWire.Encode(DllConfig.From(new Settings()));
    Expect.That(U32At(defaults, 168) == 0 && U32At(defaults, 172) == 0 && U32At(defaults, 176) == 3,
                "by default reading mode is off, with Sepia for light and Black for dark");
    Expect.Equal(0u, U32At(wire, 0), "split off at 0");
    Expect.Equal(1u, U32At(wire, 4), "theming on at 4");
    Expect.Equal(1u, U32At(wire, 8), "link handlers on at 8");
    Expect.Equal(3u, U32At(wire, 12), "the light id's length at 12");
    Expect.Equal(0u, U32At(wire, 16), "no dark id at 16");
    Expect.Equal("abc", Encoding.ASCII.GetString(wire, 20, 3), "the light id at 20");
    Expect.Equal(0u, U32At(wire, ConfigWire.ReservedOffset), "the reserved field is zero");
    Expect.Equal(152, ConfigWire.EnhancedFindActiveOffset, "find at 152");
    Expect.Equal(156, ConfigWire.WebUiFollowsThemeActiveOffset, "WebUI at 156");
    Expect.Equal(160, ConfigWire.LivePreviewActiveOffset, "live preview at 160");
    Expect.Equal(1u, U32At(wire, 152), "find on");
    Expect.Equal(1u, U32At(wire, 156), "WebUI on");
    Expect.Equal(1u, U32At(wire, 160), "live preview on");

    byte[] tooLong = ConfigWire.Encode(new DllConfig(
        false, false, false, new string('x', 100), "", false, false, false, false, false,
        ReadingTheme.Sepia, ReadingTheme.Black));
    Expect.Equal((uint)Shared.WireIdCapacity, U32At(tooLong, 12), "an id is cut to its field");
    Expect.Equal(0, tooLong[ConfigWire.DarkIdOffset], "a cut id does not spill into the next");

    var settings = new Settings();
    settings.Theming.Assign(ColorMode.Dark, "dark-id");
    DllConfig fromSettings = DllConfig.From(settings);
    Expect.That(fromSettings.IsSplitActive && !fromSettings.IsThemingActive,
                "the config follows the switches");
    Expect.Equal("dark-id", fromSettings.DarkId, "the config follows the assignment");
  }

  private static uint U32At(byte[] wire, int offset) =>
      BinaryPrimitives.ReadUInt32LittleEndian(wire.AsSpan(offset));
}
