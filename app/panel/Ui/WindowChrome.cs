using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Media;
using Windows.Graphics;

namespace FixEdge;

// Dressed the way any-stt's panel is, so the two tools read as one family.
internal static class WindowChrome {
  public static void Apply(Window window, UIElement titleBar) {
    window.SystemBackdrop = new DesktopAcrylicBackdrop();
    window.ExtendsContentIntoTitleBar = true;
    window.SetTitleBar(titleBar);
    window.Title = Product.Name;
    if (window.AppWindow.Presenter is OverlappedPresenter presenter) {
      presenter.IsResizable = false;
      presenter.IsMaximizable = false;
    }
  }

  // Centre on the work area of the monitor the window landed on, clear of the
  // taskbar.
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
