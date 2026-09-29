# Third-party components

Cutlery is original application code. It does not contain CapCut assets, templates, branding, private protocols or proprietary models. No application-wide distribution licence has been selected for this private project.

| Component | Use | Distribution notes / upstream source |
|---|---|---|
| Qt 6.8.3 (Core, Gui, Quick, QuickControls2, Multimedia, supporting modules) | Dynamically loaded UI/playback runtime | LGPL/GPL/commercial licensing varies by module and bundled dependency. This build uses public SDK modules; preserve notices and replacement/relinking rights. [Qt source archive](https://download.qt.io/archive/qt/6.8/6.8.3/single/) and [Qt licensing](https://doc.qt.io/qt-6/licensing.html). |
| FFmpeg n8.1.3-6-gff48edd8b2, BtbN Windows LGPL shared build | Separate ffmpeg/ffprobe child processes and their DLLs | Pinned archive/hash in `tools/Get-FFmpeg.ps1`; rejects GPL/nonfree build flags. [FFmpeg source](https://github.com/FFmpeg/FFmpeg/commit/ff48edd8b2), [build recipes](https://github.com/BtbN/FFmpeg-Builds/tree/autobuild-2026-09-28-13-06), [licensing](https://ffmpeg.org/legal.html). Codec patent questions are separate from software licences. |
| Qt Multimedia's codec backend | Cached file playback | Deployed by windeployqt; separate from the explicit FFmpeg command runtime. Keep Qt's dependency notices/SBOM. |
| Microsoft C/C++ runtime | Windows runtime | Deployed by windeployqt from the build environment, subject to Microsoft's redistributable terms. |
| System fonts | User titles/captions | Rasterized at render time. No font files are bundled. |

The portable pack includes upstream FFmpeg distribution notices/docs, available Qt SBOM data, licence texts in `licenses`, and a build manifest. This is an inventory, not a completed legal audit or a patent licence. Before public redistribution, audit the actual packaged DLLs, retain all applicable notices and corresponding-source obligations, and decide the application's own licence. No inference models or model weights are bundled.
