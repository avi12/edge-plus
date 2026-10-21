using System;
using Windows.UI;

namespace EdgePlus;

// The colour of each reading theme's swatch in Edge's Themes grid
// (learning_tools' theme table, docs/re/reading-mode-theme.md) — the chip a
// picker shows beside a theme's name. The Tauri panel's readingThemes.ts.
internal static class ReadingSwatches {
  private const byte Opaque = 0xFF;

  public static Color Of(ReadingTheme theme) => theme switch {
    ReadingTheme.Sepia => Rgb(0xF9F5E9),
    ReadingTheme.Light => Rgb(0xFCFCFC),
    ReadingTheme.Grey => Rgb(0xE6E6E6),
    ReadingTheme.Blue => Rgb(0x87FAFF),
    ReadingTheme.Yellow => Rgb(0xFEFF5C),
    ReadingTheme.Green => Rgb(0x91FFA6),
    ReadingTheme.Lavender => Rgb(0xD1BFEB),
    ReadingTheme.LightBlue => Rgb(0xB3CAEC),
    ReadingTheme.SkyBlue => Rgb(0xA3CFE4),
    ReadingTheme.LightGreen => Rgb(0xA5DA90),
    ReadingTheme.Black => Rgb(0x000000),
    ReadingTheme.DarkGrey => Rgb(0x242424),
    _ => throw new ArgumentOutOfRangeException(nameof(theme)),
  };

  private static Color Rgb(int rgb) =>
      Color.FromArgb(Opaque, (byte)(rgb >> 16), (byte)(rgb >> 8), (byte)rgb);
}
