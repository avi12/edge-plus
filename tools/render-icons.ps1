# Renders the extension's toolbar and store icons into src/public/icon, where
# WXT picks them up: a three-pane grid, the
# first pane in Edge's accent blue, the way the popup draws its layouts.
#   pwsh tools/render-icons.ps1
Add-Type -AssemblyName System.Drawing

$outputDirectory = Join-Path $PSScriptRoot '..\src\public\icon'
$accent = [Drawing.Color]::FromArgb(15, 108, 189)
$tile = [Drawing.Color]::FromArgb(120, 165, 205)
$sizes = 16, 32, 48, 128

New-Item -ItemType Directory -Force $outputDirectory | Out-Null
foreach ($size in $sizes) {
  $bitmap = New-Object Drawing.Bitmap $size, $size
  $graphics = [Drawing.Graphics]::FromImage($bitmap)
  $graphics.SmoothingMode = 'AntiAlias'
  $graphics.Clear([Drawing.Color]::Transparent)
  $unit = $size / 16
  $gap = [Math]::Max(1, $unit)
  $inset = $unit
  $full = $size - 2 * $inset
  $half = ($full - $gap) / 2
  $second = $inset + $half + $gap
  # In an array literal the comma binds tighter than arithmetic, hence the
  # precomputed values.
  $panes = @(
    @($accent, $inset, $inset, $half, $full),
    @($tile, $second, $inset, $half, $half),
    @($tile, $second, $second, $half, $half)
  )
  foreach ($pane in $panes) {
    $brush = New-Object Drawing.SolidBrush $pane[0]
    $graphics.FillRectangle($brush, [float]$pane[1], [float]$pane[2], [float]$pane[3], [float]$pane[4])
    $brush.Dispose()
  }
  $bitmap.Save((Join-Path $outputDirectory "$size.png"), [Drawing.Imaging.ImageFormat]::Png)
  $graphics.Dispose()
  $bitmap.Dispose()
}
