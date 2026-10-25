# Draws icons/edgeplus.svg's geometry into icons/icon.ico (PNG frames), the
# icon the core embeds and hands to the window.
# Keep the numbers in step with the SVG, which is the design source.
Add-Type -AssemblyName System.Drawing

$icons = Join-Path $PSScriptRoot '..\app\panel-tauri\src-tauri\icons'
$sizes = 16, 24, 32, 48, 64, 256
$viewBoxSize = 120.0

function New-Frame([int]$size) {
  $bitmap = New-Object System.Drawing.Bitmap $size, $size
  $g = [System.Drawing.Graphics]::FromImage($bitmap)
  $g.SmoothingMode = 'AntiAlias'
  $g.Clear([System.Drawing.Color]::Transparent)
  $k = $size / $viewBoxSize

  $tile = [System.Drawing.PointF[]]@(
    [System.Drawing.PointF]::new(18 * $k, 0), [System.Drawing.PointF]::new(120 * $k, 0),
    [System.Drawing.PointF]::new(120 * $k, 102 * $k), [System.Drawing.PointF]::new(102 * $k, 120 * $k),
    [System.Drawing.PointF]::new(0, 120 * $k), [System.Drawing.PointF]::new(0, 18 * $k))
  $navy = [System.Drawing.ColorTranslator]::FromHtml('#06182b')
  $g.FillPolygon((New-Object System.Drawing.SolidBrush $navy), $tile)

  # glyph space: translate(12 12) scale(.8)
  $m = New-Object System.Drawing.Drawing2D.Matrix
  $m.Scale($k, $k)
  $m.Translate(12, 12)
  $m.Scale(0.8, 0.8)
  $g.Transform = $m

  $ice = [System.Drawing.ColorTranslator]::FromHtml('#e2f4fa')
  $g.DrawLine((New-Object System.Drawing.Pen $ice, 14), 60, 14, 60, 106)

  $wavePath = New-Object System.Drawing.Drawing2D.GraphicsPath
  $wavePath.AddBezier(10, 60, 25, 28, 40, 28, 60, 60)
  $wavePath.AddBezier(60, 60, 80, 92, 95, 92, 110, 60)
  $gradient = New-Object System.Drawing.Drawing2D.LinearGradientBrush ([System.Drawing.PointF]::new(10, 28)), ([System.Drawing.PointF]::new(110, 92)), ([System.Drawing.ColorTranslator]::FromHtml('#6ee7b7')), ([System.Drawing.ColorTranslator]::FromHtml('#4d9fff'))
  $blend = New-Object System.Drawing.Drawing2D.ColorBlend 3
  $blend.Colors = [System.Drawing.Color[]]@([System.Drawing.ColorTranslator]::FromHtml('#6ee7b7'), [System.Drawing.ColorTranslator]::FromHtml('#00e5ff'), [System.Drawing.ColorTranslator]::FromHtml('#4d9fff'))
  $blend.Positions = [single[]]@(0, 0.5, 1)
  $gradient.InterpolationColors = $blend
  $g.DrawPath((New-Object System.Drawing.Pen $gradient, 14), $wavePath)

  $g.FillEllipse((New-Object System.Drawing.SolidBrush $navy), 54, 54, 12, 12)
  $g.Dispose()
  return $bitmap
}

$pngs = foreach ($size in $sizes) {
  $bitmap = New-Frame $size
  $stream = New-Object System.IO.MemoryStream
  $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
  $bitmap.Dispose()
  , $stream.ToArray()
}

$out = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter $out
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
  $dim = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
  $writer.Write([byte]$dim); $writer.Write([byte]$dim); $writer.Write([byte]0); $writer.Write([byte]0)
  $writer.Write([uint16]1); $writer.Write([uint16]32)
  $writer.Write([uint32]$pngs[$i].Length); $writer.Write([uint32]$offset)
  $offset += $pngs[$i].Length
}
foreach ($png in $pngs) { $writer.Write($png) }
$writer.Flush()
[System.IO.File]::WriteAllBytes((Join-Path $icons 'icon.ico'), $out.ToArray())
Write-Host "wrote icon.ico ($($sizes -join ', '))"
