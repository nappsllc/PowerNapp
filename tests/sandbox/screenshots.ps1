# Screenshots, run inside Windows Sandbox (Run-Sandbox.ps1 -Suite screenshots [-Languages all|de,fr,...]).
# Installs, schedules Sleep at 11 PM with Wake up at 7:00, and captures the English window in each theme
# (README: screenshot-light.png, screenshot-dark.png). With -Languages: for each language (all = every
# lang\*.txt the installer shipped), light theme, <tag>\main.png, flyout.png (Action), action.png and
# language.png (the window with the Action or the footer's Language dropdown open, taken from the
# screen), about.png and reminder.png. The dark theme also gets the English set in en-dark\.
# Every capture is cropped to the visible window: the main window's invisible resize border (8 px left,
# right and bottom at 100%) is cut off using DWM's extended frame bounds.
#
# -NativeRegion: before each language's captures the sandbox user's regional format is switched to that
# language's main country (de -> de-DE: 24-hour clock, Monday first), so the Store shots show what a user
# there sees. The app is restarted, so it reads the new format. The English sets use en-US.
param([string]$Languages = '', [switch]$NativeRegion)
. "$PSScriptRoot\helpers.ps1"

$Region = @{ ar = 'ar-SA'; bg = 'bg-BG'; cs = 'cs-CZ'; da = 'da-DK'; de = 'de-DE'; el = 'el-GR'; en = 'en-US'; es = 'es-ES'
    fi = 'fi-FI'; fr = 'fr-FR'; he = 'he-IL'; hi = 'hi-IN'; hr = 'hr-HR'; hu = 'hu-HU'; id = 'id-ID'; it = 'it-IT'
    ja = 'ja-JP'; ko = 'ko-KR'; ms = 'ms-MY'; nb = 'nb-NO'; nl = 'nl-NL'; pl = 'pl-PL'; 'pt-BR' = 'pt-BR'; 'pt-PT' = 'pt-PT'
    ro = 'ro-RO'; ru = 'ru-RU'; sk = 'sk-SK'; 'sr-Latn' = 'sr-Latn-RS'; sv = 'sv-SE'; th = 'th-TH'; tr = 'tr-TR'
    uk = 'uk-UA'; vi = 'vi-VN'; 'zh-Hans' = 'zh-CN'; 'zh-Hant' = 'zh-TW' }
function SetRegion($tag) {
    $name = $Region[$tag]; if (-not $name) { $name = 'en-US' }
    Set-Culture $name; [H]::IntlChanged(); Start-Sleep -Milliseconds 700
    Log "  region $name : short time '$([H]::LocaleInfo(0x79))', time '$([H]::LocaleInfo(0x1003))', first day $([H]::LocaleInfo(0x100C))"
}

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System; using System.Runtime.InteropServices; using System.Drawing;
public static class Crop {
  [StructLayout(LayoutKind.Sequential)] struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("dwmapi.dll")] static extern int DwmGetWindowAttribute(IntPtr h, int a, out RECT r, int n);
  [DllImport("user32.dll")] static extern int GetWindowLongW(IntPtr h, int i);
  // the captured file (whole window rect) cut down to DWMWA_EXTENDED_FRAME_BOUNDS (9)
  public static string Apply(IntPtr h, string path) {
    RECT w, e; GetWindowRect(h, out w);
    if (DwmGetWindowAttribute(h, 9, out e, 16) != 0) return "no frame bounds";
    int l = Math.Max(0, e.L - w.L), t = Math.Max(0, e.T - w.T), r = Math.Max(0, w.R - e.R), b = Math.Max(0, w.B - e.B);
    if (l + t + r + b == 0) return "no border";
    // and the 1 px outline inside them, which PrintWindow paints black: 8 px in all at 100%
    if (l > 0) l++; if (r > 0) r++; if (b > 0) b++;
    // a mirrored main window was flipped back by Capture: its left and right borders swap
    if ((GetWindowLongW(h, -20) & 0x400000) != 0 && (GetWindowLongW(h, -16) & 0x00C00000) == 0x00C00000) { int x = l; l = r; r = x; }
    Bitmap c;
    using (var src = new Bitmap(path)) {
      c = src.Clone(new Rectangle(l, t, src.Width - l - r, src.Height - t - b), src.PixelFormat); }
    c.Save(path); c.Dispose();
    return string.Format("cropped l{0} t{1} r{2} b{3}", l, t, r, b);
  }
  // the screen under the visible frames of a and b together (a popup open over its window)
  public static string Screen(IntPtr a, IntPtr b, string path) {
    RECT x, y;
    if (DwmGetWindowAttribute(a, 9, out x, 16) != 0 || DwmGetWindowAttribute(b, 9, out y, 16) != 0) return "no frame bounds";
    int l = Math.Min(x.L, y.L), t = Math.Min(x.T, y.T), r = Math.Max(x.R, y.R), bt = Math.Max(x.B, y.B);
    using (var bm = new Bitmap(r - l, bt - t)) { using (var g = Graphics.FromImage(bm)) g.CopyFromScreen(l, t, 0, 0, bm.Size); bm.Save(path); }
    return string.Format("screen {0}x{1}", r - l, bt - t);
  }
}
'@

