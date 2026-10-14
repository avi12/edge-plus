using System;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace FixEdge;

// Toggles the native N-pane split by loading (or unloading) fixedge-split.dll in
// Edge's browser process, live. This is the "no extension, DLL hot-swap" path:
// turning the switch on injects the DLL (LoadLibraryW on a remote thread, the
// same move any-stt makes); turning it off signals the DLL's unload event, and
// it removes its hooks and drops out of Edge without a restart.
internal static class NativeSplit {
  // Must match kUnloadEvent in app/native-split/src/dllmain.cpp.
  private const string UnloadEventName = @"Local\fixedge-split-unload";
  private const uint LoadLibraryTimeoutMilliseconds = 10000;

  // Edge keeps one browser process per profile; only it owns top-level windows,
  // so a main window handle tells the browser process from its renderers/GPU.
  private static Process[] EdgeBrowserProcesses() =>
      Process.GetProcessesByName(EdgeBrowser.ExecutableName)
          .Where(process => process.MainWindowHandle != IntPtr.Zero)
          .ToArray();

  public static bool IsEdgeRunning => EdgeBrowserProcesses().Length > 0;

  // The DLL holds its unload event open for as long as it is loaded, so the
  // event existing is exactly "the native split is active right now".
  public static bool IsActive() {
    try {
      using EventWaitHandle _ = EventWaitHandle.OpenExisting(UnloadEventName);
      return true;
    } catch (WaitHandleCannotBeOpenedException) {
      return false;
    }
  }

  // Loads the DLL into Edge's browser processes unless it is loaded already.
  // Returns true if Edge was there to inject into.
  public static bool Enable(string dllPath) {
    Process[] browsers = EdgeBrowserProcesses();
    bool isEdgeRunning = browsers.Length > 0;
    if (!isEdgeRunning) {
      return false;
    }
    // Loading it a second time would only leak a reference.
    if (IsActive()) {
      return true;
    }
    foreach (Process browser in browsers) {
      InjectLoadLibrary(browser.Id, dllPath);
    }
    return true;
  }

  // Signals the DLL to remove its hooks and unload itself.
  public static void Disable() {
    try {
      using EventWaitHandle unload = EventWaitHandle.OpenExisting(UnloadEventName);
      unload.Set();
    } catch (WaitHandleCannotBeOpenedException) {
      // Not loaded - nothing to do.
    }
  }

  private static void InjectLoadLibrary(int processId, string dllPath) {
    IntPtr process = OpenProcess(
        ProcessAccess.CreateThread | ProcessAccess.QueryInformation | ProcessAccess.VmOperation |
        ProcessAccess.VmWrite | ProcessAccess.VmRead,
        false, processId);
    if (process == IntPtr.Zero) {
      return;
    }
    try {
      byte[] pathBytes = Encoding.Unicode.GetBytes(dllPath + "\0");
      IntPtr remotePath = VirtualAllocEx(process, IntPtr.Zero, (uint)pathBytes.Length,
                                         AllocationType.Commit | AllocationType.Reserve,
                                         MemoryProtection.ReadWrite);
      if (remotePath == IntPtr.Zero) {
        return;
      }
      try {
        bool isPathWritten =
            WriteProcessMemory(process, remotePath, pathBytes, (uint)pathBytes.Length, out _);
        if (!isPathWritten) {
          return;
        }
        IntPtr loadLibrary = GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryW");
        IntPtr thread =
            CreateRemoteThread(process, IntPtr.Zero, 0, loadLibrary, remotePath, 0, out _);
        if (thread != IntPtr.Zero) {
          WaitForSingleObject(thread, LoadLibraryTimeoutMilliseconds);
          CloseHandle(thread);
        }
      } finally {
        VirtualFreeEx(process, remotePath, 0, FreeType.Release);
      }
    } finally {
      CloseHandle(process);
    }
  }

  [Flags]
  private enum ProcessAccess : uint {
    CreateThread = 0x0002,
    VmOperation = 0x0008,
    VmRead = 0x0010,
    VmWrite = 0x0020,
    QueryInformation = 0x0400,
  }

  [Flags]
  private enum AllocationType : uint {
    Commit = 0x1000,
    Reserve = 0x2000,
  }

  private enum FreeType : uint {
    Release = 0x8000,
  }

  private enum MemoryProtection : uint {
    ReadWrite = 0x04,
  }

  [DllImport("kernel32.dll", SetLastError = true)]
  private static extern IntPtr OpenProcess(ProcessAccess access, bool inheritHandle, int processId);
  [DllImport("kernel32.dll", SetLastError = true)]
  private static extern IntPtr VirtualAllocEx(
      IntPtr process, IntPtr address, uint size, AllocationType allocationType,
      MemoryProtection protection);
  [DllImport("kernel32.dll", SetLastError = true)]
  private static extern bool WriteProcessMemory(
      IntPtr process, IntPtr address, byte[] buffer, uint size, out IntPtr written);
  [DllImport("kernel32.dll", SetLastError = true)]
  private static extern IntPtr CreateRemoteThread(
      IntPtr process, IntPtr attributes, uint stackSize, IntPtr start, IntPtr parameter,
      uint flags, out uint threadId);
  [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi)]
  private static extern IntPtr GetModuleHandle(string name);
  [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi)]
  private static extern IntPtr GetProcAddress(IntPtr module, string name);
  [DllImport("kernel32.dll")]
  private static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);
  [DllImport("kernel32.dll")]
  private static extern bool VirtualFreeEx(
      IntPtr process, IntPtr address, uint size, FreeType freeType);
  [DllImport("kernel32.dll")]
  private static extern bool CloseHandle(IntPtr handle);
}
