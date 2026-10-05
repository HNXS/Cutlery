# Drag/drop and magnetic tracks

Drag a video, audio file or still image from the media library onto any unlocked track. The highlighted rectangle shows placement; release to insert a new clip. The library item stays available for reuse. Double-clicking a library item still appends it to the selected track.

Drop files from Windows Explorer directly onto a track to import and insert them in drop order, starting at the highlighted time. Outside the timeline, a file drop imports into the library. Still images default to five seconds. Up to 500 queued files are accepted; folders and remote URLs are not imported. Invalid files are listed after the batch, while successfully imported files remain. Each successful import/insertion is one undo step.

Drag the body of a video, audio, image or title clip to move it along a track or between tracks. Drag its edges to trim instead. Near the timeline viewport edges, the view scrolls during a drag. Press Escape to cancel. Locked tracks reject insertion, moves and trimming.

Ctrl+click a clip to add it to the selection or take it out again, or drag a frame across empty timeline to select every clip it touches (hold Ctrl to add to the selection); dragging any selected clip moves them all, and Delete removes them all. Ctrl+C copies all selected clips; Ctrl+V puts the earliest at the playhead and keeps the others' spacing and tracks. Ctrl+G groups the selected clips, so clicking one selects the group; Ctrl+Shift+G ungroups. The transition button of a cut sits on the clips' top edge.

Hold **Alt** for edits that keep everything around the clip in place:

- **Slip:** Alt+drag the body to show an earlier part of the source (drag right) or a later part (drag left), at the same position and length.
- **Slide:** Alt+Shift+drag the body to move the clip. The touching clips before and after it grow or shrink to match.
- **Roll:** Alt+drag the edge between two touching clips to move the cut.

Each edit is one undo step.

| Control | Behavior |
|---|---|
| Edge snap, in the toolbar | Master switch for aligning clip edges to other edges, the playhead and frame zero. |
| Snap, on each track | Enables that alignment for moves/drops/trims targeting this track when Edge snap is on. It does not close gaps. |
| Magnet, on each track | Keeps this track's clips together, in sequence, starting at frame zero. Independent of Edge snap. |

Enabling Magnet packs the track's current clips in chronological order, closing gaps and removing overlaps without changing their source ranges. This can change the timing of existing clips; **Undo restores the previous arrangement**. Other tracks retain their timing. Turn Magnet off for free placement, intentional gaps and overlaps; disabling it keeps the current positions.

On magnetic tracks, dropping a clip chooses the nearest join based on neighboring clip midpoints. Existing clips shift to make room. Moving a clip out or deleting it closes its old gap. Trimming or changing duration/speed shifts later clips on that same track. Source trim boundaries remain enforced. A complete edit and the resulting shifts form one undo step. Audio detached from a video stays linked to it (⛓). Moving or trimming either one moves or trims both while they stay aligned; "Unlink picture and sound" in the inspector separates them. Other clips on other tracks are not synchronized.

SRT import requires Magnet to be off on the destination caption track, so imported timestamps remain intact. New and migrated projects start with Magnet off and Snap on for every track. Both settings are saved in schema 3 projects.

File probes may still be running after a drop. If its destination track is removed or locked before a file is ready, that file is kept in the library and a placement error is shown. It is not redirected to a different track.

Multiple clip selection/group dragging, overwrite edits, roll/slip/slide tools, and dragging media out to another application remain open.
