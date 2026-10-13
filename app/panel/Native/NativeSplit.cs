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

  // Edge keeps one browser process per profile; only it owns top-level windows,
  // so a main window handle tells the browser process from its renderers/GPU.
  private static Process[] EdgeBrowserProcesses() =>
      Process.GetProcessesByName("msedge")
          .Where(process => process.MainWindowHandle != IntPtr.Zero)
          .ToArray();

  public static bool IsEdgeRunning => EdgeBrowserProcesses().Length > 0;

  /// The DLL holds its unload event open for as long as it is loaded, so the
  /// event existing is exactly "the native split is active right now".
  public static bool IsActive() {
    try {
      using EventWaitHandle _ = EventWaitHandle.OpenExisting(UnloadEventName);
      return true;
    } catch (WaitHandleCannotBeOpenedException) {
      return false;
    }
  }

  /// Loads the DLL into every Edge browser process that does not have it yet.
  /// Returns true if Edge was there to inject into.
  public static bool Enable(string dllPath) {
    Process[] browsers = EdgeBrowserProcesses();
    if (browsers.Length == 0) {
      return false;
    }
    if (IsActive()) {
      return true;  // already loaded; LoadLibrary a second time would only leak a ref
    }
    foreach (Process browser in browsers) {
      InjectLoadLibrary(browser.Id, dllPath);
    }
    return true;
  }

  /// Signals the DLL to remove its hooks and unload itself.
  public static void Disable() {
    try {
      using EventWaitHandle unload = EventWaitHandle.OpenExisting(UnloadEventName);
      unload.Set();
    } catch (WaitHandleCannotBeOpenedException) {
      // Not loaded - nothing to do.
    }
  }

  private static void InjectLoadLibrary(int pid, string dllPath) {
    IntPtr process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE | PROCESS_VM_READ, false, pid);
    if (process == IntPtr.Zero) {
      return;
    }
    try {
      byte[] pathBytes = Encoding.Unicode.GetBytes(dllPath + "\0");
      IntPtr remote = VirtualAllocEx(process, IntPtr.Zero, (uint)pathBytes.Length,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
      if (remote == IntPtr.Zero) {
        return;
      }
      try {
        if (!WriteProcessMemory(process, remote, pathBytes, (uint)pathBytes.Length, out _)) {
          return;
        }
        IntPtr loadLibrary = GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryW");
        IntPtr thread = CreateRemoteThread(process, IntPtr.Zero, 0, loadLibrary, remote, 0, out _);
        if (thread != IntPtr.Zero) {
          WaitForSingleObject(thread, 10000);
          CloseHandle(thread);
        }
      } finally {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
      }
    } finally {
      CloseHandle(process);
    }
  }

  private const uint PROCESS_CREATE_THREAD = 0x0002;
  private const uint PROCESS_QUERY_INFORMATION = 0x0400;
  private const uint PROCESS_VM_OPERATION = 0x0008;
  private const uint PROCESS_VM_WRITE = 0x0020;
  private const uint PROCESS_VM_READ = 0x0010;
  private const uint MEM_COMMIT = 0x1000;
  private const uint MEM_RESERVE = 0x2000;
  private const uint MEM_RELEASE = 0x8000;
  private const uint PAGE_READWRITE = 0x04;

  [DllImport("kernel32.dll", SetLastError = true)]
  private static extern IntPtr OpenProcess(uint access, bool inheritHandle, int processId);
  [DllImport("kernel32.dll", SetLastError = true)]
  private static extern IntPtr VirtualAllocEx(IntPtr process, IntPtr address, uint size, uint allocationType, uint protect);
  [DllImport("kernel32.dll", SetLastError = true)]
  private static extern bool WriteProcessMemory(IntPtr process, IntPtr address, byte[] buffer, uint size, out IntPtr written);
  [DllImport("kernel32.dll", SetLastError = true)]
  private static extern IntPtr CreateRemoteThread(IntPtr process, IntPtr attributes, uint stackSize, IntPtr start, IntPtr parameter, uint flags, out uint threadId);
  [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi)]
  private static extern IntPtr GetModuleHandle(string name);
  [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Ansi)]
  private static extern IntPtr GetProcAddress(IntPtr module, string name);
  [DllImport("kernel32.dll")]
  private static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);
  [DllImport("kernel32.dll")]
  private static extern bool VirtualFreeEx(IntPtr process, IntPtr address, uint size, uint freeType);
  [DllImport("kernel32.dll")]
  private static extern bool CloseHandle(IntPtr handle);
}
