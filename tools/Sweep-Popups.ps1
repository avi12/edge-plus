<#
.SYNOPSIS
  Opens a catalogue of Edge popups one after another and checks each one's top
  border line against the Cyberpunk skin's rule.

.DESCRIPTION
  The rule (docs/re/cyberpunk-skin.md, "Menu border"): every popup's border
  carries a 1px red line on its top edge (like a CSS border), except a bubble
  that hangs from an anchor directly above it (a bubble with a top arrow:
  favorites, profile, site-info, downloads). Every menu has the line.

  Runs in a throwaway Edge (its own --user-data-dir and LOCALAPPDATA under
  app\dll\out\, found and killed by that marker only) with a private-unload-name
  copy of the DLL injected into that one pid, the skin forced on (channel code
  908) and dark (905). Input goes only to window handles enumerated from that
  pid and asserted before every post, never to a screen point; every popup is
  captured with PrintWindow. After each step every other window of the pid is
  listed: one the catalogue does not name is reported UNKNOWN with its top-row
  colour.

  It injects only into the Edge it starts, but `pnpm dev` injects into every
  Edge, so run it on a desktop of its own when that is running.

.EXAMPLE
  pwsh tools\Sweep-Popups.ps1
#>
[CmdletBinding()]
param(
  [string]$DllPath = '',
  [string]$InjectPath = '',
  [string]$EdgePath = 'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe',
  [string]$OutRoot = '',
  [string[]]$Only = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$buildDirectory = Join-Path $repositoryRoot 'app\dll\build'
if (-not $DllPath) {
  $DllPath = Join-Path $buildDirectory 'edgeplus-split.dll'
}
if (-not $InjectPath) {
  $InjectPath = Join-Path $buildDirectory 'inject.exe'
}
if (-not $OutRoot) {
  $OutRoot = Join-Path $repositoryRoot 'app\dll\out'
}

# The channel codes the DLL's control window takes (src/split/control.h).
enum ControlCode {
  SkinOn = 908
  Theme = 905
  Verbose = 906
}

enum StepKind {
  ClickMain
  RightClickMain
  ClickPopup
  Wait
}

# What the rule says of a popup's top edge.
enum TopLine {
  Line
  NoLine
}

enum Verdict {
  Pass
  Fail
  Unknown
}

# The skin's colours in dark: the line, and the menu's own ground it replaces.
[int]$script:LineRedMinimum = 0xC0
[int]$script:LineGreenMaximum = 0x60
[int]$script:EdgeStartSeconds = 3
[int]$script:PopupSettleMilliseconds = 2500
[int]$script:SubmenuSettleMilliseconds = 3000
[string]$script:MainClass = 'Chrome_WidgetWin_1'

class Step {
  [StepKind]$Kind
  [int]$X
  [int]$Y
  [int]$Rank
  [double]$Seconds

  static [Step] Click([int]$x, [int]$y) {
    $step = [Step]::new()
    $step.Kind = [StepKind]::ClickMain
    $step.X = $x
    $step.Y = $y
    return $step
  }

  static [Step] RightClick([int]$x, [int]$y) {
    $step = [Step]::new()
    $step.Kind = [StepKind]::RightClickMain
    $step.X = $x
    $step.Y = $y
    return $step
  }

  # Click inside the Rank-th popup, largest first.
  static [Step] ClickPopup([int]$rank, [int]$x, [int]$y) {
    $step = [Step]::new()
    $step.Kind = [StepKind]::ClickPopup
    $step.Rank = $rank
    $step.X = $x
    $step.Y = $y
    return $step
  }

  static [Step] Wait([double]$seconds) {
    $step = [Step]::new()
    $step.Kind = [StepKind]::Wait
    $step.Seconds = $seconds
    return $step
  }
}

class Popup {
  [string]$Name
  [Step[]]$Steps
  # One per popup window the steps leave open, largest first.
  [string[]]$WindowNames
  [TopLine[]]$Expected
}

class Row {
  [string]$Popup
  [string]$Size
  [string]$TopRow
  [string]$Expected
  [string]$Verdict
}

# Main window 1200x800 at 100,100, the layout the toolbar below is read from
# (a profile with the vertical tabs off): the ⋯ button, the favorites button,
# the profile button, the site-info icon, a tab and the page.
[int]$menuButtonX = 1073
[int]$favoritesButtonX = 1001
[int]$profileButtonX = 1037
[int]$siteInfoButtonX = 99
[int]$toolbarY = 60
[int]$tabX = 150
[int]$tabY = 20
[int]$pageX = 600
[int]$pageY = 400
# Rows of the ⋯ menu (client y, near the left edge of its window).
[int]$menuItemX = 100
[int]$tabGroupsY = 329
[int]$downloadsY = 361
[int]$extensionsY = 393
[int]$screenshotY = 569
[int]$moreToolsY = 633
[int]$helpY = 713

function New-Popup([string]$name, [Step[]]$steps, [string[]]$windows, [TopLine[]]$expected) {
  $popup = [Popup]::new()
  $popup.Name = $name
  $popup.Steps = $steps
  $popup.WindowNames = $windows
  $popup.Expected = $expected
  return $popup
}

$overflow = [Step]::Click($menuButtonX, $toolbarY)
$catalogue = [Popup[]]@(
  (New-Popup 'overflow menu' @($overflow) @('overflow menu') @([TopLine]::Line))
  (New-Popup 'overflow > More tools' @($overflow, [Step]::Wait(2), [Step]::ClickPopup(0, $menuItemX, $moreToolsY)) @('overflow menu', 'More tools') @([TopLine]::Line, [TopLine]::Line))
  (New-Popup 'overflow > Tab groups' @($overflow, [Step]::Wait(2), [Step]::ClickPopup(0, $menuItemX, $tabGroupsY)) @('overflow menu', 'Tab groups') @([TopLine]::Line, [TopLine]::Line))
  (New-Popup 'overflow > Extensions' @($overflow, [Step]::Wait(2), [Step]::ClickPopup(0, $menuItemX, $extensionsY)) @('overflow menu', 'Extensions') @([TopLine]::Line, [TopLine]::Line))
  (New-Popup 'overflow > Help and feedback' @($overflow, [Step]::Wait(2), [Step]::ClickPopup(0, $menuItemX, $helpY)) @('overflow menu', 'Help and feedback') @([TopLine]::Line, [TopLine]::Line))
  (New-Popup 'overflow > Downloads' @($overflow, [Step]::Wait(2), [Step]::ClickPopup(0, $menuItemX, $downloadsY)) @('downloads flyout') @([TopLine]::NoLine))
  (New-Popup 'overflow > Screenshot' @($overflow, [Step]::Wait(2), [Step]::ClickPopup(0, $menuItemX, $screenshotY)) @('capture toolbar') @([TopLine]::Line))
  (New-Popup 'tab context menu' @([Step]::RightClick($tabX, $tabY)) @('tab context menu') @([TopLine]::Line))
  (New-Popup 'page right-click menu' @([Step]::RightClick($pageX, $pageY)) @('page context menu') @([TopLine]::Line))
  (New-Popup 'favorites flyout' @([Step]::Click($favoritesButtonX, $toolbarY)) @('favorites flyout') @([TopLine]::NoLine))
  (New-Popup 'profile bubble' @([Step]::Click($profileButtonX, $toolbarY)) @('profile bubble') @([TopLine]::NoLine))
  (New-Popup 'site-info bubble' @([Step]::Click($siteInfoButtonX, $toolbarY)) @('site-info bubble') @([TopLine]::NoLine))
)

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System; using System.Collections.Generic; using System.Drawing; using System.Drawing.Imaging;
using System.Runtime.InteropServices; using System.Text;
public static class SweepWin {
  delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc p, IntPtr l);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h, StringBuilder n, int c);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessageW(string s);
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] static extern bool SetProcessDpiAwarenessContext(IntPtr c);
  public struct RECT { public int L, T, R, B; }
  public static void Dpi() { SetProcessDpiAwarenessContext(new IntPtr(-4)); }
  public static uint PidOf(IntPtr h) { uint p; GetWindowThreadProcessId(h, out p); return p; }
  public static List<IntPtr> Of(uint owner) {
    var l = new List<IntPtr>();
    EnumWindows((h, x) => { if (PidOf(h) == owner && IsWindowVisible(h)) l.Add(h); return true; }, IntPtr.Zero);
    return l;
  }
  public static string Cls(IntPtr h) { var s = new StringBuilder(256); GetClassName(h, s, 256); return s.ToString(); }
  public static int[] Size(IntPtr h) { RECT r; GetWindowRect(h, out r); return new[] { r.R - r.L, r.B - r.T }; }
  public static string TopRowMiddle(IntPtr h, string path) {
    RECT r; GetWindowRect(h, out r);
    using (var b = new Bitmap(r.R - r.L, r.B - r.T)) {
      using (var g = Graphics.FromImage(b)) { var dc = g.GetHdc(); PrintWindow(h, dc, 2); g.ReleaseHdc(dc); }
      b.Save(path, ImageFormat.Png);
      var c = b.GetPixel(b.Width / 2, 0);
      return string.Format("{0:x2}{1:x2}{2:x2}", c.R, c.G, c.B);
    }
  }
}
'@
[SweepWin]::Dpi()

