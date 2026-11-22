<#
  Builds libcrawlermenu.so (arm64-v8a) with the Android NDK + CMake.
  Output: build\native\libcrawlermenu.so
#>
param([string]$AndroidHome = "C:\Android")
$ErrorActionPreference = "Stop"
$root  = Resolve-Path (Join-Path $PSScriptRoot "..")
$ndk   = Join-Path $AndroidHome "ndk\27.2.12479018"
$cmake = Join-Path $AndroidHome "cmake\3.22.1\bin\cmake.exe"
$ninja = Join-Path $AndroidHome "cmake\3.22.1\bin\ninja.exe"
$src   = Join-Path $root "native"
$out   = Join-Path $root "build\native"

if (-not (Test-Path (Join-Path $src "third_party\shadowhook\shadowhook\src\main\cpp\include\shadowhook.h"))) {
    throw "ShadowHook missing. Run: git submodule update --init"
}
& $cmake -S $src -B $out -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" `
    "-DCMAKE_TOOLCHAIN_FILE=$ndk\build\cmake\android.toolchain.cmake" `
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
& $cmake --build $out
if ($LASTEXITCODE -ne 0) { throw "Build failed" }
Write-Host "== Built $out\libcrawlermenu.so"
