# Saves a PNG of one process's window, as Windows draws it (PrintWindow, never
# the screen); the window is found by the pid the caller started.
#
#   powershell -NoProfile -File capture.ps1 -ProcessId <pid> -Path <file.png>
param(
  [Parameter(Mandatory)][int]$ProcessId,
  [Parameter(Mandatory)][string]$Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\window-tools.ps1"

$hwnd = [ExampleWin]::Find([uint32]$ProcessId)
if ($hwnd -eq [IntPtr]::Zero) { throw 'no window' }
[ExampleWin]::Save($hwnd, $Path)
