param([string]$Destination = "$PSScriptRoot/../.deps/ffmpeg")
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
# Immutable upstream release + hash. Never silently track "latest".
$url = 'https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-09-28-13-06/ffmpeg-n8.1.3-6-gff48edd8b2-win64-lgpl-shared-8.1.zip'
$sha256 = '9f67aa7e0665f2be183a2c760cd199fb5bf7ad0b72e1a1c97265faf13ce4d3eb'
New-Item -ItemType Directory -Force $Destination | Out-Null
$archive = Join-Path $Destination 'upstream.zip'
Invoke-WebRequest -Uri $url -OutFile $archive
if ((Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha256) { throw 'FFmpeg archive checksum mismatch' }
Expand-Archive -Path $archive -DestinationPath "$Destination/unpacked" -Force
$exe = Get-ChildItem "$Destination/unpacked" -Recurse -Filter ffmpeg.exe | Select-Object -First 1
if (!$exe) { throw 'FFmpeg executable missing' }
$bin = $exe.Directory.FullName
$version = (& $exe.FullName -version 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { throw 'FFmpeg failed to start' }
if ($version -match '--enable-gpl|--enable-nonfree') { throw 'Expected an LGPL FFmpeg build' }
$filters = (& $exe.FullName -hide_banner -filters 2>&1 | Out-String)
foreach ($filter in @('overlay','lutrgb','hue','rotate','fade','atempo','amix','alimiter','colorchannelmixer','realtime','arealtime','xfade','tpad','sendcmd','afade','adelay','apad','settb','scale','trim','atrim','fps','reverse','areverse','crop','hflip','lut','format')) {
    if ($filters -notmatch "\s$filter\s") { throw "Required FFmpeg filter missing: $filter" }
}
@{ url=$url; sha256=$sha256; version=$version; source='https://github.com/FFmpeg/FFmpeg/commit/ff48edd8b2'; build_recipe='https://github.com/BtbN/FFmpeg-Builds/tree/autobuild-2026-09-28-13-06' } | ConvertTo-Json | Set-Content "$Destination/manifest.json" -Encoding utf8
Write-Output $bin