function Get-Popups([uint32]$edgePid, [IntPtr]$main, [IntPtr[]]$known) {
  [SweepWin]::Of($edgePid) | Where-Object {
    $isPopup = $_ -ne $main -and [SweepWin]::Cls($_) -eq $script:MainClass -and $known -notcontains $_
    $isPopup
  } | Sort-Object { $size = [SweepWin]::Size($_); -($size[0] * $size[1]) }
}

function Assert-Mine([IntPtr]$hwnd, [uint32]$edgePid) {
  $owner = [SweepWin]::PidOf($hwnd)
  if ($owner -ne $edgePid) {
    throw "hwnd $hwnd belongs to pid $owner, not the test Edge $edgePid"
  }
}

function Send-Mouse([IntPtr]$hwnd, [uint32]$edgePid, [int]$x, [int]$y, [bool]$isRight) {
  Assert-Mine $hwnd $edgePid
  $lparam = [IntPtr](($y -shl 16) -bor $x)
  $down = if ($isRight) { 0x204 } else { 0x201 }
  $up = if ($isRight) { 0x205 } else { 0x202 }
  $flag = if ($isRight) { 2 } else { 1 }
  [void][SweepWin]::PostMessageW($hwnd, 0x200, [IntPtr]0, $lparam)
  Start-Sleep -Milliseconds 100
  [void][SweepWin]::PostMessageW($hwnd, $down, [IntPtr]$flag, $lparam)
  Start-Sleep -Milliseconds 100
  [void][SweepWin]::PostMessageW($hwnd, $up, [IntPtr]0, $lparam)
}

