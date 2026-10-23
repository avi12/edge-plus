using System;
using System.Runtime.InteropServices;
using Microsoft.UI.Xaml;

namespace EdgePlus;

// AppWindow sizes in physical pixels; this gives the scale before any content
// has loaded, when XamlRoot's scale is not yet known.
internal static class WindowDpi {
  public static uint Of(Window window) =>
      GetDpiForWindow(WinRT.Interop.WindowNative.GetWindowHandle(window));

  [DllImport("user32.dll")]
  private static extern uint GetDpiForWindow(IntPtr hwnd);
}
