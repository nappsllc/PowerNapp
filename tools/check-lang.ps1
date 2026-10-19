<#
.SYNOPSIS
  Checks lang\<tag>.txt (and, with -Listing, store\listing\<tag>.txt) against the English originals.
.DESCRIPTION
  Language files (default: every lang\*.txt; -Tag de checks one): valid UTF-8 (a BOM is fine), @name
  set, @rtl = 1 exactly for ar and he, every key of lang\en.txt present, no unknown or duplicate keys,
  the same ordered sequence of %ls / %d / %lld placeholders as English (the loader ignores a value
  that differs), "PowerNapp" / "PowerNapps" kept where English has them, and the length guards: the
  Task Scheduler description (ts.desc) <= 400 characters and the tray tooltip text (tray.delegated,
  appended to the tray tooltip) <= 120, since longer values get cut off.
  -Listing checks the Store listing files instead: sections "# Description", "# Short description"
  (<= 270 chars), "# Features" (one per line, <= 20, each <= 200), "# Keywords" (<= 7, each <= 30 chars, <= 21 words in total)
  and "# What's new", each exactly once, none empty, no other "# " lines.
  -Dir / -ListingDir point at other folders (tests). Exits 1 and lists the problems, else 0.
#>
param(
    [string]$Tag = '',
    [switch]$Listing,
    [string]$Dir = '',
    [string]$ListingDir = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $Dir) { $Dir = Join-Path $root 'lang' }
if (-not $ListingDir) { $ListingDir = Join-Path $root 'store\listing' }
$strict = New-Object System.Text.UTF8Encoding($false, $true)
$problems = New-Object System.Collections.Generic.List[string]
function Problem([string]$file, [string]$msg) { $problems.Add("${file}: $msg") }

function Read-Utf8([string]$path, [string]$name) {
    $b = [IO.File]::ReadAllBytes($path)
    $skip = 0
    if ($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF) { $skip = 3 }
    try { return $strict.GetString($b, $skip, $b.Length - $skip) }
    catch { Problem $name 'not valid UTF-8'; return $null }
}

function Get-Lines([string]$text) { $text -split "`r?`n" }

# key = value table of a language file; duplicates and bad lines are reported
function Read-Lang([string]$path, [string]$name) {
    $text = Read-Utf8 $path $name
    if ($null -eq $text) { return $null }
    $t = [ordered]@{}
    $n = 0
    foreach ($l in (Get-Lines $text)) {
        $n++
        $s = $l.Trim()
        if (-not $s -or $s.StartsWith('#')) { continue }
        $i = $s.IndexOf('=')
        if ($i -lt 1) { Problem $name "line ${n}: not 'key = value'"; continue }
        $k = $s.Substring(0, $i).TrimEnd()
        $v = $s.Substring($i + 1).TrimStart()
        if ($t.Contains($k)) { Problem $name "line ${n}: duplicate key '$k'"; continue }
        $t[$k] = $v
    }
    return $t
}

function Get-Ph([string]$v) { (([regex]::Matches($v, '%(?:ls|lld|d)') | ForEach-Object { $_.Value }) -join ' ') }
function Get-Brand([string]$v) { (([regex]::Matches($v, 'PowerNapps?') | ForEach-Object { $_.Value } | Sort-Object) -join ' ') }

# ------------------------------------------------------------------ language files
function Check-Lang([string]$file, [System.Collections.IDictionary]$en) {
    $name = Split-Path -Leaf $file
    $tag = [IO.Path]::GetFileNameWithoutExtension($file)
    $t = Read-Lang $file $name
    if ($null -eq $t) { return }
    if (-not $t.Contains('@name') -or -not $t['@name']) { Problem $name '@name is missing or empty' }
    $wantRtl = $tag -in 'ar', 'he'
    $rtl = $t.Contains('@rtl') -and $t['@rtl'] -eq '1'
    if ($wantRtl -and -not $rtl) { Problem $name '@rtl = 1 is required for this right-to-left language' }
    if (-not $wantRtl -and $t.Contains('@rtl') -and $t['@rtl'] -ne '0') { Problem $name '@rtl = 1 is only for ar and he' }
    foreach ($k in $t.Keys) {
        if ($k.StartsWith('@')) {
            if ($k -notin '@name', '@rtl') { Problem $name "unknown header '$k'" }
        } elseif (-not $en.Contains($k)) { Problem $name "unknown key '$k'" }
    }
    foreach ($k in $en.Keys) {
        if ($k.StartsWith('@')) { continue }
        if (-not $t.Contains($k)) { Problem $name "missing key '$k'"; continue }
        $v = $t[$k]
        if (-not $v) { Problem $name "empty value for '$k'"; continue }
        if ((Get-Ph $v) -ne (Get-Ph $en[$k])) {
            Problem $name "placeholders of '$k' differ: expected [$(Get-Ph $en[$k])], found [$(Get-Ph $v)]"
        }
        if ((Get-Brand $v) -ne (Get-Brand $en[$k])) {
            Problem $name "'$k' must keep the name(s): $(Get-Brand $en[$k])"
        }
    }
    if ($t.Contains('ts.desc') -and $t['ts.desc'].Length -gt 400) { Problem $name "ts.desc is $($t['ts.desc'].Length) chars (max 400)" }
    if ($t.Contains('tray.delegated') -and $t['tray.delegated'].Length -gt 120) { Problem $name "tray.delegated is $($t['tray.delegated'].Length) chars (max 120)" }
}

