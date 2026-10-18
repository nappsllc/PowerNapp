<#
.SYNOPSIS
  Generates lang\en.txt (the template translators copy) from the built-in English in src\strings.h.
.DESCRIPTION
  Reads every X(ID, "key", L"English") entry of the STRINGS table, decodes the C escapes (\xHHHH, \n,
  \", \\) and writes "key = value" lines (UTF-8, no BOM, LF), escaping only \n and \\ as the loader
  expects. -Check writes nothing and exits 1 if lang\en.txt on disk differs (used by CI).
#>
param([switch]$Check)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$src = Join-Path $root 'src\strings.h'
$out = Join-Path $root 'lang\en.txt'

function Decode-C([string]$s) {
    $sb = [System.Text.StringBuilder]::new()
    for ($i = 0; $i -lt $s.Length; $i++) {
        $c = $s[$i]
        if ($c -ne '\') { [void]$sb.Append($c); continue }
        $i++
        $e = $s[$i]
        switch -CaseSensitive ($e) {
            'n' { [void]$sb.Append("`n") }
            't' { [void]$sb.Append("`t") }
            'x' {
                $j = $i + 1
                while ($j -lt $s.Length -and $s[$j] -match '[0-9A-Fa-f]') { $j++ }  # C: all hex digits
                if ($j -eq $i + 1) { throw "bad \x escape in: $s" }
                [void]$sb.Append([char][Convert]::ToInt32($s.Substring($i + 1, $j - $i - 1), 16))
                $i = $j - 1
            }
            default {
                if ($e -in '"', '\', "'") { [void]$sb.Append($e) } else { throw "unsupported escape \$e in: $s" }
            }
        }
    }
    $sb.ToString()
}

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('@name = English')
$lines.Add('# PowerNapp English template, generated from src\strings.h by tools\gen-lang-template.ps1: do not edit.')
$lines.Add('# To translate: copy to lang\<tag>.txt (e.g. de.txt), set @name to the language''s own name, add')
$lines.Add('# "@rtl = 1" for right-to-left scripts, and translate the values. Keep %ls / %d / %lld in the same')
$lines.Add('# order (a value whose placeholders differ is ignored). \n is a line break, \\ a backslash.')
$lines.Add('# A missing key falls back to English.')
$lines.Add('')
$rx = '^\s*X\(\s*(\w+)\s*,\s*"([^"]+)"\s*,\s*L"((?:[^"\\]|\\.)*)"\s*\)'
$seen = @{}
foreach ($l in Get-Content -LiteralPath $src -Encoding UTF8) {
    $m = [regex]::Match($l, $rx)
    if (-not $m.Success) { continue }
    $key = $m.Groups[2].Value
    if ($seen.ContainsKey($key)) { throw "duplicate key $key" }
    $seen[$key] = 1
    $v = (Decode-C $m.Groups[3].Value).Replace('\', '\\').Replace("`n", '\n')
    $lines.Add("$key = $v")
}
if ($seen.Count -eq 0) { throw "no STRINGS entries found in $src" }
$text = ($lines -join "`n") + "`n"

if ($Check) {
    $disk = if (Test-Path -LiteralPath $out) { [System.IO.File]::ReadAllText($out) } else { '' }
    if ($disk.Replace("`r`n", "`n") -ne $text) {
        Write-Host "lang\en.txt is out of date: run tools\gen-lang-template.ps1"
        exit 1
    }
    Write-Host "lang\en.txt is up to date ($($seen.Count) keys)"
    exit 0
}
New-Item -ItemType Directory -Force -Path (Split-Path $out) | Out-Null
[System.IO.File]::WriteAllText($out, $text, [System.Text.UTF8Encoding]::new($false))
Write-Host "wrote lang\en.txt ($($seen.Count) keys)"
