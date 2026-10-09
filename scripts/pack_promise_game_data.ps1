# Sync / pack Promise (前传) assets into repo-root game_data/ and optional dist zip.
# Source default: local stable release 前传64位-sdl3 (override with -Source).
#
# Usage (repo root):
#   powershell -ExecutionPolicy Bypass -File scripts/pack_promise_game_data.ps1
#   powershell -ExecutionPolicy Bypass -File scripts/pack_promise_game_data.ps1 -Source "D:\path\to\前传64位-sdl3"
#   powershell -ExecutionPolicy Bypass -File scripts/pack_promise_game_data.ps1 -SkipZip
#
# Result:
#   - game_data/{resource,save,fight,eft,list,music,sound}  (stable location for GitHub code)
#   - dist/kys_promise_game_data/                           (clean portable tree)
#   - dist/kys_promise_game_data.zip                        (optional archive)
#   - build/Debug junctions via link_game_data.ps1

param(
    [string]$Source = '',
    [switch]$SkipZip,
    [switch]$SkipLink,
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Config = 'Debug'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$gameData = Join-Path $repoRoot 'game_data'
$distRoot = Join-Path $repoRoot 'dist'
$stage = Join-Path $distRoot 'kys_promise_game_data'
$dirs = @('resource', 'save', 'fight', 'eft', 'list', 'music', 'sound')

function Find-DefaultSource {
    $parent = Split-Path $repoRoot -Parent
    $candidates = @(
        (Join-Path $parent '金庸群侠前传1.22.3.21\前传64位-sdl3'),
        (Join-Path $parent '金庸群侠前传1.22.3.21/前传64位-sdl3'),
        (Join-Path $repoRoot '前传')
    )
    # Also accept any sibling folder matching *前传*64*
    Get-ChildItem -LiteralPath $parent -Directory -ErrorAction SilentlyContinue | ForEach-Object {
        $sub = Join-Path $_.FullName '前传64位-sdl3'
        if (Test-Path -LiteralPath $sub) { $candidates += $sub }
        $sub2 = Get-ChildItem -LiteralPath $_.FullName -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like '*sdl3*' -or $_.Name -like '*前传*64*' } |
            Select-Object -First 3
        foreach ($s in $sub2) { $candidates += $s.FullName }
    }
    foreach ($c in $candidates) {
        if (-not $c) { continue }
        $smp = Join-Path $c 'resource\smp'
        if ((Test-Path -LiteralPath $c) -and (Test-Path -LiteralPath $smp)) {
            # Prefer release tree over existing game_data
            if ($c -ne $gameData) { return $c }
        }
    }
    $smpGd = Join-Path $gameData 'resource\smp'
    if (Test-Path -LiteralPath $smpGd) { return $gameData }
    return $null
}

if (-not $Source) {
    $Source = Find-DefaultSource
}
if (-not $Source -or -not (Test-Path -LiteralPath $Source)) {
    Write-Error "Source not found. Pass -Source to a 前传 folder that contains resource/smp."
}
$smp = Join-Path $Source 'resource\smp'
if (-not (Test-Path -LiteralPath $smp)) {
    Write-Error "Incomplete source (missing resource\smp): $Source"
}

Write-Host "Source : $Source"
Write-Host "Target : $gameData"
Write-Host "Stage  : $stage"

New-Item -ItemType Directory -Force -Path $gameData | Out-Null
New-Item -ItemType Directory -Force -Path $distRoot | Out-Null
if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stage | Out-Null

function Invoke-Robo([string]$from, [string]$to) {
    if (-not (Test-Path -LiteralPath $from)) {
        Write-Warning "Skip missing source dir: $from"
        return
    }
    New-Item -ItemType Directory -Force -Path $to | Out-Null
    # Exclude backups / editor junk; keep game binaries only.
    & robocopy $from $to /E /XO /NFL /NDL /NJH /NJS /nc /ns /np `
        /XD editor .git `
        /XF *.bak *.corrupt_* *.tmp *.log *.pdb *.exe *.dll *.ico *.ini
    if ($LASTEXITCODE -ge 8) {
        throw "robocopy failed $from -> $to (exit $LASTEXITCODE)"
    }
}

