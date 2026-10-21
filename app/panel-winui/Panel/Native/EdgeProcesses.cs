using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;

namespace EdgePlus;

// Finding Edge's browser processes, and what each has loaded.
//
// Edge runs one browser process per profile plus many renderer, GPU and utility
// children, all named msedge.exe. Only the browser process owns a top-level
// window, so the browser processes are the msedge.exe pids that own a visible,
// titled top-level window.
internal static class EdgeProcesses {
  private const string EdgeImageName = "msedge.exe";
  private const uint ProcessQueryLimitedInformation = 0x1000;
  private const uint SnapModule = 0x00000008;
  private const uint SnapModule32 = 0x00000010;
  private const int MaxModuleName = 256;
  private const int MaxPath = 260;
  private static readonly IntPtr s_invalidHandle = new(-1);

  // The pids of Edge's browser processes (one per open profile), or none when
  // Edge is not running.
  public static int[] BrowserPids() {
    var candidates = new HashSet<int>();
    EnumWindowsProc collect = (window, context) => {
      bool isShown = IsWindowVisible(window) && GetWindowTextLengthW(window) != 0;
      if (isShown) {
        _ = GetWindowThreadProcessId(window, out uint processId);
        if (processId != 0) {
          candidates.Add((int)processId);
        }
      }
      return true;
    };
    EnumWindows(collect, IntPtr.Zero);
    GC.KeepAlive(collect);
    return candidates.Where(IsEdgeProcess).ToArray();
  }

  // The browser processes that have a module named `moduleName` loaded.
  public static int[] PidsWithModule(string moduleName) =>
      BrowserPids().Where(pid => FindModule(pid, moduleName) is not null).ToArray();

  // The browser processes that do NOT have `moduleName` loaded — the ones an
  // injection still has to reach. Per process, so a second profile opened
  // after the DLL loaded is still injected.
  public static int[] PidsMissingModule(string moduleName) =>
      BrowserPids().Where(pid => FindModule(pid, moduleName) is null).ToArray();

  // The full path `pid` loaded `moduleName` from, or null when it is not
  // loaded. Edge keeps each version in its own folder, so this names the build
  // the process is actually running, not whatever is installed now.
  public static string? ModulePath(int pid, string moduleName) =>
      FindModule(pid, moduleName)?.ExePath;

  // Where `moduleName` is loaded in `pid`, or null when it is not.
  public static IntPtr? ModuleBase(int pid, string moduleName) =>
      FindModule(pid, moduleName)?.BaseAddress;

  private static ModuleEntry? FindModule(int pid, string moduleName) {
    IntPtr snapshot = CreateToolhelp32Snapshot(SnapModule | SnapModule32, (uint)pid);
    if (snapshot == s_invalidHandle) {
      return null;
    }
    try {
      var entry = new ModuleEntry { Size = (uint)Marshal.SizeOf<ModuleEntry>() };
      bool hasMore = Module32FirstW(snapshot, ref entry);
      while (hasMore) {
        bool isNamedModule = string.Equals(entry.ModuleName, moduleName,
                                           StringComparison.OrdinalIgnoreCase);
        if (isNamedModule) {
          return entry;
        }
        hasMore = Module32NextW(snapshot, ref entry);
      }
      return null;
    } finally {
      CloseHandle(snapshot);
    }
  }

  private static bool IsEdgeProcess(int pid) =>
      string.Equals(ImageBaseName(pid), EdgeImageName, StringComparison.OrdinalIgnoreCase);

  private static string? ImageBaseName(int pid) {
    IntPtr process = OpenProcess(ProcessQueryLimitedInformation, false, (uint)pid);
    if (process == IntPtr.Zero) {
      return null;
    }
    try {
      var buffer = new StringBuilder(MaxPath);
      uint length = (uint)buffer.Capacity;
      if (!QueryFullProcessImageNameW(process, 0, buffer, ref length)) {
        return null;
      }
      return Path.GetFileName(buffer.ToString());
    } finally {
      CloseHandle(process);
    }
  }

  private delegate bool EnumWindowsProc(IntPtr window, IntPtr lParam);

  // MODULEENTRY32W.
  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
  private struct ModuleEntry {
    public uint Size;
    public uint ModuleId;
    public uint ProcessId;
    public uint GlobalUsage;
    public uint ProcessUsage;
    public IntPtr BaseAddress;
    public uint BaseSize;
    public IntPtr Module;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = MaxModuleName)]
    public string ModuleName;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = MaxPath)]
    public string ExePath;
  }

  [DllImport("user32.dll")]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);

  [DllImport("user32.dll")]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool IsWindowVisible(IntPtr window);

  [DllImport("user32.dll")]
  private static extern int GetWindowTextLengthW(IntPtr window);

  [DllImport("user32.dll")]
  private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

  [DllImport("kernel32.dll", SetLastError = true)]
  private static extern IntPtr CreateToolhelp32Snapshot(uint flags, uint processId);

  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool Module32FirstW(IntPtr snapshot, ref ModuleEntry entry);

  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool Module32NextW(IntPtr snapshot, ref ModuleEntry entry);

  [DllImport("kernel32.dll", SetLastError = true)]
  private static extern IntPtr OpenProcess(uint access, bool inheritHandle, uint processId);

  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool QueryFullProcessImageNameW(
      IntPtr process, uint flags, StringBuilder name, ref uint size);

  [DllImport("kernel32.dll")]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool CloseHandle(IntPtr handle);
}
