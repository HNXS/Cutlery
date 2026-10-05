# Keyboard shortcuts

Open **Help → Keyboard shortcuts** (default **Ctrl+/**) to view or change any of these 30 bindings. Type one combination such as `Ctrl+Shift+B` and click Apply. Clear a field to disable that command; clear an existing assignment before reusing its key. Conflicts and invalid combinations are rejected. Restore defaults resets the whole list. Changes are saved with the portable application data.

Click the timeline before using editing/playback keys. During text entry, normal typing and text editing take priority; timeline shortcuts are disabled. Modal dialogs suppress application shortcuts. Arrow keys continue to work in sliders, lists and numeric controls.

| Command | Category | Default |
|---|---|---|
| New project | Project | `Ctrl+N` |
| Open project | Project | `Ctrl+O` |
| Import media | Project | `Ctrl+I` |
| Save project | Project | `Ctrl+S` |
| Save project as | Project | `Ctrl+Shift+S` |
| Export video | Project | `Ctrl+E` |
| Undo | Edit | `Ctrl+Z` |
| Redo | Edit | `Ctrl+Y` |
| Split selected clip | Edit | `Ctrl+B` |
| Duplicate clip | Edit | `Ctrl+D` |
| Delete clip (all selected clips) | Edit | `Del` |
| Select all clips | Edit | `Ctrl+A` |
| Group selected clips | Edit | `Ctrl+G` |
| Ungroup | Edit | `Ctrl+Shift+G` |
| Delete and close track gap | Edit | `Shift+Del` |
| Trim start to playhead | Edit | `Q` |
| Trim end to playhead | Edit | `W` |
| Detach audio | Edit | `Ctrl+Shift+A` |
| Add title | Edit | `Ctrl+T` |
| Play / pause | Playback | `Space` |
| Pause at current frame | Playback | `K` |
| Previous frame | Navigation | `Left` |
| Next frame | Navigation | `Right` |
| Previous edit | Navigation | `Up` |
| Next edit | Navigation | `Down` |
| Start of timeline | Navigation | `Home` |
| End of timeline | Navigation | `End` |
| Zoom in | Timeline | `Ctrl+=` |
| Zoom out | Timeline | `Ctrl+-` |
| Fit timeline | Timeline | `Shift+Z` |
| Toggle snapping | Timeline | `N` |
| Add track | Timeline | `Ctrl+Shift+N` |
| Keyboard shortcuts | Help | `Ctrl+/` |

**Playback:** Space starts live playback from the playhead within a fraction of a second; nothing is rendered to disk first. Edits made while playing restart playback from the current frame. Space pauses/resumes; K pauses at the displayed frame. Export video creates a separate file that can be played in a compatible video player. The 0.3 `Ctrl+R` render-cache command no longer exists; a saved binding for it is ignored.

**Escape** cancels an active library or timeline drag. Editing shortcuts are suspended during a drag.

Q/W require the playhead to be inside the selected clip. On free tracks, Delete closes no gap; Shift+Delete moves later clips on the same track back by the removed duration. On magnetic tracks, Delete closes the gap automatically. Locked tracks reject clip edits. Up/Down navigate clip starts and ends. These bindings cover implemented Cutlery commands; they are not a complete CapCut keymap. J/L shuttle, markers, copy/paste and other future editing features are not implemented yet.
