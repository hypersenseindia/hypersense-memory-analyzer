param(
    [string]$ProcName = "HyperSense64",
    [string]$OutFile = "",
    [ValidateSet("txt","html")][string]$Format = "txt"
)
# Verifies the SAVE TXT / SAVE HTML buttons through the real UI: mouse-clicks the
# button, types a custom path into the common dialog with the keyboard, presses Enter.
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;using System.Text;using System.Collections.Generic;using System.Runtime.InteropServices;
public class SV2 {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, uint d, IntPtr e);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
    public static string Cls(IntPtr h){var s=new StringBuilder(128);GetClassName(h,s,128);return s.ToString();}
    public static string Txt(IntPtr h){var s=new StringBuilder(512);GetWindowText(h,s,512);return s.ToString();}
    public static List<IntPtr> Kids(IntPtr p){var l=new List<IntPtr>();EnumChildWindows(p,(h,x)=>{l.Add(h);return true;},IntPtr.Zero);return l;}
    public static List<IntPtr> TopWindows(uint pid){var l=new List<IntPtr>();EnumWindows((h,x)=>{uint p;GetWindowThreadProcessId(h,out p);if(p==pid)l.Add(h);return true;},IntPtr.Zero);return l;}
}
"@
if (-not $OutFile) { $OutFile = Join-Path (Get-Location) "tools\report_test.$Format" }

$p = Get-Process $ProcName -ErrorAction Stop | Select-Object -First 1
$main = $p.MainWindowHandle
$kids = [SV2]::Kids($main)

$buttonText = if ($Format -eq "html") { "SAVE HTML" } else { "SAVE TXT" }
$save = $kids | Where-Object { [SV2]::Cls($_) -eq "HyperSenseButton" -and [SV2]::Txt($_) -eq $buttonText } | Select-Object -First 1
if (-not $save) { throw "save button not found ($buttonText)" }

# real mouse click - the custom button only notifies on a genuine click
[SV2]::SetForegroundWindow($main) | Out-Null
Start-Sleep -Milliseconds 300
$r = New-Object SV2+RECT
[SV2]::GetWindowRect($save, [ref]$r) | Out-Null
[SV2]::SetCursorPos([int](($r.L+$r.R)/2), [int](($r.T+$r.B)/2)) | Out-Null
[SV2]::mouse_event(2,0,0,0,[IntPtr]::Zero)
[SV2]::mouse_event(4,0,0,0,[IntPtr]::Zero)
Start-Sleep -Milliseconds 1500

$dlg = [IntPtr]::Zero
foreach ($w in [SV2]::TopWindows([uint32]$p.Id)) {
    if ([SV2]::Cls($w) -eq "#32770" -and [SV2]::Txt($w) -like "*Save HYPERSENSEGLOBAL report*") { $dlg = $w; break }
}
if ($dlg -eq [IntPtr]::Zero) { throw "save dialog not found" }

[SV2]::SetForegroundWindow($dlg) | Out-Null
Start-Sleep -Milliseconds 600
[System.Windows.Forms.SendKeys]::SendWait("^a")
Start-Sleep -Milliseconds 150
[System.Windows.Forms.SendKeys]::SendWait($OutFile)
Start-Sleep -Milliseconds 300
[System.Windows.Forms.SendKeys]::SendWait("{ENTER}")
Start-Sleep -Milliseconds 1800

# confirm the overwrite prompt if it appears, then the info box
for ($i = 0; $i -lt 3; $i++) {
    $handled = $false
    foreach ($w in [SV2]::TopWindows([uint32]$p.Id)) {
        if ($w -eq $main -or $w -eq $dlg) { continue }
        if ([SV2]::Cls($w) -eq "#32770") {
            $ok = [SV2]::Kids($w) | Where-Object { [SV2]::Cls($_) -eq "Button" } | Select-Object -First 1
            if ($ok) { [SV2]::SendMessage($ok, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; $handled = $true }
        }
    }
    if (-not $handled) { break }
    Start-Sleep -Milliseconds 600
}
Start-Sleep -Milliseconds 500
if (Test-Path $OutFile) { Write-Output "report written: $OutFile ($((Get-Item $OutFile).Length) bytes)" }
else { throw "report file not created at $OutFile" }
