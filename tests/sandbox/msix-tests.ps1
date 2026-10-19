# PowerNapp Store (MSIX) package tests, run inside Windows Sandbox (Run-Sandbox.ps1 -Suite msix).
# Registers the unsigned package layout with Developer Mode (sandbox only), then checks the
# packaged behavior: alias, tasks running the alias, Start with Windows as a logon task.
. "$PSScriptRoot\helpers.ps1"
$alias = "$env:LOCALAPPDATA\Microsoft\WindowsApps\powernapp.exe"

# Developer Mode lets an unsigned layout register (sandbox only; it is thrown away)
reg add 'HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock' /v AllowDevelopmentWithoutDevLicense /t REG_DWORD /d 1 /f | Out-Null
Copy-Item -Recurse "$T\layout" 'C:\PowerNappPkg'
$shipped = @(Get-ChildItem 'C:\PowerNappPkg\lang' -Filter *.txt -ErrorAction SilentlyContinue | ForEach-Object Name)
# the pseudo-language (test-only) goes into the registered layout, to see the packaged app load lang\
if (Test-Path 'C:\PowerNappPkg\lang' -PathType Container) { Copy-Item "$T\qps-ploc.txt" 'C:\PowerNappPkg\lang' -Force }

Log "`n## Package`n"
Check 'MSIX layout registers' { Add-AppxPackage -Register 'C:\PowerNappPkg\AppxManifest.xml' -ErrorAction Stop; $p = Get-AppxPackage *PowerNapp*; Note "$($p.PackageFullName)"; $p -ne $null }
$pkg = Get-AppxPackage *PowerNapp*
Check 'Start menu entry' { $a = Get-StartApps | Where-Object Name -eq 'PowerNapp'; Note "$($a.AppID)"; $a -ne $null }
Check 'app execution alias powernapp.exe exists' { Note $alias; Test-Path $alias }
Check 'alias runs the packaged exe (--help)' { $o = & $alias --help | Out-String; Note ($o.Split("`n")[0].Trim()); $o -match 'PowerNapp' }

Log "`n## Packaged app`n"
Start-Process "shell:AppsFolder\$($pkg.PackageFamilyName)!PowerNapp"
for ($i = 0; $i -lt 30 -and [H]::F('PowerNappWnd') -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 300 }
Start-Sleep -Seconds 1
$main = [H]::F('PowerNappWnd')
Check 'window opens from the Start menu entry' { $n = [H]::Uia($main, 30005); Note "name=$n"; $n -eq 'PowerNapp' }
[H]::Capture($main, "$R\msix-main.png") | Out-Null
Check 'process runs from the package' { $p = Get-Process powernapp | Select-Object -First 1; Note $p.Path; $p.Path -like '*PowerNappPkg*' -or $p.Path -like '*WindowsApps*' }

Cmd 240   # Start
Check 'Start registers the task, which runs the alias' { $x = TaskXml 'PowerNapp'; $c = $x.Task.Actions.Exec.Command; Note "command=$c args=$($x.Task.Actions.Exec.Arguments)"; (TaskExists 'PowerNapp') -and $c -eq $alias }
Cmd 240   # Cancel
Check 'Cancel deletes the task' { -not (TaskExists 'PowerNapp') }

