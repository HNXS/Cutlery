param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [string]$BuildDir = "$PSScriptRoot/../build",
    [Parameter(Mandatory=$true)][string]$FFmpegBin,
    [string]$OutputDir = "$PSScriptRoot/../dist/Cutlery-0.4.0-win64-portable",
    # Optional AI worker runtime (ONNX Runtime release folder) and model folder for the AI pack.
    [string]$OnnxRuntime = '',
    [string]$Models = '',
    [string]$AiPackDir = "$PSScriptRoot/../dist/Cutlery-0.4.0-AI-pack"
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot/..").Path
New-Item -ItemType Directory -Force $OutputDir | Out-Null
$OutputDir = (Resolve-Path $OutputDir).Path
Copy-Item "$BuildDir/bin/Release/Cutlery.exe" $OutputDir
& "$QtRoot/bin/windeployqt.exe" --release --qmldir "$root/qml" --compiler-runtime --dir $OutputDir "$OutputDir/Cutlery.exe"
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed' }
New-Item -ItemType Directory -Force "$OutputDir/codecs","$OutputDir/licenses","$OutputDir/licenses/FFmpeg-upstream","$OutputDir/docs" | Out-Null
Copy-Item "$FFmpegBin/*" "$OutputDir/codecs" -Recurse -Force
$ffRoot = Split-Path $FFmpegBin
$ffManifest = Join-Path (Split-Path (Split-Path $ffRoot)) 'manifest.json'
Copy-Item $ffManifest "$OutputDir/codecs/manifest.json"
Get-ChildItem $ffRoot | Where-Object { $_.Name -ne 'bin' } | Copy-Item -Destination "$OutputDir/licenses/FFmpeg-upstream" -Recurse -Force
Copy-Item "$root/docs/THIRD_PARTY.md","$root/docs/PORTABLE.md","$root/docs/SHORTCUTS.md","$root/docs/TIMELINE.md","$root/docs/ROADMAP.md","$root/docs/AI.md" "$OutputDir/docs"
Copy-Item "$root/README.md" $OutputDir
if ($OnnxRuntime) {
    # The worker runs in its own process; onnxruntime.dll sits next to it so the copy Windows
    # ships in System32 is never picked up.
    Copy-Item "$BuildDir/bin/Release/cutlery-matte.exe","$OnnxRuntime/lib/onnxruntime.dll" $OutputDir
    New-Item -ItemType Directory -Force "$OutputDir/licenses/onnxruntime" | Out-Null
    Copy-Item "$OnnxRuntime/LICENSE","$OnnxRuntime/ThirdPartyNotices.txt" "$OutputDir/licenses/onnxruntime"
}
Copy-Item "$root/licenses/*" "$OutputDir/licenses" -Recurse -Force
if (Test-Path "$QtRoot/sbom") { Copy-Item "$QtRoot/sbom" "$OutputDir/licenses/Qt-sbom" -Recurse -Force }
'{"format":"cutlery-portable","version":1}' | Set-Content "$OutputDir/portable.json" -Encoding utf8
'[Paths]
Prefix=.
Plugins=.
QmlImports=qml' -replace '\\n',"`n" | Set-Content "$OutputDir/qt.conf" -Encoding utf8
$commit = git -C $root rev-parse HEAD
@{ application='Cutlery'; version='0.4.0'; commit=$commit; qt='6.8.3'; target='Windows x64'; mode='portable'; built_utc=(Get-Date).ToUniversalTime().ToString('o') } | ConvertTo-Json | Set-Content "$OutputDir/build-manifest.json" -Encoding utf8
Get-ChildItem $OutputDir -File -Recurse | ForEach-Object { "{0}  {1}" -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(),$_.FullName.Substring($OutputDir.Length+1) } | Set-Content "$OutputDir/SHA256SUMS.txt" -Encoding utf8
if ($Models) {
    # Separate download: models are large and optional. Unpack next to Cutlery.exe.
    New-Item -ItemType Directory -Force "$AiPackDir/models","$AiPackDir/licenses" | Out-Null
    Copy-Item "$Models/u2net_human_seg.onnx" "$AiPackDir/models"
    Copy-Item "$Models/manifest.json" "$AiPackDir/models/manifest.json"
    Copy-Item "$root/licenses/Apache-2.0.txt" "$AiPackDir/licenses/U-2-Net-Apache-2.0.txt"
    Copy-Item "$root/docs/AI.md" "$AiPackDir/README-AI.md"
    Get-ChildItem $AiPackDir -File -Recurse | ForEach-Object { "{0}  {1}" -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(),$_.FullName.Substring((Resolve-Path $AiPackDir).Path.Length+1) } | Set-Content "$AiPackDir/SHA256SUMS.txt" -Encoding utf8
}
Write-Output $OutputDir