function Send-Escape([IntPtr]$hwnd, [uint32]$edgePid) {
  Assert-Mine $hwnd $edgePid
  [void][SweepWin]::PostMessageW($hwnd, 0x100, [IntPtr]0x1B, [IntPtr]1)
  Start-Sleep -Milliseconds 50
  [void][SweepWin]::PostMessageW($hwnd, 0x101, [IntPtr]0x1B, [IntPtr]1)
}

class EdgeSession {
  [string]$Marker
  [string]$Directory
  [System.Diagnostics.Process]$Process
  [uint32]$ProcessId
  [IntPtr]$Main
  [IntPtr[]]$Baseline
}

function Start-EdgeSession([string]$outRoot, [string]$dllPath, [string]$injectPath, [string]$edgePath) {
  $ticks = [DateTime]::UtcNow.Ticks
  $session = [EdgeSession]::new()
  $session.Marker = "re-sweep-$ticks"
  $session.Directory = Join-Path $outRoot $session.Marker
  New-Item -ItemType Directory -Force (Join-Path $session.Directory 'profile'), (Join-Path $session.Directory 'localappdata\EdgePlus') | Out-Null

  # A copy whose unload event is private: the session-wide one is signalled by
  # other Edge+ instances.
  $bytes = [IO.File]::ReadAllBytes($dllPath)
  $from = [Text.Encoding]::Unicode.GetBytes('edgeplus-split-unload')
  $privateName = ("edgeplus-sw$($ticks % 10000)-unload").PadRight(21, 'x').Substring(0, 21)
  $to = [Text.Encoding]::Unicode.GetBytes($privateName)
  for ($i = 0; $i -lt $bytes.Length - $from.Length; $i++) {
    $isMatch = $true
    for ($j = 0; $j -lt $from.Length; $j++) {
      if ($bytes[$i + $j] -ne $from[$j]) {
        $isMatch = $false
        break
      }
    }
    if ($isMatch) {
      [Array]::Copy($to, 0, $bytes, $i, $to.Length)
    }
  }
  $dll = Join-Path $outRoot "edgeplus-split-sweep-$ticks.dll"
  [IO.File]::WriteAllBytes($dll, $bytes)

  $info = New-Object Diagnostics.ProcessStartInfo $edgePath
  $profile = Join-Path $session.Directory 'profile'
  $info.Arguments = "--user-data-dir=`"$profile`" --no-first-run --no-default-browser-check --window-position=100,100 --window-size=1200,800 about:blank"
  $info.UseShellExecute = $false
  $info.EnvironmentVariables['LOCALAPPDATA'] = Join-Path $session.Directory 'localappdata'
  $session.Process = [Diagnostics.Process]::Start($info)
  $session.ProcessId = [uint32]$session.Process.Id

  $session.Main = [IntPtr]::Zero
  for ($tries = 0; $tries -lt 60 -and $session.Main -eq [IntPtr]::Zero; $tries++) {
    Start-Sleep -Milliseconds 500
    foreach ($each in [SweepWin]::Of($session.ProcessId)) {
      if ([SweepWin]::Cls($each) -eq $script:MainClass -and [SweepWin]::Size($each)[0] -eq 1200) {
        $session.Main = $each
      }
    }
  }
  if ($session.Main -eq [IntPtr]::Zero) {
    throw 'the test Edge opened no main window'
  }
  Start-Sleep -Seconds $script:EdgeStartSeconds
  & $injectPath $session.ProcessId $dll | Out-Null
  $log = Join-Path $session.Directory 'localappdata\EdgePlus\split-native.log'
  for ($tries = 0; $tries -lt 60; $tries++) {
    Start-Sleep -Seconds 1
    $isInjected = (Get-Content $log -Raw -ErrorAction SilentlyContinue) -match 'subclassed'
    if ($isInjected) {
      break
    }
  }
  Start-Sleep -Seconds 2
  Send-Control $session ([ControlCode]::SkinOn) 0
  Start-Sleep -Milliseconds 800
  Send-Control $session ([ControlCode]::Theme) 2
  Start-Sleep -Seconds 2
  $session.Baseline = @([SweepWin]::Of($session.ProcessId) | Where-Object { [SweepWin]::Cls($_) -eq $script:MainClass })
  return $session
}

function Send-Control([EdgeSession]$session, [ControlCode]$code, [int]$argument) {
  Assert-Mine $session.Main $session.ProcessId
  $message = [SweepWin]::RegisterWindowMessageW('edgeplus.addpane')
  [void][SweepWin]::PostMessageW($session.Main, $message, [IntPtr][int]$code, [IntPtr]$argument)
}

# Kills only the processes whose command line carries this session's marker.
function Stop-EdgeSession([EdgeSession]$session) {
  Get-CimInstance Win32_Process -Filter "Name='msedge.exe'" |
    Where-Object { $_.CommandLine -like "*$($session.Marker)*" } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
  if (-not $session.Process.HasExited) {
    Stop-Process -Id $session.Process.Id -Force -ErrorAction SilentlyContinue
  }
}

function Get-Remaining([string]$marker) {
  @(Get-CimInstance Win32_Process -Filter "Name='msedge.exe'" | Where-Object { $_.CommandLine -like "*$marker*" })
}

function Invoke-Steps([EdgeSession]$session, [Step[]]$steps) {
  foreach ($step in $steps) {
    switch ($step.Kind) {
      ([StepKind]::ClickMain) { Send-Mouse $session.Main $session.ProcessId $step.X $step.Y $false }
      ([StepKind]::RightClickMain) { Send-Mouse $session.Main $session.ProcessId $step.X $step.Y $true }
      ([StepKind]::Wait) { Start-Sleep -Milliseconds ([int]($step.Seconds * 1000)) }
      ([StepKind]::ClickPopup) {
        $popups = @(Get-Popups $session.ProcessId $session.Main $session.Baseline)
        if ($popups.Count -le $step.Rank) {
          throw "no popup of rank $($step.Rank) to click in"
        }
        Send-Mouse $popups[$step.Rank] $session.ProcessId $step.X $step.Y $false
      }
    }
  }
}

function Close-Popups([EdgeSession]$session) {
  for ($tries = 0; $tries -lt 4; $tries++) {
    $open = @(Get-Popups $session.ProcessId $session.Main $session.Baseline)
    if ($open.Count -eq 0) {
      return $true
    }
    foreach ($each in $open) {
      Send-Escape $each $session.ProcessId
    }
    Start-Sleep -Milliseconds 700
  }
  return @(Get-Popups $session.ProcessId $session.Main $session.Baseline).Count -eq 0
}

function Get-Verdict([string]$topRow, [TopLine]$expected) {
  $red = [Convert]::ToInt32($topRow.Substring(0, 2), 16)
  $green = [Convert]::ToInt32($topRow.Substring(2, 2), 16)
  $hasLine = $red -ge $script:LineRedMinimum -and $green -le $script:LineGreenMaximum
  $isAsExpected = $hasLine -eq ($expected -eq [TopLine]::Line)
  if ($isAsExpected) {
    return [Verdict]::Pass
  }
  return [Verdict]::Fail
}

$shots = Join-Path $OutRoot 'shots-re'
New-Item -ItemType Directory -Force $shots | Out-Null
$session = Start-EdgeSession $OutRoot $DllPath $InjectPath $EdgePath
$rows = [System.Collections.Generic.List[Row]]::new()
try {
  # Windows that were open before any step (Edge's first-run window) are
  # reported once, as the rule would judge them.
  $baselinePopups = @($session.Baseline | Where-Object { $_ -ne $session.Main })
  foreach ($each in $baselinePopups) {
    $row = [Row]::new()
    $row.Popup = 'first-run window (open from the start)'
    $size = [SweepWin]::Size($each)
    $row.Size = "$($size[0])x$($size[1])"
    $row.TopRow = '#' + [SweepWin]::TopRowMiddle($each, (Join-Path $shots 'sweep-baseline.png'))
    $row.Expected = [TopLine]::Line
    $row.Verdict = [string](Get-Verdict $row.TopRow.Substring(1) ([TopLine]::Line))
    $rows.Add($row)
  }

  foreach ($popup in $catalogue) {
    # -Only names a popup with underscores for its spaces: overflow_menu.
    $isSelected = $Only.Count -eq 0 -or $Only -contains ($popup.Name -replace ' ', '_')
    if (-not $isSelected) {
      continue
    }
    Invoke-Steps $session $popup.Steps
    Start-Sleep -Milliseconds $script:SubmenuSettleMilliseconds
    $open = @(Get-Popups $session.ProcessId $session.Main $session.Baseline)
    $slug = $popup.Name -replace '[^A-Za-z0-9]+', '-'
    for ($index = 0; $index -lt $open.Count; $index++) {
      $row = [Row]::new()
      $isNamed = $index -lt $popup.WindowNames.Count
      $row.Popup = if ($isNamed) { "$($popup.Name): $($popup.WindowNames[$index])" } else { "$($popup.Name): UNNAMED WINDOW" }
      $size = [SweepWin]::Size($open[$index])
      $row.Size = "$($size[0])x$($size[1])"
      $topRow = [SweepWin]::TopRowMiddle($open[$index], (Join-Path $shots "sweep-$slug-$index.png"))
      $row.TopRow = "#$topRow"
      if ($isNamed) {
        $row.Expected = [string]$popup.Expected[$index]
        $row.Verdict = [string](Get-Verdict $topRow $popup.Expected[$index])
      } else {
        $row.Expected = '?'
        $row.Verdict = [string][Verdict]::Unknown
      }
      $rows.Add($row)
    }
    if ($open.Count -lt $popup.WindowNames.Count) {
      $row = [Row]::new()
      $row.Popup = "$($popup.Name): only $($open.Count) of $($popup.WindowNames.Count) windows opened"
      $row.Size = '-'
      $row.TopRow = '-'
      $row.Expected = '-'
      $row.Verdict = [string][Verdict]::Fail
      $rows.Add($row)
    }
    $isClosed = Close-Popups $session
    if (-not $isClosed) {
      # A popup that would not close: start over in a fresh Edge.
      Stop-EdgeSession $session
      $session = Start-EdgeSession $OutRoot $DllPath $InjectPath $EdgePath
    }
  }
} finally {
  Stop-EdgeSession $session
}

$rows | Format-Table -AutoSize | Out-String | Write-Host
$remaining = @(Get-Remaining $session.Marker)
Write-Host "Edge processes left from this run: $($remaining.Count)"
$hasProblem = @($rows | Where-Object { $_.Verdict -ne [string][Verdict]::Pass }).Count -gt 0
if ($hasProblem) {
  exit 1
}