Check 'Task Scheduler can start the app through the alias' {
    # a one-off task like the app's own, running the harmless --wake (display on)
    schtasks /create /tn 'PowerNapp alias test' /tr "`"$alias`" --wake" /sc once /st 23:59 /f | Out-Null
    schtasks /run /tn 'PowerNapp alias test' | Out-Null
    $r = $null
    for ($i = 0; $i -lt 30; $i++) { Start-Sleep -Seconds 1; $r = (Get-ScheduledTaskInfo -TaskName 'PowerNapp alias test').LastTaskResult; if ($r -ne 267011 -and $r -ne 267009) { break } }
    schtasks /delete /tn 'PowerNapp alias test' /f | Out-Null
    Note "LastTaskResult=$r"; $r -eq 0 }

$before = TaskExists 'PowerNapp startup'
Cmd 232   # Start with Windows on
Check 'Start with Windows: a logon task runs the alias with --tray' {
    $x = TaskXml 'PowerNapp startup'
    Note "before=$before user=$($x.Task.Triggers.LogonTrigger.UserId) command=$($x.Task.Actions.Exec.Command) args=$($x.Task.Actions.Exec.Arguments) limit=$($x.Task.Settings.ExecutionTimeLimit)"
    $x.Task.Triggers.LogonTrigger -and $x.Task.Actions.Exec.Command -eq $alias -and $x.Task.Actions.Exec.Arguments -eq '--tray' -and $x.Task.Settings.ExecutionTimeLimit -eq 'PT0S' }
Check 'the toggle reads On (state comes from the task)' { $s = [H]::Uia([H]::GetDlgItem($main, 232), 30086); Note "toggle=$s"; $s -eq '1' }
Check 'no Run value written' { $v = Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name PowerNapp -ErrorAction SilentlyContinue; $v -eq $null }
Cmd 232   # off
Check 'Start with Windows off deletes the logon task' { -not (TaskExists 'PowerNapp startup') }

Check 'logon task starts the tray process' {
    Cmd 232; Get-Process powernapp | Stop-Process -Force; Start-Sleep -Seconds 1
    schtasks /run /tn 'PowerNapp startup' | Out-Null
    for ($i = 0; $i -lt 20 -and [H]::F('PowerNappTray') -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 500 }
    $t = [H]::F('PowerNappTray') -ne [IntPtr]::Zero; $p = @(Get-Process powernapp -ErrorAction SilentlyContinue)
    Note "tray window=$t processes=$($p.Count)"; $t }

Log "`n## Languages`n"
Check 'the package ships lang\ with the translations, no pseudo-languages' {
    $d = Join-Path $pkg.InstallLocation 'lang'; Note "$d : $($shipped -join ', ')"
    (Test-Path $d -PathType Container) -and $shipped -contains 'en.txt' -and -not ($shipped -like 'qps-*') }
Check 'lang=qps-ploc: the packaged app shows the pseudo-language' {
    $pseudo = @{}; foreach ($l in Get-Content "$T\qps-ploc.txt" -Encoding UTF8) { if ($l -match '^([a-z_.]+) = (.*)$') { $pseudo[$Matches[1]] = $Matches[2] } }
    StopApp
    # a packaged app's writes to %APPDATA% go to the package's LocalCache: set the language in both places
    $real = $ini; $virt = "$env:LOCALAPPDATA\Packages\$($pkg.PackageFamilyName)\LocalCache\Roaming\PowerNapp\powernapp.ini"
    foreach ($f in @($real) + @($virt | Where-Object { Test-Path $_ })) { $ini = $f; SetIni 'lang' 'qps-ploc' }; $ini = $real
    Start-Process "shell:AppsFolder\$($pkg.PackageFamilyName)!PowerNapp"
    for ($i = 0; $i -lt 30 -and [H]::F('PowerNappWnd') -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 300 }
    Start-Sleep -Seconds 1; $m = [H]::F('PowerNappWnd'); Snap 'PowerNappWnd' 'msix-pseudo'
    $b = [H]::Uia([H]::GetDlgItem($m, 240), 30005)
    Note "start='$b' want '$($pseudo['btn.start'])', virtualized ini: $(Test-Path $virt)"; $b -eq $pseudo['btn.start'] }
# makepri indexes the layout: lang\de.txt must stay a plain file the app reads, not a German-qualified resource
Check 'lang=de: the packaged app reads lang\de.txt (Start and Language in German)' {
    $de = @{}; foreach ($l in Get-Content 'C:\PowerNappPkg\lang\de.txt' -Encoding UTF8) { if ($l -match '^([a-z_.]+) = (.*)$') { $de[$Matches[1]] = $Matches[2] } }
    StopApp
    $real = $ini; $virt = "$env:LOCALAPPDATA\Packages\$($pkg.PackageFamilyName)\LocalCache\Roaming\PowerNapp\powernapp.ini"
    foreach ($f in @($real) + @($virt | Where-Object { Test-Path $_ })) { $ini = $f; SetIni 'lang' 'de' }; $ini = $real
    Start-Process "shell:AppsFolder\$($pkg.PackageFamilyName)!PowerNapp"
    for ($i = 0; $i -lt 30 -and [H]::F('PowerNappWnd') -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 300 }
    Start-Sleep -Seconds 1; $m = [H]::F('PowerNappWnd'); Snap 'PowerNappWnd' 'msix-de'
    # German Start is also "Start": the footer Language picker's name (Sprache) tells German from English
    $b = [H]::Uia([H]::GetDlgItem($m, 240), 30005); $l = [H]::Uia([H]::GetDlgItem($m, 205), 30005)
    Note "start='$b' want '$($de['btn.start'])', language picker='$l' want '$($de['language.title'])'"
    $de['language.title'] -and $de['language.title'] -ne 'Language' -and $b -eq $de['btn.start'] -and $l -eq $de['language.title'] }

Log "`n**$($script:pass) passed, $($script:fail) failed** - finished $(Get-Date -Format 'HH:mm:ss')"
Set-Content "$R\DONE" (Get-Date -Format o)
Start-Sleep -Seconds 2
shutdown /s /t 0
