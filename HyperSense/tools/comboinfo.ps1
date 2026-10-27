param([string]$ProcName = "HyperSense64")
Add-Type @"
using System;using System.Text;using System.Collections.Generic;using System.Runtime.InteropServices;
public class CBX {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
    public static string Cls(IntPtr h){var s=new StringBuilder(128);GetClassName(h,s,128);return s.ToString();}
    public static List<IntPtr> TopWindows(uint pid){var l=new List<IntPtr>();EnumWindows((h,x)=>{uint p;GetWindowThreadProcessId(h,out p);if(p==pid)l.Add(h);return true;},IntPtr.Zero);return l;}
    public static List<IntPtr> Kids(IntPtr p){var l=new List<IntPtr>();EnumChildWindows(p,(h,x)=>{l.Add(h);return true;},IntPtr.Zero);return l;}
}
"@
$p = Get-Process $ProcName -ErrorAction Stop | Select-Object -First 1
$main = $p.MainWindowHandle
[CBX]::SetForegroundWindow($main) | Out-Null
$combo = [CBX]::Kids($main) | Where-Object { [CBX]::Cls($_) -eq "ComboBox" } | Select-Object -First 1
if (-not $combo) { throw "combo not found" }

$closed = New-Object CBX+RECT
[CBX]::GetWindowRect($combo, [ref]$closed) | Out-Null
Write-Output ("combo window : {0} x {1}" -f ($closed.R-$closed.L), ($closed.B-$closed.T))

[CBX]::SendMessage($combo, 0x014F, [IntPtr]1, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 900

$found = $false
foreach ($w in [CBX]::TopWindows([uint32]$p.Id)) {
    if ([CBX]::Cls($w) -eq "ComboLBox") {
        $r = New-Object CBX+RECT
        [CBX]::GetWindowRect($w, [ref]$r) | Out-Null
        Write-Output ("dropdown     : {0} x {1}  at {2},{3}" -f ($r.R-$r.L), ($r.B-$r.T), $r.L, $r.T)
        $found = $true
    }
}
if (-not $found) { Write-Output "dropdown not open" }
