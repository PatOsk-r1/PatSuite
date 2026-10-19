# Screenshots of the installer without clicking anything: builds a preview setup that opens
# straight on one page, captures its window to dist\installer-<page>.png and closes it.
# Usage: tools\preview-installer.ps1 [-Pages 1,2,6,7]   (1 welcome, 2 license, 6 folder, 7 plugins)
param([int[]]$Pages = @(1, 2, 6, 7))
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$projects = Split-Path $root -Parent
$iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe") |
    Where-Object { Test-Path $_ } | Select-Object -First 1

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices; using System.Text;
public static class Win {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr p);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  public static IntPtr Find(string part) {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, p) => { var sb = new StringBuilder(256); GetWindowText(h, sb, 256);
      if (IsWindowVisible(h) && sb.ToString().Contains(part)) { found = h; return false; } return true; }, IntPtr.Zero);
    return found;
  }
}
'@

foreach ($page in $Pages) {
    & $iscc /Qp "/DPreviewPage=$page" "/DProjects=$projects" (Join-Path $root 'installer\PatSuite.iss') | Out-Null
    $exe = Join-Path $root 'dist\PatSuite-preview.exe'
    $p = Start-Process $exe -ArgumentList '/LANG=spanish' -PassThru
    $h = [IntPtr]::Zero
    for ($i = 0; $i -lt 60 -and $h -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 250; $h = [Win]::Find('PatSuite') }
    Start-Sleep -Milliseconds 1200
    $r = New-Object Win+RECT; [Win]::GetWindowRect($h, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
    [Win]::PrintWindow($h, $dc, 2) | Out-Null
    $g.ReleaseHdc($dc); $g.Dispose()
    $bmp.Save((Join-Path $root "dist\installer-$page.png")); $bmp.Dispose()
    Get-Process | Where-Object { $_.Path -like "*PatSuite-preview*" -or $_.Path -like '*\Temp\is-*' } | Stop-Process -Force
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
    Write-Host "page $page ok"
}
Remove-Item (Join-Path $root 'dist\PatSuite-preview.exe') -ErrorAction SilentlyContinue
