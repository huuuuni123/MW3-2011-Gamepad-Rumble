# Builds both ASI plugins into .\dist using Zig (https://ziglang.org).
#   winget install zig.zig
#   .\build.ps1                                  -> dist\*.asi + MW3Rumble.ini
#   .\build.ps1 -AsiLoader <path\to\winmm.dll>   -> also dist\MW3-2011-Gamepad-Rumble-v<Version>.zip
# The release zip holds everything a player copies next to iw5sp.exe. Ultimate ASI Loader
# (ThirteenAG, MIT, x64 9.7.4 - the same winmm.dll MW3GamepadFix ships) is not kept in git.
param(
    [string]$AsiLoader,
    [string]$Version = '1.0'
)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force $dist | Out-Null

# Keep Zig's cache inside the repo; the default cache location can break the mingw libc build.
$cache = Join-Path $root '.zig-cache'
$env:ZIG_GLOBAL_CACHE_DIR = $cache
$env:ZIG_LOCAL_CACHE_DIR = $cache

if (-not (Get-Command zig -ErrorAction SilentlyContinue)) {
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'User') + ';' + $env:Path
}

foreach ($name in 'MW3KoreanBridge', 'MW3Rumble') {
    $src = Join-Path $root "src\$name.c"
    $out = Join-Path $dist "$name.asi"
    zig cc -target x86_64-windows-gnu -shared -O2 -s -Wall -o $out $src -lkernel32
    if ($LASTEXITCODE -ne 0) { throw "build failed: $name" }
    Write-Host "built $out"
}
Copy-Item (Join-Path $root 'config\MW3Rumble.ini') $dist -Force

if ($AsiLoader) {
    $stage = Join-Path $dist 'package'
    if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
    New-Item -ItemType Directory -Force (Join-Path $stage 'licenses') | Out-Null
    Copy-Item (Join-Path $dist 'MW3KoreanBridge.asi'), (Join-Path $dist 'MW3Rumble.asi'),
              (Join-Path $dist 'MW3Rumble.ini') $stage
    Copy-Item $AsiLoader (Join-Path $stage 'winmm.dll')
    Copy-Item (Join-Path $root 'licenses\Ultimate-ASI-Loader.txt') (Join-Path $stage 'licenses')
    Copy-Item (Join-Path $root 'LICENSE') (Join-Path $stage 'licenses\MW3-2011-Gamepad-Rumble.txt')
    Copy-Item (Join-Path $root 'config\README.txt') $stage
    $zip = Join-Path $dist "MW3-2011-Gamepad-Rumble-v$Version.zip"
    if (Test-Path $zip) { Remove-Item $zip }
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
    Write-Host "packaged $zip"
}
