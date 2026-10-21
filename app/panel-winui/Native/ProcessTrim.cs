using System;
using System.Runtime;
using System.Runtime.InteropServices;

namespace EdgePlus;

// Hands memory back while the process has no window: the always-on listener
// only watches Edge, so the pages a window (or startup) touched go back to
// Windows, and come back on demand if a window opens again.
internal static class ProcessTrim {
  // The values that tell SetProcessWorkingSetSize to empty the working set.
  private static readonly IntPtr s_trimAll = new(-1);

  public static void Run() {
    GCSettings.LargeObjectHeapCompactionMode = GCLargeObjectHeapCompactionMode.CompactOnce;
    GC.Collect(GC.MaxGeneration, GCCollectionMode.Aggressive, blocking: true, compacting: true);
    GC.WaitForPendingFinalizers();
    _ = SetProcessWorkingSetSize(GetCurrentProcess(), s_trimAll, s_trimAll);
  }

  [DllImport("kernel32.dll")]
  private static extern IntPtr GetCurrentProcess();

  [DllImport("kernel32.dll", SetLastError = true)]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool SetProcessWorkingSetSize(IntPtr process, IntPtr minimum,
                                                      IntPtr maximum);
}
