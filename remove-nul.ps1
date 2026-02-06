# Powershell script to remove files named 'nul' in the current project recursively

# Function to check existence robustly
function Test-FileExists {
    param([string]$path)
    return [System.IO.File]::Exists($path)
}

$root = Get-Location
Write-Host "Searching for 'nul' files in: $root"

# Find all files recursively.
Get-ChildItem -Path $root -Recurse -Force -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -eq "nul" } | ForEach-Object {
    $fullPath = $_.FullName
    $uncPath = "\\?\$fullPath"
    
    Write-Host "Found 'nul' file at: $fullPath" -ForegroundColor Red

    # Method 1: .NET Delete
    try {
        Write-Host "Method 1: Attempting [System.IO.File]::Delete..."
        [System.IO.File]::Delete($uncPath)
    } catch {
        Write-Warning "Method 1 failed: $_"
    }

    if (Test-FileExists $uncPath) {
        Write-Host "File still exists. Trying Method 2..."
        
        # Method 2: CMD Delete
        $cmdArgs = "/c del ""$uncPath"""
        Start-Process -FilePath "cmd.exe" -ArgumentList $cmdArgs -Wait -NoNewWindow
    }

    if (Test-FileExists $uncPath) {
        Write-Host "File still exists. Trying Method 3 (Rename then Delete)..."
        
        # Method 3: Rename to safe name then delete
        $tempName = "$($_.DirectoryName)\delete_me_$(New-Guid)"
        $tempUnc = "\\?\$tempName"
        try {
            [System.IO.File]::Move($uncPath, $tempUnc)
            Write-Host "Renamed to: $tempName"
            [System.IO.File]::Delete($tempUnc)
            Write-Host "Deleted renamed file." -ForegroundColor Green
        } catch {
            Write-Warning "Method 3 failed: $_"
        }
    }

    # Final check
    if (Test-FileExists $uncPath) {
        Write-Host "FAILURE: Could not delete '$fullPath' after all attempts." -ForegroundColor Red
    } else {
        Write-Host "SUCCESS: '$fullPath' is gone." -ForegroundColor Green
    }
}

Write-Host "Operation complete."
