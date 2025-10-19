param(
    [string]$ApkPath = "releases/TIAGA-release.apk"
)

function Find-ApkSigner {
    # Check PATH first
    $apksigner = Get-Command apksigner -ErrorAction SilentlyContinue
    if ($apksigner) { return $apksigner.Path }

    # Check common ANDROID_SDK_ROOT and ANDROID_HOME locations
    $candidates = @()
    if ($Env:ANDROID_SDK_ROOT) { $candidates += Join-Path $Env:ANDROID_SDK_ROOT 'build-tools' }
    if ($Env:ANDROID_HOME) { $candidates += Join-Path $Env:ANDROID_HOME 'build-tools' }

    foreach ($base in $candidates | Where-Object { Test-Path $_ }) {
        Get-ChildItem -Path $base -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            $p = Join-Path $_.FullName 'apksigner.bat'
            if (Test-Path $p) { return $p }
            $p = Join-Path $_.FullName 'apksigner'
            if (Test-Path $p) { return $p }
        }
    }

    return $null
}

if (-not (Test-Path $ApkPath)) {
    Write-Host "APK not found at '$ApkPath'. Build it first with: flutter build apk --release" -ForegroundColor Yellow
    exit 2
}

$apksignerPath = Find-ApkSigner
if (-not $apksignerPath) {
    Write-Host "apksigner was not found on PATH or in ANDROID_SDK_ROOT/ANDROID_HOME build-tools." -ForegroundColor Yellow
    Write-Host "If you have the Android SDK installed, set the ANDROID_SDK_ROOT environment variable to your SDK root (e.g. C:\\Users\\<you>\\AppData\\Local\\Android\\sdk) and ensure build-tools are installed." -ForegroundColor Yellow
    Write-Host "You can also add the build-tools path to PATH, or run this script from a machine with Android SDK installed." -ForegroundColor Yellow
    exit 3
}

Write-Host "Using apksigner at: $apksignerPath" -ForegroundColor Green

& "$apksignerPath" verify --print-certs $ApkPath
$last = $LASTEXITCODE
if ($last -eq 0) { Write-Host "apksigner verified the APK successfully." -ForegroundColor Green } else { Write-Host "apksigner verification failed with exit code $last" -ForegroundColor Red }
exit $last
