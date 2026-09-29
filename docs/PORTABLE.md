# Portable use

Extract the complete package to a writable folder and launch `Cutlery.exe`. The adjacent `portable.json` selects portable mode. Cutlery puts its recovery snapshot, render cache and instance lock in `data/` beside the executable. It never silently falls back to AppData when a portable folder is unwritable. Without the marker, development builds use Qt's per-user local application data directory.

The package contains the Qt runtime/plugins/QML modules and FFmpeg/ffprobe. The application's editing/rendering code makes no network requests. No auto-updater or telemetry client is implemented. Building and downloading the package need a network connection; ordinary use with local media does not. Windows itself and third-party runtime components may have their own OS-level behavior.

Projects reference source files with paths relative to the project file where possible. Moving a project **does not collect its media**. Move the media with it, preserve the directory structure, or select a clip and choose Relink source media. Replacements must have the same media type and cover every clip's source range.

Use Project → Recover autosave to load the last recovery snapshot, then Save As. Saving normally clears that snapshot. There is only one recovery snapshot per application data folder. Do not run multiple copies sharing one data folder; the instance lock prevents it.

Close Cutlery before deleting `data/cache` to reclaim disk space. Completed playback caches are disposable. Preserve `data/recovery.cutlery` if you need recovery. A hard kill can leave `.cutlery-<id>.mp4`/`.webm` partial files in an export destination; these can be deleted after confirming no render is active.

The alpha has no installer, file association, auto-update or signing certificate. Do not treat an unsigned alpha and CI smoke test as completed enterprise deployment qualification.
