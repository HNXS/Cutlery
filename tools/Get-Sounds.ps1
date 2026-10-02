# Recorded sound effects for the portable build (see sound-pack.json): downloads each pinned
# file, checks its SHA-256 and writes sounds.json, which Cutlery reads from the sounds folder
# next to Cutlery.exe. Returns the folder.
param([string]$Destination = "$PSScriptRoot/../.deps/sounds")
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
New-Item -ItemType Directory -Force $Destination | Out-Null
$pack = Get-Content "$PSScriptRoot/sound-pack.json" -Raw | ConvertFrom-Json
$entries = @()
foreach ($s in $pack.sounds) {
    $from = $pack.sources.($s.from)
    $name = "$($s.id)$([IO.Path]::GetExtension($s.file))"
    $file = Join-Path $Destination $name
    if (!(Test-Path $file) -or (Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $s.sha256) {
        Invoke-WebRequest -Uri ($from.base + $s.file) -OutFile $file
    }
    if ((Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $s.sha256) { throw "Checksum mismatch: $($s.file)" }
    $entries += [ordered]@{
        id = $s.id; file = $name; name = $s.name; category = $s.category
        seconds = $s.seconds; peak = $s.peak; licence = $from.licence; source = $from.source
    }
}
ConvertTo-Json -InputObject $entries -Depth 3 | Set-Content "$Destination/sounds.json" -Encoding utf8
(Resolve-Path $Destination).Path
