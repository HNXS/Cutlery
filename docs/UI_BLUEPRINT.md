# UI blueprint

How Cutlery's editor window is being reorganised so that every tool has a clear place. The
structure follows the conventions of today's consumer editors: assets on the left, the player in
the middle, a context-sensitive inspector on the right and a full-width timeline below. Names,
icons, colours and artwork are Cutlery's own.

It is based on a study of 256 screenshots of a popular desktop editor's tutorials (layout, tab
order, control types, colours). Only conventions were taken from them, nothing else.

## Window

- Three panels on top (assets about 35 % of the width, player about 35 %, inspector about 30 %),
  the timeline across the full width below (about 45 % of the height). Splitters between all of
  them.
- A slim top bar: menu, save state, project name in the centre, shortcuts and a prominent
  accent-coloured Export button on the right.
- Panels are dark rounded cards on an almost black background, with gaps of about 4 px.

## Inspector (right) — package 1, done

The inspector shows tabs for what is selected and, for the bigger tabs, sub-tabs as a segmented
control. The page chosen is kept per kind of selection.

| Selection | Tabs (sub-tabs) |
|---|---|
| Video, picture, nested sequence | Video (Basic · Cutout · Mask · Canvas · Enhance) · Audio (Basic · Voice · Clean-up), when it has sound · Speed · Animation · Adjust (Basic · HSL · Curves · Wheels · LUT) · Effects · More |
| Sound clip | Basic · Voice · Clean-up · Speed · More |
| Title or caption | Text · Basic · Animation · More |
| Shape | Shape · Basic · Animation · More |
| Blur or mosaic area | Effect · Basic · More |
| Adjustment layer | Adjust (sub-tabs as above) · Basic · More |
| Nothing | Project details (name, file, canvas, frame rate, length) and Preferences |

What goes where:

- **Video › Basic:** placement in the corners, position, scale, rotation, opacity and their
  keyframes, anchor, crop, perspective and 3D tilt, blend mode, flip, hide.
- **Video › Cutout:** AI cutout, colour key, luma key.
- **Video › Mask:** shape, free mask, soft edge, border, shadow.
- **Video › Canvas:** the background around pictures that do not fill the frame.
- **Video › Enhance:** stabilising, video noise and flicker, source colours and HDR, AI upscale
  and eye contact.
- **Audio › Basic:** volume, pan, fades (with keyframes), mute, save sound as a file.
- **Audio › Voice:** voice changer, pitch, reverb, echo.
- **Audio › Clean-up:** sound presets, low cut, noise reduction, room echo, gate, EQ, de-esser,
  compressor, even loudness, learn noise.
- **Speed:** start, length, source in, speed, slow motion, speed ramps, reverse, freeze frame,
  constant frame rate.
- **Animation:** ready-made motions, the transition from the previous clip.
- **Adjust:** looks, auto colour, exposure to grain; HSL; curves; colour wheels; LUT.
- **Effects:** style effects, motion blur, blur.
- **More:** edit by text, remove pauses, beats and markers, scene detection, track, copy and
  paste attributes, detach, unlink, relink.

## Inspector sections — package 1b, done

- **Sections:** each part of a page is a section with a bold title, a fold arrow and, where it
  makes sense, a reset button (↺, one undo step) and an on/off box that dims the section while it
  is off (e.g. Stabilize).
- **Value rows:** each value shows its name, a number box with its unit (%, °, dB, Hz, s, EV) and
  ▴/▾ steppers, a keyframe diamond (◇, ◆ at a keyframe, yellow while animated) for animatable
  values, and a slider. A double-click on the slider resets the value; typing in the box sets it.
- **Converted so far:** Transform, Opacity, Crop, Light, Blur, Volume and fades, the sound tools
  (Clean-up, Tone, Dynamics, Room and pitch), Colour, Tones, Details, Edge and frame, Stabilize,
  Noise and flicker, and Motion blur. The other pages have section titles; their controls keep
  their own form.

## Asset panel — package 2, done

The left panel has icon tabs: Media, Audio, Text, Stickers, Effects, Transitions, Filters and
Layouts. The panel is wider (400 px, at least 330), so the labels fit.

- **Media:** the library as before (import, views and folders, search, tiles).
- **Audio:** sound effects, a shortcut to the library's sound files, and a hint to the voice-over
  button.
- **Text:** title, caption, the five title templates, and auto captions.
- **Stickers:** shapes, icons and the brand kit (colours and logo).
- **Effects:** blur and mosaic areas, and the adjustment layer. Below them, style effects as tiles
  for the selected clip; its current effect is framed in mint.
- **Transitions:** tiles for each transition into the selected clip. They are enabled only when
  the clip directly follows another; the length is set in the inspector.
- **Filters:** looks as tiles, the LUT library as tiles, and adding a LUT file.
- **Layouts:** arranging the selected pictures, and your own layouts.

Tiles show a glyph over a label, a mint "+" on hover and a mint frame for the current choice; each
click is one undo step.

## Timeline and top bar — package 3, done

- **Tool row above the timeline:**
  - Left: undo, redo, split, cut away before the playhead, cut away after it, delete, marker,
    freeze frame, reverse, mirror and turn by 90°.
  - Right: add a track, edge snap, fit, and zoom out, slider and zoom in.
  - Each tool is a glyph with its name in the tooltip. A tool is enabled only when it applies to
    the selection (split, trim and freeze need the playhead inside the clip); switches such as
    reverse and mirror turn mint while on.
- **Track headers:** the name and ⋯ menu, then drawn icons for lock, picture (eye), sound
  (speaker), solo, magnet and snap. A hidden or muted track shows its eye or speaker crossed out.
  Tracks are lower (68 px).
- **Clip colours by kind:** video and pictures teal, sound blue, text red, shapes amber, blur and
  mosaic areas purple, adjustment layers ochre.
- **Top bar:** the name, size and frame rate in the middle ("not saved" when there are changes),
  Save and a filled mint Export button on the right. Undo and redo moved to the tool row.

## Player bar and start screen — package 4, done

- **Player:**
  - A "Player" heading, with "Playing" or "Paused · exact frame" on the right.
  - Left of the bar below: the time in mint, the total and the level meter.
  - Middle: frame back, play or pause, and frame on.
  - Right: voice-over, scopes and the frame's shape (e.g. 16:9).
- **Start screen:**
  - A side column with Cutlery, Home, Open project, Recover autosave, Preferences, Show at start and Skip.
  - On the right: a large "New project" banner that starts in the preferred shape in one click, the six shapes, the notice after an unclean exit, recent projects as tiles and the templates.

## Sounds, media and remaining controls — package 5, done

- **Audio tab:** the sound effects are listed as tiles by category; a click adds one at the playhead. "Listen and more…" opens the list for listening and for a whoosh on every transition.
- **Media tiles:** media used in the timeline carries an "Added" mark on its picture.
- **Sections:** perspective (on/off, reset), 3D tilt, green or blue screen (on/off, reset), removing by brightness, text spacing, and outline, shadow and glow with their colours now use sections and value rows.

## Still to do

- **Asset panel:** a narrow category column per tab, once the tabs hold more than a screenful.
- A section look for the rest: shape and frame, canvas, and the title settings above spacing.
