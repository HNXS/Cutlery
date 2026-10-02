param([string]$Destination = "$PSScriptRoot/../.deps/whisper")
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
# whisper.cpp (MIT) built from a pinned release commit: whisper-cli.exe with ggml's backends as
# DLLs that load at run time. The Vulkan backend runs on any GPU with a Vulkan driver (NVIDIA,
# AMD, Intel); without one it does not load and the CPU backend runs instead. The CPU backend is
# built for every x64 level (SSE4.2 to AVX-512) and the best one for the machine loads. It uses
# OpenMP like the upstream Windows builds: ggml's own spinning thread pool slows down by orders of
# magnitude when threads outnumber free cores. The OpenMP runtime (vcomp140.dll) ships with it.
# Needs the Vulkan SDK (Get-VulkanSdk.ps1) in $env:VULKAN_SDK.
$tag = 'v1.9.4'
$commit = '927cfce34f31707e17f2bff35c349632fb9e2c3a'
$source = "$Destination/src"
$out = "$Destination/bin"
$build = 'shared, Vulkan + CPU variants, MSVC OpenMP'
# A finished build of the same commit and options is reused (CI caches this folder).
if (Test-Path "$out/manifest.json") {
    $m = Get-Content "$out/manifest.json" -Raw | ConvertFrom-Json
    if ($m.commit -eq $commit -and $m.build -eq $build) { Write-Output (Resolve-Path $out).Path; return }
}
if (!$env:VULKAN_SDK -or !(Test-Path "$env:VULKAN_SDK/Bin/glslc.exe")) { throw 'The Vulkan SDK is required: set VULKAN_SDK (tools/Get-VulkanSdk.ps1)' }
if (Test-Path $source) { Remove-Item $source -Recurse -Force }
git clone --quiet --depth 1 --branch $tag https://github.com/ggml-org/whisper.cpp $source
if ($LASTEXITCODE -ne 0) { throw 'whisper.cpp download failed' }
if ((git -C $source rev-parse HEAD) -ne $commit) { throw 'whisper.cpp tag does not match the pinned commit' }
cmake -S $source -B "$source/build" -G 'Visual Studio 17 2022' -A x64 -DBUILD_SHARED_LIBS=ON -DGGML_BACKEND_DL=ON -DGGML_CPU_ALL_VARIANTS=ON -DGGML_NATIVE=OFF -DGGML_VULKAN=ON -DGGML_OPENMP=ON -DWHISPER_BUILD_TESTS=OFF -DWHISPER_SDL2=OFF | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'whisper.cpp configuration failed' }
cmake --build "$source/build" --config Release --target whisper-cli --parallel 2 | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'whisper.cpp build failed' }
$exe = Get-ChildItem "$source/build" -Recurse -Filter whisper-cli.exe | Select-Object -First 1
if (!$exe) { throw 'whisper-cli.exe missing' }
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force $out | Out-Null
Copy-Item $exe.FullName "$out/whisper-cli.exe"
# whisper.dll, ggml.dll, ggml-base.dll and the backends sit next to whisper-cli.exe.
Copy-Item "$($exe.DirectoryName)/*.dll" $out
foreach ($dll in 'whisper.dll', 'ggml.dll', 'ggml-base.dll', 'ggml-vulkan.dll', 'ggml-cpu-x64.dll', 'ggml-cpu-haswell.dll') {
    if (!(Test-Path "$out/$dll")) { throw "$dll missing from the whisper.cpp build" }
}
Copy-Item "$source/LICENSE" "$out/whisper.cpp-LICENSE.txt"
$vs = & "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -property installationPath
$omp = Get-ChildItem "$vs/VC/Redist/MSVC" -Recurse -Filter vcomp140.dll | Where-Object { $_.FullName -match '\\x64\\Microsoft\.VC\d+\.OpenMP\\' } | Select-Object -First 1
if (!$omp) { throw 'MSVC OpenMP runtime (vcomp140.dll) not found' }
Copy-Item $omp.FullName $out
# Silero VAD v6.2.0 (MIT) as converted by whisper.cpp; skips non-speech before recognition.
Copy-Item "$source/models/for-tests-silero-v6.2.0-ggml.bin" "$out/ggml-silero-v6.2.0.bin"
@{ tag=$tag; commit=$commit; source='https://github.com/ggml-org/whisper.cpp'; license='MIT'; build=$build } | ConvertTo-Json | Set-Content "$out/manifest.json" -Encoding utf8
Write-Output (Resolve-Path $out).Path
