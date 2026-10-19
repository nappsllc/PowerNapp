# PowerNapp end-to-end tests, run inside Windows Sandbox (start with Run-Sandbox.ps1).
# Results: C:\Test\results\results.md + screenshots. Ends by shutting the sandbox down
# with PowerNapp itself.
. "$PSScriptRoot\helpers.ps1"

# ----------------------------------------------------------------------------- environment
Section 'Environment'
$os = Get-CimInstance Win32_OperatingSystem
Log "- Windows $($os.Version) $($os.Caption), user $env:USERNAME"
$pc = (powercfg /a | Out-String)
$avail = ($pc -split 'The following sleep states are not available')[0]
Log "- Sleep states available: $((($avail -split "`n") | Where-Object { $_ -match '^\s{4}\S' } | ForEach-Object { $_.Trim() }) -join ', ')"
Log ("- VC++ runtime present on this machine: " + (Test-Path "$env:windir\System32\vcruntime140.dll"))

# ----------------------------------------------------------------------------- installer
# ----------------------------------------------------------------------------- upgrade from PowerOff 1.0.1
# The app's previous name. Install the real 1.0.1 and give it what a user's PC has: settings,
# a daily schedule, a wake task, the startup entry and a running tray copy. The PowerNapp
# installer below must carry all of it over and remove PowerOff.
Section 'Upgrade from PowerOff 1.0.1 (setup)'
$old = "$env:LOCALAPPDATA\Programs\PowerOff\poweroff.exe"
$runkey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
Check 'PowerOff 1.0.1 installs' { Start-Process "$T\PowerOff-1.0.1-setup.exe" -ArgumentList '/S' -Wait; Test-Path $old }
New-Item -ItemType Directory -Force "$env:APPDATA\PowerOff" | Out-Null
Set-Content "$env:APPDATA\PowerOff\poweroff.ini" "[PowerOff]`r`ntask=2`r`nmode=0`r`nh=22`r`nm=15`r`nremind=1`r`ntray=1"
schtasks /create /tn 'PowerOff' /tr "`"$old`" --task sleep --fire-now --warn-secs 60" /sc daily /st 22:14 /f | Out-Null
schtasks /create /tn 'PowerOff wake' /tr "`"$old`" --wake" /sc daily /st 07:00 /f | Out-Null
Set-ItemProperty $runkey PowerOff "`"$old`" --tray"
Start-Process $old -ArgumentList '--tray'; Start-Sleep -Seconds 2
Check 'PowerOff 1.0.1 is set up and running' { (TaskExists 'PowerOff') -and (TaskExists 'PowerOff wake') -and @(Get-Process poweroff -ErrorAction SilentlyContinue).Count -ge 1 }

# A task that cannot be moved (here 'PowerOff daily' edited to a COM handler: no command to point at
# PowerNapp) must not be lost: --migrate exits nonzero, the installer keeps PowerOff (its uninstaller
# would delete the task) and migrated=1 is not written. Then the task goes and the real upgrade runs.
Section 'Upgrade from PowerOff 1.0.1: a task that cannot be moved'
$bad = "$T\results\bad-task.xml"
Set-Content $bad -Encoding Unicode @'
<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <Settings><Enabled>false</Enabled></Settings>
  <Actions><ComHandler><ClassId>{0F87369F-A4E5-4CFC-BD3E-73E6154572DD}</ClassId></ComHandler></Actions>
</Task>
'@
schtasks /create /tn 'PowerOff daily' /xml $bad /f | Out-Null
Check 'setup with an unmovable task: exits 0, PowerOff kept with its task, not marked migrated' {
    $p = Start-Process "$T\PowerNapp-setup.exe" -ArgumentList '/S' -PassThru -Wait
    $kept = (Test-Path $old) -and (Test-Path "$env:LOCALAPPDATA\Programs\PowerOff\uninstall.exe")
    Note "exit $($p.ExitCode), PowerOff kept=$kept, 'PowerOff daily'=$(TaskExists 'PowerOff daily'), migrated=$(Ini 'migrated'), moved: PowerNapp=$(TaskExists 'PowerNapp') wake=$(TaskExists 'PowerNapp wake')"
    $p.ExitCode -eq 0 -and $kept -and (TaskExists 'PowerOff daily') -and -not (Ini 'migrated') -and (TaskExists 'PowerNapp') -and (TaskExists 'PowerNapp wake') }
Check '--migrate exits 1 while a task cannot be moved' { $c = RunCli @('--migrate'); Note "exit $c"; $c -eq 1 }
schtasks /delete /tn 'PowerOff daily' /f | Out-Null

Section 'Installer'
Check 'silent install exits 0' { $p = Start-Process "$T\PowerNapp-setup.exe" -ArgumentList '/S' -PassThru -Wait; Note "exit $($p.ExitCode)"; $p.ExitCode -eq 0 }
Check 'files installed' { (Test-Path $exe) -and (Test-Path "$env:LOCALAPPDATA\Programs\PowerNapp\uninstall.exe") }
Check 'lang\ installed next to the exe: every repo translation (de.txt among them), no pseudo-languages' {
    $want = @(Get-ChildItem "$T\lang" -Filter *.txt | ForEach-Object Name | Sort-Object)
    $got = @(Get-ChildItem "$env:LOCALAPPDATA\Programs\PowerNapp\lang" -Filter *.txt -ErrorAction SilentlyContinue | ForEach-Object Name | Sort-Object)
    $de = if ($want -contains 'de.txt') { $got -contains 'de.txt' } else { 'n/a (no de.txt in the repo yet)' }
    Note "$($got.Count) of $($want.Count) files, de.txt=$de"
    $got.Count -gt 0 -and -not (Compare-Object $want $got) -and $de -and -not ($got -like 'qps-*') }
