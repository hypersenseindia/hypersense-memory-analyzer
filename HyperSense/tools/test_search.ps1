param(
    [string]$Filter = "expl",
    [string]$ProcName = "HyperSense64",
    [int]$WaitSeconds = 60,
    [string]$Pick = "explorer.exe",
    [switch]$NoRun
)

Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class U2 {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr parent, EnumProc cb, IntPtr l);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr SendMessageStr(IntPtr h, uint msg, IntPtr wp, StringBuilder lp);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr SendMessageText(IntPtr h, uint msg, IntPtr wp, string lp);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }

    public static List<IntPtr> Children(IntPtr parent) {
        var list = new List<IntPtr>();
        EnumChildWindows(parent, (h, l) => { list.Add(h); return true; }, IntPtr.Zero);
        return list;
    }
    public static string Text(IntPtr h) {
        var sb = new StringBuilder(4096); GetWindowText(h, sb, 4096); return sb.ToString();
    }
    public static string Cls(IntPtr h) {
        var sb = new StringBuilder(128); GetClassName(h, sb, 128); return sb.ToString();
    }
    public static int Top(IntPtr h) {
        RECT r; GetWindowRect(h, out r); return r.Top;
    }
}
"@

$p = Get-Process $ProcName -ErrorAction Stop | Select-Object -First 1
$main = $p.MainWindowHandle
$kids = [U2]::Children($main)

$combo = $kids | Where-Object { [U2]::Cls($_) -eq "ComboBox" } | Select-Object -First 1
if (-not $combo) { throw "combo not found" }

# The search box is the top-most plain EDIT child (above the combo in the target card).
$edits = $kids | Where-Object { [U2]::Cls($_) -eq "Edit" } | Sort-Object { [U2]::Top($_) }
if (-not $edits) { throw "search edit not found" }
$search = $edits | Select-Object -First 1

[U2]::SendMessageText($search, 0x000C, [IntPtr]::Zero, $Filter) | Out-Null
Start-Sleep -Milliseconds 500

$count = [int][U2]::SendMessage($combo, 0x0146, [IntPtr]::Zero, [IntPtr]::Zero)
$items = @()
for ($i = 0; $i -lt $count; $i++) {
    $sb = New-Object System.Text.StringBuilder 512
    [U2]::SendMessageStr($combo, 0x0148, [IntPtr]$i, $sb) | Out-Null
    $items += $sb.ToString()
}
Write-Output "filter='$Filter' comboItems=$count"
$items | ForEach-Object { Write-Output "  [$_]" }

$found = -1
for ($i = 0; $i -lt $count; $i++) { if ($items[$i] -like "*$Pick*") { $found = $i; break } }
if ($found -lt 0) {
    if ($NoRun) { Write-Output "no match for '$Pick'"; return }
    throw "no $Pick in filtered list"
}
if ($NoRun) { Write-Output "match='$Pick' index=$found"; return }

[U2]::SendMessage($combo, 0x014E, [IntPtr]$found, [IntPtr]::Zero) | Out-Null
[U2]::SendMessage($main, 0x0111, [IntPtr](100 -bor (1 -shl 16)), $combo) | Out-Null
Write-Output "selected=$found ($($items[$found]))"

$run = $kids | Where-Object { [U2]::Cls($_) -eq "HyperSenseButton" -and [U2]::Text($_) -eq "RUN ANALYSIS" } | Select-Object -First 1
if (-not $run) { throw "run button not found" }
[U2]::SendMessage($run, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Write-Output "run clicked"

$rich = $kids | Where-Object { [U2]::Cls($_) -like "RICHEDIT*" } | Select-Object -First 1
if (-not $rich) { throw "output pane not found" }

$deadline = (Get-Date).AddSeconds($WaitSeconds)
$text = ""
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 800
    $len = [int][U2]::SendMessage($rich, 0x000E, [IntPtr]::Zero, [IntPtr]::Zero)
    if ($len -le 0) { continue }
    $sb = New-Object System.Text.StringBuilder ($len + 32)
    [U2]::SendMessageStr($rich, 0x000D, [IntPtr]($len + 16), $sb) | Out-Null
    $text = $sb.ToString()
    if ($text -match "RESULT") { Start-Sleep -Milliseconds 600; break }
}
$lines = $text -split "`r`n|`n"
Write-Output "outputLines=$($lines.Count)"
$lines | Select-Object -Last 14 | ForEach-Object { Write-Output "  $_" }
