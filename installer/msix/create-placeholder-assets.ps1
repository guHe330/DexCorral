# Create minimal placeholder PNG assets for MSIX package
# These should be replaced with actual branded icons

$Assets = @(
    @{Name="Square44x44Logo.png"; Width=44; Height=44},
    @{Name="Square150x150Logo.png"; Width=150; Height=150},
    @{Name="Wide310x150Logo.png"; Width=310; Height=150},
    @{Name="StoreLogo.png"; Width=50; Height=50}
)

Add-Type -AssemblyName System.Drawing

foreach ($Asset in $Assets) {
    $Path = Join-Path "Assets" $Asset.Name
    
    if (Test-Path $Path) {
        Write-Host "Skipping $($Asset.Name) - already exists" -ForegroundColor Yellow
        continue
    }
    
    Write-Host "Creating placeholder $($Asset.Name)..." -ForegroundColor Green

    # Extract dimensions as integers
    [int]$width = $Asset.Width
    [int]$height = $Asset.Height

    $bitmap = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias

    # Fill with a blue background
    $brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 0, 120, 215))
    $graphics.FillRectangle($brush, 0, 0, $width, $height)

    # Draw white text "DC" in center (DexCorral)
    [int]$minDim = [Math]::Min($width, $height)
    [float]$fontSize = $minDim / 3.0
    $font = New-Object System.Drawing.Font("Arial", $fontSize, [System.Drawing.FontStyle]::Bold)
    $textBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $text = "DC"
    $textSize = $graphics.MeasureString($text, $font)
    $x = ($width - $textSize.Width) / 2
    $y = ($height - $textSize.Height) / 2
    $graphics.DrawString($text, $font, $textBrush, $x, $y)

    # Save as PNG
    $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    
    $graphics.Dispose()
    $bitmap.Dispose()
    $brush.Dispose()
    $textBrush.Dispose()
    $font.Dispose()
    
    Write-Host "  Created: $Path" -ForegroundColor DarkGray
}

Write-Host ""
Write-Host "Placeholder assets created successfully!" -ForegroundColor Cyan
Write-Host "Replace these with actual DexCorral branded icons before distribution." -ForegroundColor Yellow
