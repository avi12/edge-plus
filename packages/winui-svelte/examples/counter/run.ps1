# Runs the counter example in a background window (off screen, never
# activated), clicks its buttons through UI Automation on that window only
# (found by the pid started here), reads the texts back through UI Automation,
# saves a PrintWindow capture and checks them. Ends only the process it started.
param([string]$Out = "$env:TEMP\counter.png")
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
$node = Start-Process node -WorkingDirectory $package -WindowStyle Hidden -PassThru -ArgumentList @('--disable-warning=ExperimentalWarning', 'examples/counter/launch.ts')
$automation = [System.Windows.Automation.AutomationElement]
$scope = [System.Windows.Automation.TreeScope]::Descendants
function Texts($root) {
  $condition = New-Object System.Windows.Automation.PropertyCondition ($automation::ControlTypeProperty), ([System.Windows.Automation.ControlType]::Text)
  ($root.FindAll($scope, $condition) | ForEach-Object { $_.Current.Name }) -join ' | '
}
function Press($root, $name) {
  $condition = New-Object System.Windows.Automation.PropertyCondition ($automation::NameProperty), $name
  $root.FindFirst($scope, $condition).GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
  Start-Sleep -Milliseconds 400
}
try {
  $hwnd = [IntPtr]::Zero
  for ($i = 0; $i -lt 40 -and $hwnd -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 500; $hwnd = [Win]::Find([uint32]$node.Id) }
  if ($hwnd -eq [IntPtr]::Zero) { throw 'no window' }
  [Win]::Hide($hwnd)
  $root = $automation::FromHandle($hwnd)
  Start-Sleep -Seconds 1
  $steps = [ordered]@{}
  $steps['start'] = Texts $root
  Press $root 'Host +1'; Press $root 'Host +1'
  $steps['after two host clicks'] = Texts $root
  Press $root 'Local +1'
  $steps['after a local click'] = Texts $root
  Press $root 'Host fails'
  $steps['after a refused action'] = Texts $root
  $steps.GetEnumerator() | ForEach-Object { "{0,-24} {1}" -f $_.Key, $_.Value }
  [Win]::Save($hwnd, $Out)
  $expected = @{
    'start' = 'host count: 0'
    'after two host clicks' = 'host count: 2 | host history length: 2'
    'after a local click' = 'local count: 1 (doubled 2) | buttons used: local'
    'after a refused action' = 'outcome: fail: the host does not do that'
  }
  $failed = $false
  foreach ($key in $expected.Keys) {
    if (-not $steps[$key].Contains($expected[$key])) { "FAIL $key : expected '$($expected[$key])'"; $failed = $true }
  }
  if ($failed) { exit 1 }
  'OK: host state, local state, context and rejected actions all update the controls'
} finally {
  Stop-Process -Id $node.Id -Force -ErrorAction SilentlyContinue
}
