# Update glaze headers from https://github.com/stephenberry/glaze

param(
    [string]$TempDir = "$env:TEMP\glaze-update"
)

Write-Host "Updating glaze headers..." -ForegroundColor Cyan

# Create temp directory
if (Test-Path $TempDir) {
    Remove-Item -Recurse -Force $TempDir
}
New-Item -ItemType Directory -Path $TempDir | Out-Null

try {
    # Clone with sparse-checkout to only get include/glaze
    Write-Host "Cloning glaze repository..." -ForegroundColor Yellow

    & git clone --filter=blob:none --sparse https://github.com/stephenberry/glaze.git $TempDir
    if ($LASTEXITCODE -ne 0) { throw "Failed to clone repository" }

    # Configure sparse-checkout to only get include/glaze
    Push-Location $TempDir
    & git sparse-checkout add include/glaze
    if ($LASTEXITCODE -ne 0) { throw "Failed to sparse-checkout" }
    Pop-Location

    # Remove old glaze headers
    $GlazeDir = "$PSScriptRoot\external\include\glaze"
    if (Test-Path $GlazeDir) {
        Remove-Item -Recurse -Force $GlazeDir
    }

    # Copy new headers
    Write-Host "Copying headers to external\include\glaze..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path (Split-Path $GlazeDir) -Force | Out-Null
    Copy-Item -Path "$TempDir\include\glaze" -Destination $GlazeDir -Recurse

    Write-Host "[OK] Glaze headers updated successfully!" -ForegroundColor Green
    Write-Host "Location: $GlazeDir" -ForegroundColor Green
}
catch {
    Write-Host "[ERROR] $_" -ForegroundColor Red
    exit 1
}
finally {
    # Cleanup
    if (Test-Path $TempDir) {
        Remove-Item -Recurse -Force $TempDir
    }
}
