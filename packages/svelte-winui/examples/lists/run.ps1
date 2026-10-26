# Runs the lists example in a background window (off screen, never activated):
# reads how many rows exist against how many items UI Automation sees realized,
# scrolls, saves a PrintWindow capture, and tries the keyboard reorder with a
# posted Alt+Down. Ends only the process it started.
param([string]$Out = "$env:TEMP\lists.png")
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
. "$PSScriptRoot\..\harness\window-tools.ps1"

function Count-Realized($hwnd) {
  $root = [System.Windows.Automation.AutomationElement]::FromHandle($hwnd)
  $condition = New-Object System.Windows.Automation.PropertyCondition ([System.Windows.Automation.AutomationElement]::ControlTypeProperty), ([System.Windows.Automation.ControlType]::ListItem)
  $root.FindAll([System.Windows.Automation.TreeScope]::Descendants, $condition).Count
}

$example = Start-Example 'examples/lists/main.ts'
try {
  $hwnd = Wait-Window $example
  $null = Ask $example @{ cmd = 'stats' } 'stats'
  Start-Sleep -Seconds 2
  $opened = Ask $example @{ cmd = 'stats' } 'stats'
  "opened:  $($opened.rows) rows, $($opened.live) row components live, $(Count-Realized $hwnd) list items visible to UI Automation"
  [ExampleWin]::Save($hwnd, $Out)
  $null = Ask $example @{ cmd = 'scroll'; key = 'row-900' } 'scrolled'
  Start-Sleep -Seconds 2
  $scrolled = Ask $example @{ cmd = 'stats' } 'stats'
  "scrolled: $($scrolled.created) created, $($scrolled.destroyed) ended, $($scrolled.live) live"

  # The keyboard path for real: focus a row, post Alt+Down to the window's input site.
  $null = Ask $example @{ cmd = 'scroll'; key = 'row-0' } 'scrolled'
  Start-Sleep -Seconds 1
  $null = Ask $example @{ cmd = 'focus'; key = 'row-2' } 'focused'
  'children: ' + ([ExampleWin]::Children($hwnd) -join '; ')
  $site = [ExampleWin]::FindChild($hwnd, 'Microsoft.UI.Content.DesktopChildSiteBridge')
  if ($site -ne [IntPtr]::Zero) { [ExampleWin]::PostAltKey($site, 0x28) } else { [ExampleWin]::PostAltKey($hwnd, 0x28) }
  Start-Sleep -Seconds 1
  $keyed = Ask $example @{ cmd = 'stats' } 'stats'
  "after a posted Alt+Down: reorders = $($keyed.reorders), first = $($keyed.first -join ',')"
} finally {
  Stop-Example $example
}
