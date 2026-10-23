using System;
using System.Runtime;
using System.Runtime.InteropServices;

namespace EdgePlus;

// The windowless listener only watches Edge, so the pages a window or startup
// touched go back to Windows until a window opens again.
internal static class ProcessTrim {
  // (-1, -1) tells SetProcessWorkingSetSize to empty the working set.
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
