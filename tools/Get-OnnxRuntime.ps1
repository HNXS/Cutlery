param([string]$Destination = "$PSScriptRoot/../.deps/onnxruntime")
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
# Immutable upstream release + hash (MIT licence). CPU build; used only by cutlery-matte.
$url = 'https://github.com/microsoft/onnxruntime/releases/download/v1.22.0/onnxruntime-win-x64-1.22.0.zip'
$sha256 = '174c616efc0271194488642a72f1a514e01487da4dfe84c49296d66e40ebe0da'
New-Item -ItemType Directory -Force $Destination | Out-Null
$archive = Join-Path $Destination 'upstream.zip'
Invoke-WebRequest -Uri $url -OutFile $archive
if ((Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha256) { throw 'ONNX Runtime archive checksum mismatch' }
Expand-Archive -Path $archive -DestinationPath "$Destination/unpacked" -Force
$root = (Get-ChildItem "$Destination/unpacked" -Directory | Select-Object -First 1).FullName
foreach ($file in @('include/onnxruntime_cxx_api.h','lib/onnxruntime.dll','lib/onnxruntime.lib','LICENSE','ThirdPartyNotices.txt')) {
    if (!(Test-Path "$root/$file")) { throw "ONNX Runtime file missing: $file" }
}
@{ url=$url; sha256=$sha256; version='1.22.0'; license='MIT'; source='https://github.com/microsoft/onnxruntime/tree/v1.22.0' } | ConvertTo-Json | Set-Content "$Destination/manifest.json" -Encoding utf8
Write-Output $root
