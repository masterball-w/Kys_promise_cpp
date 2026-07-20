# Link (or copy) local game_data/ into build/<Config>/ for running kys_cpp.
# Assets are NOT in git. Keep them in repo-root game_data/ so deleting build/ is safe.
#
# Usage (from repo root):
#   powershell -ExecutionPolicy Bypass -File scripts/link_game_data.ps1
#   powershell -ExecutionPolicy Bypass -File scripts/link_game_data.ps1 -Config Release

param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Config = 'Debug',
    [switch]$CopyInsteadOfJunction
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$gameData = Join-Path $repoRoot 'game_data'
$runDir = Join-Path $repoRoot "build\$Config"
$dirs = @('resource', 'save', 'fight', 'eft', 'list', 'music', 'sound')

if (-not (Test-Path -LiteralPath $gameData)) {
    Write-Error "Missing game_data/. Place original assets there first (resource/save/fight/eft/...)."
}

if (-not (Test-Path -LiteralPath $runDir)) {
    New-Item -ItemType Directory -Force -Path $runDir | Out-Null
}

$smp = Join-Path $gameData 'resource\smp'
if (-not (Test-Path -LiteralPath $smp)) {
    Write-Error "game_data/resource/smp not found. Incomplete asset pack."
}

foreach ($d in $dirs) {
    $src = Join-Path $gameData $d
    $dst = Join-Path $runDir $d
    if (-not (Test-Path -LiteralPath $src)) {
        Write-Warning "Skip missing $d"
        continue
    }
    if (Test-Path -LiteralPath $dst) {
        $item = Get-Item -LiteralPath $dst -Force
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            cmd /c "rmdir `"$dst`"" | Out-Null
        } else {
            Remove-Item -LiteralPath $dst -Recurse -Force
        }
    }
    if ($CopyInsteadOfJunction) {
        Write-Host "Copy $d -> build\$Config\$d"
        & robocopy $src $dst /E /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
        if ($LASTEXITCODE -ge 8) { throw "robocopy failed for $d (exit $LASTEXITCODE)" }
    } else {
        Write-Host "Junction $d -> game_data\$d"
        cmd /c "mklink /J `"$dst`" `"$src`"" | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "mklink failed for $d" }
    }
}

Write-Host "Done. Run: $runDir\kys_cpp.exe"
