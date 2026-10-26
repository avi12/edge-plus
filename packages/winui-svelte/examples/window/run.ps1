# Runs the window example once per backdrop in a background window (off screen,
# never activated), reads back from WinUI which material the window holds, and
# saves a PrintWindow capture of each as <OutDir>\window-<backdrop>.png. Ends
# only the processes it started.
param([string]$OutDir = $env:TEMP, [string[]]$Backdrops = @('acrylic', 'mica', 'micaAlt', 'none'))
. "$PSScriptRoot\..\harness\window-tools.ps1"

foreach ($backdrop in $Backdrops) {
  $example = Start-Example 'examples/window/main.ts' @('--backdrop', $backdrop)
  try {
    $hwnd = Wait-Window $example
    $null = Ask $example @{ cmd = 'info' } 'info'
    Start-Sleep -Seconds 3
    $info = Ask $example @{ cmd = 'info' } 'info'
    $file = Join-Path $OutDir "window-$backdrop.png"
    [ExampleWin]::Save($hwnd, $file)
    "{0,-8} holds {1,-8} title bar extended: {2}, {3}x{4}, saved {5}" -f $backdrop, $info.backdrop, $info.extendsIntoTitleBar, $info.width, $info.height, $file
  } finally {
    Stop-Example $example
  }
}
