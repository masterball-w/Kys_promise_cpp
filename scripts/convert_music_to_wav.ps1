# Convert music/*.(mid|ogg|mp3) -> music/*.wav for Android/Linux (SDL3 WAV path).
# Requires ffmpeg on PATH. Does not delete originals.
param(
    [string]$MusicDir = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not $MusicDir) {
    $MusicDir = Join-Path $root "game_data\music"
}
if (-not (Test-Path $MusicDir)) {
    Write-Error "Music dir not found: $MusicDir"
}

$ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
if (-not $ffmpeg) {
    Write-Error "ffmpeg not found on PATH. Install ffmpeg first."
}

$exts = @("*.mid", "*.midi", "*.ogg", "*.mp3")
$count = 0
foreach ($pat in $exts) {
    Get-ChildItem -Path $MusicDir -Filter $pat -File | ForEach-Object {
        $out = Join-Path $_.DirectoryName ($_.BaseName + ".wav")
        if (Test-Path $out) {
            Write-Host "skip (exists): $out"
            return
        }
        Write-Host "convert $($_.Name) -> $($_.BaseName).wav"
        & ffmpeg -y -i $_.FullName -acodec pcm_s16le -ar 44100 -ac 2 $out
        if ($LASTEXITCODE -eq 0) { $count++ }
    }
}
Write-Host "Done. Converted $count file(s)."
