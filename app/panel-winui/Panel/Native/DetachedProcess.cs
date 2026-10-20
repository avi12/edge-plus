using System;
using System.Runtime.InteropServices;

namespace EdgePlus;

// Starting a process that outlives this one: detached, with no window, and
// broken away from any job this panel runs in (a terminal, an IDE, Edge's own
// job when Edge launched it), which would otherwise end it with the panel.
// Process.Start cannot ask for CREATE_BREAKAWAY_FROM_JOB.
internal static class DetachedProcess {
  public static bool Start(string commandLine) =>
      Create(commandLine, CreationFlags.BreakawayFromJob) || Create(commandLine, CreationFlags.None);

  // A job that forbids breaking away refuses the first form; the second still
  // detaches from the console and the window.
  private static bool Create(string commandLine, CreationFlags extraFlags) {
    var startupInfo = new StartupInfo { Size = Marshal.SizeOf<StartupInfo>() };
    CreationFlags flags = CreationFlags.Detached | CreationFlags.NoWindow | extraFlags;
    bool isStarted = CreateProcessW(null, commandLine, IntPtr.Zero, IntPtr.Zero, false, flags,
                                    IntPtr.Zero, null, ref startupInfo,
                                    out ProcessInformation information);
    if (isStarted) {
      CloseHandle(information.Process);
      CloseHandle(information.Thread);
    }
    return isStarted;
  }

  [Flags]
  private enum CreationFlags : uint {
    None = 0,
    Detached = 0x00000008,
    BreakawayFromJob = 0x01000000,
    NoWindow = 0x08000000,
  }

  // STARTUPINFOW.
  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
  private struct StartupInfo {
    public int Size;
    public IntPtr Reserved;
    public IntPtr Desktop;
    public IntPtr Title;
    public int X;
    public int Y;
    public int XSize;
    public int YSize;
    public int XCountChars;
    public int YCountChars;
    public int FillAttribute;
    public int Flags;
    public short ShowWindow;
    public short Reserved2;
    public IntPtr Reserved3;
    public IntPtr StandardInput;
    public IntPtr StandardOutput;
    public IntPtr StandardError;
  }

  // PROCESS_INFORMATION.
  [StructLayout(LayoutKind.Sequential)]
  private struct ProcessInformation {
    public IntPtr Process;
    public IntPtr Thread;
    public int ProcessId;
    public int ThreadId;
  }

  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool CreateProcessW(
      string? applicationName, string commandLine, IntPtr processAttributes,
      IntPtr threadAttributes, bool inheritHandles, CreationFlags creationFlags,
      IntPtr environment, string? currentDirectory, ref StartupInfo startupInfo,
      out ProcessInformation processInformation);

  [DllImport("kernel32.dll")]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool CloseHandle(IntPtr handle);
}
