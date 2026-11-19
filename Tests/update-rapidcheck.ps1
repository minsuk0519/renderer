# Update RapidCheck headers and sources from https://github.com/emil-e/rapidcheck

param(
    [string]$Commit = "2c3c4365aca21ef4e612768fdc95b1ce8b39a651",
    [string]$TempDir = "$env:TEMP\rapidcheck-update"
)

Write-Host "Updating RapidCheck to commit $Commit..." -ForegroundColor Cyan

if (Test-Path $TempDir) {
    Remove-Item -Recurse -Force $TempDir
}
New-Item -ItemType Directory -Path $TempDir | Out-Null

try {
    Write-Host "Fetching rapidcheck commit (shallow)..." -ForegroundColor Yellow

    & git init --quiet $TempDir
    if ($LASTEXITCODE -ne 0) { throw "Failed to initialize temp repository" }

    & git -C $TempDir remote add origin https://github.com/emil-e/rapidcheck.git
    if ($LASTEXITCODE -ne 0) { throw "Failed to add remote" }

    & git -C $TempDir fetch --depth 1 origin $Commit
    if ($LASTEXITCODE -ne 0) { throw "Failed to fetch commit $Commit" }

    & git -C $TempDir checkout --quiet FETCH_HEAD
    if ($LASTEXITCODE -ne 0) { throw "Failed to check out commit $Commit" }

    $RapidCheckDir = "$PSScriptRoot\external\rapidcheck"
    if (Test-Path $RapidCheckDir) {
        Remove-Item -Recurse -Force $RapidCheckDir
    }
    New-Item -ItemType Directory -Path $RapidCheckDir -Force | Out-Null

    Write-Host "Copying include, src and LICENSE.md to external\rapidcheck..." -ForegroundColor Yellow
    foreach ($entry in @("include", "src", "LICENSE.md")) {
        Copy-Item -Path "$TempDir\$entry" -Destination $RapidCheckDir -Recurse
    }

    Write-Host "[OK] RapidCheck updated successfully!" -ForegroundColor Green
    Write-Host "Location: $RapidCheckDir" -ForegroundColor Green
}
catch {
    Write-Host "[ERROR] $_" -ForegroundColor Red
    exit 1
}
finally {
    if (Test-Path $TempDir) {
        Remove-Item -Recurse -Force $TempDir
    }
}
