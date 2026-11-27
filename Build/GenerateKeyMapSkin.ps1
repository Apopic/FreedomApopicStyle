param([string]$OutputPath = (Join-Path $PSScriptRoot '../Android/app/src/main/assets/Default/Image/Playing/KeyMapRounded.png'))
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
# Draw the simple skin panel directly at 2x resolution: no border or shadow.
$side=168
$bitmap=[Drawing.Bitmap]::new($side,$side*2,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics=[Drawing.Graphics]::FromImage($bitmap)
try {
    $graphics.Clear([Drawing.Color]::Transparent)
    $graphics.SmoothingMode=[Drawing.Drawing2D.SmoothingMode]::AntiAlias
    for($frame=0;$frame -lt 2;$frame++) {
        $x=3; $y=$frame*$side+3; $width=$side-6; $height=$side-6; $diameter=56
        $path=[Drawing.Drawing2D.GraphicsPath]::new()
        $brush=$null
        try {
            $path.AddArc($x,$y,$diameter,$diameter,180,90)
            $path.AddArc($x+$width-$diameter,$y,$diameter,$diameter,270,90)
            $path.AddArc($x+$width-$diameter,$y+$height-$diameter,$diameter,$diameter,0,90)
            $path.AddArc($x,$y+$height-$diameter,$diameter,$diameter,90,90)
            $path.CloseFigure()
            $top=if($frame -eq 0){[Drawing.Color]::FromArgb(145,105,113,121)}else{[Drawing.Color]::FromArgb(190,135,144,153)}
            $bottom=if($frame -eq 0){[Drawing.Color]::FromArgb(105,58,65,72)}else{[Drawing.Color]::FromArgb(155,78,88,98)}
            $brush=[Drawing.Drawing2D.LinearGradientBrush]::new([Drawing.RectangleF]::new($x,$y,$width,$height),$top,$bottom,[single]90)
            $graphics.FillPath($brush,$path)
        } finally { if($brush){$brush.Dispose()};$path.Dispose() }
    }
    $bitmap.Save([IO.Path]::GetFullPath($OutputPath),[Drawing.Imaging.ImageFormat]::Png)
} finally { $graphics.Dispose();$bitmap.Dispose() }