param(
    [string]$ProcName = "HyperSense64",
    [string]$OutFile = ""
)
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class SV {
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr SendMessageStr(IntPtr h, uint m, IntPtr w, string l);
    [DllImport("user32.dll")] public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, uint d, IntPtr e);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
    public static List<IntPtr> Kids(IntPtr p) { var l = new List<IntPtr>(); EnumChildWindows(p, (h,x)=>{ l.Add(h); return true; }, IntPtr.Zero); return l; }
    public static string Text(IntPtr h) { var s = new StringBuilder(512); GetWindowText(h, s, 512); return s.ToString(); }
    public static string Cls(IntPtr h) { var s = new StringBuilder(128); GetClassName(h, s, 128); return s.ToString(); }
    public static List<IntPtr> TopWindows(uint pid) { var l = new List<IntPtr>(); EnumWindows((h,x)=>{ uint p; GetWindowThreadProcessId(h, out p); if (p==pid) l.Add(h); return true; }, IntPtr.Zero); return l; }
}
"@
if (-not $OutFile) { $OutFile = Join-Path (Get-Location) "tools\report_test.txt" }
$p = Get-Process $ProcName -ErrorAction Stop | Select-Object -First 1
$main = $p.MainWindowHandle
$kids = [SV]::Kids($main)
$save = $kids | Where-Object { [SV]::Cls($_) -eq "HyperSenseButton" -and [SV]::Text($_) -eq "SAVE REPORT" } | Select-Object -First 1
if (-not $save) { throw "save button not found" }
[SV]::SetForegroundWindow($main) | Out-Null
Start-Sleep -Milliseconds 250
$r = New-Object SV+RECT
[SV]::GetWindowRect($save, [ref]$r) | Out-Null
[SV]::SetCursorPos([int](($r.L+$r.R)/2), [int](($r.T+$r.B)/2)) | Out-Null
[SV]::mouse_event(2,0,0,0,[IntPtr]::Zero)
[SV]::mouse_event(4,0,0,0,[IntPtr]::Zero)
Start-Sleep -Milliseconds 1500

$dlg = [IntPtr]::Zero
foreach ($w in [SV]::TopWindows([uint32]$p.Id)) {
    $c = [SV]::Cls($w); $t = [SV]::Text($w)
    if ($c -eq "#32770" -and $t -like "*Save*") { $dlg = $w; break }
}
if ($dlg -eq [IntPtr]::Zero) { throw "save dialog not found" }
Write-Output "dialog: $dlg"

$dkids = [SV]::Kids($dlg)
Write-Output "children: $($dkids.Count)"
$edit = [IntPtr]::Zero
foreach ($k in $dkids) {
    $c = [SV]::Cls($k); $t = [SV]::Text($k)
    if ($c -eq "Edit" -and $edit -eq [IntPtr]::Zero) { $edit = $k }
    Write-Output ("  {0} {1} '{2}'" -f $k, $c, $t)
}
if ($edit -eq [IntPtr]::Zero) { throw "filename edit not found" }
# CDM_SETCONTROLTEXT(edt1) is the canonical way to seed a common dialog's filename field
[SV]::SendMessageStr($dlg, 0x0404, [IntPtr]0x047C, $OutFile) | Out-Null
Start-Sleep -Milliseconds 400

# Press the default save button in the dialog
$btn = $dkids | Where-Object { [SV]::Cls($_) -eq "Button" -and [SV]::Text($_) -match "Save" } | Select-Object -First 1
if ($btn) { [SV]::SendMessage($btn, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null }
else { [SV]::SendMessage($dlg, 0x0111, [IntPtr]1, [IntPtr]::Zero) | Out-Null }
Start-Sleep -Milliseconds 1500

# dismiss any info / overwrite message box
foreach ($w in [SV]::TopWindows([uint32]$p.Id)) {
    if ($w -eq $main -or $w -eq $dlg) { continue }
    if ([SV]::Cls($w) -eq "#32770") {
        $mk = [SV]::Kids($w)
        $ok = $mk | Where-Object { [SV]::Cls($_) -eq "Button" } | Select-Object -First 1
        if ($ok) { [SV]::SendMessage($ok, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null }
    }
}
Start-Sleep -Milliseconds 700
if (Test-Path $OutFile) { Write-Output "report written: $OutFile ($((Get-Item $OutFile).Length) bytes)" }
else { throw "report file not created" }
