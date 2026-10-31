# Runs a command on a desktop of its own and waits for it: the windows the
# command opens (and the windows of every process it starts) belong to that
# desktop object, which is never the input desktop, so none of them can take
# the foreground from the person at the keyboard. The desktop ends with the
# command. The exit code is the command's.
#
#   powershell -NoProfile -File run-on-desktop.ps1 `
#     -CommandLine '"C:\node.exe" test.js' -WorkingDirectory C:\work
#
# Only the shell's notification area (a tray icon) does not work there: it
# belongs to the shell on the default desktop.
param(
  [Parameter(Mandatory)][string]$CommandLine,
  [Parameter(Mandatory)][string]$WorkingDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class OwnDesktop {
  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
  public struct StartupInfo {
    public int Size;
    public string Reserved;
    public string Desktop;
    public string Title;
    public int X, Y, Width, Height, CountCharsX, CountCharsY, FillAttribute, Flags;
    public short ShowWindow, ReservedBytes;
    public IntPtr ReservedPointer, StandardInput, StandardOutput, StandardError;
  }

  [StructLayout(LayoutKind.Sequential)]
  public struct ProcessInformation {
    public IntPtr Process, Thread;
    public int ProcessId, ThreadId;
  }

  [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  public static extern IntPtr CreateDesktopW(string name, IntPtr device,
    IntPtr mode, int flags, uint access, IntPtr attributes);

  [DllImport("user32.dll")]
  public static extern bool CloseDesktop(IntPtr desktop);

  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  public static extern bool CreateProcessW(string application, string command,
    IntPtr processAttributes, IntPtr threadAttributes, bool inheritHandles,
    uint flags, IntPtr environment, string directory,
    ref StartupInfo startup, out ProcessInformation process);

  [DllImport("kernel32.dll")]
  public static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

  [DllImport("kernel32.dll")]
  public static extern bool GetExitCodeProcess(IntPtr handle, out uint code);

  [DllImport("kernel32.dll")]
  public static extern bool CloseHandle(IntPtr handle);
}
'@

$genericAll = 0x10000000
$createNoWindow = 0x08000000
$name = "winui-svelte-$PID"
$desktop = [OwnDesktop]::CreateDesktopW($name, [IntPtr]::Zero, [IntPtr]::Zero,
  0, $genericAll, [IntPtr]::Zero)
if ($desktop -eq [IntPtr]::Zero) {
  $code = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
  throw "CreateDesktop failed with $code"
}

try {
  $startup = New-Object OwnDesktop+StartupInfo
  $startup.Size = [Runtime.InteropServices.Marshal]::SizeOf($startup)
  $startup.Desktop = "winsta0\$name"
  $started = New-Object OwnDesktop+ProcessInformation
  # PowerShell turns $null into '' for a string argument; the application
  # name must be a real null so the command line names the program.
  $isStarted = [OwnDesktop]::CreateProcessW([NullString]::Value, $CommandLine,
    [IntPtr]::Zero, [IntPtr]::Zero, $false, $createNoWindow, [IntPtr]::Zero,
    $WorkingDirectory, [ref]$startup, [ref]$started)
  if (-not $isStarted) {
    $code = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw "CreateProcess failed with $code"
  }

  [void][OwnDesktop]::WaitForSingleObject($started.Process, [uint32]::MaxValue)
  $exitCode = 0
  [void][OwnDesktop]::GetExitCodeProcess($started.Process, [ref]$exitCode)
  [void][OwnDesktop]::CloseHandle($started.Process)
  [void][OwnDesktop]::CloseHandle($started.Thread)
  exit $exitCode
} finally {
  [void][OwnDesktop]::CloseDesktop($desktop)
}
