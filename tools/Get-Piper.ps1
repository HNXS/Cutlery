param([string]$Destination = "$PSScriptRoot/../.deps/piper")
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
# Piper (MIT, https://github.com/rhasspy/piper) for offline text to speech: the pinned Windows
# release, piper.exe with piper-phonemize (MIT), eSpeak NG (GPL-3.0-or-later, phonemes from text,
# with its data folder) and ONNX Runtime 1.14 (MIT). It runs as a separate process from its own
# folder (tts/ in the AI pack), so its ONNX Runtime is not the one cutlery-ai uses.
$url = 'https://github.com/rhasspy/piper/releases/download/2023.11.14-2/piper_windows_amd64.zip'
$sha = 'f3c58906402b24f3a96d92145f58acba6d86c9b5db896d207f78dc80811efcea'
New-Item -ItemType Directory -Force $Destination | Out-Null
$zip = "$Destination/piper_windows_amd64.zip"
if (!(Test-Path $zip) -or (Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha) {
    Invoke-WebRequest -Uri $url -OutFile $zip
}
if ((Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha) { throw 'Piper checksum mismatch' }
$out = "$Destination/piper"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
Expand-Archive $zip -DestinationPath $Destination -Force
if (!(Test-Path "$out/piper.exe") -or !(Test-Path "$out/espeak-ng-data")) { throw 'Piper archive is incomplete' }
Remove-Item "$out/pkgconfig" -Recurse -Force -ErrorAction SilentlyContinue
Write-Output (Resolve-Path $out).Path