Check 'Start menu shortcut points at the exe' { $s = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk); Note $s.TargetPath; $s.TargetPath -eq $exe }
Check 'Settings > Apps entry, its size counts the exe and lang\' {
    $k = Get-ItemProperty $ukey; $kb = [int]((Get-ChildItem (Split-Path $exe) -Recurse -File | Measure-Object Length -Sum).Sum / 1KB)
    Note "$($k.DisplayName) $($k.DisplayVersion) EstimatedSize=$($k.EstimatedSize) KB, files $kb KB"
    $k.DisplayName -eq 'PowerNapp' -and $k.UninstallString -and [math]::Abs($k.EstimatedSize - $kb) -le 64 }
Check 'upgrade: settings carried over' { Note "task=$(Ini 'task') h=$(Ini 'h') m=$(Ini 'm')"; (Ini 'task') -eq '2' -and (Ini 'h') -eq '22' -and (Ini 'm') -eq '15' }
Check 'upgrade: tasks renamed and run PowerNapp' {
    $a = (TaskXml 'PowerNapp').Task.Actions.Exec; $w = (TaskXml 'PowerNapp wake').Task.Actions.Exec
    Note "PowerNapp: $($a.Command) $($a.Arguments) | wake: $($w.Command) $($w.Arguments)"
    $a.Command -eq $exe -and $a.Arguments -match '--task sleep' -and $w.Command -eq $exe -and $w.Arguments -eq '--wake' -and
        -not (TaskExists 'PowerOff') -and -not (TaskExists 'PowerOff wake') }
