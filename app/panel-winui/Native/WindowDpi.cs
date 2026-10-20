using System;
using System.Runtime.InteropServices;
using Microsoft.UI.Xaml;

namespace EdgePlus;

// The DPI of the monitor a window is on. AppWindow sizes in physical pixels and
// the window's layout is in device-independent ones; this converts between them
// before any content has loaded (XamlRoot's scale is only known after).
internal static class WindowDpi {
  public static uint Of(Window window) =>
      GetDpiForWindow(WinRT.Interop.WindowNative.GetWindowHandle(window));

  [DllImport("user32.dll")]
  private static extern uint GetDpiForWindow(IntPtr hwnd);
}
