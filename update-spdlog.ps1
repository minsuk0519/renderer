# Update spdlog headers from https://github.com/gabime/spdlog

param(
    [string]$TempDir = "$env:TEMP\spdlog-update"
)

Write-Host "Updating spdlog headers..." -ForegroundColor Cyan

# Create temp directory
if (Test-Path $TempDir) {
    Remove-Item -Recurse -Force $TempDir
}
New-Item -ItemType Directory -Path $TempDir | Out-Null

try {
    # Clone with sparse-checkout to only get include/spdlog
    Write-Host "Cloning spdlog repository..." -ForegroundColor Yellow

    & git clone --filter=blob:none --sparse --branch v1.x https://github.com/gabime/spdlog.git $TempDir
    if ($LASTEXITCODE -ne 0) { throw "Failed to clone repository" }

    # Configure sparse-checkout to only get include/spdlog
    Push-Location $TempDir
    & git sparse-checkout add include/spdlog
    if ($LASTEXITCODE -ne 0) { throw "Failed to sparse-checkout" }
    Pop-Location

    # Remove old spdlog headers
    $SpdlogDir = "$PSScriptRoot\external\include\spdlog"
    if (Test-Path $SpdlogDir) {
        Remove-Item -Recurse -Force $SpdlogDir
    }

    # Copy new headers
    Write-Host "Copying headers to external\include\spdlog..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path (Split-Path $SpdlogDir) -Force | Out-Null
    Copy-Item -Path "$TempDir\include\spdlog" -Destination $SpdlogDir -Recurse

    Write-Host "[OK] Spdlog headers updated successfully!" -ForegroundColor Green
    Write-Host "Location: $SpdlogDir" -ForegroundColor Green
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
