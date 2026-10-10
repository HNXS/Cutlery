param([string]$Destination = "$PSScriptRoot/../.deps/models")
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
New-Item -ItemType Directory -Force $Destination | Out-Null
function Get-Pinned($url, $file, $sha256) {
    if (!(Test-Path $file) -or (Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha256) {
        Invoke-WebRequest -Uri $url -OutFile $file
    }
    $actual = (Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant()
    if (!$sha256) { Write-Host "::warning::UNPINNED $file sha256 $actual"; return }
    if ($actual -ne $sha256) { throw "Checksum mismatch: $file is $actual" }
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
# German voice for text to speech: Piper's Thorsten (medium), trained on the Thorsten-Voice
# dataset (CC0, https://github.com/thorstenMueller/Thorsten-Voice), from the pinned v1.0.0 release
# of the piper-voices repository. Piper reads the .onnx.json next to the model.
$voiceBase = 'https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0/de/de_DE/thorsten/medium'
$voiceSha = '7e64762d8e5118bb578f2eea6207e1a35a8e0c30595010b666f983fc87bb7819'
$voiceJsonSha = '974adee790533adb273a1ac88f49027d2a1b8f0f2cf4905954a4791e79264e85'
$voiceCardSha = '5196b5ab0794e6056263a1f37c18bec407b61ac187529bee29d1c366871e5c9e'
Get-Pinned "$voiceBase/de_DE-thorsten-medium.onnx" "$Destination/de_DE-thorsten-medium.onnx" $voiceSha
Get-Pinned "$voiceBase/de_DE-thorsten-medium.onnx.json" "$Destination/de_DE-thorsten-medium.onnx.json" $voiceJsonSha
Get-Pinned "$voiceBase/MODEL_CARD" "$Destination/de_DE-thorsten-medium.MODEL_CARD.txt" $voiceCardSha
Get-Content "$Destination/de_DE-thorsten-medium.MODEL_CARD.txt" | Where-Object { $_.Trim() } | ForEach-Object { Write-Host "::notice::Thorsten model card: $_" }
# Voice and music separation: UVR-MDX-NET-Inst_HQ_3 (MDX-Net, trained by the Ultimate Vocal
# Remover developers; MIT with credit per the UVR README), from the pinned public model release.
$stemsUrl = 'https://github.com/TRvlvr/model_repo/releases/download/all_public_uvr_models/UVR-MDX-NET-Inst_HQ_3.onnx'
$stemsSha = '317554b07fe1ea5279a77f2b1520a41ea4b93432560c4ffd08792c30fddf9adc'
Get-Pinned $stemsUrl "$Destination/UVR-MDX-NET-Inst_HQ_3.onnx" $stemsSha
# Caption translation: Opus-MT German-English and English-German (Marian models by the Helsinki
# NLP group, CC-BY 4.0): the int8 ONNX encoder and decoder exported by Xenova (transformers.js),
# with the original SentencePiece model, vocabulary and settings from Helsinki-NLP.
$translation = @(
    @{ pair='de-en'; files=@(
        @{ url='https://huggingface.co/Xenova/opus-mt-de-en/resolve/main/onnx/encoder_model_quantized.onnx'; name='encoder_model.onnx'; sha='' },
        @{ url='https://huggingface.co/Xenova/opus-mt-de-en/resolve/main/onnx/decoder_model_quantized.onnx'; name='decoder_model.onnx'; sha='' },
        @{ url='https://huggingface.co/Helsinki-NLP/opus-mt-de-en/resolve/main/source.spm'; name='source.spm'; sha='' },
        @{ url='https://huggingface.co/Helsinki-NLP/opus-mt-de-en/resolve/main/vocab.json'; name='vocab.json'; sha='' },
        @{ url='https://huggingface.co/Helsinki-NLP/opus-mt-de-en/resolve/main/config.json'; name='config.json'; sha='' }) },
    @{ pair='en-de'; files=@(
        @{ url='https://huggingface.co/Xenova/opus-mt-en-de/resolve/main/onnx/encoder_model_quantized.onnx'; name='encoder_model.onnx'; sha='' },
        @{ url='https://huggingface.co/Xenova/opus-mt-en-de/resolve/main/onnx/decoder_model_quantized.onnx'; name='decoder_model.onnx'; sha='' },
        @{ url='https://huggingface.co/Helsinki-NLP/opus-mt-en-de/resolve/main/source.spm'; name='source.spm'; sha='' },
        @{ url='https://huggingface.co/Helsinki-NLP/opus-mt-en-de/resolve/main/vocab.json'; name='vocab.json'; sha='' },
        @{ url='https://huggingface.co/Helsinki-NLP/opus-mt-en-de/resolve/main/config.json'; name='config.json'; sha='' }) }
)
foreach ($t in $translation) {
    New-Item -ItemType Directory -Force "$Destination/opus-mt-$($t.pair)" | Out-Null
    foreach ($f in $t.files) { Get-Pinned $f.url "$Destination/opus-mt-$($t.pair)/$($f.name)" $f.sha }
}
python -m pip install --quiet onnx==1.17.0
if ($LASTEXITCODE -ne 0) { throw 'Installing the onnx package failed' }
python "$PSScriptRoot/convert-realesrgan.py" $weights "$Destination/realesr-general-x4v3.onnx"
if ($LASTEXITCODE -ne 0) { throw 'Model conversion failed' }
$srOnnx = (Get-FileHash "$Destination/realesr-general-x4v3.onnx" -Algorithm SHA256).Hash.ToLowerInvariant()
# Eye contact: Google MediaPipe's face detection, face mesh and iris landmark models
# (Apache-2.0, https://github.com/google-ai-edge/mediapipe), taken from the official mediapipe
# 0.10.18 wheel on PyPI (pinned) and converted from TFLite to ONNX with tf2onnx. The TFLite
# files are pinned; the conversion is not byte-reproducible, so the ONNX hashes are recorded.
$wheelDir = Join-Path (Split-Path $Destination) 'mediapipe'
New-Item -ItemType Directory -Force $wheelDir | Out-Null
$wheel = "$wheelDir/mediapipe-0.10.18-cp312-cp312-manylinux_2_17_x86_64.manylinux2014_x86_64.whl"
$wheelSha = 'edbabfb9728dc1fcd93fea47abece12d43300530a1d4261e257f6bd4e3be09d1'
if (!(Test-Path $wheel) -or (Get-FileHash $wheel -Algorithm SHA256).Hash.ToLowerInvariant() -ne $wheelSha) {
    python -m pip download --quiet --no-deps mediapipe==0.10.18 --only-binary=:all: --platform manylinux_2_17_x86_64 --python-version 3.12 -d $wheelDir
    if ($LASTEXITCODE -ne 0) { throw 'Downloading the mediapipe wheel failed' }
}
if ((Get-FileHash $wheel -Algorithm SHA256).Hash.ToLowerInvariant() -ne $wheelSha) { throw 'mediapipe wheel checksum mismatch' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($wheel)
$faceModels = @(
    @{ name='face_detection_short_range'; entry='mediapipe/modules/face_detection/face_detection_short_range.tflite'; sha256='bbff11cebd1eb27a1e004cae0b0e63ec8c551cbf34a4451148b4908b8db3eca8' },
    @{ name='face_landmark'; entry='mediapipe/modules/face_landmark/face_landmark.tflite'; sha256='1055cb9d4a9ca8b8c688902a3a5194311138ba256bcc94e336d8373a5f30c814' },
    @{ name='iris_landmark'; entry='mediapipe/modules/iris_landmark/iris_landmark.tflite'; sha256='d1744d2a09c25f501d39eba4faff47e53ecca8852c5ce19bce8eeac39357521f' }
)
try {
    foreach ($m in $faceModels) {
        $tflite = "$wheelDir/$($m.name).tflite"
        [IO.Compression.ZipFileExtensions]::ExtractToFile($zip.GetEntry($m.entry), $tflite, $true)
        if ((Get-FileHash $tflite -Algorithm SHA256).Hash.ToLowerInvariant() -ne $m.sha256) { throw "Checksum mismatch: $($m.name).tflite" }
    }
} finally { $zip.Dispose() }
python -m pip install --quiet tensorflow==2.17.1 tf2onnx==1.16.1 onnx==1.17.0
if ($LASTEXITCODE -ne 0) { throw 'Installing tf2onnx failed' }
foreach ($m in $faceModels) {
    python -m tf2onnx.convert --tflite "$wheelDir/$($m.name).tflite" --output "$Destination/$($m.name).onnx" --opset 17 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Converting $($m.name) failed" }
    $m.onnx = (Get-FileHash "$Destination/$($m.name).onnx" -Algorithm SHA256).Hash.ToLowerInvariant()
}
@{ models=@(
    @{ file='u2net_human_seg.onnx'; url=$matteUrl; sha256=$matteSha; license='Apache-2.0'; source='https://github.com/xuebinqin/U-2-Net'; purpose='person matte for AI background removal' },
    @{ file='ggml-large-v3-turbo-q5_0.bin'; url=$speechUrl; sha1=$speechSha1; sha256=$speechSha; license='MIT'; source='https://github.com/openai/whisper'; purpose='speech recognition for automatic captions' },
    @{ file='realesr-general-x4v3.onnx'; converted_from=$srUrl; source_sha256=$srSha; sha256=$srOnnx; license='BSD-3-Clause'; source='https://github.com/xinntao/Real-ESRGAN'; purpose='4x super-resolution for AI upscale' },
    @{ file='de_DE-thorsten-medium.onnx'; url="$voiceBase/de_DE-thorsten-medium.onnx"; sha256=$voiceSha; settings_sha256=$voiceJsonSha; license='Dataset CC0-1.0 (Thorsten-Voice); fine-tuned from the lessac voice per the model card'; source='https://github.com/thorstenMueller/Thorsten-Voice'; purpose='German voice for text to speech (Piper)' },
    @{ file='UVR-MDX-NET-Inst_HQ_3.onnx'; url=$stemsUrl; sha256=$stemsSha; license='MIT (credit Ultimate Vocal Remover and its developers)'; source='https://github.com/Anjok07/ultimatevocalremovergui'; purpose='voice and music separation' }
) + @($translation | ForEach-Object { @{ folder="opus-mt-$($_.pair)"; files=@($_.files | ForEach-Object { @{ file=$_.name; url=$_.url; sha256=$_.sha } }); license='CC-BY-4.0'; source='https://github.com/Helsinki-NLP/Opus-MT'; purpose='caption translation' } }) + @($faceModels | ForEach-Object { @{ file="$($_.name).onnx"; converted_from="mediapipe==0.10.18:$($_.entry)"; source_sha256=$_.sha256; sha256=$_.onnx; license='Apache-2.0'; source='https://github.com/google-ai-edge/mediapipe'; purpose='face, eye and iris landmarks for eye contact' } }) } | ConvertTo-Json -Depth 4 | Set-Content "$Destination/manifest.json" -Encoding utf8
Write-Output (Resolve-Path $Destination).Path
