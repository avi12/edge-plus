using System;
using System.Runtime.InteropServices;
using Microsoft.UI.Xaml;

namespace EdgePlus;

// Rewrites WM_NCHITTEST for vertical-only resizing, and reports
// WM_SETTINGCHANGE, which is how the "Animation effects" switch is followed live.
internal static class WindowSubclass {
  private const uint WmNcHitTest = 0x0084;
  private const uint WmNcDestroy = 0x0082;
  private const uint WmSettingChange = 0x001A;
  private static readonly UIntPtr s_subclassId = new(0xED6E0001);

  // Held for the process's life: the window calls it until it is destroyed.
  private static readonly SubclassProc s_procedure = Procedure;

  // Raised on the window's thread.
  public static event Action? SettingChanged;

  // Must run on the window's thread.
  public static bool Attach(Window window) =>
      SetWindowSubclass(WinRT.Interop.WindowNative.GetWindowHandle(window), s_procedure,
                        s_subclassId, UIntPtr.Zero);

  private static IntPtr Procedure(IntPtr hwnd, uint message, UIntPtr wParam, IntPtr lParam,
                                  UIntPtr subclassId, UIntPtr referenceData) {
    if (message == WmNcDestroy) {
      RemoveWindowSubclass(hwnd, s_procedure, s_subclassId);
      return DefSubclassProc(hwnd, message, wParam, lParam);
    }
    IntPtr result = DefSubclassProc(hwnd, message, wParam, lParam);
    if (message == WmSettingChange) {
      SettingChanged?.Invoke();
    }
    // A negative answer (HTERROR, HTTRANSPARENT) is not a code to remap.
    bool isRemappable = message == WmNcHitTest && result >= 0;
    if (isRemappable) {
      return ResizeHandles.VerticalOnly((int)result);
    }
    return result;
  }

  private delegate IntPtr SubclassProc(IntPtr hwnd, uint message, UIntPtr wParam, IntPtr lParam,
                                       UIntPtr subclassId, UIntPtr referenceData);

  [DllImport("comctl32.dll")]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool SetWindowSubclass(IntPtr hwnd, SubclassProc procedure,
                                               UIntPtr subclassId, UIntPtr referenceData);

  [DllImport("comctl32.dll")]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool RemoveWindowSubclass(IntPtr hwnd, SubclassProc procedure,
                                                  UIntPtr subclassId);

  [DllImport("comctl32.dll")]
  private static extern IntPtr DefSubclassProc(IntPtr hwnd, uint message, UIntPtr wParam,
                                               IntPtr lParam);
}