function Shot($cls, $file) {
    $h = [H]::F($cls); if ($h -eq [IntPtr]::Zero) { Log "  - $file : no $cls window"; return }
    try { if ([H]::Capture($h, $file)) { $c = [Crop]::Apply($h, $file); Log "  - $(Split-Path $file -Leaf) $c" } else { Log "  - $file : capture failed" } }
    catch { Log "  - $file : error $($_.Exception.Message)" }
}
function Esc($cls) { $h = [H]::F($cls); if ($h -ne [IntPtr]::Zero) { [H]::SendMessage($h, $WM_KEYDOWN, [IntPtr]0x1B, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 400 } }
function Fresh { StopApp; $m = StartApp; [H]::PostMessage($m, 0x0200, [IntPtr]0, [H]::MK(5, 5)) | Out-Null; Start-Sleep -Seconds 2; $m }
# the window with a dropdown open (Cmd: 200 Action, 205 Language), taken from the screen
function ScreenWith($main, $id, $file) {
    Cmd $id; Start-Sleep -Milliseconds 500
    $f = [H]::F('PowerNappFly')
    if ($f -ne [IntPtr]::Zero) { Log "  - $(Split-Path $file -Leaf) $([Crop]::Screen($main, $f, $file))" } else { Log "  - $(Split-Path $file -Leaf) : no dropdown" }
    Esc 'PowerNappFly'
}
# the reminder card for the scheduled Sleep; the process is killed long before the countdown ends
function ReminderShot($file) {
    StopApp
    $p = Start-Process $exe -ArgumentList '--task', 'sleep', '--fire-now', '--warn-secs', '120' -PassThru
    for ($i = 0; $i -lt 20 -and [H]::F('PowerNappReminder') -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 250 }
    Start-Sleep -Milliseconds 1500; Shot 'PowerNappReminder' $file
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue; StopApp
}

Start-Process "$T\PowerNapp-setup.exe" -ArgumentList '/S' -Wait
# the README example: Sleep at 11 PM every day, reminder on, Wake up at 7:00
SetIni 'task' 2; SetIni 'mode' 0; SetIni 'h' 23; SetIni 'm' 0; SetIni 'remind' 1; SetIni 'force' 0
SetIni 'tray' 1; SetIni 'wake' 1; SetIni 'wake_h' 7; SetIni 'wake_m' 0; SetIni 'lang' 'en'
$theme = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize'
foreach ($mode in 'light', 'dark') {
    Set-ItemProperty $theme AppsUseLightTheme ([int]($mode -eq 'light'))
    Start-Sleep -Seconds 2
    StopApp; $main = StartApp
    if (-not (TaskExists 'PowerNapp')) { Cmd 240 }   # Start: scheduled
    $main = Fresh                                     # fresh window: no hover, no focus ring
    Log "- $mode (task=$(TaskExists 'PowerNapp'))"
    Shot 'PowerNappWnd' "$R\screenshot-$mode.png"
    if ($mode -eq 'dark') { # the English set in the dark theme (Store screenshots): en-dark\main, action, reminder
        $d = "$R\en-dark"; New-Item -ItemType Directory -Force $d | Out-Null
        Shot 'PowerNappWnd' "$d\main.png"
        ScreenWith $main 200 "$d\action.png"
        ReminderShot "$d\reminder.png"
    }
}

if ($Languages) {
    Set-ItemProperty $theme AppsUseLightTheme 1; Start-Sleep -Seconds 2
    $tags = if ($Languages -eq 'all') { @(Get-ChildItem (Join-Path (Split-Path $exe) 'lang') -Filter *.txt | ForEach-Object BaseName | Sort-Object) }
            else { @($Languages -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ }) }
    Log "`n## Languages ($($tags.Count))`n"
    foreach ($tag in $tags) {
        $d = "$R\$tag"; New-Item -ItemType Directory -Force $d | Out-Null
        SetIni 'lang' $tag
        if ($NativeRegion) { StopApp; SetRegion $tag }
        $main = Fresh
        Log "- $tag : Start='$([H]::Uia([H]::GetDlgItem($main, 240), 30005))'"
        Shot 'PowerNappWnd' "$d\main.png"
        Cmd 200; Start-Sleep -Milliseconds 500; Shot 'PowerNappFly' "$d\flyout.png"; Esc 'PowerNappFly'
        ScreenWith $main 200 "$d\action.png"
        ScreenWith $main 205 "$d\language.png"
        Cmd 251; Start-Sleep -Milliseconds 700; Shot 'PowerNappAbout' "$d\about.png"
        $a = [H]::F('PowerNappAbout'); if ($a -ne [IntPtr]::Zero) { [H]::SendMessage($a, $WM_CLOSE, [IntPtr]0, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 300 }
        ReminderShot "$d\reminder.png"
    }
}
Log "`nfinished $(Get-Date -Format 'HH:mm:ss')"
Set-Content "$R\DONE" 'done'; Start-Sleep 2; shutdown /s /t 0