Check 'upgrade: startup entry moved' { $v = (Get-ItemProperty $runkey).PowerNapp; $o = (Get-ItemProperty $runkey).PowerOff; Note "PowerNapp=$v PowerOff=$o"; $v -match 'powernapp\.exe' -and -not $o }
Check 'upgrade: PowerOff removed' {
    $left = @("$env:LOCALAPPDATA\Programs\PowerOff", "$env:APPDATA\PowerOff", "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\PowerOff.lnk", 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\PowerOff') | Where-Object { Test-Path $_ }
    $p = @(Get-Process poweroff -ErrorAction SilentlyContinue).Count
    Note "left: $($left -join ', ') processes: $p"; $left.Count -eq 0 -and $p -eq 0 }
# back to a fresh install for the rest of the suite
schtasks /delete /tn 'PowerNapp' /f 2>$null | Out-Null; schtasks /delete /tn 'PowerNapp wake' /f 2>$null | Out-Null
Remove-ItemProperty $runkey PowerNapp -ErrorAction SilentlyContinue

# ----------------------------------------------------------------------------- CLI
Section 'Command line'
Check '--help prints usage (piped, UTF-8)' { $o = & $exe --help | Out-String; Note ($o.Split("`n")[0].Trim()); $o -match 'PowerNapp' -and $o -match '--fire-now' }
Check 'unknown task: exit 2' { $c = RunCli @('--task', 'bogus', '--in', '5'); Note "exit $c"; $c -eq 2 }
Check '--task without timing: exit 2' { $c = RunCli @('--task', 'sleep'); Note "exit $c"; $c -eq 2 }
Check 'starts without the VC++ redistributable' { $p = Start-Process $exe -ArgumentList '--help' -PassThru -Wait -WindowStyle Hidden; Note "exit $($p.ExitCode)"; $p.ExitCode -eq 0 }

# ----------------------------------------------------------------------------- window
Section 'Window'
SetIni 'task' 5; SetIni 'mode' 0; SetIni 'h' 23; SetIni 'm' 0; SetIni 'remind' 1; SetIni 'tray' 1; SetIni 'force' 0
$main = StartApp
Check 'window opens' { $main -ne [IntPtr]::Zero }
$dpi = [H]::GetDpiForWindow($main)
Check 'client width = 640 x DPI' { $w = [H]::ClientW($main); Note "dpi $dpi, client ${w}x$([H]::ClientH($main))"; $w -eq [math]::Round(640 * $dpi / 96) }
Snap 'PowerNappWnd' 'main'
Check 'single instance: second launch exits' { $p = Start-Process $exe -PassThru; Start-Sleep -Seconds 2; $p.Refresh(); $n = @(Get-Process powernapp).Count; Note "processes $n"; $p.HasExited -and $n -eq 1 }
Check 'Action dropdown opens with the selection on top' { Cmd 200; $f = [H]::F('PowerNappFly'); Snap 'PowerNappFly' 'dropdown'; [H]::SendMessage($f, $WM_KEYDOWN, [IntPtr]0x1B, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 300; $f -ne [IntPtr]::Zero -and [H]::F('PowerNappFly') -eq [IntPtr]::Zero }
Check 'time picker opens' { Cmd 210; $tp = [H]::F('PowerNappTimePick'); Snap 'PowerNappTimePick' 'timepicker'; [H]::SendMessage($tp, $WM_KEYDOWN, [IntPtr]0x1B, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 300; $tp -ne [IntPtr]::Zero }
Check 'window is named PowerNapp' { $n = [H]::Uia($main, 30005); Note "name=$n"; $n -eq 'PowerNapp' }
Check 'v3 layout: compact Options rows, links in the footer' {
    $k = $dpi / 96; $ch = [H]::ClientH($main)
    # daily, Lock (no Wake up): 56 + 28 + 96 + 4 + 36 + 3x(68+4) + 36 + 5x(48+4) (4 toggles + Language) + 16 + 20 + 28
    $want = [math]::Round((56 + 28 + 96 + 4 + 36 + 216 + 36 + 260 + 16 + 20 + 28) * $k)
    Note "client height $ch, expected ~$want"; [math]::Abs($ch - $want) -le 4 }
Check 'About dialog' { Cmd 251; $a = [H]::F('PowerNappAbout'); Snap 'PowerNappAbout' 'about'
    $n = [H]::Uia($a, 30005); $d = [H]::Uia($a, 30094)
    [H]::SendMessage($a, $WM_KEYDOWN, [IntPtr]0x09, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 200; $f = [H]::Uia($a, 30094)
    Note "name=$n | after Tab: $f | text has contact: $($d -match 'gig to offer' -and $d -match 'Los Angeles')"
    [H]::SendMessage($a, $WM_CLOSE, [IntPtr]0, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 300
    $n -eq 'About PowerNapp' -and $f -eq 'Contact us button' -and [H]::F('PowerNappAbout') -eq [IntPtr]::Zero }

# ----------------------------------------------------------------------------- keyboard
Section 'Keyboard'
StopApp; $main = StartApp
Check 'Tab order: Start > Action > Schedule > Time > Remind' {
    $o = @(); for ($i = 0; $i -lt 5; $i++) { Key 0x09; $o += [H]::GetDlgCtrlID([H]::Focus($main)) }
    Note ($o -join ' > '); ($o -join ',') -eq '240,200,201,210,230' }
Check 'Space toggles Force apps to close (and back)' {
    # Tab until Force has the focus: a Tab that lands while another program is briefly in front is undone
    # when the window is activated again (it puts the focus back where it was, as dialogs do)
    for ($i = 0; $i -lt 10 -and [H]::GetDlgCtrlID([H]::Focus($main)) -ne 231; $i++) { Key 0x09 }
    $f = [H]::GetDlgCtrlID([H]::Focus($main)); $a = Ini 'force'; Key 0x20; $b = Ini 'force'; Key 0x20; $c = Ini 'force'
    Note "focus $f after $i Tab(s): $a > $b > $c"; $f -eq 231 -and $a -eq '0' -and $b -eq '1' -and $c -eq '0' }
Check 'time picker by keyboard: Down + Enter changes the hour' {
    StopApp; $main = StartApp; for ($i = 0; $i -lt 10 -and [H]::GetDlgCtrlID([H]::Focus($main)) -ne 210; $i++) { Key 0x09 }   # focus Time
    function WaitWin($cls, $want) { for ($i = 0; $i -lt 40 -and (([H]::F($cls) -ne [IntPtr]::Zero) -ne $want); $i++) { Start-Sleep -Milliseconds 100 } }
    Key 0x20; WaitWin 'PowerNappTimePick' $true; Start-Sleep -Milliseconds 400; Key 0x28; Key 0x0D; WaitWin 'PowerNappTimePick' $false; $h1 = Ini 'h'
    Start-Sleep -Milliseconds 400; Key 0x20; WaitWin 'PowerNappTimePick' $true; Start-Sleep -Milliseconds 400; Key 0x26; Key 0x0D; WaitWin 'PowerNappTimePick' $false; $h2 = Ini 'h'
    Note "23 > $h1 > $h2"; $h1 -eq '12' -and $h2 -eq '23' }

# ----------------------------------------------------------------------------- accessibility
Section 'Screen reader (native UI Automation)'
StopApp; SetIni 'remind' 1; $main = StartApp
$names = @{ 240 = 'Start'; 200 = 'Action'; 201 = 'Schedule'; 210 = 'Time'; 230 = 'Remind me 1 minute before'; 205 = 'Language'; 250 = 'Power settings'; 251 = 'About' }
$types = @{ 240 = '50000'; 200 = '50003'; 201 = '50003'; 210 = '50003'; 230 = '50002'; 205 = '50003'; 250 = '50005'; 251 = '50000' }
foreach ($id in 240, 200, 201, 210, 230, 205, 250, 251) {
    $h = [H]::GetDlgItem($main, $id)
    Check "control $id is '$($names[$id])'" { $n = [H]::Uia($h, 30005); $t = [H]::Uia($h, 30003); $v = [H]::Uia($h, 30045); Note "name=$n type=$t value=$v"; $n -eq $names[$id] -and $t -eq $types[$id] }
}
Check 'Remind me 1 minute before reports On' { $s = [H]::Uia([H]::GetDlgItem($main, 230), 30086); Note "toggle=$s"; $s -eq '1' }

# ----------------------------------------------------------------------------- display scaling
Section 'Display scaling (simulated 150%)'
StopApp; $env:POWERNAPP_TEST_DPI = '144'; $main = StartApp; Remove-Item Env:POWERNAPP_TEST_DPI
Check 'client width = 960 at 150%' { $w = [H]::ClientW($main); Note "client ${w}x$([H]::ClientH($main))"; $w -eq 960 }
Snap 'PowerNappWnd' 'main-150'
StopApp

# ----------------------------------------------------------------------------- regional format
# The clock and the first day of the week follow the Windows regional format, in every language.
Section 'Regional format (24-hour clock, week from Sunday)'
$intl = 'HKCU:\Control Panel\International'
$LC = @{ sShortTime = 0x79; sTimeFormat = 0x1003; iFirstDayOfWeek = 0x100C }   # LOCALE_SSHORTTIME, _STIMEFORMAT, _IFIRSTDAYOFWEEK
# the user's effective values before the test (a value missing from the registry reads as the locale default)
$saved = @{}; foreach ($k in $LC.Keys) { $saved[$k] = [H]::LocaleInfo($LC[$k]) }
function SetIntl($v) { foreach ($k in $v.Keys) { [H]::SetLocaleInfoW(0x400, $LC[$k], $v[$k]) | Out-Null; Set-ItemProperty $intl $k $v[$k] }; [H]::IntlChanged(); Start-Sleep -Milliseconds 700 }
function FirstDayBtn { 220..226 | Sort-Object { [H]::Left([H]::GetDlgItem($main, $_)) } | Select-Object -First 1 }
Log "regional format before: $(($saved.Keys | ForEach-Object { "$_=$($saved[$_])" }) -join ', ')"
try {
SetIntl @{ sShortTime = 'HH:mm'; sTimeFormat = 'HH:mm'; iFirstDayOfWeek = '6' }
SetIni 'task' 5; SetIni 'mode' 5; SetIni 'h' 23; SetIni 'm' 0; SetIni 'days' 127
$main = StartApp
Snap 'PowerNappWnd' 'regional-24h'
Check 'time button reads 23:00' { $v = [H]::Uia([H]::GetDlgItem($main, 210), 30045); Note "value=$v"; $v -eq '23:00' }
Check 'time picker has no AM/PM column (Right wraps after the minutes, Down turns 23 into 0)' {
    for ($i = 0; $i -lt 10 -and [H]::GetDlgCtrlID([H]::Focus($main)) -ne 210; $i++) { Key 0x09 }
    Key 0x20; for ($i = 0; $i -lt 40 -and [H]::F('PowerNappTimePick') -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 100 }
    Start-Sleep -Milliseconds 400; Snap 'PowerNappTimePick' 'timepicker-24h'
    Key 0x27; Key 0x27; Key 0x27; Key 0x28; Key 0x0D   # Right x3: hour, minute, back to hour (AM/PM with 3 columns)
    for ($i = 0; $i -lt 40 -and [H]::F('PowerNappTimePick') -ne [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 100 }
    $h = Ini 'h'; Note "h 23 > $h"; $h -eq '0' }
Check 'the first day button is Sunday, and first in the Tab order' {
    $id = FirstDayBtn; $n = [H]::Uia([H]::GetDlgItem($main, $id), 30005)
    Key 0x09; $t = [H]::GetDlgCtrlID([H]::Focus($main))
    Note "leftmost $id '$n', Tab after Time: $t"; $id -eq 226 -and $n -eq 'Sunday' -and $t -eq 226 }
# changed while the app runs: 12-hour clock, week from Monday
SetIntl @{ sShortTime = 'h:mm tt'; sTimeFormat = 'h:mm:ss tt'; iFirstDayOfWeek = '0' }
Check 'live change: time button reads 12:00 AM, the first day button is Monday' {
    $v = [H]::Uia([H]::GetDlgItem($main, 210), 30045); $id = FirstDayBtn; $n = [H]::Uia([H]::GetDlgItem($main, $id), 30005)
    Note "value=$v, leftmost $id '$n'"; $v -eq "12:00$([char]0xA0)AM" -and $id -eq 220 -and $n -eq 'Monday' }
} finally {   # restore exactly the values from before, whatever failed above
    StopApp
    SetIntl $saved
}
Check 'regional format restored' { $now = Get-ItemProperty $intl; $ok = $true; foreach ($k in $saved.Keys) { $e = [H]::LocaleInfo($LC[$k]); if ($e -ne $saved[$k] -or $now.$k -ne $saved[$k]) { $ok = $false; Note "$k=$e (registry '$($now.$k)') want '$($saved[$k])'" } }; $ok }

# ----------------------------------------------------------------------------- language picker
# The installer ships lang\; add the test-only pseudo-language (tests\sandbox\qps-ploc.txt, accented English
# ~40% longer, @name = Pseudo) next to the installed exe.
Section 'Language picker'
$langDir = Join-Path (Split-Path $exe) 'lang'
Copy-Item "$T\qps-ploc.txt" $langDir -Force
$pseudo = @{}; foreach ($l in Get-Content "$T\qps-ploc.txt" -Encoding UTF8) { if ($l -match '^([a-z_.]+) = (.*)$') { $pseudo[$Matches[1]] = $Matches[2] } }
function LangValue { [H]::Uia([H]::GetDlgItem([H]::F('PowerNappWnd'), 205), 30045) }
# open the picker, End (its value announces the last entry), then Enter picks it / Esc closes
function PickLast($vk) { Cmd 205; $f = [H]::F('PowerNappFly'); $h = [H]::ClientH($f)
    [H]::SendMessage($f, $WM_KEYDOWN, [IntPtr]0x23, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 300; $last = LangValue
    [H]::SendMessage($f, $WM_KEYDOWN, [IntPtr]$vk, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 800
    @{ shown = $f -ne [IntPtr]::Zero; h = $h; last = $last } }
# open the picker, Home, then Down until its value announces $name (the entries are sorted by name,
# so a pseudo-language sits among the translations, not at the end); Enter picks it. last = what was picked.
function PickNamed($name) { Cmd 205; $f = [H]::F('PowerNappFly')
    [H]::SendMessage($f, $WM_KEYDOWN, [IntPtr]0x24, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 200; $v = LangValue
    for ($i = 0; $i -lt 60 -and $v -ne $name; $i++) { [H]::SendMessage($f, $WM_KEYDOWN, [IntPtr]0x28, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 100; $v = LangValue }
    [H]::SendMessage($f, $WM_KEYDOWN, [IntPtr]0x0D, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 800
    @{ shown = $f -ne [IntPtr]::Zero; last = $v } }
StopApp; SetIni 'lang' ''; SetIni 'task' 5; SetIni 'mode' 0; SetIni 'h' 23; SetIni 'm' 0; SetIni 'tray' 1
$main = StartApp
Check 'picker reads Windows default (English)' { $v = LangValue; $n = [H]::Uia([H]::GetDlgItem($main, 205), 30005); Note "name=$n value=$v"; $v -eq 'Windows default (English)' }
Check 'choosing Pseudo switches the window without a restart and saves lang=qps-ploc' {
    $r = PickNamed 'Pseudo'; $b = [H]::Uia([H]::GetDlgItem($main, 240), 30005)
    Note "picked=$($r.last) start='$b' want '$($pseudo['btn.start'])' lang=$(Ini 'lang') same window=$([H]::F('PowerNappWnd') -eq $main)"
    $r.last -eq 'Pseudo' -and $b -eq $pseudo['btn.start'] -and (Ini 'lang') -eq 'qps-ploc' -and [H]::F('PowerNappWnd') -eq $main }
Snap 'PowerNappWnd' 'language-pseudo'
StopApp; $main = StartApp
Check 'restarting keeps the language' { $b = [H]::Uia([H]::GetDlgItem($main, 240), 30005); $v = LangValue; Note "start='$b' picker=$v"; $b -eq $pseudo['btn.start'] -and $v -eq 'Pseudo' }
StopApp
Check 'the reminder card from --fire-now shows the pseudo text' {
    $p = Start-Process $exe -ArgumentList '--task', 'display', '--fire-now', '--warn-secs', '30' -PassThru
    for ($i = 0; $i -lt 20 -and [H]::F('PowerNappReminder') -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 250 }
    Start-Sleep -Seconds 1; Snap 'PowerNappReminder' 'reminder-pseudo'
    $rm = [H]::F('PowerNappReminder'); $n = [H]::Uia($rm, 30005)
    [H]::SendMessage($rm, $WM_KEYDOWN, [IntPtr]0x1B, [IntPtr]0) | Out-Null; Start-Sleep -Seconds 1; $p.Refresh()
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
    Note "name='$n'"; $rm -ne [IntPtr]::Zero -and $n.Contains($pseudo['task.display']) }
Check 'saved language missing: starts in English, the picker shows Windows default' {
    Remove-Item "$langDir\qps-ploc.txt"; $main = StartApp
    $b = [H]::Uia([H]::GetDlgItem($main, 240), 30005); $v = LangValue; Note "lang=$(Ini 'lang') start=$b picker=$v"
    $b -eq 'Start' -and $v -eq 'Windows default (English)' }
StopApp
Check 'no lang folder: the picker lists only Windows default and English' {
    Rename-Item $langDir 'lang-away'; $main = StartApp
    $r = PickLast 0x1B; $want = [math]::Round((2 * 36 + 4) * $dpi / 96)
    Note "last entry=$($r.last) flyout height $($r.h) (2 entries: $want)"; $r.shown -and $r.last -eq 'English' -and [math]::Abs($r.h - $want) -le 2 }
StopApp
Rename-Item "$(Split-Path $exe)\lang-away" 'lang'; SetIni 'lang' ''

# The tray-only process stays tiny in any language: Task Manager's "Memory" column (private working
# set) once it has settled, with a translation loaded (de, ja) and following Windows (English here).
Section 'Tray-only memory'
foreach ($l in 'de', 'ja', '') {
    StopApp; SetIni 'lang' $l; SetIni 'tray' 1
    Check "tray-only process, lang='$l': private working set <= 256 KB after 6 s" {
        $p = Start-Process $exe -ArgumentList '--tray' -PassThru; Start-Sleep -Seconds 6; $p.Refresh()
        $ws = (Get-CimInstance Win32_PerfFormattedData_PerfProc_Process -Filter "IDProcess=$($p.Id)").WorkingSetPrivate
        $kb = [math]::Round($ws / 1KB)
        Note "private working set $kb KB (private bytes $([int]($p.PrivateMemorySize64 / 1KB)) KB), window=$([H]::F('PowerNappWnd') -ne [IntPtr]::Zero)"
        -not $p.HasExited -and $ws -gt 0 -and $kb -le 256 -and [H]::F('PowerNappWnd') -eq [IntPtr]::Zero }
}
StopApp; SetIni 'lang' ''

# ----------------------------------------------------------------------------- right-to-left
# tests\sandbox\qps-plocm.txt: the Pseudo strings as a right-to-left language (@rtl = 1, @name = Pseudo Mirrored,
# picked by name). Every window mirrors: caption buttons and controls on the left, the reminder card bottom left.
Section 'Right-to-left (mirrored pseudo-language)'
Copy-Item "$T\qps-ploc.txt", "$T\qps-plocm.txt" $langDir -Force
StopApp; SetIni 'lang' 'qps-plocm'; SetIni 'task' 5; SetIni 'mode' 0; SetIni 'h' 23; SetIni 'm' 0; SetIni 'tray' 1; SetIni 'remind' 1
$main = StartApp
$k = $dpi / 96
function IsRtl($h) { ([H]::ExStyle($h) -band 0x400000) -ne 0 }
Check 'the main window is mirrored (WS_EX_LAYOUTRTL)' { Note ('exstyle 0x{0:X}' -f [H]::ExStyle($main)); IsRtl $main }
Snap 'PowerNappWnd' 'rtl-main'
Check 'the Start button is left of the hero text' {
    $w = [H]::Win($main); $s = [H]::Win([H]::GetDlgItem($main, 240)); Note "window $($w.L)..$($w.R), Start $($s.L)..$($s.R)"
    $s.R -lt ($w.L + $w.R) / 2 }
Check 'caption buttons on the left: close and minimize hit-test there, the right end is title bar' {
    $w = [H]::Win($main); $y = $w.T + [int](16 * $k)   # the window rect has ~8 px invisible borders at the sides
    $close = [H]::Hit($main, $w.L + [int](30 * $k), $y); $min = [H]::Hit($main, $w.L + [int](123 * $k), $y); $title = [H]::Hit($main, $w.R - [int](30 * $k), $y)
    Note "close=$close min=$min right end=$title (HTCLOSE 20, HTMINBUTTON 8, HTCAPTION 2)"
    $close -eq 20 -and $min -eq 8 -and $title -eq 2 }
Check 'popups are mirrored too: dropdown, time picker, About' {
    Cmd 200; $f = [H]::F('PowerNappFly'); $fr = IsRtl $f; Snap 'PowerNappFly' 'rtl-dropdown'
    [H]::SendMessage($f, $WM_KEYDOWN, [IntPtr]0x1B, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 300
    Cmd 210; $tp = [H]::F('PowerNappTimePick'); $tr = IsRtl $tp; Snap 'PowerNappTimePick' 'rtl-timepicker'
    [H]::SendMessage($tp, $WM_KEYDOWN, [IntPtr]0x1B, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 300
    Cmd 251; $a = [H]::F('PowerNappAbout'); $ar = IsRtl $a; Snap 'PowerNappAbout' 'rtl-about'
    [H]::SendMessage($a, $WM_CLOSE, [IntPtr]0, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 300
    Note "dropdown=$fr timepicker=$tr about=$ar"; $fr -and $tr -and $ar }
StopApp
Check 'the reminder card is mirrored, at the bottom left' {
    Add-Type -AssemblyName System.Windows.Forms; $wa = [System.Windows.Forms.Screen]::PrimaryScreen.WorkingArea
    $p = Start-Process $exe -ArgumentList '--task', 'display', '--fire-now', '--warn-secs', '30' -PassThru
    for ($i = 0; $i -lt 20 -and [H]::F('PowerNappReminder') -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 250 }
    Start-Sleep -Seconds 1; Snap 'PowerNappReminder' 'rtl-reminder'
    $rm = [H]::F('PowerNappReminder'); $r = [H]::Win($rm); $rtl = IsRtl $rm
    [H]::SendMessage($rm, $WM_KEYDOWN, [IntPtr]0x1B, [IntPtr]0) | Out-Null; Start-Sleep -Seconds 1; $p.Refresh()
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
    Note "card $($r.L)..$($r.R), work area $($wa.Left)..$($wa.Right), mirrored=$rtl"
    $rm -ne [IntPtr]::Zero -and $rtl -and $r.R -lt ($wa.Left + $wa.Right) / 2 }
SetIni 'lang' 'qps-ploc'; $main = StartApp
Check 'switch to rtl with flyout open: no dropdown left, the window comes back mirrored where it was' {
    $w0 = [H]::Win($main)
    Cmd 200; $f1 = [H]::F('PowerNappFly') -ne [IntPtr]::Zero   # Action dropdown open
    $r = PickNamed 'Pseudo Mirrored'                            # the Language dropdown takes over: pick Pseudo Mirrored
    $m2 = [H]::F('PowerNappWnd'); $w1 = [H]::Win($m2)
    Note "action dropdown was open=$f1, picked '$($r.last)', dropdown left=$([H]::F('PowerNappFly') -ne [IntPtr]::Zero), mirrored=$(IsRtl $m2), at ($($w1.L),$($w1.T)) was ($($w0.L),$($w0.T)), lang=$(Ini 'lang')"
    $f1 -and $r.last -eq 'Pseudo Mirrored' -and [H]::F('PowerNappFly') -eq [IntPtr]::Zero -and (IsRtl $m2) -and
        $w1.L -eq $w0.L -and $w1.T -eq $w0.T -and (Ini 'lang') -eq 'qps-plocm' }
Snap 'PowerNappWnd' 'rtl-after-switch'
StopApp
Remove-Item "$langDir\qps-ploc.txt", "$langDir\qps-plocm.txt"; SetIni 'lang' ''

# ----------------------------------------------------------------------------- Task Scheduler is the schedule
Section 'Task Scheduler is the schedule'
schtasks /delete /tn PowerNapp /f 2>$null | Out-Null
SetIni 'task' 5; SetIni 'mode' 0; SetIni 'h' 23; SetIni 'm' 0; SetIni 'tray' 1; SetIni 'force' 0; SetIni 'remind' 1
$main = StartApp
Cmd 240
Check 'Start registers the task right away' { $b = [H]::Uia([H]::GetDlgItem($main, 240), 30005); Note "button=$b"; $b -eq 'Cancel' -and (TaskExists 'PowerNapp') }
Check 'daily trigger, 1 min early, runs on battery' { $x = TaskXml 'PowerNapp'; $sb = $x.Task.Triggers.CalendarTrigger.StartBoundary; Note "start=$sb battery=$($x.Task.Settings.DisallowStartIfOnBatteries) args=$($x.Task.Actions.Exec.Arguments)"; $sb -match 'T22:59:00' -and $x.Task.Settings.DisallowStartIfOnBatteries -eq 'false' }
Check 'an edit while scheduled updates the task (Force on)' {
    for ($i = 0; $i -lt 6; $i++) { Key 0x09 }   # Start > Action > Schedule > Time > Remind > Force
    Key 0x20; $a = (TaskXml 'PowerNapp').Task.Actions.Exec.Arguments; Note "args=$a"; Key 0x20
    $a -match '--force' -and (TaskXml 'PowerNapp').Task.Actions.Exec.Arguments -notmatch '--force' }
[H]::PostMessage($main, $WM_CLOSE, [IntPtr]0, [IntPtr]0) | Out-Null; Start-Sleep -Seconds 4
Check 'closing keeps the task; only a small tray process is left' { $p = @(Get-Process powernapp); $pm = [int]($p[0].PrivateMemorySize64 / 1KB); Note "processes $($p.Count), private $pm KB"; (TaskExists 'PowerNapp') -and $p.Count -eq 1 -and [H]::F('PowerNappWnd') -eq [IntPtr]::Zero -and $pm -lt 4096 }
StopApp
schtasks /change /tn PowerNapp /st 21:29 2>$null | Out-Null
$main = StartApp
Check 'a time changed in Task Scheduler shows in the app' { $v = [H]::Uia([H]::GetDlgItem($main, 210), 30045); $b = [H]::Uia([H]::GetDlgItem($main, 240), 30005); Note "time=$v button=$b"; $v -eq "9:30$([char]0xA0)PM" -and $b -eq 'Cancel' }
schtasks /delete /tn PowerNapp /f 2>$null | Out-Null
[H]::SendMessage($main, 0x0006, [IntPtr]1, [IntPtr]0) | Out-Null; Start-Sleep -Milliseconds 800   # WM_ACTIVATE
Check 'a task deleted in Task Scheduler: back to Not scheduled' { $b = [H]::Uia([H]::GetDlgItem($main, 240), 30005); Note "button=$b"; $b -eq 'Start' }
Cmd 240
Check 'Cancel deletes the task' { $ok1 = TaskExists 'PowerNapp'; Cmd 240; $ok2 = -not (TaskExists 'PowerNapp'); Note "after Start=$ok1 after Cancel=$ok2"; $ok1 -and $ok2 }
StopApp

# ----------------------------------------------------------------------------- wake
Section 'Wake up'
SetIni 'task' 2; SetIni 'wake' 0
$main = StartApp
Cmd 234
Check 'Wake up on: wake task with WakeToRun' { $x = TaskXml 'PowerNapp wake'; Note "WakeToRun=$($x.Task.Settings.WakeToRun) args=$($x.Task.Actions.Exec.Arguments)"; $x.Task.Settings.WakeToRun -eq 'true' }
Check '--wake runs' { $c = RunCli @('--wake'); Note "exit $c"; $c -eq 0 }
Cmd 234
Check 'Wake up off: task deleted' { -not (TaskExists 'PowerNapp wake') }
Cmd 232
Check 'Start with Windows writes the Run key' { $v = (Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run').PowerNapp; Note $v; $v -match '--tray' }
StopApp

# ----------------------------------------------------------------------------- reminder card
Section 'Reminder card'
Check 'card shows and counts down; Esc stops without running' {
    $p = Start-Process $exe -ArgumentList '--task', 'display', '--fire-now', '--warn-secs', '30' -PassThru
    for ($i = 0; $i -lt 20 -and [H]::F('PowerNappReminder') -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 250 }
    Start-Sleep -Seconds 2; Snap 'PowerNappReminder' 'reminder'
    $rm = [H]::F('PowerNappReminder'); [H]::SendMessage($rm, $WM_KEYDOWN, [IntPtr]0x1B, [IntPtr]0) | Out-Null
    Start-Sleep -Seconds 1; $p.Refresh(); Note "shown=$($rm -ne [IntPtr]::Zero) exited=$($p.HasExited)"; $rm -ne [IntPtr]::Zero -and $p.HasExited -and $p.ExitCode -eq 0 }
Check 'Snooze hides the card and waits' {
    $p = Start-Process $exe -ArgumentList '--task', 'display', '--fire-now', '--warn-secs', '30' -PassThru
    for ($i = 0; $i -lt 20 -and [H]::F('PowerNappReminder') -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 250 }
    $rm = [H]::F('PowerNappReminder'); [H]::SendMessage($rm, 0x0202, [IntPtr]0, [H]::MK(240, 152)) | Out-Null
    Start-Sleep -Seconds 2; $p.Refresh(); $gone = [H]::F('PowerNappReminder') -eq [IntPtr]::Zero; $alive = -not $p.HasExited
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue; Note "card gone=$gone waiting=$alive"; $gone -and $alive }

# ----------------------------------------------------------------------------- a time closer than the reminder
Section 'Daily time picked less than a minute ahead still runs today'
schtasks /delete /tn PowerNapp /f 2>$null | Out-Null
while ((Get-Date).Second -lt 15 -or (Get-Date).Second -gt 40) { Start-Sleep -Milliseconds 500 }   # 20-45 s before the next minute
$at = (Get-Date).AddMinutes(1)
SetIni 'task' 6; SetIni 'mode' 0; SetIni 'h' $at.Hour; SetIni 'm' $at.Minute; SetIni 'remind' 1; SetIni 'tray' 0; SetIni 'force' 0
$main = StartApp; Cmd 240; $t0 = Get-Date
Check 'the task gets a catch-up trigger and --at' { $x = TaskXml 'PowerNapp'; $a = $x.Task.Actions.Exec.Arguments; Note "args=$a calendar=$([bool]$x.Task.Triggers.CalendarTrigger) catchup=$([bool]$x.Task.Triggers.TimeTrigger)"; $x.Task.Triggers.CalendarTrigger -and $x.Task.Triggers.TimeTrigger -and $a -match ('--at {0:D2}:{1:D2}' -f $at.Hour, $at.Minute) }
Check 'the app still shows the daily schedule' { $v = [H]::Uia([H]::GetDlgItem($main, 201), 30045); $b = [H]::Uia([H]::GetDlgItem($main, 240), 30005); Note "schedule=$v button=$b"; $v -eq 'At a specific time' -and $b -eq 'Cancel' }
$shown = $null; while (((Get-Date) - $t0).TotalSeconds -lt 15 -and -not $shown) { if ([H]::F('PowerNappReminder') -ne [IntPtr]::Zero) { $shown = Get-Date } else { Start-Sleep -Milliseconds 300 } }
Check 'the reminder appears right away' { Note "after $(if ($shown) { [int]($shown - $t0).TotalSeconds } else { 'never' }) s"; $shown -ne $null }
while ([H]::F('PowerNappReminder') -ne [IntPtr]::Zero -and ((Get-Date) - $t0).TotalSeconds -lt 90) { Start-Sleep -Milliseconds 300 }
$gone = Get-Date; Start-Sleep -Seconds 3
Check 'the action runs at the picked time, today' {
    $i = Get-ScheduledTaskInfo -TaskName PowerNapp; $off = [int]($gone - [datetime]::new($at.Year, $at.Month, $at.Day, $at.Hour, $at.Minute, 0)).TotalSeconds
    Note "card closed $off s from $('{0:HH:mm}' -f $at), LastTaskResult=$($i.LastTaskResult), next run $($i.NextRunTime)"; [math]::Abs($off) -le 5 -and $i.LastTaskResult -eq 0 }
Cmd 240
Check 'Cancel removes it' { -not (TaskExists 'PowerNapp') }
StopApp

# ----------------------------------------------------------------------------- end to end: Task Scheduler fires a countdown
Section 'End to end: Windows runs a 5-minute countdown (Turn off display, reminder on)'
function CountdownToCard {
    schtasks /delete /tn PowerNapp /f 2>$null | Out-Null
    SetIni 'task' 6; SetIni 'mode' 2; SetIni 'cd' 5; SetIni 'remind' 1; SetIni 'tray' 0; SetIni 'snooze_until' 0
    $m = StartApp; Cmd 240; $script:t0 = Get-Date
    [H]::PostMessage($m, $WM_CLOSE, [IntPtr]0, [IntPtr]0) | Out-Null; Start-Sleep -Seconds 4
    $script:shown = $null
    while (((Get-Date) - $script:t0).TotalSeconds -lt 330 -and -not $script:shown) { if ([H]::F('PowerNappReminder') -ne [IntPtr]::Zero) { $script:shown = Get-Date } else { Start-Sleep -Seconds 1 } }
}
CountdownToCard
Check 'tray off: closing leaves no PowerNapp process' { $n = @(Get-Process powernapp -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -ne 'PowerNapp reminder' }).Count; Note "card shown=$($script:shown -ne $null)"; $script:shown -ne $null }
Check 'reminder card appears 4 min after Start' { if ($script:shown) { Start-Sleep -Seconds 3; Snap 'PowerNappReminder' 'reminder-scheduled'; $s = [int]($script:shown - $script:t0).TotalSeconds; Note "after $s s"; $s -ge 225 -and $s -le 265 } else { $false } }
while (((Get-Date) - $script:t0).TotalSeconds -lt 400 -and [H]::F('PowerNappReminder') -ne [IntPtr]::Zero) { Start-Sleep -Seconds 1 }
$gone = Get-Date; Start-Sleep -Seconds 5
Check 'countdown ran out at 5 min and the action ran (task result 0)' {
    $i = Get-ScheduledTaskInfo -TaskName PowerNapp -ErrorAction SilentlyContinue; $s = [int]($gone - $script:t0).TotalSeconds
    Note "card closed after $s s, LastTaskResult=$($i.LastTaskResult)"; $s -ge 285 -and $s -le 330 -and $i.LastTaskResult -eq 0 }
$main = StartApp
Check 'after the run the app shows Not scheduled' { $b = [H]::Uia([H]::GetDlgItem($main, 240), 30005); Note "button=$b"; $b -eq 'Start' }
StopApp

Section 'End to end: Snooze on the scheduled card shows in the app'
CountdownToCard
Check 'Snooze on the card from Task Scheduler' {
    $rm = [H]::F('PowerNappReminder'); [H]::SendMessage($rm, 0x0202, [IntPtr]0, [H]::MK(240, 152)) | Out-Null; Start-Sleep -Seconds 2
    $su = Ini 'snooze_until'; Note "card shown=$($script:shown -ne $null) snooze_until=$su"; $script:shown -ne $null -and [int64]$su -gt 0 }
$main = StartApp
Check 'the window shows the snoozed time (about 10 min left)' {
    $d = [H]::Uia([H]::GetDlgItem($main, 240), 30094)   # LegacyIAccessible.Description of Start = hero text
    if (-not $d) { $d = [H]::Uia([H]::GetDlgItem($main, 240), 30013) }
    Note "hero='$d'"; $d -match '(9|10):\d\d left' }
StopApp
Get-Process powernapp -ErrorAction SilentlyContinue | Stop-Process -Force   # the snoozed --fire-now process
schtasks /delete /tn PowerNapp /f 2>$null | Out-Null

# ----------------------------------------------------------------------------- unsupported actions fail cleanly
Section 'Unsupported actions'
if ($avail -notmatch 'Hibernate') { Check 'Hibernate unavailable: exit 3, no hang' { $c = RunCli @('--task', 'hibernate', '--fire-now'); Note "exit $c"; $c -eq 3 } } else { Log '- SKIP Hibernate is available here' }
if ($avail -notmatch 'Standby') { Check 'Sleep unavailable: exit 3, no hang' { $c = RunCli @('--task', 'sleep', '--fire-now'); Note "exit $c"; $c -eq 3 } } else { Log '- SKIP Sleep is available here' }

# ----------------------------------------------------------------------------- lock (session stays locked afterwards)
Section 'Lock (reminder runs out)'
Check '--warn-secs 5 then Lock: card, then the session locks' {
    $p = Start-Process $exe -ArgumentList '--task', 'lock', '--fire-now', '--warn-secs', '5' -PassThru
    Start-Sleep -Seconds 2; $card = [H]::F('PowerNappReminder') -ne [IntPtr]::Zero
    Start-Sleep -Seconds 7; $p.Refresh(); $locked = [bool](Get-Process LogonUI -ErrorAction SilentlyContinue)
    Note "card=$card exited=$($p.HasExited) exit=$($p.ExitCode) LogonUI=$locked"; $card -and $p.HasExited -and $p.ExitCode -eq 0 -and $locked }

# ----------------------------------------------------------------------------- uninstall
Section 'Uninstaller'
& $exe --install-daily 23:00 --task sleep | Out-Null
Check 'before: PowerNapp daily task + Run key exist' { (TaskExists 'PowerNapp daily') -and (Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run').PowerNapp }
Check 'silent uninstall exits 0' { $p = Start-Process "$env:LOCALAPPDATA\Programs\PowerNapp\uninstall.exe" -ArgumentList '/S' -PassThru -Wait; Start-Sleep -Seconds 3; Note "exit $($p.ExitCode)"; $p.ExitCode -eq 0 }
Check 'files, shortcut and Apps entry removed' { -not (Test-Path $exe) -and -not (Test-Path $lnk) -and -not (Test-Path $ukey) }
Check 'scheduled tasks removed' { -not (TaskExists 'PowerNapp') -and -not (TaskExists 'PowerNapp daily') -and -not (TaskExists 'PowerNapp wake') }
Check 'lang folder and install folder removed' { $d = Split-Path $exe; Note "left: $(@(Get-ChildItem $d -Recurse -Name -ErrorAction SilentlyContinue) -join ', ')"; -not (Test-Path "$d\lang") -and -not (Test-Path $d) }
Check 'Run key and settings removed' { -not (Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run').PowerNapp -and -not (Test-Path (Split-Path $ini)) }

# ----------------------------------------------------------------------------- done; shut down with PowerNapp
Log "`n**$($script:pass) passed, $($script:fail) failed** - finished $(Get-Date -Format 'HH:mm:ss')"
Log "`nLast step: PowerNapp shuts the sandbox down (``--task shutdown --fire-now --force``)."
Set-Content "$R\DONE" (Get-Date -Format o)
Start-Sleep -Seconds 2
$p = Start-Process "$T\powernapp.exe" -ArgumentList '--task','shutdown','--fire-now','--force' -PassThru -Wait
Add-Content "$R\DONE" "shutdown exit code: $($p.ExitCode) at $(Get-Date -Format HH:mm:ss)"
