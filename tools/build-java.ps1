<#
  Compiles the Java menu (java/src) to a dex file.
  Output: build\java\classes.dex
#>
param([string]$AndroidHome = "C:\Android")
$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$androidJar = Join-Path $AndroidHome "platforms\android-35\android.jar"
$d8 = Join-Path $AndroidHome "build-tools\35.0.0\d8.bat"
if (-not $env:JAVA_HOME) { $env:JAVA_HOME = "C:\Program Files\Eclipse Adoptium\jdk-17.0.21.10002-hotspot" }
$javac = Join-Path $env:JAVA_HOME "bin\javac.exe"

$out = Join-Path $root "build\java"
$classes = Join-Path $out "classes"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force $classes | Out-Null

$sources = Get-ChildItem (Join-Path $root "java\src") -Recurse -Filter *.java | ForEach-Object { $_.FullName }
& $javac --release 11 -classpath $androidJar -d $classes -encoding UTF-8 @sources
if ($LASTEXITCODE -ne 0) { throw "javac failed" }

$classFiles = Get-ChildItem $classes -Recurse -Filter *.class | ForEach-Object { $_.FullName }
& $d8 --release --min-api 26 --lib $androidJar --output $out @classFiles
if ($LASTEXITCODE -ne 0) { throw "d8 failed" }
Write-Host "== Built $out\classes.dex"
