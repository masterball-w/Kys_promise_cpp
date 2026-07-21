# Push local game_data/ to Android shared storage: /sdcard/kys_promise/
# Usage (from repo root):
#   powershell -ExecutionPolicy Bypass -File scripts/push_game_data_android.ps1

param(
    [string]$Adb = "",
    [string]$RemoteDir = "/sdcard/kys_promise"
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$gameData = Join-Path $repoRoot 'game_data'

if (-not (Test-Path -LiteralPath (Join-Path $gameData 'resource\smp'))) {
    Write-Error "game_data/resource/smp not found. Copy assets to game_data/ first."
}

if (-not $Adb) {
    $candidates = @(
        "$env:ANDROID_HOME\platform-tools\adb.exe",
        "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe",
        'C:\Android\Sdk\platform-tools\adb.exe'
    )
    foreach ($c in $candidates) {
        if ($c -and (Test-Path -LiteralPath $c)) { $Adb = $c; break }
    }
}
if (-not $Adb -or -not (Test-Path -LiteralPath $Adb)) {
    Write-Error 'adb not found. Set ANDROID_HOME or pass -Adb path.'
}

& $Adb devices | Out-Host
& $Adb shell "mkdir -p `"$RemoteDir`"" | Out-Null

$dirs = @('resource', 'save', 'fight', 'eft', 'list', 'music', 'sound')
foreach ($d in $dirs) {
    $src = Join-Path $gameData $d
    if (-not (Test-Path -LiteralPath $src)) {
        Write-Warning "Skip missing $d"
        continue
    }
    Write-Host "Pushing $d -> $RemoteDir/$d"
    & $Adb push $src "${RemoteDir}/${d}"
    if ($LASTEXITCODE -ne 0) { throw "adb push failed for $d (exit $LASTEXITCODE)" }
}

Write-Host "Done. Game data on device: $RemoteDir"
