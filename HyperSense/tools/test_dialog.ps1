param([string]$ProcName = "HyperSense64", [switch]$Escape)
# Exercises the custom dark dialog: triggers a validation message, then closes it
# with either the mouse (default) or the ESC key.
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;using System.Text;using System.Collections.Generic;using System.Runtime.InteropServices;
public class DLG {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, uint d, IntPtr e);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
    public static string Cls(IntPtr h){var s=new StringBuilder(128);GetClassName(h,s,128);return s.ToString();}
    public static string Txt(IntPtr h){var s=new StringBuilder(256);GetWindowText(h,s,256);return s.ToString();}
    public static List<IntPtr> Kids(IntPtr p){var l=new List<IntPtr>();EnumChildWindows(p,(h,x)=>{l.Add(h);return true;},IntPtr.Zero);return l;}
    public static List<IntPtr> Top(uint pid){var l=new List<IntPtr>();EnumWindows((h,x)=>{uint p;GetWindowThreadProcessId(h,out p);if(p==pid)l.Add(h);return true;},IntPtr.Zero);return l;}
}
"@
$p = Get-Process $ProcName -ErrorAction Stop | Select-Object -First 1
$main = $p.MainWindowHandle
$kids = [DLG]::Kids($main)

function Click($hwnd) {
    [DLG]::SetForegroundWindow($main) | Out-Null
    Start-Sleep -Milliseconds 250
    $r = New-Object DLG+RECT
    [DLG]::GetWindowRect($hwnd, [ref]$r) | Out-Null
    [DLG]::SetCursorPos([int](($r.L+$r.R)/2), [int](($r.T+$r.B)/2)) | Out-Null
    [DLG]::mouse_event(2,0,0,0,[IntPtr]::Zero)
    [DLG]::mouse_event(4,0,0,0,[IntPtr]::Zero)
    Start-Sleep -Milliseconds 600
}

# put the app into REGION mode with no target so RUN raises the info dialog
$region = $kids | Where-Object { [DLG]::Cls($_) -eq "HyperSenseButton" -and [DLG]::Txt($_) -eq "REGION" } | Select-Object -First 1
$run = $kids | Where-Object { [DLG]::Cls($_) -eq "HyperSenseButton" -and [DLG]::Txt($_) -eq "RUN ANALYSIS" } | Select-Object -First 1
Click $region
Click $run
Start-Sleep -Milliseconds 500

$dlg = [DLG]::Top([uint32]$p.Id) | Where-Object { [DLG]::Cls($_) -eq "HyperSenseDialog" } | Select-Object -First 1
if (-not $dlg) { throw "dark dialog did not appear" }
$r = New-Object DLG+RECT
[DLG]::GetWindowRect($dlg, [ref]$r) | Out-Null
Write-Output ("dialog '{0}' at {1}x{2}" -f [DLG]::Txt($dlg), ($r.R-$r.L), ($r.B-$r.T))

$dkids = [DLG]::Kids($dlg)
$ok = $dkids | Where-Object { [DLG]::Cls($_) -eq "HyperSenseButton" -and [DLG]::Txt($_) -eq "OK" } | Select-Object -First 1
if (-not $ok) { throw "OK button missing from dialog" }
Write-Output ("dialog children: " + (($dkids | ForEach-Object { [DLG]::Cls($_) }) -join ", "))

if ($Escape) {
    [DLG]::SetForegroundWindow($dlg) | Out-Null
    Start-Sleep -Milliseconds 300
    [System.Windows.Forms.SendKeys]::SendWait("{ESC}")
    Start-Sleep -Milliseconds 800
} else {
    $rr = New-Object DLG+RECT
    [DLG]::GetWindowRect($ok, [ref]$rr) | Out-Null
    [DLG]::SetCursorPos([int](($rr.L+$rr.R)/2), [int](($rr.T+$rr.B)/2)) | Out-Null
    [DLG]::mouse_event(2,0,0,0,[IntPtr]::Zero)
    [DLG]::mouse_event(4,0,0,0,[IntPtr]::Zero)
    Start-Sleep -Milliseconds 800
}

$still = [DLG]::Top([uint32]$p.Id) | Where-Object { [DLG]::Cls($_) -eq "HyperSenseDialog" }
$p.Refresh()
Write-Output ("closed=" + (-not [bool]$still) + " app_responding=" + $p.Responding)
