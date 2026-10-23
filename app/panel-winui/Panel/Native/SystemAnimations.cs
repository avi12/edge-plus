using System.Runtime.InteropServices;

namespace EdgePlus;

// Windows' "Animation effects" switch, read fresh each time so Motion follows it
// live.
internal static class SystemAnimations {
  private const uint GetClientAreaAnimation = 0x1042;

  public static bool IsOn() {
    bool isRead = SystemParametersInfoW(GetClientAreaAnimation, 0, out int isOn, 0);
    // Unreadable counts as on, Windows' own default.
    return !isRead || isOn != 0;
  }

  [DllImport("user32.dll", SetLastError = true)]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool SystemParametersInfoW(uint action, uint parameter, out int value,
                                                   uint update);
}
