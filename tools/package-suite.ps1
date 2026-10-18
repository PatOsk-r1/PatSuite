# Builds dist\PatSuite-<version>-Setup.exe with every Pat plugin that exists next to this folder.
# For each plugin: collects your presets (they ship inside the plugin), builds the release VST3 +
# standalone, then Inno Setup bundles them all into one installer.
#   powershell -ExecutionPolicy Bypass -File tools/package-suite.ps1
param([string]$Version = '0.1.0')
$ErrorActionPreference = 'Stop'
$suite = Split-Path -Parent $PSScriptRoot
$projects = Split-Path -Parent $suite

# Plugin folders with their preset extension. Add new plugins here.
$plugins = [ordered]@{ PatWave = 'pwpreset'; PatVerb = 'pvpreset' }  # stand-alone sibling projects

foreach ($name in $plugins.Keys) {
    $dir = Join-Path $projects $name
    if (-not (Test-Path (Join-Path $dir 'CMakeLists.txt'))) { Write-Host "[--] $name (not created yet)"; continue }
    Write-Host "[..] $name"
    Push-Location $dir
    try {
        if (Test-Path tools/collect-presets.ps1) {
            & powershell -ExecutionPolicy Bypass -File tools/collect-presets.ps1 -Plugin $name -Extension $plugins[$name]
            if ($LASTEXITCODE) { throw "Collecting presets of $name failed" }
        }
        $copyFlag = '-D' + $name.ToUpper() + '_COPY_VST3=OFF'
        & powershell -ExecutionPolicy Bypass -File tools/build.ps1 -Preset dist -Target "${name}_All" -CMakeArgs $copyFlag
        if ($LASTEXITCODE) { throw "Build of $name failed" }
    } finally { Pop-Location }
    Write-Host "[ok] $name"
}

# The vocal suite (PatDeEsser, PatSidechain, PatDoubler, PatTune) is one CMake project: this folder.
if (Test-Path (Join-Path $suite 'plugins\PatDeEsser')) {
    Write-Host '[..] PatSuite vocal plugins'
    Push-Location $suite
    try {
        & powershell -ExecutionPolicy Bypass -File tools/build.ps1 -Preset dist -CMakeArgs '-DPATSUITE_COPY_VST3=OFF'
        if ($LASTEXITCODE) { throw 'Build of the PatSuite plugins failed' }
    } finally { Pop-Location }
}

$iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe") |
    Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw 'Inno Setup 6 not found' }
Set-Location $suite
& $iscc /Qp "/DAppVersion=$Version" "/DProjects=$projects" installer\PatSuite.iss
if ($LASTEXITCODE) { throw 'Installer build failed' }
Get-ChildItem dist\*.exe | ForEach-Object { "{0}  ({1:N1} MB)" -f $_.FullName, ($_.Length / 1MB) }
