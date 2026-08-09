# Android assembleDebug verification (requires non-TSD NDK environment).
# See ANDROID.md for JDK/NDK/CMake prerequisites.

$Root = Split-Path -Parent $PSScriptRoot
$AndroidDir = Join-Path $Root "kys-promise-androidstudio"

if (-not (Test-Path $AndroidDir)) {
    Write-Error "Android project not found: $AndroidDir"
    exit 1
}

# TSD probe on NDK headers
$ndk = $env:ANDROID_HOME
if ($ndk) {
    $stdio = Get-ChildItem -Path (Join-Path $ndk "ndk") -Filter "stdio.h" -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($stdio) {
        $head = Get-Content $stdio.FullName -TotalCount 1
        if ($head -match "TSD-Header") {
            Write-Warning "NDK headers appear TSD-encrypted. assembleDebug will fail on this machine."
            Write-Warning "Use a non-TSD machine or CI. See ANDROID.md."
            exit 3
        }
    }
}

Push-Location $AndroidDir
try {
    if (-not (Test-Path "local.properties")) {
        if ($env:ANDROID_HOME) {
            "sdk.dir=$($env:ANDROID_HOME -replace '\\','/')" | Set-Content "local.properties" -Encoding ASCII
        }
    }
    .\gradlew.bat :app:assembleDebug
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
