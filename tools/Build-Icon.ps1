# Draws icons/edgeplus.svg's geometry into icons/icon.ico (PNG frames), the
# icon the core embeds and hands to the window.
# Keep the numbers in step with the SVG, which is the design source.
Add-Type -AssemblyName System.Drawing

[string] $icons = Join-Path $PSScriptRoot '..\app\panel\src-tauri\icons'
[int[]] $sizes = 16, 24, 32, 48, 64, 256
[double] $viewBoxSize = 120.0
[int] $iconDirectoryHeaderBytes = 6
[int] $iconDirectoryEntryBytes = 16
[int] $largestFrameSize = 256

function New-Frame {
  [OutputType([System.Drawing.Bitmap])]
  param([int] $size)
  [System.Drawing.Bitmap] $bitmap = [System.Drawing.Bitmap]::new($size, $size)
  [System.Drawing.Graphics] $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  $graphics.SmoothingMode = 'AntiAlias'
  $graphics.Clear([System.Drawing.Color]::Transparent)
  [double] $scale = $size / $viewBoxSize

  [System.Drawing.PointF[]] $tile = @(
    [System.Drawing.PointF]::new(18 * $scale, 0), [System.Drawing.PointF]::new(120 * $scale, 0),
    [System.Drawing.PointF]::new(120 * $scale, 102 * $scale), [System.Drawing.PointF]::new(102 * $scale, 120 * $scale),
    [System.Drawing.PointF]::new(0, 120 * $scale), [System.Drawing.PointF]::new(0, 18 * $scale))
  [System.Drawing.Color] $navy = [System.Drawing.ColorTranslator]::FromHtml('#06182b')
  $graphics.FillPolygon((New-Object System.Drawing.SolidBrush $navy), $tile)

  # glyph space: translate(12 12) scale(.8)
  [System.Drawing.Drawing2D.Matrix] $matrix = New-Object System.Drawing.Drawing2D.Matrix
  $matrix.Scale($scale, $scale)
  $matrix.Translate(12, 12)
  $matrix.Scale(0.8, 0.8)
  $graphics.Transform = $matrix

  [System.Drawing.Color] $ice = [System.Drawing.ColorTranslator]::FromHtml('#e2f4fa')
  $graphics.DrawLine((New-Object System.Drawing.Pen $ice, 14), 60, 14, 60, 106)

  [System.Drawing.Drawing2D.GraphicsPath] $wavePath = New-Object System.Drawing.Drawing2D.GraphicsPath
  $wavePath.AddBezier(10, 60, 25, 28, 40, 28, 60, 60)
  $wavePath.AddBezier(60, 60, 80, 92, 95, 92, 110, 60)
  [System.Drawing.Color] $mint = [System.Drawing.ColorTranslator]::FromHtml('#6ee7b7')
  [System.Drawing.Color] $cyan = [System.Drawing.ColorTranslator]::FromHtml('#00e5ff')
  [System.Drawing.Color] $blue = [System.Drawing.ColorTranslator]::FromHtml('#4d9fff')
  [System.Drawing.Drawing2D.LinearGradientBrush] $gradient = New-Object System.Drawing.Drawing2D.LinearGradientBrush ([System.Drawing.PointF]::new(10, 28)), ([System.Drawing.PointF]::new(110, 92)), $mint, $blue
  [System.Drawing.Drawing2D.ColorBlend] $blend = New-Object System.Drawing.Drawing2D.ColorBlend 3
  $blend.Colors = [System.Drawing.Color[]]@($mint, $cyan, $blue)
  $blend.Positions = [single[]]@(0, 0.5, 1)
  $gradient.InterpolationColors = $blend
  $graphics.DrawPath((New-Object System.Drawing.Pen $gradient, 14), $wavePath)

  $graphics.FillEllipse((New-Object System.Drawing.SolidBrush $navy), 54, 54, 12, 12)
  $graphics.Dispose()
  return $bitmap
}

[Collections.Generic.List[byte[]]] $pngs = [Collections.Generic.List[byte[]]]::new()
foreach ($size in $sizes) {
  [System.Drawing.Bitmap] $bitmap = New-Frame $size
  [System.IO.MemoryStream] $stream = New-Object System.IO.MemoryStream
  $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
  $bitmap.Dispose()
  $pngs.Add($stream.ToArray())
}

[System.IO.MemoryStream] $out = New-Object System.IO.MemoryStream
[System.IO.BinaryWriter] $writer = New-Object System.IO.BinaryWriter $out
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
[int] $offset = $iconDirectoryHeaderBytes + $iconDirectoryEntryBytes * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
  # The directory spells 256 as 0.
  [int] $dim = if ($sizes[$i] -ge $largestFrameSize) { 0 } else { $sizes[$i] }
  $writer.Write([byte]$dim); $writer.Write([byte]$dim); $writer.Write([byte]0); $writer.Write([byte]0)
  $writer.Write([uint16]1); $writer.Write([uint16]32)
  $writer.Write([uint32]$pngs[$i].Length); $writer.Write([uint32]$offset)
  $offset += $pngs[$i].Length
}
foreach ($png in $pngs) {
  $writer.Write($png)
}
$writer.Flush()
[System.IO.File]::WriteAllBytes((Join-Path $icons 'icon.ico'), $out.ToArray())
Write-Host "wrote icon.ico ($($sizes -join ', '))"
