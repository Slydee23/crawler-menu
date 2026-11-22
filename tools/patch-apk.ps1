<#
  Patches the user's own Vampire Crawlers APK with libcrawlermenu.so.
  Output: build\patched.apk (signed with a local debug key). Never distribute the result.

  Usage:  powershell -ExecutionPolicy Bypass -File tools\patch-apk.ps1 -Apk game-files\<your>.apk
  Needs:  JDK 17, apktool, Android build-tools (zipalign, apksigner), NDK + CMake (for the native lib) in C:\Android
#>
param(
    [Parameter(Mandatory = $true)][string]$Apk,
    [string]$AndroidHome = "C:\Android",
    [switch]$SkipNativeBuild
)
$ErrorActionPreference = "Stop"
$root    = Resolve-Path (Join-Path $PSScriptRoot "..")
$build   = Join-Path $root "build"
$decoded = Join-Path $build "decoded"
$apktool = Join-Path $AndroidHome "tools\apktool.jar"
$bt      = Join-Path $AndroidHome "build-tools\35.0.0"
$apkPath = Resolve-Path $Apk
if (-not $env:JAVA_HOME) { $env:JAVA_HOME = "C:\Program Files\Eclipse Adoptium\jdk-17.0.21.10002-hotspot" }
$java    = Join-Path $env:JAVA_HOME "bin\java.exe"
$keytool = Join-Path $env:JAVA_HOME "bin\keytool.exe"

function Run($exe, $argList) {
    & $exe @argList
    if ($LASTEXITCODE -ne 0) { throw "$exe failed with exit code $LASTEXITCODE" }
}

# 1. Native library
if (-not $SkipNativeBuild) {
    Write-Host "== Building native library"
    & (Join-Path $PSScriptRoot "build-native.ps1") -AndroidHome $AndroidHome
}
$lib = Join-Path $build "native\libcrawlermenu.so"
if (-not (Test-Path $lib)) { throw "Native library not found: $lib" }

# 2. Decode (no resources needed)
Write-Host "== Decoding APK"
Run $java @("-jar", $apktool, "d", "-r", "-f", "-o", $decoded, $apkPath)

# 3. Add our library
Copy-Item $lib (Join-Path $decoded "lib\arm64-v8a\libcrawlermenu.so") -Force
# ShadowHook loads this empty helper during init
Copy-Item (Join-Path $build "native\libshadowhook_nothing.so") (Join-Path $decoded "lib\arm64-v8a\libshadowhook_nothing.so") -Force

# 4. Load it first thing in the app: static initializer of the Application class
$appSmali = Join-Path $decoded "smali\com\pairip\application\Application.smali"
$text = [System.IO.File]::ReadAllText($appSmali)
$marker = "# crawlermenu"
if ($text.Contains($marker)) { throw "Already patched" }
$re = [regex]'(?s)(\.method static constructor <clinit>\(\)V\s+\.locals \d+\s*\n)'
if (-not $re.IsMatch($text)) { throw "Could not find <clinit> in Application.smali (game layout changed?)" }
$inject = "`n    $marker`n    const-string v0, `"crawlermenu`"`n    invoke-static {v0}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V`n"
$text = $re.Replace($text, { param($m) $m.Value + $inject }, 1)
[System.IO.File]::WriteAllText($appSmali, $text, (New-Object System.Text.UTF8Encoding($false)))

# 5. Rebuild, align, sign
Write-Host "== Rebuilding"
$unsigned = Join-Path $build "unsigned.apk"
$aligned  = Join-Path $build "aligned.apk"
$patched  = Join-Path $build "patched.apk"
Run $java @("-jar", $apktool, "b", "-f", "-o", $unsigned, $decoded)
if (Test-Path $aligned) { Remove-Item $aligned -Force }
Run (Join-Path $bt "zipalign.exe") @("-p", "4", $unsigned, $aligned)

$ks = Join-Path $build "debug.keystore"
if (-not (Test-Path $ks)) {
    Run $keytool @("-genkeypair", "-keystore", $ks, "-storepass", "android", "-keypass", "android", "-alias", "crawlermenu",
        "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000", "-dname", "CN=CrawlerMenu Debug")
}
Run (Join-Path $bt "apksigner.bat") @("sign", "--ks", $ks, "--ks-pass", "pass:android", "--key-pass", "pass:android",
    "--ks-key-alias", "crawlermenu", "--out", $patched, $aligned)
Remove-Item $unsigned, $aligned -Force
Write-Host "== Done: $patched"
