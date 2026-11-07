# Runs `node <arguments>` on a desktop of its own (run-on-desktop.ps1) and
# waits, for a script that opens windows, so none of them reaches the desktop
# the person is working on. The output goes to the log file.
#
#   powershell -NoProfile -File on-desktop.ps1 -Log <file> -Arguments 'scripts/capture.ts x.png'
param(
  [Parameter(Mandatory)][string]$Log,
  [Parameter(Mandatory)][string]$Arguments
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

[string] $root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
[string] $node = (Get-Command node).Source
[string] $script = Join-Path ([IO.Path]::GetTempPath()) "edgeplus-desktop-$PID.cmd"
Set-Content -LiteralPath $script -Encoding ascii -Value @(
  '@echo off',
  "`"$node`" --disable-warning=ExperimentalWarning $Arguments > `"$Log`" 2>&1"
)
try {
  & powershell.exe -NoProfile -ExecutionPolicy Bypass `
    -File "$PSScriptRoot\run-on-desktop.ps1" `
    -CommandLine "cmd.exe /c `"$script`"" -WorkingDirectory $root
  [int] $code = $LASTEXITCODE
  Get-Content -LiteralPath $Log -ErrorAction SilentlyContinue | Select-Object -Last 60
  exit $code
} finally {
  Remove-Item -LiteralPath $script -ErrorAction SilentlyContinue
}
