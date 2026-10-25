# Regenerate the geometric app icon. Tray geometry lives in createTrayIcon.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$artworkDir = Join-Path (Split-Path $PSScriptRoot) 'installer/artwork'
$bitmap = [System.Drawing.Bitmap]::new(1024,1024)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = 'AntiAlias'
$graphics.ScaleTransform(4,4)
$tile = [System.Drawing.Drawing2D.GraphicsPath]::new()
foreach ($corner in @(@(0,0,180),@(128,0,270),@(128,128,0),@(0,128,90))) {
    $tile.AddArc($corner[0],$corner[1],128,128,$corner[2],90)
}
$tile.CloseFigure()
$background = [System.Drawing.SolidBrush]::new([System.Drawing.ColorTranslator]::FromHtml('#101827'))
$cyan = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255,47,191,208))
$blue = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255,85,110,218))
$ring = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255,190,132,255),18)
$graphics.FillPath($background,$tile)
# Far arc, opaque planet, near arc: the hidden half must never cross the planet.
$graphics.TranslateTransform(128,128)
$graphics.RotateTransform(-40)
$graphics.DrawEllipse($ring,-124,-64,248,128)
$graphics.ResetTransform()
$graphics.ScaleTransform(4,4)
$graphics.FillEllipse($cyan,30,30,196,196)
$graphics.FillEllipse($blue,64,40,158,174)
$graphics.TranslateTransform(128,128)
$graphics.RotateTransform(-40)
$graphics.DrawArc($ring,-124,-64,248,128,0,180)
$graphics.Dispose()

$sizes = @(16,20,24,32,48,64,128,256)
$frames = foreach ($size in $sizes) {
    $frame = [System.Drawing.Bitmap]::new($size,$size)
    $scaled = [System.Drawing.Graphics]::FromImage($frame)
    $scaled.InterpolationMode = 'HighQualityBicubic'
    $scaled.PixelOffsetMode = 'Half'
    $scaled.DrawImage($bitmap,0,0,$size,$size)
    $scaled.Dispose()
    $stream = [System.IO.MemoryStream]::new()
    $frame.Save($stream,[System.Drawing.Imaging.ImageFormat]::Png)
    if ($size -eq 256) { $frame.Save((Join-Path $artworkDir 'worlds-icon.png'),[System.Drawing.Imaging.ImageFormat]::Png) }
    [pscustomobject]@{Size=$size;Bytes=$stream.ToArray()}
    $stream.Dispose()
    $frame.Dispose()
}
$output = [System.IO.File]::Create((Join-Path $artworkDir 'worlds.ico'))
$writer = [System.IO.BinaryWriter]::new($output)
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$frames.Count)
$offset = 6 + 16 * $frames.Count
foreach ($frame in $frames) {
    $dimension = [byte]($frame.Size % 256)
    $writer.Write($dimension); $writer.Write($dimension)
    $writer.Write([byte]0); $writer.Write([byte]0)
    $writer.Write([uint16]1); $writer.Write([uint16]32)
    $writer.Write([uint32]$frame.Bytes.Length); $writer.Write([uint32]$offset)
    $offset += $frame.Bytes.Length
}
foreach ($frame in $frames) { $writer.Write([byte[]]$frame.Bytes) }
$writer.Dispose()
$bitmap.Dispose(); $tile.Dispose(); $background.Dispose(); $cyan.Dispose(); $blue.Dispose(); $ring.Dispose()
Write-Output 'Updated worlds-icon.png and worlds.ico (16–256 px).'
