# Copies the author's own presets (Documents\PatSuite\<Plugin>\Presets) into presets\<Plugin>\ so
# the next build ships them inside each plugin for everyone. Sub-folders become categories;
# presets saved in the root go to "Community"; a sub-folder named "Privado" is never shared.
#   powershell -ExecutionPolicy Bypass -File tools/collect-presets.ps1
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$docs = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'PatSuite'
$count = 0
foreach ($plugin in Get-ChildItem (Join-Path $root 'plugins') -Directory) {
    $name = $plugin.Name
    $src = Join-Path $docs "$name\Presets"
    $dst = Join-Path $root "presets\$name"
    if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }
    if (-not (Test-Path $src)) { continue }
    foreach ($f in Get-ChildItem $src -Recurse -File -Filter '*.patpreset') {
        $rel = $f.DirectoryName.Substring($src.Length).TrimStart('\')
        if ($rel -eq 'Privado' -or $rel.StartsWith('Privado\')) { continue }
        $category = if ($rel) { $rel.Replace('\', '/') } else { 'Community' }
        [xml]$xml = Get-Content -LiteralPath $f.FullName -Raw -Encoding UTF8
        $xml.DocumentElement.SetAttribute('category', $category)
        $outDir = if ($rel) { Join-Path $dst $rel } else { Join-Path $dst 'Community' }
        New-Item -ItemType Directory -Force $outDir | Out-Null
        $xml.Save((Join-Path $outDir $f.Name))
        $count++
    }
}
Write-Host "Shared presets collected: $count"