# ------------------------------------------------------------------ store listing files
function Check-Listing([string]$file) {
    $name = 'listing\' + (Split-Path -Leaf $file)
    $text = Read-Utf8 $file $name
    if ($null -eq $text) { return }
    $sections = [ordered]@{}
    $cur = $null
    foreach ($l in (Get-Lines $text)) {
        if ($l -match '^#\s') {
            $h = ($l -replace '^#\s*', '').Trim()
            if ($h -notin 'Description', 'Short description', 'Features', 'Keywords', "What's new") { Problem $name "unknown section '# $h'"; $cur = $null; continue }
            if ($sections.Contains($h)) { Problem $name "section '# $h' appears twice"; $cur = $null; continue }
            $sections[$h] = New-Object System.Collections.Generic.List[string]
            $cur = $h
            continue
        }
        if ($null -ne $cur) { $sections[$cur].Add($l) }
    }
    foreach ($h in 'Description', 'Short description', 'Features', 'Keywords', "What's new") {
        if (-not $sections.Contains($h)) { Problem $name "section '# $h' is missing"; continue }
        $body = ($sections[$h] -join "`n").Trim()
        if (-not $body) { Problem $name "section '# $h' is empty" }
    }
    function Nonblank($h) { @($sections[$h] | Where-Object { $_.Trim() } | ForEach-Object { $_.Trim() }) }
    if ($sections.Contains('Short description')) {
        $s = (Nonblank 'Short description') -join ' '
        if ($s.Length -gt 270) { Problem $name "Short description is $($s.Length) chars (max 270)" }
    }
    if ($sections.Contains('Features')) {
        $f = Nonblank 'Features'
        if ($f.Count -gt 20) { Problem $name "$($f.Count) features (max 20)" }
        foreach ($x in $f) { if ($x.Length -gt 200) { Problem $name "feature is $($x.Length) chars (max 200): $($x.Substring(0, 30))..." } }
    }
    if ($sections.Contains('Keywords')) {
        $k = Nonblank 'Keywords'
        if ($k.Count -gt 7) { Problem $name "$($k.Count) keywords (max 7)" }
        # Partner Center: each search term <= 30 characters, <= 21 words across all of them
        foreach ($x in $k) { if ($x.Length -gt 30) { Problem $name "keyword is $($x.Length) chars (max 30): $x" } }
        $words = ($k | ForEach-Object { ($_ -split '\s+' | Where-Object { $_ }).Count } | Measure-Object -Sum).Sum
        if ($words -gt 21) { Problem $name "keywords have $words words in total (max 21)" }
    }
}

# ------------------------------------------------------------------ run
if ($Listing) {
    $files = @(Get-ChildItem $ListingDir -Filter *.txt | Sort-Object Name)
    if ($Tag) { $files = @($files | Where-Object { $_.BaseName -eq $Tag }) }
    if (-not $files) { Problem 'listing' 'no listing files found' }
    foreach ($f in $files) { Check-Listing $f.FullName }
} else {
    $enPath = Join-Path $Dir 'en.txt'
    if (-not (Test-Path $enPath)) { Write-Host "check-lang: $enPath not found"; exit 1 }
    $en = Read-Lang $enPath 'en.txt'
    $files = @(Get-ChildItem $Dir -Filter *.txt | Sort-Object Name)
    if ($Tag) { $files = @($files | Where-Object { $_.BaseName -eq $Tag }) }
    if (-not $files) { Problem 'lang' 'no language files found' }
    foreach ($f in $files) { Check-Lang $f.FullName $en }
}

if ($problems.Count) {
    $problems | ForEach-Object { Write-Host $_ }
    Write-Host "check-lang: $($problems.Count) problem(s)"
    exit 1
}
Write-Host "check-lang: ok ($($files.Count) file(s))"
exit 0
