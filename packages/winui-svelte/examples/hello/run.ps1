# Runs the hello example in a background window (off screen, never activated),
# flips its ToggleSwitch through UI Automation on that window only (found by
# the pid started here), reads the texts back, saves a PrintWindow capture and
# checks them. Ends only the process it started.
#
#   pwsh run.ps1 [-Out hello.png] [-Edit]
#
# -Edit also rewrites app.svelte while the window runs, to check that the open
# window follows the saved file (hot reload), and puts the file back.
param([string]$Out = "$env:TEMP\hello.png", [switch]$Edit)
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
$package = Split-Path -Parent (Split-Path -Parent $here)
$source = Join-Path $here 'App.svelte'
$original = [IO.File]::ReadAllText($source)
$env:WINUI_SVELTE_BACKGROUND = '1'
$log = Join-Path $env:TEMP 'winui-svelte-hello.log'
$node = Start-Process node -WorkingDirectory $package -WindowStyle Hidden -PassThru -RedirectStandardError $log -ArgumentList @('--disable-warning=ExperimentalWarning', 'examples/hello/main.ts')
$automation = [System.Windows.Automation.AutomationElement]
$scope = [System.Windows.Automation.TreeScope]::Descendants
function Texts($root) {
  $condition = New-Object System.Windows.Automation.PropertyCondition ($automation::ControlTypeProperty), ([System.Windows.Automation.ControlType]::Text)
  ($root.FindAll($scope, $condition) | ForEach-Object { $_.Current.Name }) -join ' | '
}
function Toggle($root) {
  $condition = New-Object System.Windows.Automation.PropertyCondition ($automation::ClassNameProperty), 'ToggleSwitch'
  $root.FindFirst($scope, $condition).GetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern).Toggle()
  Start-Sleep -Milliseconds 500
}
try {
  $hwnd = [IntPtr]::Zero
  for ($i = 0; $i -lt 60 -and $hwnd -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 500; $hwnd = [Win]::Find([uint32]$node.Id) }
  if ($hwnd -eq [IntPtr]::Zero) { throw 'no window' }
  [Win]::Hide($hwnd)
  $root = $automation::FromHandle($hwnd)
  Start-Sleep -Seconds 1
  $steps = [ordered]@{}
  $steps['start'] = Texts $root
  Toggle $root
  $steps['after a toggle'] = Texts $root
  [Win]::Save($hwnd, $Out)
  if ($Edit) {
    [IO.File]::WriteAllText($source, $original.Replace('Hello from WinUI', 'Hot reloaded'))
    Start-Sleep -Seconds 3
    $steps['after a saved edit'] = Texts $root
  }
  $steps.GetEnumerator() | ForEach-Object { "{0,-20} {1}" -f $_.Key, $_.Value }
  $expected = [ordered]@{
    'start' = 'The lights are on'
    'after a toggle' = 'The lights are off'
  }
  if ($Edit) { $expected['after a saved edit'] = 'Hot reloaded' }
  $failed = $false
  foreach ($key in $expected.Keys) {
    if (-not $steps[$key].Contains($expected[$key])) { "FAIL $key : expected '$($expected[$key])'"; $failed = $true }
  }
  if ($failed) { "--- the window's stderr ($log)"; Get-Content $log; exit 1 }
  'OK: the controls render, the binding flows back, and the window follows the file'
} finally {
  [IO.File]::WriteAllText($source, $original)
  Stop-Process -Id $node.Id -Force -ErrorAction SilentlyContinue
}
