param([string]$Destination = "$PSScriptRoot/../.deps/onnxruntime")
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
# ONNX Runtime with the DirectML provider (GPU inference on any DirectX 12 GPU, CPU fallback)
# and the DirectML redistributable it requires. Immutable NuGet packages, pinned by hash.
$packages = @(
    @{ name='Microsoft.ML.OnnxRuntime.DirectML'; version='1.22.0'; sha256='29f9872d786236b79aa83f94482f3a17c14297e4833768d6d0ed4883ee732e60' },
    @{ name='Microsoft.AI.DirectML'; version='1.15.4'; sha256='4e7cb7ddce8cf837a7a75dc029209b520ca0101470fcdf275c1f49736a3615b9' }
)
New-Item -ItemType Directory -Force $Destination | Out-Null
$mismatch = @()
foreach ($p in $packages) {
    $archive = Join-Path $Destination "$($p.name).$($p.version).zip"
    Invoke-WebRequest -Uri "https://www.nuget.org/api/v2/package/$($p.name)/$($p.version)" -OutFile $archive
    $hash = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hash -ne $p.sha256) { $mismatch += "$($p.name) $($p.version): got $hash" }
}
if ($mismatch) { throw "Checksum mismatch: $($mismatch -join '; ')" }
foreach ($p in $packages) {
    Expand-Archive -Path (Join-Path $Destination "$($p.name).$($p.version).zip") -DestinationPath "$Destination/$($p.name)" -Force
}
# Lay the packages out like an ONNX Runtime release: include/, lib/, licence files.
$ort = "$Destination/Microsoft.ML.OnnxRuntime.DirectML"
$dml = "$Destination/Microsoft.AI.DirectML"
$root = "$Destination/runtime"
New-Item -ItemType Directory -Force "$root/include","$root/lib","$root/licenses" | Out-Null
Copy-Item "$ort/build/native/include/*" "$root/include" -Recurse -Force
Copy-Item "$ort/runtimes/win-x64/native/onnxruntime.dll","$ort/runtimes/win-x64/native/onnxruntime.lib" "$root/lib"
Copy-Item "$dml/bin/x64-win/DirectML.dll" "$root/lib"
Copy-Item "$ort/LICENSE" "$root/licenses/onnxruntime-LICENSE.txt"
Copy-Item "$ort/ThirdPartyNotices.txt" "$root/licenses/onnxruntime-ThirdPartyNotices.txt"
Get-ChildItem $dml -File | Where-Object { $_.Name -match 'LICENSE|ThirdParty' } | ForEach-Object { Copy-Item $_.FullName "$root/licenses/DirectML-$($_.Name)" }
foreach ($file in @('include/onnxruntime_cxx_api.h','include/dml_provider_factory.h','lib/onnxruntime.dll','lib/onnxruntime.lib','lib/DirectML.dll')) {
    if (!(Test-Path "$root/$file")) { throw "ONNX Runtime file missing: $file" }
}
if (!(Get-ChildItem "$root/licenses" -Filter 'DirectML-*')) { throw 'DirectML licence missing' }
@{ packages=$packages; license='MIT (ONNX Runtime); Microsoft DirectML redistributable licence'; source='https://github.com/microsoft/onnxruntime/tree/v1.22.0' } | ConvertTo-Json -Depth 3 | Set-Content "$root/manifest.json" -Encoding utf8
Write-Output (Resolve-Path $root).Path
