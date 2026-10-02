param([string]$Destination = "$PSScriptRoot/../.deps/vulkan")
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
# LunarG Vulkan SDK, a build-time tool only: headers, the loader import library and the glslc
# shader compiler for whisper.cpp's Vulkan backend. Nothing from it ships; at run time the
# Vulkan loader comes with the graphics driver. Pinned by hash.
$version = '1.4.321.1'
$sha256 = 'baaa4f7ca11ed3d82aa1c102b21208915485bbaa473068c763daa425cca468bd'
New-Item -ItemType Directory -Force $Destination | Out-Null
$installer = Join-Path $Destination "vulkansdk-windows-X64-$version.exe"
Invoke-WebRequest -Uri "https://sdk.lunarg.com/sdk/download/$version/windows/vulkansdk-windows-X64-$version.exe" -OutFile $installer
$size = (Get-Item $installer).Length
if ($size -lt 50MB) { throw "Vulkan SDK download is only $size bytes" }
$hash = (Get-FileHash $installer -Algorithm SHA256).Hash.ToLowerInvariant()
if ($hash -ne $sha256) { throw "Checksum mismatch: Vulkan SDK $($version): got $hash" }
$root = (New-Item -ItemType Directory -Force "$Destination/$version").FullName
$p = Start-Process $installer -ArgumentList @('--root', $root, '--accept-licenses', '--default-answer', '--confirm-command', 'install') -Wait -PassThru
if ($p.ExitCode -ne 0) { throw "Vulkan SDK installation failed ($($p.ExitCode))" }
if (!(Test-Path "$root/Bin/glslc.exe")) { throw 'glslc.exe missing from the Vulkan SDK' }
Write-Output $root