foreach ($d in $dirs) {
    $from = Join-Path $Source $d
    $toGame = Join-Path $gameData $d
    $toStage = Join-Path $stage $d
    Write-Host "Sync $d ..."
    Invoke-Robo $from $toGame
    Invoke-Robo $from $toStage
}

# Prefer stable template names for ranger (keep both casings if present).
$rangerSrc = @(
    (Join-Path $Source 'save\Ranger.grp'),
    (Join-Path $Source 'save\ranger.grp')
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($rangerSrc) {
    $rangerBytes = [IO.File]::ReadAllBytes($rangerSrc)
    foreach ($name in @('Ranger.grp', 'ranger.grp')) {
        [IO.File]::WriteAllBytes((Join-Path $gameData "save\$name"), $rangerBytes)
        [IO.File]::WriteAllBytes((Join-Path $stage "save\$name"), $rangerBytes)
    }
    $idxSrc = Join-Path $Source 'save\ranger.idx'
    if (-not (Test-Path $idxSrc)) { $idxSrc = Join-Path $Source 'save\Ranger.idx' }
    if (Test-Path $idxSrc) {
        $idxBytes = [IO.File]::ReadAllBytes($idxSrc)
        foreach ($name in @('ranger.idx', 'Ranger.idx')) {
            [IO.File]::WriteAllBytes((Join-Path $gameData "save\$name"), $idxBytes)
            [IO.File]::WriteAllBytes((Join-Path $stage "save\$name"), $idxBytes)
        }
    }
}

# Drop corrupt backups from staged pack (keep game_data local backups as-is if already there).
Get-ChildItem -LiteralPath (Join-Path $stage 'save') -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '\.bak$|corrupt_' } |
    ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }

# Manifest
$manifest = @()
$manifest += "kys_promise game_data pack"
$manifest += "created=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$manifest += "source=$Source"
$manifest += "layout=resource,save,fight,eft,list,music,sound"
$manifest += "place_at=<repo>/game_data/ then run scripts/link_game_data.ps1"
$manifest += ""
foreach ($d in $dirs) {
    $p = Join-Path $stage $d
    if (Test-Path $p) {
        $n = (Get-ChildItem $p -Recurse -File -ErrorAction SilentlyContinue | Measure-Object).Count
        $manifest += ("{0}={1} files" -f $d, $n)
    }
}
$manifestPath = Join-Path $stage 'PACK_MANIFEST.txt'
$manifest | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $gameData 'PACK_MANIFEST.txt') -Force

# Verify staged + game_data
& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'verify_game_data.ps1') -Root $stage
if ($LASTEXITCODE -ne 0) { throw "stage verify failed" }
& powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'verify_game_data.ps1') -Root $gameData
if ($LASTEXITCODE -ne 0) { throw "game_data verify failed" }

if (-not $SkipZip) {
    $zip = Join-Path $distRoot 'kys_promise_game_data.zip'
    if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
    Write-Host "Zipping $zip ..."
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
    $zipSize = (Get-Item $zip).Length
    Write-Host ("Zip OK ({0:N1} MB)" -f ($zipSize / 1MB))
}

if (-not $SkipLink) {
    & powershell -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'link_game_data.ps1') -Config $Config
}

Write-Host ""
Write-Host "Done."
Write-Host "  Stable data : $gameData"
Write-Host "  Portable    : $stage"
if (-not $SkipZip) { Write-Host "  Archive     : $(Join-Path $distRoot 'kys_promise_game_data.zip')" }
Write-Host "  Run         : build\$Config\kys_cpp.exe"
