param(
    [string]$ProcName = "HyperSense64",
    [string]$Match = "notepad.exe"
)
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class U {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, EnumProc cb, IntPtr l);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr SendMessageStr(IntPtr h, uint msg, IntPtr wp, StringBuilder lp);
    [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);

    public static List<IntPtr> Children(IntPtr parent) {
        var list = new List<IntPtr>();
        EnumChildWindows(parent, (h, l) => { list.Add(h); return true; }, IntPtr.Zero);
        return list;
    }
    public static string Text(IntPtr h) {
        var sb = new StringBuilder(512); GetWindowText(h, sb, 512); return sb.ToString();
    }
    public static string Cls(IntPtr h) {
        var sb = new StringBuilder(128); GetClassName(h, sb, 128); return sb.ToString();
    }
}
"@

$p = Get-Process $ProcName -ErrorAction Stop | Select-Object -First 1
$main = $p.MainWindowHandle
$kids = [U]::Children($main)
$refresh = $kids | Where-Object { [U]::Cls($_) -eq "HyperSenseButton" -and [U]::Text($_) -eq "REFRESH" } | Select-Object -First 1
if ($refresh) { [U]::SendMessage($refresh, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 700 }
$combo = $kids | Where-Object { [U]::Cls($_) -eq "ComboBox" } | Select-Object -First 1
if (-not $combo) { throw "combo not found" }

$count = [int][U]::SendMessage($combo, 0x0146, [IntPtr]::Zero, [IntPtr]::Zero)
$found = -1
for ($i = 0; $i -lt $count; $i++) {
    $sb = New-Object System.Text.StringBuilder 512
    [U]::SendMessageStr($combo, 0x0148, [IntPtr]$i, $sb) | Out-Null
    if ($sb.ToString() -like "*$Match*") { $found = $i; break }
}
if ($found -lt 0) { throw "target item not found ($Match)" }
[U]::SendMessage($combo, 0x014E, [IntPtr]$found, [IntPtr]::Zero) | Out-Null
[U]::SendMessage($main, 0x0111, [IntPtr](100 -bor (1 -shl 16)), $combo) | Out-Null
Write-Output "selected index $found"

$run = $kids | Where-Object { [U]::Cls($_) -eq "HyperSenseButton" -and [U]::Text($_) -eq "RUN ANALYSIS" } | Select-Object -First 1
if (-not $run) { throw "run button not found" }
[U]::SendMessage($run, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Write-Output "run clicked"
