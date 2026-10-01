param([string]$Destination = "$PSScriptRoot/../.deps/models")
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
New-Item -ItemType Directory -Force $Destination | Out-Null
function Get-Pinned($url, $file, $sha256) {
    if (!(Test-Path $file) -or (Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha256) {
        Invoke-WebRequest -Uri $url -OutFile $file
    }
    if ((Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha256) { throw "Checksum mismatch: $file" }
}
# Person segmentation for AI background removal: U²-Net trained on human segmentation
# (Apache-2.0, https://github.com/xuebinqin/U-2-Net), ONNX export published by rembg.
$matteUrl = 'https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2net_human_seg.onnx'
$matteSha = '01eb6a29a5c4d8edb30b56adad9bb3a2a0535338e480724a213e0acfd2d1c73c'
Get-Pinned $matteUrl "$Destination/u2net_human_seg.onnx" $matteSha
# Super-resolution for AI upscale: Real-ESRGAN realesr-general-x4v3 (BSD-3-Clause,
# https://github.com/xinntao/Real-ESRGAN). The published PyTorch weights are converted to ONNX
# by tools/convert-realesrgan.py, which needs only numpy and onnx.
$srUrl = 'https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesr-general-x4v3.pth'
$srSha = '8dc7edb9ac80ccdc30c3a5dca6616509367f05fbc184ad95b731f05bece96292'
$weights = Join-Path (Split-Path $Destination) 'realesr-general-x4v3.pth'
Get-Pinned $srUrl $weights $srSha
# Speech recognition for automatic captions: OpenAI Whisper large-v3-turbo (MIT) in
# whisper.cpp's 5-bit format. The SHA-1 is the one whisper.cpp publishes in models/README.md.
$speechUrl = 'https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q5_0.bin'
$speechSha1 = 'e050f7970618a659205450ad97eb95a18d69c9ee'
$speech = "$Destination/ggml-large-v3-turbo-q5_0.bin"
if (!(Test-Path $speech) -or (Get-FileHash $speech -Algorithm SHA1).Hash.ToLowerInvariant() -ne $speechSha1) {
    Invoke-WebRequest -Uri $speechUrl -OutFile $speech
}
if ((Get-FileHash $speech -Algorithm SHA1).Hash.ToLowerInvariant() -ne $speechSha1) { throw 'Whisper model checksum mismatch' }
$speechSha = (Get-FileHash $speech -Algorithm SHA256).Hash.ToLowerInvariant()
python -m pip install --quiet onnx==1.17.0
if ($LASTEXITCODE -ne 0) { throw 'Installing the onnx package failed' }
python "$PSScriptRoot/convert-realesrgan.py" $weights "$Destination/realesr-general-x4v3.onnx"
if ($LASTEXITCODE -ne 0) { throw 'Model conversion failed' }
$srOnnx = (Get-FileHash "$Destination/realesr-general-x4v3.onnx" -Algorithm SHA256).Hash.ToLowerInvariant()
@{ models=@(
    @{ file='u2net_human_seg.onnx'; url=$matteUrl; sha256=$matteSha; license='Apache-2.0'; source='https://github.com/xuebinqin/U-2-Net'; purpose='person matte for AI background removal' },
    @{ file='ggml-large-v3-turbo-q5_0.bin'; url=$speechUrl; sha1=$speechSha1; sha256=$speechSha; license='MIT'; source='https://github.com/openai/whisper'; purpose='speech recognition for automatic captions' },
    @{ file='realesr-general-x4v3.onnx'; converted_from=$srUrl; source_sha256=$srSha; sha256=$srOnnx; license='BSD-3-Clause'; source='https://github.com/xinntao/Real-ESRGAN'; purpose='4x super-resolution for AI upscale' }
) } | ConvertTo-Json -Depth 4 | Set-Content "$Destination/manifest.json" -Encoding utf8
Write-Output (Resolve-Path $Destination).Path
