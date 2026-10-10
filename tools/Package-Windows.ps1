param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [string]$BuildDir = "$PSScriptRoot/../build",
    [Parameter(Mandatory=$true)][string]$FFmpegBin,
    [string]$OutputDir = "$PSScriptRoot/../dist/Cutlery-0.5.0-win64-portable",
    # Optional AI worker runtime (ONNX Runtime release folder) and model folder for the AI pack.
    [string]$OnnxRuntime = '',
    [string]$Models = '',
    # Optional whisper.cpp build folder from Get-Whisper.ps1.
    [string]$Whisper = '',
    # Optional Piper folder from Get-Piper.ps1 (text to speech, AI pack).
    [string]$Piper = '',
    # Optional recorded sound effects from Get-Sounds.ps1.
    [string]$Sounds = '',
    [string]$AiPackDir = "$PSScriptRoot/../dist/Cutlery-0.5.0-AI-pack"
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
Copy-Item "$root/docs/THIRD_PARTY.md","$root/docs/PORTABLE.md","$root/docs/SHORTCUTS.md","$root/docs/TIMELINE.md","$root/docs/ROADMAP.md","$root/docs/AI.md","$root/docs/RELEASE_NOTES.md" "$OutputDir/docs"
Copy-Item "$root/README.md" $OutputDir
if ($Sounds) {
    New-Item -ItemType Directory -Force "$OutputDir/sounds" | Out-Null
    Copy-Item "$Sounds/*" "$OutputDir/sounds"
}
if ($OnnxRuntime) {
    # The worker runs in its own process; onnxruntime.dll and DirectML.dll sit next to it so the
    # older copies Windows ships in System32 are never picked up.
    Copy-Item "$BuildDir/bin/Release/cutlery-ai.exe" $OutputDir
    Copy-Item "$OnnxRuntime/lib/*.dll" $OutputDir
    New-Item -ItemType Directory -Force "$OutputDir/licenses/onnxruntime" | Out-Null
    Copy-Item "$OnnxRuntime/licenses/*" "$OutputDir/licenses/onnxruntime"
}
if ($Whisper) {
    # whisper-cli loads its ggml backends (Vulkan GPU, CPU variants) from its own folder.
    Copy-Item "$Whisper/whisper-cli.exe","$Whisper/*.dll" $OutputDir
    Copy-Item "$Whisper/whisper.cpp-LICENSE.txt" "$OutputDir/licenses"
}
Copy-Item "$root/licenses/*" "$OutputDir/licenses" -Recurse -Force
if (Test-Path "$QtRoot/sbom") { Copy-Item "$QtRoot/sbom" "$OutputDir/licenses/Qt-sbom" -Recurse -Force }
'{"format":"cutlery-portable","version":1}' | Set-Content "$OutputDir/portable.json" -Encoding utf8
'[Paths]
Prefix=.
Plugins=.
QmlImports=qml' -replace '\\n',"`n" | Set-Content "$OutputDir/qt.conf" -Encoding utf8
$commit = git -C $root rev-parse HEAD
@{ application='Cutlery'; version='0.5.0'; commit=$commit; qt='6.8.3'; target='Windows x64'; mode='portable'; built_utc=(Get-Date).ToUniversalTime().ToString('o') } | ConvertTo-Json | Set-Content "$OutputDir/build-manifest.json" -Encoding utf8
# The listing is complete before SHA256SUMS.txt is written; an older one (from an earlier
# packaging into the same folder) is not listed.
@(Get-ChildItem $OutputDir -File -Recurse | Where-Object { $_.Name -ne 'SHA256SUMS.txt' }) | ForEach-Object { "{0}  {1}" -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(),$_.FullName.Substring($OutputDir.Length+1) } | Set-Content "$OutputDir/SHA256SUMS.txt" -Encoding utf8
if ($Models) {
    # Separate download: models are large and optional. Unpack next to Cutlery.exe.
    New-Item -ItemType Directory -Force "$AiPackDir/models","$AiPackDir/licenses" | Out-Null
    Copy-Item "$Models/u2net_human_seg.onnx","$Models/realesr-general-x4v3.onnx","$Models/ggml-large-v3-turbo-q5_0.bin" "$AiPackDir/models"
    Copy-Item "$Models/face_detection_short_range.onnx","$Models/face_landmark.onnx","$Models/iris_landmark.onnx" "$AiPackDir/models"
    Copy-Item "$Models/UVR-MDX-NET-Inst_HQ_3.onnx" "$AiPackDir/models"
    Copy-Item "$root/licenses/UVR-MDX-Net-MIT.txt" "$AiPackDir/licenses"
    Copy-Item "$root/licenses/Apache-2.0.txt" "$AiPackDir/licenses/MediaPipe-Apache-2.0.txt"
    if ($Whisper) { Copy-Item "$Whisper/ggml-silero-v6.2.0.bin" "$AiPackDir/models" }
    # Text to speech: Piper in tts/ (its own ONNX Runtime and eSpeak NG data), voices in models/.
    Copy-Item "$Models/de_DE-thorsten-medium.onnx","$Models/de_DE-thorsten-medium.onnx.json","$Models/de_DE-thorsten-medium.MODEL_CARD.txt" "$AiPackDir/models"
    if ($Piper) {
        New-Item -ItemType Directory -Force "$AiPackDir/tts" | Out-Null
        Copy-Item "$Piper/*" "$AiPackDir/tts" -Recurse -Force
        Copy-Item "$root/licenses/Piper-MIT.txt" "$AiPackDir/licenses"
        Copy-Item "$root/licenses/GPL-3.0-only.txt" "$AiPackDir/licenses/eSpeak-NG-GPL-3.0-or-later.txt"
    }
    Copy-Item "$Models/manifest.json" "$AiPackDir/models/manifest.json"
    Copy-Item "$root/licenses/Apache-2.0.txt" "$AiPackDir/licenses/U-2-Net-Apache-2.0.txt"
    Copy-Item "$root/licenses/Real-ESRGAN-BSD-3-Clause.txt","$root/licenses/Whisper-MIT.txt","$root/licenses/Silero-VAD-MIT.txt" "$AiPackDir/licenses"
    Copy-Item "$root/docs/AI.md" "$AiPackDir/README-AI.md"
    @(Get-ChildItem $AiPackDir -File -Recurse | Where-Object { $_.Name -ne 'SHA256SUMS.txt' }) | ForEach-Object { "{0}  {1}" -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(),$_.FullName.Substring((Resolve-Path $AiPackDir).Path.Length+1) } | Set-Content "$AiPackDir/SHA256SUMS.txt" -Encoding utf8
}
Write-Output $OutputDir
