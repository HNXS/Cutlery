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

## Still to do

- **Asset panel (package 2):**
  - An icon tab row: Media, Audio, Text, Stickers, Effects, Transitions, Captions, Filters,
    Adjustment.
  - Each tab has a narrow category column (e.g. Media: Local, Library; Audio: Sound effects,
    Recorded; Text: Add text, Styles, Templates, Auto captions).
  - Tiles have previews, a duration, an "Added" badge and a "+" on hover.
- **Timeline and top bar (package 3):**
  - A tool row above the timeline:
    - left: select, undo, redo, split, delete left and right, delete, marker, freeze, reverse,
      mirror, rotate, crop;
    - right: record, magnet, snapping, linking, zoom out, zoom slider, zoom in.
  - Track headers with type icon, lock, eye and speaker icons.
  - Clip colours by type: video teal, text red, sticker amber, effect purple, filter indigo,
    adjustment ochre, audio blue.
  - Player bar with the current time in the accent colour, the total, play, fit, aspect ratio and
    full screen.
