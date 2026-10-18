# Downloads every third-party dependency into third_party/.
# Safe to re-run: anything already present is skipped.
#   powershell -ExecutionPolicy Bypass -File tools/fetch-deps.ps1
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = Split-Path -Parent $PSScriptRoot
$tp = Join-Path $root 'third_party'
New-Item -ItemType Directory -Force $tp | Out-Null

$JUCE_TAG = '9.0.4'
$GTEST_VERSION = '1.17.0'

function Get-Zip($url, $dest, $name) {
    if (Test-Path $dest) { Write-Host "[ok] $name"; return }
    Write-Host "[..] $name"
    $zip = Join-Path $env:TEMP ([IO.Path]::GetRandomFileName() + '.zip')
    Invoke-WebRequest $url -OutFile $zip -UseBasicParsing
    $tmp = Join-Path $env:TEMP ([IO.Path]::GetRandomFileName())
    Expand-Archive $zip $tmp
    $inner = Get-ChildItem $tmp | Select-Object -First 1
    Move-Item $inner.FullName $dest
    Remove-Item $zip, $tmp -Recurse -Force
}

# JUCE (AGPLv3 / JUCE licence — see third_party/JUCE/LICENSE.md)
if (-not (Test-Path "$tp/JUCE")) {
    Write-Host "[..] JUCE $JUCE_TAG"
    # git writes progress to stderr; run through cmd so PowerShell 5.1 doesn't treat it as an error
    cmd /c "git clone --quiet --depth 1 --branch $JUCE_TAG https://github.com/juce-framework/JUCE.git `"$tp\JUCE`" 2>nul"
    if ($LASTEXITCODE -ne 0) { throw 'git clone of JUCE failed' }
} else { Write-Host '[ok] JUCE' }

# GoogleTest (BSD-3) — unit tests only
Get-Zip "https://github.com/google/googletest/archive/refs/tags/v$GTEST_VERSION.zip" "$tp/googletest" "GoogleTest $GTEST_VERSION"

Write-Host 'Dependencies ready.'
