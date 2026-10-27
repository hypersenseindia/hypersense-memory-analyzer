Add-Type @"
using System;using System.Text;using System.Collections.Generic;using System.Runtime.InteropServices;
public class UI2 {
 public delegate bool EnumProc(IntPtr h, IntPtr l);
 [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc cb, IntPtr l);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
 [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
 public static List<IntPtr> Kids(IntPtr p){var l=new List<IntPtr>();EnumChildWindows(p,(h,x)=>{l.Add(h);return true;},IntPtr.Zero);return l;}
 public static string Text(IntPtr h){var s=new StringBuilder(512);GetWindowText(h,s,512);return s.ToString();}
 public static string Cls(IntPtr h){var s=new StringBuilder(128);GetClassName(h,s,128);return s.ToString();}
}
"@
$p = Get-Process HyperSense64 | Select-Object -First 1
$main = $p.MainWindowHandle
$kids = [UI2]::Kids($main)
$all = $kids | Where-Object { [UI2]::Cls($_) -eq "HyperSenseButton" -and [UI2]::Text($_) -eq "ALL" } | Select-Object -First 1
$run = $kids | Where-Object { [UI2]::Cls($_) -eq "HyperSenseButton" -and [UI2]::Text($_) -eq "RUN ANALYSIS" } | Select-Object -First 1
$stop = $kids | Where-Object { [UI2]::Cls($_) -eq "HyperSenseButton" -and [UI2]::Text($_) -eq "STOP" } | Select-Object -First 1
$combo = $kids | Where-Object { [UI2]::Cls($_) -eq "ComboBox" } | Select-Object -First 1
[UI2]::SendMessage($all, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
[UI2]::SendMessage($combo, 0x014E, [IntPtr]1, [IntPtr]::Zero) | Out-Null
[UI2]::SendMessage($main, 0x0111, [IntPtr](100 -bor (1 -shl 16)), $combo) | Out-Null
Start-Sleep -Milliseconds 300
[UI2]::SendMessage($run, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Start-Sleep -Seconds 2
[UI2]::SendMessage($stop, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Start-Sleep -Seconds 3
$p.Refresh()
Write-Output "responding=$($p.Responding) exited=$($p.HasExited)"
