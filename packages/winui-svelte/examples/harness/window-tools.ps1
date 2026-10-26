# What the examples' run.ps1 scripts share: start an example as a process we own
# (examples/harness/launch.ts), talk to it in JSON lines, find its window by the
# pid we started (never by title or name), keep it off screen, capture it with
# PrintWindow (never the screen), and end only that pid.

$packageRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

if (-not ('ExampleWin' -as [type])) {
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System; using System.Collections.Generic; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices; using System.Text;
public static class ExampleWin {
  delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc p, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr parent, EnumProc p, IntPtr l);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] static extern bool SetProcessDpiAwarenessContext(IntPtr c);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr h, StringBuilder name, int count);
  public struct RECT { public int L, T, R, B; }
  public static IntPtr Find(uint owner) {
    SetProcessDpiAwarenessContext(new IntPtr(-4));
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, l) => { uint pid; GetWindowThreadProcessId(h, out pid);
      if (pid == owner && IsWindowVisible(h)) { found = h; return false; } return true; }, IntPtr.Zero);
    return found;
  }
  // Far off screen, not activated (SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE).
  public static void Hide(IntPtr h) { SetWindowPos(h, IntPtr.Zero, -8000, 0, 0, 0, 0x0001 | 0x0004 | 0x0010); }
  public static void Save(IntPtr h, string path) { RECT r; GetWindowRect(h, out r);
    using (var b = new Bitmap(r.R - r.L, r.B - r.T)) { using (var g = Graphics.FromImage(b)) { var dc = g.GetHdc(); PrintWindow(h, dc, 2); g.ReleaseHdc(dc); } b.Save(path, ImageFormat.Png); } }
  public static string[] Children(IntPtr parent) {
    var names = new List<string>();
    EnumChildWindows(parent, (h, l) => { var s = new StringBuilder(256); GetClassName(h, s, 256); names.Add(h.ToInt64() + " " + s); return true; }, IntPtr.Zero);
    return names.ToArray();
  }
  public static IntPtr FindChild(IntPtr parent, string className) {
    IntPtr found = IntPtr.Zero;
    EnumChildWindows(parent, (h, l) => { var s = new StringBuilder(256); GetClassName(h, s, 256); if (s.ToString() == className) { found = h; return false; } return true; }, IntPtr.Zero);
    return found;
  }
  // Alt+<key> as the keyboard sends it to a window (WM_SYSKEYDOWN with the
  // context bit, then WM_SYSKEYUP), without taking the foreground.
  public static void PostAltKey(IntPtr h, int vk) {
    PostMessage(h, 0x0104, new IntPtr(vk), new IntPtr(0x20000001));
    PostMessage(h, 0x0105, new IntPtr(vk), unchecked((IntPtr)(long)0xE0000001L));
  }
}
'@
}

function Start-Example {
  param([Parameter(Mandatory)][string]$Entry, [string[]]$Flags = @())
  $info = New-Object System.Diagnostics.ProcessStartInfo 'node'
  $info.WorkingDirectory = $packageRoot
  $info.Arguments = (@('--disable-warning=ExperimentalWarning', 'examples/harness/launch.ts', $Entry) + $Flags) -join ' '
  $info.UseShellExecute = $false
  $info.CreateNoWindow = $true
  $info.RedirectStandardInput = $true
  $info.RedirectStandardOutput = $true
  $process = [System.Diagnostics.Process]::Start($info)
  [pscustomobject]@{ Process = $process; Read = $null }
}

function Send-Command {
  param($Example, [hashtable]$Command)
  # Bytes, not WriteLine: Windows PowerShell's writer would put a BOM first.
  $bytes = [System.Text.Encoding]::UTF8.GetBytes(($Command | ConvertTo-Json -Compress) + "`n")
  $stream = $Example.Process.StandardInput.BaseStream
  $stream.Write($bytes, 0, $bytes.Length)
  $stream.Flush()
}

# The next report of `$Kind` (reports of other kinds are skipped).
function Read-Report {
  param($Example, [string]$Kind, [int]$Seconds = 90)
  $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
  while ([DateTime]::UtcNow -lt $deadline) {
    if ($null -eq $Example.Read) { $Example.Read = $Example.Process.StandardOutput.ReadLineAsync() }
    if ($Example.Read.Wait(200)) {
      $line = $Example.Read.Result
      $Example.Read = $null
      if ($null -eq $line) { throw 'the example ended' }
      if ($line.StartsWith('{')) {
        $report = $line | ConvertFrom-Json
        if ($report.kind -eq $Kind) { return $report }
      }
    }
  }
  throw "no '$Kind' report"
}

function Ask {
  param($Example, [hashtable]$Command, [string]$Kind)
  Send-Command $Example $Command
  Read-Report $Example $Kind
}

function Wait-Window {
  param($Example)
  $hwnd = [IntPtr]::Zero
  for ($i = 0; $i -lt 120 -and $hwnd -eq [IntPtr]::Zero; $i++) {
    Start-Sleep -Milliseconds 500
    $hwnd = [ExampleWin]::Find([uint32]$Example.Process.Id)
  }
  if ($hwnd -eq [IntPtr]::Zero) { throw 'no window' }
  [ExampleWin]::Hide($hwnd)
  $hwnd
}

function Stop-Example {
  param($Example)
  if (-not $Example.Process.HasExited) { Stop-Process -Id $Example.Process.Id -Force -ErrorAction SilentlyContinue }
}
