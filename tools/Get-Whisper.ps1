param([string]$Destination = "$PSScriptRoot/../.deps/whisper")
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
# whisper.cpp (MIT) built from a pinned release commit as a static whisper-cli.exe. Built for
# AVX2 (Intel Haswell / AMD Zen and newer) rather than the build machine's CPU, and without
# OpenMP, so it needs only the MSVC runtime that ships with Cutlery.
$tag = 'v1.9.4'
$commit = '927cfce34f31707e17f2bff35c349632fb9e2c3a'
$source = "$Destination/src"
if (Test-Path $source) { Remove-Item $source -Recurse -Force }
git clone --quiet --depth 1 --branch $tag https://github.com/ggml-org/whisper.cpp $source
if ($LASTEXITCODE -ne 0) { throw 'whisper.cpp download failed' }
if ((git -C $source rev-parse HEAD) -ne $commit) { throw 'whisper.cpp tag does not match the pinned commit' }
cmake -S $source -B "$source/build" -G 'Visual Studio 17 2022' -A x64 -DBUILD_SHARED_LIBS=OFF -DGGML_NATIVE=OFF -DGGML_AVX=ON -DGGML_AVX2=ON -DGGML_FMA=ON -DGGML_F16C=ON -DGGML_OPENMP=OFF -DWHISPER_BUILD_TESTS=OFF -DWHISPER_SDL2=OFF | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'whisper.cpp configuration failed' }
cmake --build "$source/build" --config Release --target whisper-cli --parallel 2 | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'whisper.cpp build failed' }
$exe = Get-ChildItem "$source/build" -Recurse -Filter whisper-cli.exe | Select-Object -First 1
if (!$exe) { throw 'whisper-cli.exe missing' }
$out = "$Destination/bin"
New-Item -ItemType Directory -Force $out | Out-Null
Copy-Item $exe.FullName "$out/whisper-cli.exe"
Copy-Item "$source/LICENSE" "$out/whisper.cpp-LICENSE.txt"
# Silero VAD v6.2.0 (MIT) as converted by whisper.cpp; skips non-speech before recognition.
Copy-Item "$source/models/for-tests-silero-v6.2.0-ggml.bin" "$out/ggml-silero-v6.2.0.bin"
@{ tag=$tag; commit=$commit; source='https://github.com/ggml-org/whisper.cpp'; license='MIT'; build='static, AVX2, no OpenMP' } | ConvertTo-Json | Set-Content "$out/manifest.json" -Encoding utf8
Write-Output (Resolve-Path $out).Path
