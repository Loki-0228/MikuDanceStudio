param([string]$OutputDirectory = "$PSScriptRoot\..\build\native-media-fixtures")
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
function Encode([string[]]$Arguments) {
    & ffmpeg -hide_banner -loglevel error -nostdin -y @Arguments
    if ($LASTEXITCODE -ne 0) { throw "FFmpeg failed: $LASTEXITCODE" }
}
foreach ($format in @('wav', 'mp3', 'aac', 'm4a', 'wma')) {
    Encode @('-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000:duration=1',
        '-ac', '2', (Join-Path $OutputDirectory "tone.$format"))
}
$pattern = 'color=blue:s=64x48:r=30:d=8,drawbox=x=0:y=0:w=iw:h=ih/2:color=red:t=fill,drawbox=x=0:y=0:w=iw:h=ih/2:color=0x00FF00:t=fill:enable=mod(floor(t)\,2)'
foreach ($profile in @('baseline', 'main', 'high')) {
    Encode @('-f', 'lavfi', '-i', $pattern, '-c:v', 'libx264', '-profile:v', $profile,
        '-pix_fmt', 'yuv420p', '-crf', '10', '-g', '120', '-sc_threshold', '0',
        (Join-Path $OutputDirectory "h264-$profile.mp4"))
}
Copy-Item -LiteralPath (Join-Path $OutputDirectory 'h264-high.mp4') -Destination (Join-Path $OutputDirectory 'corners.mp4')
Encode @('-f', 'lavfi', '-i', $pattern, '-c:v', 'libx264', '-profile:v', 'high10',
    '-pix_fmt', 'yuv420p10le', '-crf', '10', '-g', '120', '-sc_threshold', '0',
    (Join-Path $OutputDirectory 'h264-high10.mp4'))
Encode @('-i', (Join-Path $OutputDirectory 'corners.mp4'), '-c', 'copy', (Join-Path $OutputDirectory 'corners.mov'))
Encode @('-f', 'lavfi', '-i', 'testsrc2=s=3840x2160:r=30:d=6', '-c:v', 'libx264',
    '-preset', 'veryfast', '-crf', '24', '-pix_fmt', 'yuv420p', '-g', '180', '-sc_threshold', '0',
    (Join-Path $OutputDirectory 'h264-4k.mp4'))
Write-Output "Media fixtures: $OutputDirectory"
