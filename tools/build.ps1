# Builds PatSuite with the Visual Studio C++ toolchain (no IDE needed).
#   powershell -ExecutionPolicy Bypass -File tools/build.ps1 [-Preset release|debug] [-Target PatSuite|patsuite_tests] [-Test] [-Run]
param(
    [ValidateSet('release', 'debug', 'dist')] [string]$Preset = 'release',
    [string]$Target = '',
    [switch]$Test,
    [switch]$Run,
    [string[]]$CMakeArgs = @()
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# Load the MSVC developer environment once per shell.
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    # -all/-prerelease: Build Tools installs are not always reported by -latest alone
    $vs = & $vswhere -all -prerelease -products * -property installationPath |
        Where-Object { Test-Path (Join-Path $_ 'VC\Auxiliary\Build\vcvars64.bat') } | Select-Object -First 1
    if (-not $vs) { throw 'No Visual Studio installation with C++ tools was found' }
    $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) { throw "Visual Studio C++ tools not found ($vcvars)" }
    cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($Matches[1])" $Matches[2] }
    }
    $cmakeDir = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake'
    $env:Path = "$cmakeDir\CMake\bin;$cmakeDir\Ninja;$env:Path"
}

if (-not (Test-Path "build/$Preset/CMakeCache.txt") -or $CMakeArgs.Count) {
    cmake --preset $Preset @CMakeArgs
    if ($LASTEXITCODE) { throw 'CMake configure failed' }
}
# Two jobs at a time: with 8 GB of RAM, more parallel (LTO) links can exhaust memory and freeze the PC.
$buildArgs = @('--build', '--preset', $Preset, '--parallel', '2')
if ($Target) { $buildArgs += @('--target', $Target) }
cmake @buildArgs
if ($LASTEXITCODE) { throw 'Build failed' }

if ($Test) {
    ctest --preset $Preset
    if ($LASTEXITCODE) { throw 'Tests failed' }
}
if ($Run) {
    # -Run opens the standalone of -Target (default PatTemplate)
    $name = if ($Target) { $Target -replace '_.*$', '' } else { 'PatTemplate' }
    $config = (Get-Content "build/$Preset/CMakeCache.txt" | Select-String '^CMAKE_BUILD_TYPE').Line.Split('=')[1]
    Start-Process "build/$Preset/plugins/$name/${name}_artefacts/$config/Standalone/$name.exe"
}
