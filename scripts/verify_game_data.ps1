# Verify Promise (前传) game_data layout required by kys_cpp / editor.
# Usage (repo root):
#   powershell -ExecutionPolicy Bypass -File scripts/verify_game_data.ps1
#   powershell -ExecutionPolicy Bypass -File scripts/verify_game_data.ps1 -Root path\to\game_data

param(
    [string]$Root = ''
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $Root) { $Root = Join-Path $repoRoot 'game_data' }

if (-not (Test-Path -LiteralPath $Root)) {
    Write-Error "Missing game data root: $Root"
}

$requiredDirs = @('resource', 'save', 'fight', 'eft', 'list')
$optionalDirs = @('music', 'sound')

$requiredFiles = @(
    'resource\smp',
    'resource\sdx',
    'resource\wmp',
    'resource\wdx',
    'resource\mmap.grp',
    'resource\mmap.idx',
    'resource\kdef.grp',
    'resource\kdef.idx',
    'resource\talk.grp',
    'resource\talk.idx',
    'resource\War.sta',
    'resource\Heads.Pic',
    'resource\Items.Pic',
    'resource\Begin.Pic',
    'resource\Background.Pic',
    'save\ranger.idx',
    'save\alldef.grp',
    'save\allsin.grp',
    'list\levelup.bin',
    'list\Set.bin'
)

# earth/surface/building: either casing
$layerAlts = @(
    @('resource\earth.002', 'resource\Earth.002'),
    @('resource\surface.002', 'resource\Surface.002'),
    @('resource\building.002', 'resource\Building.002'),
    @('resource\buildx.002', 'resource\Buildx.002'),
    @('resource\buildy.002', 'resource\Buildy.002'),
    @('save\ranger.grp', 'save\Ranger.grp'),
    @('resource\MMAP.COL', 'resource\mmap.col', 'resource\pallet.col', 'resource\Pallet.col')
)

$fail = 0
Write-Host "Verifying: $Root"

foreach ($d in $requiredDirs) {
    $p = Join-Path $Root $d
    if (-not (Test-Path -LiteralPath $p)) {
        Write-Host "FAIL dir missing: $d" -ForegroundColor Red
        $fail++
    } else {
        Write-Host "OK   dir $d"
    }
}
foreach ($d in $optionalDirs) {
    $p = Join-Path $Root $d
    if (-not (Test-Path -LiteralPath $p)) {
        Write-Host "WARN optional dir missing: $d" -ForegroundColor Yellow
    } else {
        Write-Host "OK   dir $d (optional)"
    }
}

foreach ($rel in $requiredFiles) {
    $p = Join-Path $Root $rel
    if (-not (Test-Path -LiteralPath $p)) {
        Write-Host "FAIL file missing: $rel" -ForegroundColor Red
        $fail++
    } else {
        $len = (Get-Item -LiteralPath $p).Length
        if ($len -le 0) {
            Write-Host "FAIL empty file: $rel" -ForegroundColor Red
            $fail++
        } else {
            Write-Host "OK   $rel ($len bytes)"
        }
    }
}

foreach ($alts in $layerAlts) {
    $hit = $null
    foreach ($rel in $alts) {
        $p = Join-Path $Root $rel
        if (Test-Path -LiteralPath $p) { $hit = $rel; break }
    }
    if (-not $hit) {
        Write-Host ("FAIL need one of: {0}" -f ($alts -join ' | ')) -ForegroundColor Red
        $fail++
    } else {
        Write-Host "OK   $hit"
    }
}

# Opening-template sanity: alldef/allsin size
$alldef = Join-Path $Root 'save\alldef.grp'
$allsin = Join-Path $Root 'save\allsin.grp'
if ((Test-Path $alldef) -and (Test-Path $allsin)) {
    $ad = (Get-Item $alldef).Length
    $as = (Get-Item $allsin).Length
    if ($ad -lt 400000 -or $as -lt 4000000) {
        Write-Host "WARN alldef/allsin size looks unusual (alldef=$ad allsin=$as)" -ForegroundColor Yellow
    }
}

if ($fail -gt 0) {
    Write-Host "`nVERIFY FAILED ($fail)" -ForegroundColor Red
    exit 1
}
Write-Host "`nVERIFY OK" -ForegroundColor Green
exit 0
