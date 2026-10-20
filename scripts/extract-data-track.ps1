<#
  Extracts the data track (track 1) of a BIN/CUE CD image to a standard .iso,
  which 7-Zip and Windows can open normally.

  Usage: put this script in the folder with the .cue and .bin, then run
      powershell -ExecutionPolicy Bypass -File .\extract-data-track.ps1
  or name the cue explicitly:
      powershell -ExecutionPolicy Bypass -File .\extract-data-track.ps1 -Cue "My Game.cue"

  It only reads the .bin; the original files are not changed.
#>
param([string]$Cue)

$ErrorActionPreference = 'Stop'

if (-not $Cue) {
    $cues = @(Get-ChildItem -LiteralPath $PSScriptRoot -Filter *.cue)
    if ($cues.Count -ne 1) { throw "Found $($cues.Count) .cue files here - pass one with -Cue." }
    $Cue = $cues[0].FullName
}
$Cue = (Resolve-Path -LiteralPath $Cue).Path
$cueDir = Split-Path -Parent $Cue
$lines = Get-Content -LiteralPath $Cue

# --- read the cue sheet -------------------------------------------------
$binName = $null; $mode = $null; $track2Start = $null; $track = 0
foreach ($line in $lines) {
    $l = $line.Trim()
    if (-not $binName -and $l -match '^FILE\s+"(.+)"') { $binName = $Matches[1] }
    elseif ($l -match '^TRACK\s+(\d+)\s+(\S+)') {
        $track = [int]$Matches[1]
        if ($track -eq 1) { $mode = $Matches[2].ToUpper() }
    }
    elseif ($track -eq 2 -and $null -eq $track2Start -and $l -match '^INDEX\s+0?1\s+(\d+):(\d+):(\d+)') {
        $track2Start = ([int]$Matches[1] * 60 + [int]$Matches[2]) * 75 + [int]$Matches[3]
    }
}
if (-not $binName) { throw "No FILE line in the cue." }
$bin = Join-Path $cueDir $binName
if (-not (Test-Path -LiteralPath $bin)) { throw "The cue points at '$binName', which isn't in $cueDir." }

switch ($mode) {
    'MODE1/2352' { $offset = 16 }
    'MODE2/2352' { $offset = 24 }
    'MODE1/2048' { throw "Track 1 is already plain 2048-byte data - just rename the .bin to .iso." }
    default      { throw "Track 1 mode '$mode' isn't supported by this script." }
}
$raw = 2352
$binLen = (Get-Item -LiteralPath $bin).Length
if ($null -ne $track2Start) { $sectors = $track2Start } else { $sectors = [int][Math]::Floor($binLen / $raw) }
if ([long]$sectors * $raw -gt $binLen) {
    throw "The .bin is shorter than the cue says ($binLen bytes, expected at least $([long]$sectors * $raw)). It may be incomplete."
}

$iso = Join-Path $cueDir ([IO.Path]::GetFileNameWithoutExtension($bin) + ' (data).iso')
Write-Host "Track 1: $mode, $sectors sectors -> $([Math]::Round($sectors * 2048 / 1MB)) MB"
Write-Host "Writing $iso"

# --- copy the 2048 data bytes out of each raw sector --------------------
$in  = [IO.File]::OpenRead($bin)
$out = [IO.File]::Create($iso)
try {
    $batch = 1024
    $buf = New-Object byte[] ($raw * $batch)
    $done = 0
    while ($done -lt $sectors) {
        $n = [Math]::Min($batch, $sectors - $done)
        $want = $n * $raw; $got = 0
        while ($got -lt $want) {
            $r = $in.Read($buf, $got, $want - $got)
            if ($r -le 0) { throw "Unexpected end of .bin at sector $done." }
            $got += $r
        }
        if ($done -eq 0) {
            # every raw data sector starts with the sync pattern 00 FF*10 00
            $sync = ($buf[0] -eq 0) -and ($buf[11] -eq 0)
            for ($i = 1; $i -le 10; $i++) { if ($buf[$i] -ne 255) { $sync = $false } }
            if (-not $sync) { throw "Track 1 doesn't look like raw CD data (no sync header)." }
        }
        for ($s = 0; $s -lt $n; $s++) { $out.Write($buf, $s * $raw + $offset, 2048) }
        $done += $n
        Write-Progress -Activity 'Extracting data track' -PercentComplete ([int](100 * $done / $sectors))
    }
}
finally { $in.Close(); $out.Close() }
Write-Progress -Activity 'Extracting data track' -Completed

# --- check the result is a real ISO 9660 image ---------------------------
$f = [IO.File]::OpenRead($iso)
try {
    $pvd = New-Object byte[] 2048
    [void]$f.Seek(16 * 2048, 'Begin'); [void]$f.Read($pvd, 0, 2048)
} finally { $f.Close() }
if ([Text.Encoding]::ASCII.GetString($pvd, 1, 5) -ne 'CD001') {
    throw "Wrote the file, but it isn't a valid ISO 9660 image."
}
$label = [Text.Encoding]::ASCII.GetString($pvd, 40, 32).Trim()
Write-Host "Done. Valid ISO, volume label '$label'. Open it with 7-Zip or double-click to mount."
