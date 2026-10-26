# Runs the scheduling experiment in a background window (off screen, never
# activated), clicks its button through UI Automation on that window only
# (found by the pid started here), saves a PrintWindow capture and prints the
# log. Ends only the process it started.
param([string]$Out = "$env:TEMP\scheduling.png", [string]$Log = "$env:TEMP\scheduling.log")
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes, System.Drawing
if (-not ('Win' -as [type])) {
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices;
public static class Win {
  delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc p, IntPtr l);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  public struct RECT { public int L, T, R, B; }
  public static IntPtr Find(uint owner) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, l) => { uint pid; GetWindowThreadProcessId(h, out pid);
      if (pid == owner && IsWindowVisible(h)) { found = h; return false; } return true; }, IntPtr.Zero);
    return found;
  }
  // Far off screen, not activated (SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE).
  public static void Hide(IntPtr h) { SetWindowPos(h, IntPtr.Zero, -8000, 0, 0, 0, 0x0001 | 0x0004 | 0x0010); }
  public static void Save(IntPtr h, string path) { RECT r; GetWindowRect(h, out r);
    using (var b = new Bitmap(r.R - r.L, r.B - r.T)) { using (var g = Graphics.FromImage(b)) { var dc = g.GetHdc(); PrintWindow(h, dc, 2); g.ReleaseHdc(dc); } b.Save(path, ImageFormat.Png); } }
}
'@
}
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Remove-Item $Log -ErrorAction SilentlyContinue
$env:SCHEDULING_LOG = $Log
$package = Split-Path -Parent (Split-Path -Parent $here)
$node = Start-Process node -WorkingDirectory $package -WindowStyle Hidden -PassThru -ArgumentList @('--disable-warning=ExperimentalWarning', 'examples/scheduling/launch.ts')
try {
  $hwnd = [IntPtr]::Zero
  for ($i = 0; $i -lt 40 -and $hwnd -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 500; $hwnd = [Win]::Find([uint32]$node.Id) }
  if ($hwnd -eq [IntPtr]::Zero) { throw 'no window' }
  [Win]::Hide($hwnd)
  $root = [System.Windows.Automation.AutomationElement]::FromHandle($hwnd)
  $nameIs = { param($n) New-Object System.Windows.Automation.PropertyCondition ([System.Windows.Automation.AutomationElement]::NameProperty), $n }
  $button = $root.FindFirst([System.Windows.Automation.TreeScope]::Descendants, (& $nameIs 'Click via UIA'))
  Start-Sleep -Seconds 4   # the timer (1 s) and the host message (2.5 s) have fired
  for ($i = 0; $i -lt 3; $i++) {
    $button.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
    Start-Sleep -Milliseconds 900
  }
  [Win]::Save($hwnd, $Out)
  $lines = Get-Content $Log
  $lines
  # Svelte's own microtask applied every change: after each trigger the text
  # read in queueMicrotask differs from the one read synchronously.
  $failed = $false
  foreach ($trigger in 'timer', 'host message', 'click') {
    $before = ($lines | Where-Object { $_ -match "$trigger`: sync" } | Select-Object -First 1) -replace '.*count=', ''
    $after = ($lines | Where-Object { $_ -match "$trigger`: queueMicrotask" } | Select-Object -First 1) -replace '.*count=', ''
    if ($before -eq $after) { Write-Output "FAIL $trigger not applied by the microtask"; $failed = $true }
  }
  if ($failed) { exit 1 }
  Write-Output 'OK: Svelte applies state changes in WinUI callbacks with no flushSync'
} finally {
  Stop-Process -Id $node.Id -Force -ErrorAction SilentlyContinue
}
