# Compile + link + audit LorePilotReader with the CodeWarrior command line
# tools, then copy the PRC to the repo-less output directory.
#
#   powershell -File reader/build.ps1
#   powershell -File reader/build.ps1 -OutDir C:\out
#
# The CodeWarrior .mcp project is not touched: this script passes an explicit
# file list, so new sources only need adding to $sources below.

param(
    [string]$OutDir = "$env:TEMP\lpbuild"
)

$ErrorActionPreference = 'Stop'

$cw = 'C:\Program Files (x86)\Metrowerks\CodeWarrior'
if (-not (Test-Path $cw)) { throw "CodeWarrior not found at $cw" }

$root = Split-Path -Parent $PSScriptRoot
$srcDir = Join-Path $root 'reader\Src'
$rscDir = Join-Path $root 'reader\Rsc'

$mwc = Join-Path $cw 'CW for Palm OS Tools\Command_Line_Tools\MWC68K.exe'
$link = Join-Path $cw 'CW for Palm OS Tools\Command_Line_Tools\MWLink68K.exe'
$pilrc = Join-Path $cw 'CW for Palm OS Tools\PilRC\pilrc.exe'
$runtime = Join-Path $cw 'Palm OS Support\CodeWarrior Libraries\Runtime\PalmOSRuntime_2i_A5.lib'

$env:LM_LICENSE_FILE = Join-Path $cw 'license.dat'

New-Item -ItemType Directory -Force $OutDir | Out-Null

# The SDK headers are spread over subdirectories and MWC68K's '-I' only works
# with '-I-' plus one path per directory, so mirror them into the temp dir.
$inc = Join-Path $env:TEMP 'palm_inc'
if (-not (Test-Path (Join-Path $inc 'PalmOS.h'))) {
    Copy-Item (Join-Path $cw 'Palm OS Support\Incs') $inc -Recurse -Force
}

$includes = @('-I-')
Get-ChildItem $inc -Recurse -Directory | ForEach-Object { $includes += "-I$($_.FullName)" }
$includes += @("-I$inc", "-I$srcDir", "-I$rscDir")

# 1. resources ------------------------------------------------------------
$rscOut = Join-Path $OutDir 'rsc'
New-Item -ItemType Directory -Force $rscOut | Out-Null
Push-Location $rscDir
& $pilrc -ro -o $rscOut 'LorePilotReader_Rsc.rcp' | Out-Null
if ($LASTEXITCODE -ne 0) { Pop-Location; throw 'pilrc failed' }
Pop-Location

# PilRC warns about a missing Graffiti State Indicator on forms with editable
# fields. That indicator is a Palm OS 3.0 form object, so it is correct to omit
# it for a 2.0 target; the warning is expected.

# 2. compile --------------------------------------------------------------
$sources = @(
    'UiFont', 'UiGlyphs', 'UiButton', 'Shelf', 'GotoForm', 'TextDb',
    'BookList', 'ReaderForm', 'FontList', 'GlyphFont', 'GbTest', 'GlyphTest',
    'LorePilotReader'
)

$objDir = Join-Path $OutDir 'obj'
New-Item -ItemType Directory -Force $objDir | Out-Null
Push-Location $objDir
$failed = @()
foreach ($name in $sources) {
    $log = & $mwc -cwd source @includes -DERROR_CHECK_LEVEL=0 `
        -o "$name.o" (Join-Path $srcDir "$name.c") 2>&1
    if ($log -match 'Error:') {
        $failed += $name
        Write-Host "--- $name ---"
        $log | Select-Object -Last 25 | Write-Host
    }
}
Pop-Location
if ($failed.Count -gt 0) { throw "compile failed: $($failed -join ', ')" }

# 3. link -----------------------------------------------------------------
$objects = Get-ChildItem $objDir -Filter *.o | ForEach-Object { $_.FullName }
$prc = Join-Path $OutDir 'LorePilotReader.prc'
$linkArgs = @(
    '-o', $prc,
    '-app_standard', '-entry_point', 'PilotMain',
    '-prctype', 'appl', '-prc_creator', 'LPRD',
    '-name_res', 'LorePilotReader', '-version_res', '1.0',
    '-DB_name', 'LorePilotReader',
    (Join-Path $OutDir 'rsc.ro')
) + $objects + $runtime
& $link @linkArgs 2>&1 | Select-Object -First 15 | Write-Host
if ($LASTEXITCODE -ne 0) { throw 'link failed' }

Write-Host ''
Write-Host "built $prc"
Write-Host 'audit with: node font-packer/tools/dump-prc.mjs <prc>'

# PilRC writes the generated header with CRLF; the repo standard is LF.
$hdr = Join-Path $rscDir 'LorePilotReader_Rsc.h'
if (Test-Path $hdr) {
    $text = [IO.File]::ReadAllText($hdr) -replace "`r`n", "`n"
    [IO.File]::WriteAllText($hdr, $text, (New-Object System.Text.UTF8Encoding($false)))
}
