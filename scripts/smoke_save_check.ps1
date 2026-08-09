# Save/layout smoke check — run after gameplay or editor changes.
# Usage: .\scripts\smoke_save_check.ps1 [save_dir]

param(
    [string]$SaveDir = ""
)

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build\Debug"
$Inspector = Join-Path $BuildDir "save_inspector.exe"

if (-not (Test-Path $Inspector)) {
    Write-Host "Building save_inspector..."
    cmake --build (Join-Path $Root "build") --config Debug --target save_inspector
}

if (-not $SaveDir) {
    foreach ($c in @(
        (Join-Path $Root "game_data\save"),
        (Join-Path $BuildDir "save"),
        (Join-Path $Root "build\Debug\save")
    )) {
        if (Test-Path $c) { $SaveDir = $c; break }
    }
}

if (-not $SaveDir -or -not (Test-Path $SaveDir)) {
    Write-Error "save directory not found"
    exit 1
}

Write-Host "=== save_inspector: $SaveDir ==="
& $Inspector $SaveDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Slot0 template protection
$alldef = Join-Path $SaveDir "alldef.grp"
if (Test-Path $alldef) {
    $bytes = [System.IO.File]::ReadAllBytes($alldef)
    if ($bytes.Length -lt 64) {
        Write-Error "alldef.grp too small — possible corruption"
        exit 2
    }
    Write-Host "[PASS] alldef.grp size OK ($($bytes.Length) bytes)"
}

Write-Host "=== smoke_save_check done ==="
