using System;
using System.Runtime.InteropServices;
using Windows.ApplicationModel;

namespace EdgePlus;

internal static class Packaging {
  public static bool IsPackaged {
    get;
  } = ReadIsPackaged();

  // Package.Current throws in a process with no package identity.
  private static bool ReadIsPackaged() {
    try {
      _ = Package.Current.Id;
      return true;
    } catch (Exception error) when (error is InvalidOperationException or COMException) {
      return false;
    }
  }
}
