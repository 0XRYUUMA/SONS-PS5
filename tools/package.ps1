param(
    [string]$BuildDir = "build",
    [string]$Version = "0.1.0"
)
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$build = Resolve-Path (Join-Path $repo $BuildDir)
$out = Join-Path $repo "release\SonsOfSparta-PS5-Native-$Version-win64"
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force "$out\libs", "$out\tools" | Out-Null
Copy-Item "$build\SonsOfSparta-PS5.exe" $out
Copy-Item "$repo\tools\Diagnosticar.cmd" $out
Copy-Item "$build\core\relinker\relinker.exe" "$out\tools"
Copy-Item "$build\core\libs\libs\*.prx" "$out\libs"
$gpp = (Get-Command g++).Source
$bin = Split-Path -Parent $gpp
foreach ($name in "libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll") {
    Copy-Item "$bin\$name" "$out\libs"
}
Copy-Item "$repo\LICENSE" $out
Write-Output $out
