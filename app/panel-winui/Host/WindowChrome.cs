using System.IO;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Media;
using Windows.Graphics;

namespace EdgePlus;

// Dressed like any-stt's panel, so the two tools read as one family.
internal static class WindowChrome {
  // Copied beside the exe by EdgePlus.csproj's Content item.
  private static readonly string s_icon =
      Path.Combine(System.AppContext.BaseDirectory, "Assets", "edgeplus.ico");

  public static void Apply(Window window, UIElement titleBar) {
    window.SystemBackdrop = new DesktopAcrylicBackdrop();
    window.ExtendsContentIntoTitleBar = true;
    window.SetTitleBar(titleBar);
    window.Title = Product.Name;
    if (File.Exists(s_icon)) {
      window.AppWindow.SetIcon(s_icon);
    }
  }

  public static void CenterOnScreen(AppWindow appWindow) {
    var display = DisplayArea.GetFromWindowId(appWindow.Id, DisplayAreaFallback.Nearest);
    if (display is null) {
      return;
    }
    var area = display.WorkArea;
    appWindow.Move(new PointInt32(
        area.X + (area.Width - appWindow.Size.Width) / 2,
        area.Y + (area.Height - appWindow.Size.Height) / 2));
  }
}
