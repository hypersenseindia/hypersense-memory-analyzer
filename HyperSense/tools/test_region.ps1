param([string]$ProcName = "HyperSense64")
# Verifies that REGION mode refuses to run without an address, and that a valid
# address is accepted.
Add-Type @"
using System;using System.Text;using System.Collections.Generic;using System.Runtime.InteropServices;
public class RG {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr SendMessageStr(IntPtr h, uint m, IntPtr w, string l);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
    public static string Cls(IntPtr h){var s=new StringBuilder(128);GetClassName(h,s,128);return s.ToString();}
    public static string Txt(IntPtr h){var s=new StringBuilder(512);GetWindowText(h,s,512);return s.ToString();}
    public static List<IntPtr> Kids(IntPtr p){var l=new List<IntPtr>();EnumChildWindows(p,(h,x)=>{l.Add(h);return true;},IntPtr.Zero);return l;}
    public static List<IntPtr> TopWindows(uint pid){var l=new List<IntPtr>();EnumWindows((h,x)=>{uint p;GetWindowThreadProcessId(h,out p);if(p==pid)l.Add(h);return true;},IntPtr.Zero);return l;}
}
"@
$p = Get-Process $ProcName -ErrorAction Stop | Select-Object -First 1
$main = $p.MainWindowHandle
$kids = [RG]::Kids($main)
function Btn($label) { $kids | Where-Object { [RG]::Cls($_) -eq "HyperSenseButton" -and [RG]::Txt($_) -eq $label } | Select-Object -First 1 }

$region = Btn "REGION"
$run = Btn "RUN ANALYSIS"
[RG]::SendMessage($region, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 300

$edits = $kids | Where-Object { [RG]::Cls($_) -eq "Edit" }
foreach ($e in $edits) { Write-Output ("edit {0} enabled={1}" -f $e, [RG]::IsWindowEnabled($e)) }

# RUN with an empty address must be blocked by a warning dialog
[RG]::SendMessage($run, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 1200
$warn = [IntPtr]::Zero
foreach ($w in [RG]::TopWindows([uint32]$p.Id)) {
    if ([RG]::Cls($w) -eq "#32770") { $warn = $w; break }
}
if ($warn -eq [IntPtr]::Zero) { Write-Output "FAIL: no validation dialog for empty address" }
else {
    Write-Output ("validation dialog: " + [RG]::Txt($warn))
    $ok = [RG]::Kids($warn) | Where-Object { [RG]::Cls($_) -eq "Button" } | Select-Object -First 1
    [RG]::SendMessage($ok, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 400
}

# now a valid address should start a scan
$addr = $edits | Select-Object -First 1
[RG]::SendMessageStr($addr, 0x000C, [IntPtr]::Zero, "0x7FFE0000") | Out-Null
$size = $edits | Select-Object -Last 1
[RG]::SendMessageStr($size, 0x000C, [IntPtr]::Zero, "0x1000") | Out-Null
[RG]::SendMessage($run, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Start-Sleep -Seconds 3
$p.Refresh()
Write-Output ("responding=" + $p.Responding + " exited=" + $p.HasExited)
Write-Output ("run enabled after scan=" + [RG]::IsWindowEnabled($run))
