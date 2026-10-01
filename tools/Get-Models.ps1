param([string]$Destination = "$PSScriptRoot/../.deps/models")
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
# Person segmentation for AI background removal: U²-Net trained on human segmentation
# (Apache-2.0, https://github.com/xuebinqin/U-2-Net), ONNX export published by rembg.
$url = 'https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2net_human_seg.onnx'
$sha256 = '01eb6a29a5c4d8edb30b56adad9bb3a2a0535338e480724a213e0acfd2d1c73c'
New-Item -ItemType Directory -Force $Destination | Out-Null
$model = Join-Path $Destination 'u2net_human_seg.onnx'
if (!(Test-Path $model) -or (Get-FileHash $model -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha256) {
    Invoke-WebRequest -Uri $url -OutFile $model
}
if ((Get-FileHash $model -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha256) { throw 'Model checksum mismatch' }
@{ models=@(@{ file='u2net_human_seg.onnx'; url=$url; sha256=$sha256; license='Apache-2.0'; source='https://github.com/xuebinqin/U-2-Net'; purpose='person matte for AI background removal' }) } | ConvertTo-Json -Depth 4 | Set-Content "$Destination/manifest.json" -Encoding utf8
Write-Output $Destination
