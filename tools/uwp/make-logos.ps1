<#
.SYNOPSIS
  Writes the placeholder logos of the UWP package (plain colour with a simple tile-grid diamond; no borrowed art).
.PARAMETER OutDir
  Folder to write the PNGs into (created).
#>
param([Parameter(Mandatory)][string]$OutDir)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force $OutDir | Out-Null

function New-Logo([string]$Name, [int]$W, [int]$H) {
  $bmp = New-Object System.Drawing.Bitmap $W, $H
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = 'AntiAlias'
  $g.Clear([System.Drawing.ColorTranslator]::FromHtml('#1F2A44'))
  # a diamond (one isometric floor tile) made of four smaller ones, centred, as large as the height allows
  $s = [Math]::Min($W, $H) * 0.62
  $cx = $W / 2.0; $cy = $H / 2.0
  $hw = $s / 2.0; $hh = $s / 4.0
  $light = New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml('#8FB4E8'))
  $mid = New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml('#5B83C4'))
  foreach ($row in 0, 1) {
    foreach ($col in 0, 1) {
      $ox = $cx + ($col - $row) * $hw / 2.0
      $oy = $cy - $hh + ($col + $row) * $hh / 2.0 - $hh / 2.0 * 0
      $pts = @(
        (New-Object System.Drawing.PointF $ox, ($oy - $hh / 2.0 * 0.92)),
        (New-Object System.Drawing.PointF ($ox + $hw / 2.0 * 0.92), $oy),
        (New-Object System.Drawing.PointF $ox, ($oy + $hh / 2.0 * 0.92)),
        (New-Object System.Drawing.PointF ($ox - $hw / 2.0 * 0.92), $oy))
      $brush = if ((($row + $col) % 2) -eq 0) { $light } else { $mid }
      $g.FillPolygon($brush, [System.Drawing.PointF[]]$pts)
    }
  }
  $g.Dispose()
  $bmp.Save((Join-Path $OutDir $Name), [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
}

New-Logo 'StoreLogo.png' 50 50
New-Logo 'Square44x44Logo.png' 44 44
New-Logo 'Square150x150Logo.png' 150 150
New-Logo 'Wide310x150Logo.png' 310 150
New-Logo 'SplashScreen.png' 620 300
