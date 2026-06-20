# Genera icon.ico: un punto de grabación rojo, en varios tamaños (16..256) para verse
# nítido en la barra de tareas y la bandeja del sistema.
Add-Type -AssemblyName System.Drawing

$sizes = 16, 20, 24, 32, 40, 48, 64, 128, 256
$pngStreams = @()

foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap($s, $s)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.Clear([System.Drawing.Color]::Transparent)

    # Punto de grabación rojo centrado, con un sutil gradiente vertical.
    [single]$dia = $s * 0.62
    [single]$x = ($s - $dia) / 2.0
    [single]$y = ($s - $dia) / 2.0

    $rect = New-Object System.Drawing.RectangleF($x, $y, $dia, $dia)
    $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $rect,
        [System.Drawing.Color]::FromArgb(255, 90, 70),
        [System.Drawing.Color]::FromArgb(205, 0, 0),
        90.0)
    $g.FillEllipse($grad, $rect)
    $grad.Dispose()

    # Borde blanco fino para contraste sobre fondos oscuros o claros.
    if ($s -ge 24) {
        [single]$penW = [Math]::Max(1.0, $s * 0.05)
        $white = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(235, 255, 255, 255), $penW)
        $g.DrawEllipse($white, $rect)
        $white.Dispose()
    }

    $g.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngStreams += ,($ms.ToArray())
    $bmp.Dispose()
}

# Empaquetar el contenedor .ico (cada imagen embebida como PNG).
$out = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($out)
$bw.Write([UInt16]0)            # reserved
$bw.Write([UInt16]1)            # type = icon
$bw.Write([UInt16]$sizes.Count) # image count

$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $data = $pngStreams[$i]
    $bw.Write([Byte]($(if ($s -ge 256) { 0 } else { $s })))  # width
    $bw.Write([Byte]($(if ($s -ge 256) { 0 } else { $s })))  # height
    $bw.Write([Byte]0)   # colors
    $bw.Write([Byte]0)   # reserved
    $bw.Write([UInt16]1) # planes
    $bw.Write([UInt16]32)# bpp
    $bw.Write([UInt32]$data.Length)
    $bw.Write([UInt32]$offset)
    $offset += $data.Length
}
foreach ($data in $pngStreams) { $bw.Write($data) }
$bw.Flush()
[System.IO.File]::WriteAllBytes("$PSScriptRoot\icon.ico", $out.ToArray())
Write-Host "icon.ico generado ($($sizes.Count) tamanos)."
