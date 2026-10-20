<#
  Rips the CD audio tracks (the game's music) from a BIN/CUE image to .wav
  files that the game plays, and writes a starter music.txt.

  Usage: put this script in the folder with the .cue and .bin, then run
      powershell -ExecutionPolicy Bypass -File .\extract-music.ps1
  Options:
      -Cue "My Game.cue"     pick the cue sheet if there is more than one
      -OutDir "C:\path"      where to write the files

  By default the files go to the game's cd\music folder if a "cd" folder sits
  next to the .cue (e.g. C:\games\lotr\cd\music), otherwise to .\music.
  The original files are only read, never changed.
#>
param([string]$Cue, [string]$OutDir)

$ErrorActionPreference = 'Stop'
$raw = 2352

if (-not $Cue) {
    $cues = @(Get-ChildItem -LiteralPath $PSScriptRoot -Filter *.cue)
    if ($cues.Count -ne 1) { throw "Found $($cues.Count) .cue files here - pass one with -Cue." }
    $Cue = $cues[0].FullName
}
$Cue = (Resolve-Path -LiteralPath $Cue).Path
$cueDir = Split-Path -Parent $Cue

# --- read the cue sheet -------------------------------------------------
$binName = $null
$tracks = @()
$cur = $null
function MsfToFrames($m, $s, $f) { return ([int]$m * 60 + [int]$s) * 75 + [int]$f }
foreach ($line in (Get-Content -LiteralPath $Cue)) {
    $l = $line.Trim()
    if (-not $binName -and $l -match '^FILE\s+"(.+)"') { $binName = $Matches[1] }
    elseif ($l -match '^TRACK\s+(\d+)\s+(\S+)') {
        $cur = [pscustomobject]@{ Num = [int]$Matches[1]; Type = $Matches[2].ToUpper(); I0 = $null; I1 = $null }
        $tracks += $cur
    }
    elseif ($cur -and $l -match '^INDEX\s+(\d+)\s+(\d+):(\d+):(\d+)') {
        $fr = MsfToFrames $Matches[2] $Matches[3] $Matches[4]
        if ([int]$Matches[1] -eq 0) { $cur.I0 = $fr } elseif ([int]$Matches[1] -eq 1) { $cur.I1 = $fr }
    }
}
if (-not $binName) { throw "No FILE line in the cue." }
$bin = Join-Path $cueDir $binName
if (-not (Test-Path -LiteralPath $bin)) { throw "The cue points at '$binName', which isn't in $cueDir." }
$binSectors = [long][Math]::Floor((Get-Item -LiteralPath $bin).Length / $raw)

$audio = @($tracks | Where-Object { $_.Type -eq 'AUDIO' })
if ($audio.Count -eq 0) { throw "This disc image has no audio tracks." }

if (-not $OutDir) {
    if (Test-Path -LiteralPath (Join-Path $cueDir 'cd') -PathType Container) {
        $OutDir = Join-Path (Join-Path $cueDir 'cd') 'music'
    } else {
        $OutDir = Join-Path $cueDir 'music'
    }
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Write-Host "Writing $($audio.Count) tracks to $OutDir"

# --- rip each audio track ------------------------------------------------
$in = [IO.File]::OpenRead($bin)
$summary = @()
try {
    foreach ($t in $audio) {
        $idx = [array]::IndexOf($tracks, $t)
        $start = [long]$t.I1
        if ($idx + 1 -lt $tracks.Count) {
            $next = $tracks[$idx + 1]
            if ($null -ne $next.I0) { $end = [long]$next.I0 } else { $end = [long]$next.I1 }
        } else {
            $end = $binSectors
        }
        if ($end -gt $binSectors) { throw "Track $($t.Num) runs past the end of the .bin - it may be incomplete." }
        $bytes = ($end - $start) * $raw
        $name = 'track{0:D2}.wav' -f $t.Num
        $path = Join-Path $OutDir $name

        $out = [IO.File]::Create($path)
        $w = New-Object IO.BinaryWriter($out)
        try {
            # 44-byte WAV header: PCM, 2 channels, 44100 Hz, 16-bit
            $w.Write([Text.Encoding]::ASCII.GetBytes('RIFF')); $w.Write([uint32](36 + $bytes))
            $w.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt ')); $w.Write([uint32]16)
            $w.Write([uint16]1); $w.Write([uint16]2); $w.Write([uint32]44100)
            $w.Write([uint32](44100 * 4)); $w.Write([uint16]4); $w.Write([uint16]16)
            $w.Write([Text.Encoding]::ASCII.GetBytes('data')); $w.Write([uint32]$bytes)

            [void]$in.Seek($start * $raw, 'Begin')
            $buf = New-Object byte[] (1024 * 1024)
            $left = $bytes
            while ($left -gt 0) {
                $want = [int][Math]::Min($buf.Length, $left)
                $got = $in.Read($buf, 0, $want)
                if ($got -le 0) { throw "Unexpected end of .bin in track $($t.Num)." }
                $w.Write($buf, 0, $got)
                $left -= $got
            }
        } finally { $w.Close() }

        $secs = [Math]::Round(($end - $start) / 75.0)
        $summary += [pscustomobject]@{ Track = $t.Num; Seconds = $secs }
        Write-Host ('  track {0:D2}  {1}:{2:D2}' -f $t.Num, [int][Math]::Floor($secs / 60), [int]($secs % 60))
    }
} finally { $in.Close() }

# --- starter music.txt ---------------------------------------------------
$cfg = Join-Path $OutDir 'music.txt'
if (Test-Path -LiteralPath $cfg) {
    Write-Host "Kept your existing music.txt."
} else {
    $long = @($summary | Where-Object { $_.Seconds -ge 60 } | ForEach-Object { $_.Track })
    $short = @($summary | Where-Object { $_.Seconds -lt 60 } | ForEach-Object { $_.Track })
    $lines = @(
        '# Lord of the Rings - music settings',
        '#',
        '# Background music: CD track numbers, played in order and repeated.',
        "playlist $($long -join ' ')",
        '',
        '# Music volume, 0-100.',
        'volume 80',
        '',
        ('# Short tracks (likely stingers): ' + ($short -join ' ')),
        '# The game fires music cues that the original CD version played from',
        '# disc; which track went with which cue is not documented. Every cue',
        '# is written to lotr.log as you play. To give a cue a track, add e.g.',
        '#   cue 3 7        play track 7 once when cue 3 fires',
        '#   combat 8       when a fight starts',
        '#   victory 9      when a fight is won',
        '# then the background playlist carries on.',
        '',
        '# Track lengths:'
    )
    foreach ($s in $summary) {
        $lines += ('#   track {0:D2}  {1}:{2:D2}' -f $s.Track, [int][Math]::Floor($s.Seconds / 60), [int]($s.Seconds % 60))
    }
    Set-Content -LiteralPath $cfg -Value $lines -Encoding ASCII
    Write-Host "Wrote music.txt (playlist: $($long -join ' '))"
}
Write-Host "Done."
