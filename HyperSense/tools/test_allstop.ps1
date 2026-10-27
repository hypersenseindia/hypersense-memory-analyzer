param(
    [string]$ProcName = "HyperSense64",
    [int]$RunSeconds = 6
)
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class U3 {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc cb, IntPtr l);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr SendMessageStr(IntPtr h, uint m, IntPtr w, StringBuilder l);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr SendMessageText(IntPtr h, uint m, IntPtr w, string l);
    public static List<IntPtr> Kids(IntPtr p){var l=new List<IntPtr>();EnumChildWindows(p,(h,x)=>{l.Add(h);return true;},IntPtr.Zero);return l;}
    public static string Txt(IntPtr h){var s=new StringBuilder(512);GetWindowText(h,s,512);return s.ToString();}
    public static string Cls(IntPtr h){var s=new StringBuilder(128);GetClassName(h,s,128);return s.ToString();}
}
"@

$p = Get-Process $ProcName -ErrorAction Stop | Select-Object -First 1
$main = $p.MainWindowHandle
$kids = [U3]::Kids($main)
$combo = $kids | Where-Object { [U3]::Cls($_) -eq "ComboBox" } | Select-Object -First 1
$search = $kids | Where-Object { [U3]::Cls($_) -eq "Edit" } | Select-Object -First 1

[U3]::SendMessageText($search, 0x000C, [IntPtr]::Zero, "") | Out-Null
Start-Sleep -Milliseconds 400
[U3]::SendMessage($combo, 0x014E, [IntPtr]1, [IntPtr]::Zero) | Out-Null
[U3]::SendMessage($main, 0x0111, [IntPtr](100 -bor (1 -shl 16)), $combo) | Out-Null

$run = $kids | Where-Object { [U3]::Cls($_) -eq "HyperSenseButton" -and [U3]::Txt($_) -eq "RUN ANALYSIS" } | Select-Object -First 1
$stop = $kids | Where-Object { [U3]::Cls($_) -eq "HyperSenseButton" -and [U3]::Txt($_) -eq "STOP" } | Select-Object -First 1
[U3]::SendMessage($run, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Write-Output "run clicked (ALL PROCESSES)"
Start-Sleep -Seconds $RunSeconds

[U3]::SendMessage($stop, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Write-Output "stop clicked"
Start-Sleep -Seconds 3

$rich = $kids | Where-Object { [U3]::Cls($_) -like "RICHEDIT*" } | Select-Object -First 1
$len = [int][U3]::SendMessage($rich, 0x000E, [IntPtr]::Zero, [IntPtr]::Zero)
$sb = New-Object System.Text.StringBuilder ($len + 64)
[U3]::SendMessageStr($rich, 0x000D, [IntPtr]($len + 32), $sb) | Out-Null
$text = $sb.ToString()
Write-Output "outputLines=$(($text -split "`n").Count)"
$text -split "`r`n|`n" | Select-Object -Last 6 | ForEach-Object { Write-Output "  $_" }
if ($text -match "STOPPED") { Write-Output "STOP verified" } else { Write-Output "STOP marker missing" }
