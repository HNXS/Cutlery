# Builds the portable Windows package on your own PC, the same way CI does: downloads the pinned
# dependencies into .deps, compiles, runs the tests and packages into dist/.
#
#   ./tools/Build-Local.ps1              # everything: portable build and AI pack
#   ./tools/Build-Local.ps1 -SkipAi      # without the AI worker, models and speech recognition
#   ./tools/Build-Local.ps1 -SkipTests   # package without running the tests
#
# Needs Visual Studio 2022 (or its Build Tools) with "Desktop development with C++", Python 3.9 to
# 3.12 and Git. Run it from PowerShell 7 (pwsh) or Windows PowerShell. Downloads are reused on later
# runs; delete .deps to start over.
param(
    [switch]$SkipAi,
    [switch]$SkipTests,
    [string]$QtRoot = ''
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = (Resolve-Path "$PSScriptRoot/..").Path
$deps = "$root/.deps"
New-Item -ItemType Directory -Force $deps | Out-Null
function Step($text) { Write-Host "`n== $text" -ForegroundColor Cyan }
function Check($what) { if ($LASTEXITCODE -ne 0) { throw "$what failed (exit code $LASTEXITCODE)" } }

Step 'Checking the tools'
if (!(Get-Command git -ErrorAction SilentlyContinue)) { throw 'Git is not on PATH: install Git for Windows.' }
$python = Get-Command python -ErrorAction SilentlyContinue
if (!$python) { throw 'Python is not on PATH: install Python 3.12 from python.org and tick "Add to PATH".' }
$pyVersion = [version](& python -c "import sys; print('%d.%d' % sys.version_info[:2])")
# TensorFlow 2.17, used for the model conversion, supports Python 3.9 to 3.12.
if ($pyVersion -lt [version]'3.9' -or $pyVersion -gt [version]'3.12') {
    throw "Python $pyVersion found; the model conversion needs Python 3.9 to 3.12."
}
# Visual Studio's own CMake is used when none is on PATH.
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
if (!(Test-Path $vswhere)) { throw 'Visual Studio 2022 is not installed (vswhere.exe missing).' }
$vs = & $vswhere -latest -version '[17.0,18.0)' -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio 2022 with "Desktop development with C++" is required.' }
if (!(Get-Command cmake -ErrorAction SilentlyContinue)) {
    $env:PATH = "$vs/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin;" + $env:PATH
    if (!(Get-Command cmake -ErrorAction SilentlyContinue)) { throw 'CMake not found: add the "C++ CMake tools for Windows" component or install CMake.' }
}

# Python packages (aqtinstall, and TensorFlow for the model conversion) go into a private
# environment so the system Python stays untouched.
Step 'Python environment'
$venv = "$deps/venv"
if (!(Test-Path "$venv/Scripts/python.exe")) { python -m venv $venv; Check 'Creating the Python environment' }
$env:PATH = "$venv/Scripts;" + $env:PATH
$env:VIRTUAL_ENV = $venv

Step 'Qt 6.8.3'
if (!$QtRoot) { $QtRoot = "$deps/Qt/6.8.3/msvc2022_64" }
if (!(Test-Path "$QtRoot/bin/windeployqt.exe")) {
    python -m pip install --quiet aqtinstall==3.3.0; Check 'Installing aqtinstall'
    python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --outputdir "$deps/Qt" --modules qtmultimedia; Check 'Qt installation'
}
$env:PATH = "$QtRoot/bin;" + $env:PATH

Step 'FFmpeg (LGPL)'
$ffmpeg = & "$PSScriptRoot/Get-FFmpeg.ps1" | Select-Object -Last 1
$env:PATH = "$ffmpeg;" + $env:PATH

Step 'Sound effects'
$sounds = & "$PSScriptRoot/Get-Sounds.ps1" | Select-Object -Last 1

$configure = @("-DCMAKE_PREFIX_PATH=$QtRoot")
$package = @{ QtRoot = $QtRoot; FFmpegBin = $ffmpeg; Sounds = $sounds }
if (!$SkipAi) {
    Step 'ONNX Runtime and AI models (several GB on the first run)'
    $ort = & "$PSScriptRoot/Get-OnnxRuntime.ps1" | Select-Object -Last 1
    $models = & "$PSScriptRoot/Get-Models.ps1" | Select-Object -Last 1
    Step 'Speech recognition (whisper.cpp)'
    if (!(Test-Path "$deps/whisper/bin/whisper-cli.exe")) {
        # The Vulkan SDK is only needed to compile whisper.cpp once.
        $env:VULKAN_SDK = & "$PSScriptRoot/Get-VulkanSdk.ps1" | Select-Object -Last 1
    }
    $whisper = & "$PSScriptRoot/Get-Whisper.ps1" | Select-Object -Last 1
    $configure += "-DCUTLERY_ONNXRUNTIME_DIR=$ort", "-DCUTLERY_WHISPER_CLI=$whisper/whisper-cli.exe", "-DCUTLERY_TEST_MODELS=$models"
    $piper = & "$PSScriptRoot/Get-Piper.ps1" | Select-Object -Last 1
    $package += @{ OnnxRuntime = $ort; Models = $models; Whisper = $whisper; Piper = $piper }
}

Step 'Compiling'
cmake -S $root -B "$root/build" -G 'Visual Studio 17 2022' -A x64 @configure; Check 'Configuration'
cmake --build "$root/build" --config Release --parallel; Check 'Build'

if (!$SkipTests) {
    Step 'Testing'
    $env:QT_QPA_PLATFORM = 'offscreen'
    $env:QT_QUICK_BACKEND = 'software'
    $env:QT_QPA_FONTDIR = "$env:WINDIR/Fonts"
    $env:CUTLERY_TEST_SOUNDS = $sounds
    ctest --test-dir "$root/build" -C Release --output-on-failure
    $failed = $LASTEXITCODE
    Remove-Item Env:QT_QPA_PLATFORM, Env:QT_QUICK_BACKEND, Env:CUTLERY_TEST_SOUNDS
    if ($failed -ne 0) { throw 'Tests failed; see the output above. Use -SkipTests to package anyway.' }
}

Step 'Packaging'
$dist = & "$PSScriptRoot/Package-Windows.ps1" @package | Select-Object -Last 1
$p = Start-Process "$dist/Cutlery.exe" -ArgumentList '--smoke-test' -Wait -PassThru
if ($p.ExitCode -ne 0) { throw 'The packaged Cutlery.exe did not start' }
Write-Host "`nDone. Portable build: $dist" -ForegroundColor Green
if (!$SkipAi) { Write-Host "AI pack: $root/dist/Cutlery-0.5.0-AI-pack (copy its contents next to Cutlery.exe)" -ForegroundColor Green }
