# Validation record

## Windows portable build — 2026-10-10, screen reader names, keyboard focus, left-panel crash fix (0.6 development)

[GitHub Actions run 38035874124](https://github.com/HNXS/Cutlery/actions/runs/38035874124) on commit `77b57192c8d666c3de2aa3a7582fd5692dafec75` passed every step on its first attempt.

New coverage:

- **UI tests `aiCutoutControls`, `keyframeControls` and `startScreenAndPreferences`:** every visible control has a name for screen readers on these pages:
  - all eight left tabs
  - all fourteen inspector pages of a video
  - the pages of a title
  - the start screen
- **UI test `keyframeControls`:** twelve presses of Tab move the focus through at least six controls.
- **Crash fix:** walking the left tabs every 30 ms crashed in Qt Quick Layouts when leaving the Audio tab. After the fix this walk runs in `aiCutoutControls` on every build.

## Windows portable build — 2026-10-10, 5.1 surround export (0.6 development)

[GitHub Actions run 38032563964](https://github.com/HNXS/Cutlery/actions/runs/38032563964) on commit `a17bde6ed8529cfb5a83a16e1fcecc16a754d558` passed every step on its first attempt.

New coverage:

- **Engine test `surroundExport`:** three tones on three tracks: front (440 Hz), centre (880 Hz) and rear (220 Hz).
  - A 5.1 WAV export has six channels. Each tone is in its own channels, and LFE is silent.
  - A 5.1 AAC (M4A) export also has six channels.
  - MP3 refuses 5.1, and an unknown place is refused.
  - The place is kept in the project file.
- **UI test `exportDialog`:** choosing "5.1 surround" exports six channels.

## Windows portable build — 2026-10-10, lens distortion correction (0.6 development)

[GitHub Actions run 38030909824](https://github.com/HNXS/Cutlery/actions/runs/38030909824) on commit `d5b8b0017851963a0169559427ad308981671795` passed every step on its first attempt.

New coverage:

- **Engine test `lensCorrection`:** checked on a left-to-right ramp.
  - The centre stays where it is.
  - Straightening barrel distortion takes the edge from nearer the centre of the source.
  - Straightening pincushion distortion zooms in.
  - Values outside −1..1 are refused.
- **UI test `aiCutoutControls`:** the Lens row is on Video › Enhance. Typing 40 sets 0.4, and it is one undo step.

## Windows portable build — 2026-10-10, edit history list and editing proxies on import (0.6 development)

[GitHub Actions run 38026400187](https://github.com/HNXS/Cutlery/actions/runs/38026400187) on commit `207d33a57f41f5114c46c3b14f37083316fb9870` passed every step on its first attempt.

New coverage:

- **Engine test `historyList`:**
  - The steps read "Start", "Project settings", "Added Title", "opacity of Title", "Split Title" and "Markers", with their offsets from the current step.
  - Jumping three steps back and then two forward restores those states.
  - Asking for more steps than there are is refused.
- **Engine test `proxies`:** with "proxiesOnImport" on, an imported 640 × 1200 video gets a proxy, and the 720p one does not.
- **UI test `keyframeControls`:** the History list shows every step. A click on the step before goes back, and a click on the current step goes forward again.

## Windows portable build — 2026-10-10, several open projects (0.6 development)

[GitHub Actions run 38024566775](https://github.com/HNXS/Cutlery/actions/runs/38024566775) on commit `0ee885121a49a5cfc74163c3b5f60c12f6390068` passed every step on its first attempt.

New coverage:

- **Engine test `openProjects`:**
  - An untouched project is replaced, not kept.
  - A second project has its own timeline and history.
  - Switching back restores the clips, selection, playhead and undo.
  - Opening a project that is open switches to it.
  - Closing another project, or the shown one.
  - At most 8 projects are open, and closing the last one leaves a new empty one.
- **Engine test `keyPickerMarkersStabilizeAndSound`:** opening the file of the shown, changed project is refused until that project is closed.
- **UI test `keyframeControls`:** the tabs appear for a second project. A click switches projects and × closes one.

## Windows portable build — 2026-10-09, frame rate of a project with clips (0.6 development)

[GitHub Actions run 37993929900](https://github.com/HNXS/Cutlery/actions/runs/37993929900) on commit `99f40dfa6999872eb618ac022d7333ace5dbaa86` passed every step on its first attempt.

New coverage:

- **Engine test `frameRateChange`:**
  - From 25 to 30 fps, these keep their times: touching clips (still touching), keyframes, the transition, caption words, markers, the export range and the total length.
  - At 29.97 fps a clip is not run past its 2 s of media.
  - At 10 fps, markers that meet on one frame keep the first.
  - In the editor the change is one undo step, and the playhead keeps its time.
- **UI test `keyframeControls`:** Project settings opens on 160 × 90 and 30 fps. Choosing 60 doubles the clip's frames, and undo puts back 30.

## Windows portable build — 2026-10-09, editing proxies (0.6 development)

[GitHub Actions run 37978486157](https://github.com/HNXS/Cutlery/actions/runs/37978486157) on commit `ccf71e3856943dbb7306aa5c56c1540d88a2d480` passed every step on its first attempt.

New coverage:

- **Engine test `proxies`:** a 720p video gets a 540-line proxy with sound in the data folder. A blue file put in the proxy's place shows in the preview, and the red original shows again once proxies are off. Asking again, or for every video over 1080p, is refused. The project does not mention proxies, and deleting removes the proxy.
- **UI test `aiCutoutControls`:** the library's "Make an editing proxy" refuses a video that is small enough. The player's Proxy button stays hidden while there is no proxy.

## Windows portable build — 2026-10-09, category column, shape sections, transition length (0.6 development)

[GitHub Actions run 37929386650](https://github.com/HNXS/Cutlery/actions/runs/37929386650) on commit `241692784c187b8719ba33d9785eef4d35aef77f` passed every step on its first attempt.

New coverage:

- **UI test `aiCutoutControls`:** the category column scrolls the Audio tab down to Transitions and back to the top. The Text tab has no column.
- **UI test `transitionMarkers`:** typing 1 s into the transition length box stores 30 frames.

## Windows portable build — 2026-10-09, sound tiles, Added mark, sections for keys, tilt, mask and titles (0.6 development)

[GitHub Actions run 37924472403](https://github.com/HNXS/Cutlery/actions/runs/37924472403) on commit `07a784530eb3b1d4c196b3303a3156a104278199` passed every step on its first attempt.

New coverage:

- **UI test `aiCutoutControls`:** the Mouse click tile in the Audio tab adds one clip at the playhead in one undo step. The media used on the timeline shows its "Added" mark.
- **UI test `presenterControls`:** green screen and perspective are switched on with their section boxes. The brightness key's sliders appear once it is on. 3D tilt sets the turn from its value row.
- **UI test `textStylesAndAdjustment`:** the title template choice works inside the new Style section.

## Windows portable build — 2026-10-09, player bar and start screen (0.6 development)

[GitHub Actions run 37921224959](https://github.com/HNXS/Cutlery/actions/runs/37921224959) on commit `298955a57f3feaaa052c6adadb8f8b889fd5e27b` passed every step on its first attempt.

New coverage:

- **Start screen:** a click on the "New project" banner starts a project and closes the start screen.
- **Player bar:** the time, the frame and play buttons and the frame shape are found by their names in the UI tests.
- **Screenshots:** with `CUTLERY_UI_SHOTS` set, the start screen is saved as well.

## Windows portable build — 2026-10-09, timeline tool row, track header icons, clip colours, top bar (0.6 development)

[GitHub Actions run 37909749864](https://github.com/HNXS/Cutlery/actions/runs/37909749864) on commit `cfb90d968b24c2d533be4d7b5c7bceb34969de60` passed every step on its first attempt.

New coverage:

- **UI test `aiCutoutControls`:** the tool row splits the selected clip at the playhead and cuts away everything after the playhead (the clip becomes 20 frames long). Mirror sets flip. Each is one undo step.
- **UI test `dragDropAndMagnet`:** works with the lower tracks (68 px) and turns the magnet on with the new track icon.

## Windows portable build — 2026-10-09, asset panel in icon tabs (0.6 development)

[GitHub Actions run 37906758782](https://github.com/HNXS/Cutlery/actions/runs/37906758782) on commit `07f98ce5f874c2f69098a3344721f3dbf0132b7d` passed every step on its first attempt.

New coverage in the UI test `aiCutoutControls`:

- **Effects tab:** opens from its tab button. The Glitch tile sets the selected clip's effect and is framed as the current choice.
- **Filters tab:** the Warm look tile sets the temperature, and undo puts it back.
- **Transitions tab:** the transition tiles stay disabled for a clip that does not follow another.
- **Other tests:** the tests that use the old Add tab now open the Effects, Stickers or Text tab first.

## Windows portable build — 2026-10-09, inspector sections and value rows (0.6 development)

[GitHub Actions run 37905156426](https://github.com/HNXS/Cutlery/actions/runs/37905156426) on commit `e96734f7f932b8026d473d59df21d4ba41469d0f` passed every step on its first attempt.

New coverage in the UI test `aiCutoutControls`:

- **Number box:** typing 150 into the scale box sets the scale to 1.5, and the box then shows "150%".
- **Reset:** ↺ on Transform resets the scale in one undo step.
- **On/off box:** switching Stabilize on enables its strength; undo switches it off again.
- **All other UI tests:** they pass with the new sections and value rows; the object names they use are unchanged.

## Windows portable build — 2026-10-09, inspector in tabs and sub-tabs (0.6 development)

[GitHub Actions run 37901996536](https://github.com/HNXS/Cutlery/actions/runs/37901996536) on commit `24203962a4b3ce270b99ca6669879fcd5d80e532` passed every step on its first attempt.

New coverage:

- **UI test `aiCutoutControls`:**
  - A video without sound gets the tabs Video, Speed, Animation, Adjust, Effects and More.
  - Clicking "Speed" shows the freeze controls.
  - The sub-tab chosen under Video is kept after visiting another tab.
  - With nothing selected, the project details are shown and there are no tabs.
- **Every other UI test:** each one opens the page its controls are on, so all of them work through the new tabs.

## Windows portable build — 2026-10-09, free masks, 10-bit and HDR export, less room echo (0.6 development)

[GitHub Actions run 37885377037](https://github.com/HNXS/Cutlery/actions/runs/37885377037) on commit `0c17881c0571da28d5a6ff264b6e3ecfb5c55393` passed every step on its first attempt. The FFmpeg check now also requires `sidechaingate` and `asplit`.

New coverage:

- **Engine test `freeMasksDeepColourAndRoomEcho`:**
  - **Masks:** points clicked through the editor make a mask from the third one on, and the outline on the canvas matches the clicks. Only the inside is shown; the smooth curve bulges between points; "Invert" hides the inside. Taking points away removes the mask and undo brings it back; broken masks are refused.
  - **AV1 HDR10:** 10-bit, marked PQ / BT.2020, with the timeline's white at about 203 nits (Y ≈ 572).
  - **VP9 10-bit:** BT.709, with red at its BT.709 brightness (Y ≈ 250). HDR in MPEG-4 is refused.
  - **Less room echo:** in an exported WAV the echo after a word is more than 10 dB quieter and the word less than 3 dB quieter.
- **UI tests:**
  - **`exportDialog`:** the colour choice only for formats that hold 10 bits.
  - **`aiCutoutControls`:** three clicks on the preview after "Draw mask" make a mask, and "Remove" clears it.

## Windows portable build — 2026-10-09, HDR to SDR tone mapping, source colour range and matrix, video noise and flicker reduction (0.6 development)

[GitHub Actions run 37881969826](https://github.com/HNXS/Cutlery/actions/runs/37881969826) on commit `0373610cb5af2ff7ddd3d78a288daec368c136a2` passed every step on its first attempt. The FFmpeg check now also requires `setparams`, `zscale`, `tonemap`, `atadenoise` and `deflicker`.

New coverage:

- **Engine test `sourceColoursAndNoise`:**
  - **HDR:** an HDR10 (PQ) file is recognised on import; tone-mapped it is brighter than the unconverted signal, and "Bright" differs from "Natural".
  - **Range:** a limited-range black marked as full becomes true black when read as limited.
  - **Matrix:** BT.709 gives back the pure red that BT.601 changes.
  - **Video noise:** noise between neighbouring pixels is reduced by more than 30 %.
  - **Flicker:** the brightness jump between two frames is at least halved.
  - The settings are saved, and out-of-range values are refused.
- **UI test `aiCutoutControls`:** the new controls show for a video and the HDR choice stays hidden for SDR; choosing "Full" range sets the clip and undo restores it. A picture clip does not show them.

## Windows portable build — 2026-10-08, even loudness, noise learned at the playhead, session log and unclean-exit notice (0.6 development)

[GitHub Actions run 37830678750](https://github.com/HNXS/Cutlery/actions/runs/37830678750) on commit `348d89e52d5525cba0fd02accedba2d738aefa0c` passed every step on its first attempt.

New coverage:

- **Engine test `evenLoudnessAndNoise`:**
  - **Even loudness:** a loud and a quiet tone are brought to the same loudness, so the quiet one gets about ten times the volume of the loud one. One undo restores both, and an impossible target is refused.
  - **Learn noise:** sets a noise floor between −50 and −35 dB and switches denoising on at 0.4. A title is refused.
- **Engine test `uncleanExitAndLog`:**
  - A lock left by another process is reported as an unclean exit, and the notice can be dismissed.
  - Errors and the unclean exit are written to `logs/cutlery.log`.
  - A second editor in the same process does not report an unclean exit.

## Windows portable build — 2026-10-08, highlighted title words, right lower third, banner and quote, four icons (0.6 development)

[GitHub Actions run 37798527810](https://github.com/HNXS/Cutlery/actions/runs/37798527810) on commit `57ec262f9a0ed1b2787a7cc061342c3eb3fbf226` passed every step on its first attempt.

New coverage:

- **Engine test `titlesThatBuildUp`:** a word between asterisks is drawn red on its own line while the other stays white; in a white highlight the marked title looks exactly like the unmarked one; asterisks inside words stay; a typewriter build-up ends with the same text.
- **Engine test `titleTemplates`:** the right lower third mirrors the left one with its bar on the right edge; the banner spans the full width at the bottom; the quote is centred with its accent quotation mark; the editor adds a quote and refuses unknown templates.
- **Engine test `builtInIcons`:** play, bell, pin and clock render in their colours, with the play triangle, the pin's hole and the clock's hand cut out.
- **UI test `presenterControls`:** title styles chosen from the inspector, including the quote, and a banner added from the Add tab.

## Windows portable build — 2026-10-08, drop/spin/fade letter animations, favourite fonts, numbered steps (0.6 development)

[GitHub Actions run 37791770850](https://github.com/HNXS/Cutlery/actions/runs/37791770850) on commit `dab5de72a3bcadb8ce6a32001643b90ec4ecc594` passed every step on its first attempt.

New coverage:

- **Engine test `titlesThatBuildUp`:**
  - **Drop:** the first dropping letter is above its place.
  - **Spin and fade:** both show few letters early.
  - All three animations end with the finished text.
- **Engine test `fontFavorites`:** a starred font comes first in the font list and stays starred at the next start; a star on a font that is not installed changes nothing; stars are removed again.
- **Engine test `shapes`:** two numbered steps are numbered 1 and 2 and named after their number; a step is a red disc with its white number.
- **UI test `textStylesAndAdjustment`:** the star beside the font list stars the selected title's font and turns filled.

## Windows portable build — 2026-10-08, colour wheels, speed ramps, own layouts (0.6 development)

[GitHub Actions run 37784160751](https://github.com/HNXS/Cutlery/actions/runs/37784160751) on commit `3e5c0e808413ad6216aabd29c827d15c7d8913dc` passed every step on its first attempt. Three earlier commits of this change did not compile with MSVC; each was fixed in the next commit:

- `QJsonArray` was not included in `Editor.h`.
- The clip value setter's else-if chain had grown past MSVC's nesting limit; its plain number fields now come from a lookup table.
- `<numbers>` was not included in the engine tests.

New coverage:

- **Engine test `colourWheels`:** red into the shadows tints the dark grey and leaves the light one; blue into the highlights tints the light grey only; midtones change mid-grey; values are saved and validated.
- **Engine test `speedRamps`:** flash in cuts the clip and its linked sound into eight back-to-back parts with falling speeds, the source running on; the title after the clip follows; hero makes the clip longer; unknown presets and titles are refused.
- **Engine test `savedLayouts`:** a layout is saved from two arranged pictures and applied to two others in order, border included; undo, too many clips and unknown names; kept for the next start and removed.
- **UI tests:** the highlights wheel keeps its point inside the circle; a layout is saved through its dialog and applied from the list.

## Windows portable build — 2026-10-08, undo across sessions, freeze frames of any length, 3D tilt (0.6 development)

[GitHub Actions run 37776488667](https://github.com/HNXS/Cutlery/actions/runs/37776488667) on commit `596ece7516a5f4871181e0b1fb29455557305bd3` passed every step on its first attempt.

New coverage:

- **Engine test `undoAcrossSessions`:** after saving and opening again, redo and undo continue through the earlier edits back to the empty project; a file changed elsewhere opens without history.
- **Engine test `freezeFrame`:** a reversed clip freezes the frame it shows (source frame 49 at the 11th frame) for half a second, and the still is not reversed.
- **Engine test `styleEffects`:** leaning a grey picture back narrows its top edge; turning it shortens its right side; tilt values are saved and validated.
- **UI test `lookControls`:** the turn slider sets the tilt.

## Windows portable build — 2026-10-08, corner pin, library search by speech and metadata, usage rights (0.6 development)

[GitHub Actions run 37766665238](https://github.com/HNXS/Cutlery/actions/runs/37766665238) on commit `a61771356b21efb59ecd9022cb5f70c0eff4c1fc` passed every step on its first attempt.

New coverage:

- **Engine test `styleEffects`:** a corner pin narrows the top of the picture to a trapezoid with black canvas outside, keeps the bottom at full width and squeezes the top row of bars; corners left in place change nothing; corners are saved and validated.
- **Engine test `rightsAndMediaSearch`:**
  - **Usage rights:** recorded with a trimmed credit line; the check names media for personal use on the timeline and counts the credits; the credits file is written; saving, undo and redo work.
  - **Search:** media are found by name, picture size, credit and kind, and by a word said in a cached transcript, with the time of that word; every search word must match.
- **UI tests:**
  - `lookControls`: the corner pin is switched on and one corner moved with its slider.
  - Library test: rights set in the rights dialog are then found by the library search.

## Windows portable build — 2026-10-08, canvas fill, sketch/poster/fisheye/mirror effects, voice changer (0.6 development)

[GitHub Actions run 37761592181](https://github.com/HNXS/Cutlery/actions/runs/37761592181) on commit `8ec6ee07495b47ddd37d76f1962c6ca9617b1602` passed every step on its first attempt.

New coverage:

- **Engine test `styleEffects`:**
  - **Sketch:** dark outlines between the bars on white.
  - **Poster:** at full strength only 0 or 255 per channel.
  - **Fisheye:** the centre kept at the same picture size.
  - **Mirror:** the right half reflects the left.
  - **Canvas fill:** black by default, a colour, or the blurred picture around a half-size clip, also during a transition; saved and validated.
- **Engine test `voiceChanger`:** the telephone voice drops a 100 Hz tone by more than 20 dB and keeps 1 kHz; every voice is audible; chipmunk and monster shift by seven semitones.
- **UI test `lookControls`:** the mirror effect from the effect list and the background box (blurred picture, a custom colour).

## Windows portable build — 2026-10-08, ellipse and soft-edged blur areas, brand kit, LUT library (0.6 development)

[GitHub Actions run 37751884809](https://github.com/HNXS/Cutlery/actions/runs/37751884809) on commit `c63d63bea412864dc3cedc848cad11d90ddb696e` passed every step on its first attempt.

New coverage:

- **Engine test `blurAndMosaic`:** ellipse blur and mosaic areas leave the rectangle's corners sharp; a soft edge fades the effect; the shape is saved and validated.
- **Engine test `brandKitAndLutLibrary`:**
  - **Brand colours:** normalised, without duplicates, kept for the next start and removable.
  - **Logo:** copied into the data folder; "+ Logo" puts it over the whole video on a new top track, about an eighth of the height, 3 % from the chosen corner, reusing its media; undo works.
  - **LUT library:** files are copied into luts/, listed by name, numbered on a name clash and set on a clip from the list.
- **UI test `presenterControls`:** the area shape and soft edge controls, and brand colour swatches added and removed in the Add tab.

## Windows portable build — 2026-10-07, motion presets, whites and blacks, convert or compress, lower thirds without slide (0.6 development)

[GitHub Actions run 37678983296](https://github.com/HNXS/Cutlery/actions/runs/37678983296) on commit `f3cfddfa648ea09b57b493a4678729ad8f0b9fa1` passed every step on its first attempt.

New coverage:

- **Engine test `motionsTonesSlideAndConvert`:**
  - **Motion presets:** pop in, slide in, pulse, wiggle and pop out set the expected keyframes and easing around the clip's own values; "remove motion" returns the clip to them; undo works.
  - **Whites and blacks:** they deepen or brighten the ends of the tone range, or fade and soften them.
  - **Lower thirds:** with sliding switched off, they only fade.
  - **Convert or compress:** a library video is converted at its own size and frame rate, with sound, without touching the timeline; a picture is refused.

## Windows portable build — 2026-10-07, keyframe easing, MXF/MTS/MPEG/DV import, AVIF and HEIC pictures, collage fill (0.6 development)

[GitHub Actions run 37625847330](https://github.com/HNXS/Cutlery/actions/runs/37625847330) on commit `2c2bf12adcee72134f3b90c7615ccfd4af5f3191` passed every step on its first attempt.

New coverage:

- **Engine test `keyframeEasingFormatsAndFill`:**
  - **Keyframe easing:**
    - every easing gives the expected value part-way between two keyframes;
    - a moving picture renders where each easing puts it;
    - the easing of the keyframe at the playhead is set in the editor;
    - files store it compatibly with older versions, and unknown easings are refused.
  - **Formats:** MPEG-2 in MXF, MTS and VOB imports, plays and exports; an AVIF picture is imported as a still.
  - **Collage fill:** two side-by-side pictures are cropped to fill their halves, and fitting again removes the crop.

## Windows portable build — 2026-10-07, spin, glitch and light-leak transitions, undo steps, unreadable thumbnails (0.6 development)

[GitHub Actions run 37621086944](https://github.com/HNXS/Cutlery/actions/runs/37621086944) on commit `bc3e218c34e3841ca06af7c39a297509246136e8` passed every step on its first attempt.

New coverage:

- **Engine test `ownTransitionsUndoStepsAndBrokenThumbnails`:**
  - **Transitions:**
    - outside the transition, spin, glitch and light leak leave the clips unchanged;
    - at the cut, the light leak is far warmer than a plain dissolve;
    - spin exposes the corners part-way through and is upside down at the cut;
    - glitch differs from the dissolve around the cut.
  - **Undo steps:** Undo goes back exactly the configured number of steps, and invalid values are refused.
  - **Thumbnails:** a damaged thumbnail strip in the cache is extracted again.

The previous commit on `main` was built and uploaded as a portable package and AI pack in [run 37620230177](https://github.com/HNXS/Cutlery/actions/runs/37620230177) (manual run with upload). Those artifacts expire on 2026-10-10.

## Windows portable build — 2026-10-07, overwrite, quick captions, caption layout, subtitle file beside the export (0.6 development)

[GitHub Actions run 37600363235](https://github.com/HNXS/Cutlery/actions/runs/37600363235) (attempt 3) on commit `dd988d0fff2569bda8c62a7ea784857d08337349` passed every step. Attempts 1 and 2 never started: GitHub refused the job because the Actions spending limit was reached. No build or test step ran in them. The repository was then made public, and attempt 3 ran.

New coverage:

- **Engine tests (`captionLayoutOverwriteAndSidecar`):**
  - **Caption layout:** lines break after a comma past 60 % and at the length limit, two lines are balanced, and text that fits stays as it is.
  - **Quick captions:** each lasts 2 s or runs up to the next caption, and a caption at an occupied playhead is refused.
  - **Subtitle file beside the export:** an in/out export with `captions: srt` writes an SRT named like the video, with the title and captions cut and shifted to the range; an unsupported caption format is refused.
  - **Overwrite at the playhead:**
    - the new clip replaces exactly its time on the track, and undo restores the original;
    - on a magnetic track nothing slides and the magnet stays on.
- **Interface:** the export dialog's "Captions" list feeds the choice.

## Windows portable build — 2026-10-07, key colour picker, cutting on markers, stabilize strength, mono and 44.1 kHz export (0.6 development)

[GitHub Actions run 37596464638](https://github.com/HNXS/Cutlery/actions/runs/37596464638) on commit `bb9c3bf4e28e6f07c5e072b5e7d78b2fadba126c` passed every step on its first attempt.

New coverage:

- **Engine tests (`keyPickerMarkersStabilizeAndSound`):**
  - **Key colour picker:**
    - a click on an uneven green screen in the viewer picks that colour and turns the key on, in one undo step;
    - a click outside the picture is reported.
  - **Markers:**
    - "Split at markers" cuts a clip at every marker inside it;
    - "Cut on the beat" ends each selected clip on the marker nearest to its end, within what its media reaches;
    - a missing selection or missing markers is explained.
  - **Stabilizing:** strength 1 with "Zoom in" uses a 64-pixel search and crops the edges, and the default stays at 32 pixels; the settings are saved.
  - **Sound format:** WAV and M4A exports in mono at 44.1 kHz; invalid channel counts are refused.
- **Interface:**
  - "Pick" turns the viewer into a colour picker until it is pressed again;
  - the export dialog's "Sound" list sets channels and sample rate.

## Windows portable build — 2026-10-07, pitch, exposure, transparent ProRes 4444 and PNG sequence export (0.6 development)

[GitHub Actions run 37590197487](https://github.com/HNXS/Cutlery/actions/runs/37590197487) on commit `df7b8062958687a2a0c4ae4b3c9434f7177c2ff1` passed every step on its first attempt.

New coverage:

- **Engine tests:**
  - `pitchAndExposure`:
    - pitch: an exported 440 Hz tone rises to about 880 Hz at +12 semitones and drops by the expected amount at −5, each time at the same length;
    - exposure: mid grey becomes brighter at +1 stop and darker at −1;
    - both are saved and validated.
  - `transparentExports`:
    - a title-only ProRes 4444 export has an alpha stream, transparent around the letters;
    - a PNG sequence lands in a new folder named like the file, one transparent picture per frame, with no work folder left behind;
    - an existing folder of that name is refused, and MPEG-4 stays opaque.
- **Interface:** the export format list includes the two new formats.

## Windows portable build — 2026-10-06, soft mask edges, export frame rate and bitrate, export retry, cache limit (0.6 development)

[GitHub Actions run 37534841625](https://github.com/HNXS/Cutlery/actions/runs/37534841625) on commit `8ecec29520c1af75bd648d84a7972f99f59a0f2f` passed every step on its first attempt.

New coverage:

- **Engine tests:**
  - `softEdgeMask`:
    - a feathered picture is solid in the middle, shows the background at its border and changes gradually in between;
    - without feather the edge is sharp, and a feathered circle keeps its corners clear;
    - the setting is saved and validated.
  - `exportRateBitrateAndRetry`:
    - a set bitrate replaces the quality options of every H.264 candidate, and ProRes is left alone;
    - VP9 at 29.97 fps uses `30000/1001`;
    - a real MPEG-4 export at 50 fps from a 25 fps edit has 100 frames;
    - invalid rates and bitrates are refused;
    - an export into a missing folder fails and succeeds when tried again once the folder exists.
  - `cacheLimitAndClearing`:
    - the cache size is reported, and the limit is validated;
    - "empty at next start" clears the waveform and thumbnail caches on the next start;
    - a work folder from the last hour is kept.
- **Interface:** the export dialog's frame-rate and bitrate lists change the choice and the preset; bitrate is off for ProRes.

## Windows portable build — 2026-10-06, command search, folder import, clip sound, text glow (0.6 development)

[GitHub Actions run 37530257964](https://github.com/HNXS/Cutlery/actions/runs/37530257964) on commit `ed6b488532fed102b1262de13cf7415f01ab79be` passed every step on its first attempt.

New coverage:

- **Engine tests:**
  - `folderImportAndStop`:
    - a folder's media, folders inside included, is imported in path order into a library folder of its name, and other files are skipped;
    - importing the folder again reuses that library folder, and an empty folder is reported;
    - a folder dropped on a track places its clips one after another;
    - stopping skips the file being read and those waiting, and importing works again afterwards.
  - `extractClipAudio`: the sound of a clip at 2× speed is saved as a 2-second MP3 without video; a wrong file type and a title are refused.
  - `textGlow`: the glow shows in its colour, weaker at a lower strength; it is saved, validated and kept in text styles.
  - `keyboardShortcuts` now counts 45 commands.
- **Interface (`commandSearch`):**
  - Ctrl+K opens the search with the field focused;
  - menu commands are found with their menu, and keyboard commands with their key;
  - Enter runs the first match, and a menu command opens its dialog.

## Windows portable build — 2026-10-06, blend modes, luma key, edge crop, TXT captions (0.6 development)

[GitHub Actions run 37526553602](https://github.com/HNXS/Cutlery/actions/runs/37526553602) on commit `07dc295541b76ae8106ae7ebacfdfe6a6fbcb201` passed every step on its first attempt.

New coverage:

- **Engine tests:**
  - `cropFlipBlendAndLumaKey`:
    - cropping single edges changes the picture's shape, and the picture is fitted with that shape;
    - upside down, alone and together with flip;
    - multiply and screen give the expected colours, also at half opacity and with keyframed scale;
    - the luma key removes black or white, and a transparent part of the image stays transparent;
    - the settings are saved, invalid values are refused, and the opposite crop edge gives way in the editor.
  - `plainTextCaptions`: a text file becomes captions, one per line, with long lines split after sentences, at most two lines of 42 characters, timed for reading and placed at the playhead.
  - `recentProjectsAndBackups`: project versions keep their order even when saved in the same millisecond, or when a version carries a later time.
- **Interface:** the blend-mode list, the brightness key with its sliders, and "Upside down".

The previous run on this PR ([run 37525374969](https://github.com/HNXS/Cutlery/actions/runs/37525374969)) failed in `recentProjectsAndBackups`. It was the intermittent failure also seen on PRs 25 and 26: two saves in the same millisecond were listed in the wrong order. That is fixed in this PR.

## Windows portable build — 2026-10-06, GIF, still frames, OTIO and EDL (0.6 development)

[GitHub Actions run 37520964820](https://github.com/HNXS/Cutlery/actions/runs/37520964820) on commit `1c1f9b0bbe0f8dc6a565a6123a130f47d87bb278` passed every step on its first attempt.

New coverage:

- **Engine tests:**
  - `gifExport`:
    - an animated GIF is written with a palette, the chosen frame rate and no audio;
    - the frame at the playhead is saved as PNG and JPEG at project size.
  - `timelineInterchange`:
    - OpenTimelineIO output with tracks, gaps, clips, speed changes and markers;
    - CMX 3600 EDL with video and audio events, speed (M2) lines and clip names;
    - effects that cannot be carried are listed in the loss report.
- **Locally**, both files were also read with the official OpenTimelineIO 0.18.1 library and its CMX 3600 adapter.

## Windows portable build — 2026-10-06, layouts, templates, time-lapse up to 100x (0.6 development)

[GitHub Actions run 37511355410](https://github.com/HNXS/Cutlery/actions/runs/37511355410) on commit `e4f2088a32a9dd935a6cca3ce255c7bfceab91ec` passed every step on its first attempt.

New coverage:

- **Engine tests:**
  - `anchorPointAndSpeedRange`:
    - each frame of a fast clip shows the source at exactly frame × speed: frames 10 and 20 at 10x, and frame 60 at 60x;
    - the sound of a 60x clip is 1/60 as long;
    - 100x is the upper limit.
  - `layouts`:
    - side by side;
    - picture in picture with the background full;
    - presenter with a round picture in the lower right;
    - one undo step, and back to full size.
  - `projectTemplates`:
    - a template is saved and a new unsaved project starts from it, with the same clips and canvas;
    - the new project stays out of the recent list and does not change the template;
    - templates can be removed.
- **Interface:**
  - the start screen lists templates;
  - the layout buttons need two selected clips.

## Windows portable build — 2026-10-06, adjustment layers, text styles, icons (0.6 development)

[GitHub Actions run 37470489904](https://github.com/HNXS/Cutlery/actions/runs/37470489904) on commit `3d31c1bf17e234a7a01bb6b4576af87d9a34ef10` passed every step on its first attempt.

New coverage:

- **Engine tests:**
  - `adjustmentLayers`:
    - an adjustment layer greys the picture below only while it runs;
    - at half strength it changes the colour by half;
    - it is saved, and unknown effects are refused.
  - `textStylesKit`:
    - a title's style is saved and applied to two titles in one undo step;
    - saving under the same name replaces it;
    - styles survive a restart and can be removed.
  - `builtInIcons`:
    - all nine icons render square and filled in their colour;
    - the warning sign's exclamation mark is cut out.
- **Interface:**
  - a style is saved through its dialog and applied from the list;
  - an adjustment layer from the Add tab shows its strength and the look controls.

## Windows portable build — 2026-10-06, edit by text, filler words, curves, selective and auto colour (0.6 development)

[GitHub Actions run 37457382153](https://github.com/HNXS/Cutlery/actions/runs/37457382153) on commit `f6fcd9304f343eac669eba32884fc80c3355b3aa` passed every step.

The first commit of this change, `4765a01`, did not compile with MSVC. A signed revision number was braced into an unsigned field, which MSVC rejects as narrowing and GCC allowed. CI now reports the first compiler errors as annotations.

New coverage:

- **Engine tests:**
  - `transcriptEditing`:
    - cut ranges for chosen words;
    - filler detection, which leaves the German "er" and "eh" alone;
    - a cached transcript mapped onto a clip with detached sound;
    - cutting words and fillers across the pieces the cuts leave;
    - one undo step each.
  - `curvesSelectiveAndAutoColour`:
    - curve validation;
    - the correction formula;
    - auto colour on a dark, blue clip widens its range and reduces the cast;
    - a halving master curve and the selective greying of blues render as expected;
    - the settings are saved, and invalid values are refused.
- **Interface:**
  - choose the language, click two words, cut them, and double-click a word to seek;
  - drag a point on the tone curve;
  - switch the curve channel;
  - choose colours for the selective change.

## Windows portable build — 2026-10-05, media folders, start screen, relink from a folder, nested sequences (0.6 development)

[GitHub Actions run 37370014194](https://github.com/HNXS/Cutlery/actions/runs/37370014194) on commit `20eacc6b5da5b769e1e78a252f7fbe5aca40cd73` passed every step.

The same code had failed once before, on commit `0911802`, in run 37363562214, attempt 2. Its first attempt found no runner. In attempt 2 a test failed in "Test model and actual media rendering". The failing check could not be identified, because only the job log names it. CI now reports each failing check as an annotation, so a repeat will show which test it is.

New coverage:

- **Engine tests:**
  - `mediaFolders`:
    - folders are created, renamed and deleted (their media stays);
    - imports go into the folder on show;
    - media can be moved between folders;
    - folders are saved and checked on load;
    - only unused media can be removed;
    - pasted media lands at the top level.
  - `appPreferences`:
    - invalid settings are refused;
    - new projects take the default format;
    - still images take the set length;
    - "0 earlier versions" keeps none;
    - the settings survive a restart.
  - `relinkMissingFromFolder`:
    - missing files are found by name, in any case and at any depth;
    - among duplicates, the one in the most similar folder wins;
    - all are relinked in one undo step.
  - `nestedSequences`:
    - nesting puts the clips into one clip, rendered in the background and showing the nested picture;
    - the sequence opens with its own undo history and exports wait while it is open;
    - saving from inside writes the whole project, with relative paths;
    - going back is one undo step;
    - taking it apart restores the clips;
    - only normal-speed sequences can be taken apart.
- **Interface:**
  - library search, kind and folder views;
  - the start screen opens a 9:16 project, and the preferences dialog saves its settings;
  - double-clicking a nested clip opens it, and Back returns.

## Windows portable build — 2026-10-05, rubber band, export queue, reframing, letter animations (0.6 development)

[GitHub Actions run 37324201212](https://github.com/HNXS/Cutlery/actions/runs/37324201212) on commit `09ebdc5b7e3ef0e64de74c5092791e671bd44ed3` passed every step.

New coverage:

- **Engine tests:**
  - `multipleSelectionAndGroups`: rubber-band selection takes the clips it touches and their groups; copy and paste of several clips keeps their spacing and gives pasted groups new ids.
  - `clipboardAndAudioExport`: the export queue rejects duplicates and wrong extensions, keeps a running job, and renders each job from the timeline as it was when queued, after the clip was deleted.
  - `reframeToVertical`: a 16:9 video fills a 9:16 canvas, edge to edge with no black bars; titles and a corner picture keep their place; one undo step restores the wide canvas.
  - `reframeFollowsFace` (AI pack): a face moving across the frame gives position keyframes that follow it at the expected speed and keep it centred.
  - `titlesThatBuildUp`: letters rise from below, pop up and fly in from the right, and end as the finished text; a playthrough from the middle of the animation has every frame; gradient text runs from white at the top to red at the bottom.
- **Interface:** dragging over empty timeline selects clips; the export dialog lists queued jobs and removes a waiting one; Reframe for… gives 1080 × 1920 and 1080 × 1350.

## Windows portable build — 2026-10-05, multiple selection and groups, titles that build up, pan keyframes (0.6 development)

[GitHub Actions run 37303368450](https://github.com/HNXS/Cutlery/actions/runs/37303368450) on commit `379eb83b66a4bd46aa86c159f0dea623833d5df7` passed every step on its second attempt. The first attempt stopped while installing Qt, a download from the Qt mirror, before any test ran.

New coverage:

- **Engine tests:**
  - `multipleSelectionAndGroups`: select several clips, move them together and undo in one step; group, save, select or toggle a whole group, delete a group, ungroup, select all.
  - `titlesThatBuildUp`: a typewriter title shows more of the text at each step and ends as the whole title; word by word matches.
  - `reverbAndEcho`: an animated pan moves the sound from left to right.
- **Interface:** Ctrl+click adds a clip to the selection, and dragging moves both selected clips; slip, slide and roll now press the middle of the edge.

## Windows portable build — 2026-10-05, slip/slide/roll, linked A/V, anchor, GIF and SVG (0.6 development)

[GitHub Actions run 37290506341](https://github.com/HNXS/Cutlery/actions/runs/37290506341) on commit `51f6cf5c6b7fbca26ce905f7dcc52ed21704bdf7` passed every step, including the local build script. Qt SVG, now a build dependency, comes with the standard Qt installation, so the Qt install step needed no extra module.

New coverage:

- **Engine tests:**
  - `anchorPointAndSpeedRange`:
    - a corner anchor and an animated zoom towards the bottom-right corner keep that corner in place;
    - the rotation maths;
    - the anchor is saved only when moved;
    - a 10× clip shows every tenth source frame and has a tenth of the audio;
    - 0.1× is the lower limit.
  - `gifAndSvgOverlays`:
    - a 1 s GIF loops correctly over a 4 s clip;
    - an SVG becomes a 2048 × 1024 transparent picture with the right colours.
  - `slipRollAndSlide`: the model edits and their limits; slipping in the editor takes detached audio along.
  - `linkedPictureAndSound`: move, trim, split into two linked pairs, save, unlink.
- **Interface test `slipSlideAndRollDrags`:** Alt+drag, Alt+Shift+drag and Alt-drag on an edge in the real timeline.

## Windows portable build — 2026-10-04, freeze frame, transitions, pan, recent projects and versions (0.6 development)

[GitHub Actions run 37206990048](https://github.com/HNXS/Cutlery/actions/runs/37206990048) on commit `409ced2ed4c9fb00116f87cc66ea652fd76166ad` passed every step, including the local build script; nothing was uploaded.

New coverage:

- **`transitions`:** all 29 transition types render, and a frame inside each one differs from both clips.
- **`reverbAndEcho`:** pan fully left silences the right channel; half pan lowers the far side by 6 dB.
- **`freezeFrame`:**
  - the still shows the frame at the playhead and keeps the clip's scale and look;
  - the rest of the clip and of its detached audio starts after the still, at the right source offset;
  - undo removes it all.
- **`recentProjectsAndBackups`:**
  - versions come newest first, with the right contents;
  - restoring keeps the current file as a version;
  - at most 20 versions are kept, and files from other folders are refused;
  - the recent list survives a restart, and a missing project is dropped.

## Windows portable build — 2026-10-04, one-command local build (0.6 development)

[GitHub Actions run 37190925371](https://github.com/HNXS/Cutlery/actions/runs/37190925371) on commit `690d17be44ce8379f44781d69e1fc0ac83289b89` passed every step, and uploaded nothing because uploads are now opt-in.

What passed:

- The usual build, tests, model and speech checks, packaging and the deployed smoke test.
- **`tools/Build-Local.ps1 -SkipTests`**, run after those steps. It reused the downloads and build folder, created its own Python environment for the model conversion, compiled, packaged again into the existing `dist` folder (which needed the `SHA256SUMS.txt` fix in `Package-Windows.ps1`) and started the packaged application.

Not verified: a first run on a clean PC, which includes the full Qt download, the first compilation of whisper.cpp with the Vulkan SDK, and the tests through the script.

## Windows portable build — 2026-10-02, sound effects library (0.6 development)

[GitHub Actions run 37077970890](https://github.com/HNXS/Cutlery/actions/runs/37077970890) on commit `a9f20b637f5f7a885b2dba0b90d25b7a7a8fff48`: every build, test and check step passed. The artifact uploads failed: the older builds had been deleted, but GitHub recalculates the storage quota only every 6–12 hours.

What passed:

- **Sound pack:** `Get-Sounds.ps1` downloaded the six recorded sounds from their pinned commits, and their SHA-256 checksums matched.
- **Engine tests:**
  - `recordedSoundPack`: each recorded sound decodes at its stated length; each one except the typing has its loudest moment where stated; every licence is CC0; a recorded whoosh lands on a transition with its loudest moment at the cut.
  - `soundEffects`: built-in synthesis, WAV files, placement in the editor and the rendered mix.
  - `beatDetection`: now with seeded noise.
- **Interface:** the sound dialog lists the sounds and adds one; `dragDropAndMagnet` passes with the new Sounds button.
- **Packaging:** the deployed folder contains `sounds/sounds.json` with the six recorded sounds.

Not verified: how the sounds sound on Windows speakers; listening in the dialog on Windows.

## Windows portable build — 2026-10-02, effects, beat markers, WebVTT/ASS, scopes (0.6 development)

[GitHub Actions run 37076300232](https://github.com/HNXS/Cutlery/actions/runs/37076300232) on commit `24c0f93d78e2d7fc95d0a3bfdbb4be3016241164`: every build, test and check step passed; the artifact uploads failed because the artifact storage was still full of older builds. Those older builds were deleted afterwards (the newest 0.5.0 portable build and AI pack were kept), so later runs upload again.

What passed:

- **Compile and tests:** compile, CTest (engine and interface), including `editingAndPlayback`, which fails only on the local Linux check without an audio device.
- **New engine tests:**
  - `styleEffects`: shake moves a still picture over time; glitch shifts some frames and leaves others; VHS scanlines; film sepia; no change at strength 0; stabilize renders close to the original; motion blur turns a moving bar grey in single-frame previews.
  - `reverbAndEcho`: reflections after a click, and repeats at 320 and 640 ms.
  - `beatDetection`: synthetic drum tracks at 97, 120 and 174 BPM within 2 % of the tempo and beats within 30 ms; markers on every 2nd beat of a trimmed, moved clip; snapping to a marker.
  - `subtitleFormats`: WebVTT and ASS parsing; SRT, WebVTT and ASS round trips through the editor; FFmpeg reads the exported files.
  - `videoScopes`: histogram, waveform and vectorscope values for a black and red picture, through the frame provider.
- **Checks:** the real-model checks, the speech recognition check, packaging and the deployed smoke test.

Not verified: beat detection on real music; the scopes and effects by eye on Windows.

## Windows portable build — 2026-10-02, sound tools, follow a face, JKL shuttle (0.6 development)

[GitHub Actions run 37053712510](https://github.com/HNXS/Cutlery/actions/runs/37053712510) on commit `f6cef27c63358f0242c0da49331c6bf54c16e032`: every build, test and check step passed, but the artifact uploads failed because the repository's artifact storage quota was full. There are no downloads for this build.

What passed:

- **Compile and tests:** compile, CTest (engine and interface).
- **New engine tests:**
  - `soundTools`: low cut, EQ bands, compressor, gate, and noise reduction (over 6 dB) measured on rendered audio.
  - `followFace`: a face moving at 300 px/s followed within 0.04 of the frame width with the real detector.
  - `shuttlePlayback`.
- **Checks:** the real-model checks (matte, upscale, eye contact), the speech recognition check, packaging and the deployed smoke test.

The workflow now keeps the portable build for 14 days and the AI pack for 7 days (previously 30), so runs fit the quota.

## Windows portable build — 2026-10-02, eye contact, markers, collect, VFR, image sequences (0.6 development)

[GitHub Actions run 37012362029](https://github.com/HNXS/Cutlery/actions/runs/37012362029) completed successfully for commit `9bd5f21851e05af5dae09c7765f8fe9329431f72`.

Toolchain change: `Get-Models.ps1` now takes MediaPipe's face detection, Face Mesh and Iris Landmark models from the pinned mediapipe 0.10.18 wheel and converts them to ONNX on the runner with tf2onnx 1.16.1 and TensorFlow 2.17.1. This step took about 2 minutes, and the AI pack now includes the three models.

New coverage:

- **Engine test `eyeContact`**, with the converted models and a public-domain photo:
  - a face already looking into the camera is left unchanged (under 2 % of the eye width);
  - a sideways gaze is corrected in both eyes;
  - a second pass finds less than two thirds of the first correction left;
  - an editor export changes only the eye region.
- **CI worker check:** the deployed-style worker on the CPU provider applied −0.069 and −0.070 eye widths to the sideways fixture, the same as the local Linux build.
- **Other engine tests:**
  - `markersAndRange`: sorted markers, navigation, renaming, validation; an in/out export of exactly 30 frames and 1 s of audio.
  - `collectProject`: same-named media are numbered; the LUT and an added font are copied; paths are relative; the copy opens after the originals are deleted.
  - `variableFrameRate`: detection, then conversion to a constant-rate ProRes copy and relink.
  - `imageSequences`: run detection with gaps and a `%` in the folder name; a 12-frame ProRes 4444 clip with alpha.
- **Interface:** a marker on the ruler, and the export range choice.

Not verified on CI: eye contact on recorded presenter footage and on DirectML GPUs.

Downloads (expire 2026-11-01):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/37012362029/artifacts/11228648619): 168,066,306 bytes, archive SHA-256 `699d7311180066a041901fcb48e73c2c43d2a403436b1b6111bd597676ae0b39`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/37012362029/artifacts/11228932529): 706,100,616 bytes, archive SHA-256 `9c4a98349fce048cce82832de7435352bf2843e4578259b284e806516c619cfc`.

## Windows portable build — 2026-10-02, colour and look, Whisper on Vulkan, editing tools (0.6 development)

[GitHub Actions run 36999719999](https://github.com/HNXS/Cutlery/actions/runs/36999719999) completed successfully for commit `9407ad93e98961805fcabf17180e92805f670af7`. Toolchain changes:

- **whisper.cpp v1.9.4:** now built with ggml backends as DLLs (Vulkan, plus one CPU backend per x64 level, MSVC OpenMP), using the LunarG Vulkan SDK 1.4.321.1 (installer SHA-256 `baaa4f7c…68bd`) as a build tool.
- **CI cache:** the build is cached by the hash of its scripts.

New coverage:

- **Speech recognition:** the check passes with the new build and asserts the CPU fallback on the GPU-less runner. The deployed smoke test runs the packaged whisper-cli with its backend DLLs.
- **Engine tests:**
  - `colourAndLook`: each control moves rendered pixels as expected; LUTs at full and half strength from a folder with special characters; alpha survives every filter; schema 11 round trip.
  - `clipboardAndAudioExport`: paste to a free track and into a new project; paste look or all; MP3, M4A and WAV exports with no video stream.
  - `sceneDetection`: video and detached audio split at 1 s and 2 s, and a 0.2 s flash ignored.
  - `smoothSlowMotion`: blended in-between frames versus repeats; optical flow renders.
  - `textStyles`: alignment, letter spacing, outline, box and shadow in pixels; a font file added to the data folder.
  - `shapes`: arrow, rotation, outline ring and bubble text in pixels; SRT export excludes shapes; validation.
  - `voiceOverWithoutMicrophone`: the error path when no microphone is present.
- **Interface:**
  - colour and look section, look presets, LUT load and rejection;
  - paste look;
  - shape menu and section;
  - audio formats in the export dialog.
- **`Get-FFmpeg.ps1`** now also requires `colortemperature`, `vibrance`, `curves`, `lut3d`, `cas`, `vignette`, `noise`, `alphaextract`, `scdet` and `minterpolate`.

Not verified on CI: Vulkan inference on a real GPU, and microphone recording.

Downloads (expire 2026-11-01):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36999719999/artifacts/11224285667): 168,000,472 bytes, archive SHA-256 `eeed55fd0c50720406d815c12dd44520825cd73672c9c4b3fd7058b490f36ea9`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36999719999/artifacts/11223514058): 701,994,943 bytes, archive SHA-256 `ec9d7ba1d5e8461a45bdd16b0aaa757ce72c679d3f6e9b679ea4198772384df2`.

## Windows portable build — 2026-10-02, release 0.5.0 with level meters

[GitHub Actions run 36996269492](https://github.com/HNXS/Cutlery/actions/runs/36996269492) completed successfully for commit `d35326ceeceb9c13eed0397a0a1b3d84909789b4`, with the same toolchain as the lower-thirds build. The artifacts are named 0.5.0. New coverage:

- **Engine test `pausesAndLoudness`:**
  - `pcmPeaks` for 16-bit and float PCM, and `parseTruePeak`.
  - "Measure mix" end to end: a test tone measures −33 LUFS with a true peak of −31.5 dBTP, and the result reads as stale after an edit.
- **Interface:** the export dialog's measure button and result text, and the level meter, load without QML warnings.

Downloads (expire 2026-11-01):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36996269492/artifacts/11221554077): 148,512,563 bytes, archive SHA-256 `1d8595e6618487d0deb6e9facb3c24d1df068711be8c5a97b4c1f5474130eaea`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36996269492/artifacts/11222530480): 701,994,869 bytes, archive SHA-256 `c9d252d964f95d390e276250289553609a032cc063c5e95006ca4bcdaf828d0c`.

## Windows portable build — 2026-10-02, lower thirds and title cards (0.5 development)

[GitHub Actions run 36981200885](https://github.com/HNXS/Cutlery/actions/runs/36981200885) completed successfully for commit `a4e48e1eb1b0fab9ee131d79c3a7f8c476f2497c`, with the same toolchain as the blur-and-mosaic build. New coverage:

- **Engine test `titleTemplates`:**
  - Layout: the lower third is anchored in the title-safe lower left, the title card centred, plain titles have no plate, and text scales with the clip.
  - Rendered pixels: the accent bar and plate after the slide-in, the plate still off-screen on the first frame, and movement with the clip's y offset.
  - Schema 10 round trip and validation; `addTitleTemplate`; the preview frame matches the plate.
- **Interface:** "+ Lower third" adds and selects a lower third, and the style list switches it to a title card.

Downloads (expire 2026-11-01):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36981200885/artifacts/11216301264): 148,503,915 bytes, archive SHA-256 `4e5cb3fb760967fe8eb7c224d6ab9c590ae38d242122540dcd9578d1d18ae0a1`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36981200885/artifacts/11216026844): 701,994,868 bytes, archive SHA-256 `c6c90eb0bc5c17aca66630fd6922bad9aca63853a2b392cd4e8a69a2384b9162`.

## Windows portable build — 2026-10-01, blur and mosaic (0.5 development)

[GitHub Actions run 36929907077](https://github.com/HNXS/Cutlery/actions/runs/36929907077) completed successfully for commit `7f67b2047e175c92de8aebac32705f18ac99a0fb`, with the same toolchain as the karaoke-captions build. New coverage:

- **Engine test `blurAndMosaic`**, on a fine checkerboard:
  - A blur area turns its rectangle grey while the surroundings stay sharp.
  - A mosaic area produces flat blocks.
  - A keyframed area moves from the centre to the right edge.
  - Outside its clip time an area has no effect.
  - The clip-wide blur softens the whole picture.
  - Model checks: schema 9 round trip, validation, preview size, `addEffect`, exclusion from SRT export.
- **Interface:** "+ Mosaic area" creates and selects an area, the effect section switches it to blur, and presenter controls stay hidden for areas.
- `Get-FFmpeg.ps1` now also requires `gblur`, `pixelize` and `split`.

Downloads (expire 2026-10-31):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36929907077/artifacts/11195807816): 148,494,268 bytes, archive SHA-256 `a9621cedbd54b018966bbe97b6ab2b5bb37feffcba073d718c135199ef4c28fc`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36929907077/artifacts/11196220307): 701,994,875 bytes, archive SHA-256 `7957c963773a698324e6a03bc1487bc7cc9db2f2bf46c011a24719a9e187cb96`.

## Windows portable build — 2026-10-01, karaoke captions (0.5 development)

[GitHub Actions run 36927521399](https://github.com/HNXS/Cutlery/actions/runs/36927521399) completed successfully for commit `c9104b706b931458b8ffc50a96678659cbbc2542`, with the same toolchain as the automatic-captions build. New coverage:

- **Engine test `karaokeCaptions`:**
  - Word grouping: pauses, sentence ends, line length and lone punctuation.
  - Placement with clip-local word starts; splitting a timed caption between its words; the plain fallback after a word-count change; the schema 8 round trip.
  - Pixel checks on rendered frames: the karaoke highlight moves from the first word to the second; "one word at a time" shows only the current word; a caption whose word count changed renders plainly.
- **Real Whisper model on synthesized speech:**
  - One cue per word, with sensible starts: "Welcome" 0.13 s, "to" 0.75 s, "Cutlery," 0.88 s, "this" 2.32 s, "video" 2.57 s, "editor" 2.88 s, …
  - An empty first cue from whisper.cpp is skipped when grouping.
  - Transcription took 55 s on the 2-core runner.
- Tests on Windows point the offscreen platform at `%WINDIR%/Fonts` (`QT_QPA_FONTDIR`). Without it, text in test renders used a tiny fallback font, so pixel checks on captions failed only on Windows. The application uses the normal Windows platform and system fonts.

Downloads (expire 2026-10-31):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36927521399/artifacts/11195410549): 148,489,604 bytes, archive SHA-256 `71d723affcd03eedca3626b3ba8ec3497481f128284e5e33d2a6326dc501e01e`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36927521399/artifacts/11194916737): 701,994,873 bytes, archive SHA-256 `72712465bfdd9548a2e53ebfe7fa3e08b1c705f71fc4c9e9b34c38d337fbb308`.

## Windows portable build — 2026-10-01, pause removal and loudness (0.5 development)

[GitHub Actions run 36896811347](https://github.com/HNXS/Cutlery/actions/runs/36896811347) completed successfully for commit `4eb7315b14f7df539e23c5b43726a53620700cdd`, with the same toolchain as the automatic-captions build. New coverage:

- **Engine test `pausesAndLoudness`:**
  - `Project::cutRanges` with overlapping, unsorted and out-of-range ranges, a cut at the clip start, ripple on the clip's track only, and detached-audio linking.
  - Parsing the EBU R128 summary.
  - End to end: a 3.5 s clip with a 1.5 s silence. One pause of about 1.26 s is found and removed, and undo restores the clip.
  - An MPEG-4 export normalised to −14 LUFS, measured afterwards within 1 LU of the target.
- **Interface tests:**
  - The loudness choice in the export dialog: the YouTube preset sets −14 LUFS, and a manual change makes the preset "Custom".
  - The Remove pauses dialog: find, preview text and removal.
- `Get-FFmpeg.ps1` now also requires `volume`, `ebur128` and `silencedetect`.

Downloads (expire 2026-10-31):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36896811347/artifacts/11181295204): 148,476,115 bytes, archive SHA-256 `7cb5ca46f8cc361a161765e62653f83bd830caf11c2405841832123888c2f0ca`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36896811347/artifacts/11180780838): 701,994,508 bytes, archive SHA-256 `74558d1c4eb8d6bcb973e2df1da9da5af7f9f16743ea3d8370b44d44d0f8e2f8`.

## Windows portable build — 2026-10-01, automatic captions (0.5 development)

[GitHub Actions run 36875006979](https://github.com/HNXS/Cutlery/actions/runs/36875006979) completed successfully for commit `dbc9d576e6ec3ca2af75e35d76ebaa68147f79b9`. It used Windows Server 2022, MSVC 2022, Qt 6.8.3, LGPL FFmpeg 8.1, ONNX Runtime 1.22.0 with DirectML, and whisper.cpp v1.9.4. whisper.cpp was built from commit `927cfce34f31707e17f2bff35c349632fb9e2c3a` (static, AVX2, OpenMP). The run covered:

- Compilation and both CTest suites, including the new `automaticCaptions` test:
  - SRT parsing;
  - caption placement through trims, positions and speed, with duplicate and reverse handling and no overlaps;
  - the full Editor path through `cutlery-ai transcribe` and whisper-cli on whisper.cpp's test model ("No speech found", the "AI captions" track, and cache reuse on a second run).
- The interface check that the caption dialog explains a missing speech-recognition component.
- The real Whisper large-v3-turbo q5_0 model with Silero VAD through `cutlery-ai`, on speech synthesized by Windows text-to-speech: "Welcome to Cutlery. This video editor works on your own computer." (5.7 s).
  - The transcript matched word for word.
  - On the 2-core runner the transcription took 71 s, of which 69 s was the encoder for one 30-second block.
- The packaged `whisper-cli.exe` with `PATH` limited to System32, using the bundled MSVC and OpenMP runtimes.

An earlier build without OpenMP did not finish within 15 minutes on the same runner: ggml's spinning thread pool stalls when threads outnumber free cores. It was replaced before release.

Downloads (expire 2026-10-31):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36875006979/artifacts/11169526333): 148,456,184 bytes, archive SHA-256 `7e07c1791dbec230af01a6d8aeb6525abea15b205a0dbc4590cbe9279741ada9`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36875006979/artifacts/11168892705): 701,994,440 bytes, archive SHA-256 `37870c4234d9ff7a27f19340fe8adc462b76ed2b5bb5ba175704669851e3d1bb`.

## Windows portable build — 2026-10-01, AI upscale and DirectML (0.5 development)

[GitHub Actions run 36862210410](https://github.com/HNXS/Cutlery/actions/runs/36862210410) completed successfully for commit `bd98b4ce9614e712ff27dc8972eb16203232b68d` on Windows Server 2022 with MSVC 2022, Qt 6.8.3, LGPL FFmpeg 8.1 and ONNX Runtime 1.22.0 with DirectML 1.15.4. Both NuGet packages are pinned by SHA-256. The Real-ESRGAN weights were converted to ONNX in CI by `tools/convert-realesrgan.py`. The run covered:

- Compilation of `cutlery-ai` with the DirectML provider, both CTest suites and QML startup.
- Engine test `aiCutout`: a stand-in upscaled copy replaces the clip's picture at the right source time. Both worker tasks run through `AiJobs` with stand-in ONNX models: queueing, a matte, and every frame of an upscale at twice the size.
- Interface test `aiCutoutControls`: both inspector options run their task and report the result.
- The real models through the built worker: a matte, a 320×240 → 1280×960 upscale, and the same upscale forced onto the CPU provider. The packaged worker also ran with `PATH` limited to System32.
- The CI runner has no GPU. DirectML reported "Specified display adapter handle is invalid", and the worker fell back to the CPU as designed. **GPU inference has not been run on real hardware yet.**

Downloads (expire 2026-10-31):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36862210410/artifacts/11162916313): 147,544,195 bytes, archive SHA-256 `02f4b0839b5f025be6ba9af8e9c8cc50c0428612e022fce7840607600f1475fb`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36862210410/artifacts/11163275748): 167,930,616 bytes, archive SHA-256 `c3d3c9ef65f8c894410597af7b5740d29643a30db8c245bc570dee28e70cfe8c`.

Locally (Linux, Qt 6.4, CPU) the converted model was checked against bicubic scaling on a test image and a test video, including 64-pixel tiles, with no visible seams. On 4 CPU cores a 640×360 frame takes about 2.9 s.

## Windows portable build — 2026-10-01, AI background removal (0.5 development)

[GitHub Actions run 36850705437](https://github.com/HNXS/Cutlery/actions/runs/36850705437) completed successfully for commit `e5c9937b75690c420df51a3c803d06e3e3b069f0` on Windows Server 2022 with MSVC 2022, Qt 6.8.3, LGPL FFmpeg 8.1 and ONNX Runtime 1.22.0 (CPU). It covered compilation including `cutlery-matte`, both CTest suites, QML startup, packaging, and packaged-executable startup. The suites include:

- `aiCutout` (engine): a generated matte applied through the renderer, checked for alignment between analysed frames, with offset ranges, flip and a circle shape. The worker runs end to end with a stand-in ONNX model, and the `Mattes` job manager is exercised.
- `aiCutoutControls` (interface): the inspector checkbox starts the analysis, which finishes with the matte covering the clip.
- The real U²-Net model ran on a test video through the built worker. The deployed worker also ran with `PATH` limited to System32, so the packaged `onnxruntime.dll` was loaded. Inference took about 1 s per analysed frame on the CI runner.

Downloads (expire 2026-10-31):

- [Portable build](https://github.com/HNXS/Cutlery/actions/runs/36850705437/artifacts/11156230322): 136,807,908 bytes, archive SHA-256 `73ae0be90d8305100583edc6993624b7408d1f69b8d4d8823fe2bc783a40ad02`.
- [AI pack](https://github.com/HNXS/Cutlery/actions/runs/36850705437/artifacts/11156105821): 163,395,458 bytes, archive SHA-256 `ef760bcbdf52a7f4e573d6d970db9fde4232417679d490d5933426d6cc3eeeee`.

Locally (Linux, Qt 6.4) the real model correctly cut out a drawn person figure. No photographic footage has been checked yet.

## Windows portable build — 2026-10-01, presenter overlays (0.5 development)

[GitHub Actions run 36845035491](https://github.com/HNXS/Cutlery/actions/runs/36845035491) completed successfully for commit `493f4e12d88fedb24ff1a8c65b8b450510fec478` on Windows Server 2022 with MSVC 2022, Qt 6.8.3 and LGPL FFmpeg 8.1. It covered compilation, both CTest suites, QML startup, packaging, and packaged-executable startup. The suites include:

- the new `overlayStyles` engine test, with pixel checks of a circle with border ring and shadow, rounded corners, green-screen removal, keying combined with a circle and animated scale, schema 6 persistence, clip bounds, one-step position edits, and display-rotation probing;
- the `presenterControls` interface test: corner placement, mouse drag to move and corner drag to resize in the viewer, undo, shape and green-screen controls.

A 1280×720 still of a green-screen speaker in a circle with border and shadow over a test pattern was inspected visually. It found and fixed a defect where keyed areas inside the border showed the border colour instead of the background.

[Download the portable build](https://github.com/HNXS/Cutlery/actions/runs/36845035491/artifacts/11153650251) (131,913,117 bytes, expires 2026-10-31). Archive SHA-256: `360d03f5389d2de064f868c98c2ef1d15b7dd4c9283d4b21a28da0307371353e`.

## Windows portable build — 2026-10-01, export presets (0.5 development)

[GitHub Actions run 36841760041](https://github.com/HNXS/Cutlery/actions/runs/36841760041) completed successfully for commit `acbff13afe51a6b505ea037cda4bc6626b8200ee` on Windows Server 2022 with MSVC 2022, Qt 6.8.3 and LGPL FFmpeg 8.1. It covered compilation, both CTest suites, QML startup, packaging, and packaged-executable startup. The suites include:

- the new `exportProfiles` engine test: candidate lists, output sizes, skipping a missing encoder with a cached result, and real AV1 (upscaled), VP9, ProRes and MPEG-4 exports checked with ffprobe for codec, size and audio codec;
- the `exportDialog` interface test: the YouTube preset fills the fields, a manual change becomes "Custom", and the extension follows the format.

The H.264 case accepts either a working encoder or the "No H264 encoder" message. Passing CTest output does not record which path the runner took, so hardware-encoder behaviour still needs checking on a real PC with an NVIDIA, AMD or Intel GPU.

[Download the portable build](https://github.com/HNXS/Cutlery/actions/runs/36841760041/artifacts/11152345050) (131,887,842 bytes, expires 2026-10-31). Archive SHA-256: `7b1c1cd100db808effe15a95a93f43a3c831ee9c823d500b33b7e61deaef0df0`.

## Windows portable build — 2026-10-01, keyframes (0.5 development)

[GitHub Actions run 36834388932](https://github.com/HNXS/Cutlery/actions/runs/36834388932) completed successfully for commit `1475f8a36ebdc141e56a0d80320741f78c7f06fd` on Windows Server 2022 with MSVC 2022, Qt 6.8.3 and the shipped LGPL FFmpeg 8.1. It covered compilation, both CTest suites, QML startup, packaging, and packaged-executable startup. The suites include:

- the new `keyframes` engine test: interpolation, trim/split/speed rules, schema 5 roundtrip, validation; measured animated scale, position plus rotation, and opacity; windowed frames inside animations; animated volume loudness;
- the `keyframeControls` interface test: the ◇ toggle, auto-key on value change, timeline markers, previous/next navigation, and removal keeping the last value.

The preceding run 36833107826 failed because `perspective` is GPL-only and absent from the LGPL build. Geometry now uses `rotate`, `scale=eval=frame` and `overlay`. Local tests now run against the same FFmpeg 8.1 LGPL build (21 engine tests passed).

[Download the portable build](https://github.com/HNXS/Cutlery/actions/runs/36834388932/artifacts/11148399888) (131,863,714 bytes, expires 2026-10-31). Archive SHA-256: `0145e3b239b1005dfddd7b8b286a23aa81cc2dd18c65dbaf411cd1dd6534d88c`.

## Windows portable build — 2026-09-30, transitions (0.5 development)

[GitHub Actions run 36785406583](https://github.com/HNXS/Cutlery/actions/runs/36785406583) completed successfully for commit `a37fd73aac105c2bd366fe758731de1d6f16eb50` on Windows Server 2022 with MSVC 2022 and Qt 6.8.3. It covered compilation, both CTest suites, QML startup, packaging, and packaged-executable startup. The CTest suites include:

- the new `transitions` engine test: dissolve blend at the cut, windowed frames matching the full render inside a transition, wipe halves, equal-power audio level across the crossfade, exact mix length, model rules, schema 4 roundtrip and split;
- the `transitionMarkers` interface test: clicking **+** on a cut adds a dissolve and the inspector shows it.

[Download the portable build](https://github.com/HNXS/Cutlery/actions/runs/36785406583/artifacts/11129756210) (131,846,407 bytes, expires 2026-10-30). Archive SHA-256: `2dc8a430ff87c2f0c30167bf7591c92f2efb76424a7aaabff24c9384e17c396b`.

Local (Ubuntu, Qt 6.4.2): 20 engine tests passed. A 640×360 still at a transition cut with 1080p H.264 sources rendered in 0.6 s. Audible crossfades on real hardware still require manual checking.

## Windows portable build — 2026-09-30, filmstrip thumbnails (0.5 development)

[GitHub Actions run 36781416020](https://github.com/HNXS/Cutlery/actions/runs/36781416020) completed successfully for commit `3c4a94d22da77bcc8027fb03dec70986095a9b89` on Windows Server 2022 with MSVC 2022 and Qt 6.8.3. It covered compilation, both CTest suites (the new `thumbnailStrips` engine test, and interface checks that video and image clips show loaded filmstrip tiles), QML startup, packaging, and packaged-executable startup. The application version is still 0.4.0 until the 0.5 feature set is complete.

[Download the portable build](https://github.com/HNXS/Cutlery/actions/runs/36781416020/artifacts/11127622752) (131,828,795 bytes, expires 2026-10-30). Archive SHA-256: `869bb9498cda9cb5072d60cd55af12a44f3d17e05b9c475fc4e47a3dc6b1737d`.

Local (Ubuntu, Qt 6.4.2): 19 engine tests passed. Keyframe-only extraction took 1.2 s for 3 minutes of 1080p H.264, against 15 s when decoding every frame. The filmstrip and library poster rendering was visually inspected in application screenshots.

## Windows portable build — 2026-09-30, version 0.4.0

[GitHub Actions run 36777216642](https://github.com/HNXS/Cutlery/actions/runs/36777216642) completed successfully for commit `6ea8a79eaaf3579028b81f1674413c654612150e` on Windows Server 2022 with MSVC 2022 and Qt 6.8.3. It covered compilation, both CTest suites (engine including windowed rendering and live playback; interface including Space/K live play and pause), QML startup, packaging, and packaged-executable startup without SDK paths. The preceding run 36774621755 failed at compilation (`QtVideo::MapMode` is not in Qt 6.8.3); that was fixed in `6ea8a79`.

[Download Cutlery-0.4.0-win64-portable](https://github.com/HNXS/Cutlery/actions/runs/36777216642/artifacts/11126345710) (131,809,976 bytes). The CI artifact expires on 2026-10-30. Archive SHA-256: `3e19f4b5fb7a683f6c4f5eeab4247c89739b847c82262ca1372e940e61ad031b`.

The CI runner has no audio device, so its playback tests use the wall clock. Audible output and A/V offset on real hardware still require manual checking.

## Local development run — 2026-09-30, version 0.4.0

The network policy of this development container blocks the Qt SDK download, so Qt 6.8.3 was unavailable. Ubuntu 24.04's Qt 6.4.2 (GCC 13.3, FFmpeg 6.1.1) was used with a local-only relaxed version check. The Windows Qt 6.8.3 CI run is the authoritative build.

- Engine suite: **18 passed, 0 failed**. The new `windowedRender` test compares one-frame preview windows with whole-timeline renders at clip starts, fades, overlay edges and a reversed 2× clip. It also checks exact raw frame/PCM sizes for streamed playback and wall-clock pacing. The job test plays live, edits during playback (automatic restart), reaches the end, and pauses at an advanced frame.
- Interface suite: not meaningful on Qt 6.4. Unmodified single-key shortcuts (Space, K, Right) do not fire in that offscreen build, and the unchanged 0.3 suite crashes there. The 0.4 UI playback flow passed on Windows CI (above).
- Benchmark on a 3-minute 1080p30 H.264 file, 4-core container, 640×360 preview:

| Operation | 0.3 | 0.4 |
|---|---|---|
| Preview frame at 0:10 | 1,377 ms | 130 ms |
| Preview frame at 1:30 | 11,890 ms | 129 ms |
| Preview frame at 2:30 | 19,629 ms | 119 ms |
| Play from 1:30 → first frame (960×540) | cache render of the full timeline first (6,815 ms per 30 s) | 164 ms |

Audible playback and A/V offset on real Windows audio hardware still require manual checking.

## Windows portable build — 2026-09-30, version 0.3.0

[GitHub Actions run 36768658353](https://github.com/HNXS/Cutlery/actions/runs/36768658353) completed successfully for commit `d80150d78c218ca3e3457ad6dd48915c876dac00` on Windows Server 2022 with MSVC 2022 and Qt 6.8.3. Compilation including the native icon resource, both engine/interface test suites, QML startup, packaging and packaged-executable startup without SDK paths passed.

[Download Cutlery-0.3.0-win64-portable](https://github.com/HNXS/Cutlery/actions/runs/36768658353/artifacts/11122917795) (131,803,167 bytes). The CI artifact expires on 2026-10-30. Archive SHA-256: `823a0e62430bf0ae7615e8b4c9c1a067d59eba9c6d76ab113060d95e676d95c0`.

## Local development run — 2026-09-30, version 0.3.0

Built with GCC 13.3, Qt 6.8.3 and CMake 4.4.3 on Ubuntu 24.04.

- Engine suite: **17 passed, 0 failed, 0 skipped**, including setup/cleanup. New coverage checks magnetic compaction/reordering/cross-track moves/trims/deletion/duplication, independent track timing, undo, lock guards, schema 2 migration and schema 3 roundtrip.
- File-drop model checks cover ordered insertion, a missing file between valid files, undo/redo, destination-track renumbering during asynchronous import, locked destinations, and removal of a destination while probing.
- Interface suite: **4 passed, 0 failed, 0 skipped**, including setup/cleanup. Actual QML gestures cover library-to-track drops for generated video/audio/images, same-track and cross-track clip moves, per-track snapping, Magnet toggle/reorder/undo, Escape cancellation for both drag types, locked-drop rejection, OS URL-MIME drops to a track and library, plus the existing trimming/keyboard/playback flow. No QML warnings were emitted.
- Visual inspection: actual demo window and the original vector app icon rendered at 256 pixels. The Windows ICO contains 16, 24, 32, 48, 64, 128 and 256 pixel variants; Windows resource compilation is part of CI.

These are automated event-level drop checks, not manual Explorer testing on a user's managed Windows desktop. Audible output, high-DPI behavior and long-session performance still require manual qualification.

## Windows portable build — 2026-09-30, version 0.2.0

[GitHub Actions run 36689916787](https://github.com/HNXS/Cutlery/actions/runs/36689916787) completed successfully for commit `4cfd7ba2aa84187f39aff8b8eeeb5496991447ae` on Windows Server 2022 with MSVC 2022 and Qt 6.8.3. Configure/compile, engine and interface tests, QML startup, portable packaging, and packaged-executable startup with SDK paths removed all passed.

[Download Cutlery-0.2.0-win64-portable](https://github.com/HNXS/Cutlery/actions/runs/36689916787/artifacts/11085730483) (131,738,605 bytes). The CI artifact expires on 2026-10-30; later successful workflow runs can provide fresh artifacts. Archive SHA-256: `eb9e3979246fb202edf113dada069025b464650aa2da633d35afd9012f6367a4`.

## Local development run — 2026-09-30, version 0.2.0

Built with GCC 13.3, Qt 6.8.3 and CMake 4.4.3 on Ubuntu 24.04. This is a development verification host, not a supported end-user distribution.

- `cutlery_tests`: **15 passed, 0 failed, 0 skipped**, including test setup/cleanup.
- `cutlery_ui_tests`: **3 passed, 0 failed, 0 skipped**, including test setup/cleanup. Actual QML mouse/keyboard checks cover drag trimming, split/undo, typing without triggering timeline commands, retained title drafts, shortcut remapping, and render-then-play with an advancing Qt Multimedia position followed by pause. No QML warnings were emitted.
- Actual-render coverage: generate source video/audio, compose a timed image overlay, render MP4, check 60 output frames, inspect foreground/background colours at multiple times, verify audio data, and compare a shared-graph preview still.
- Asynchronous job coverage: image import, timeline insertion, WebM export, refusal to overwrite an existing destination, playback-cache rendering/invalidation, cancellation and partial-file cleanup.
- Offline boundary: a local HLS playlist referencing HTTPS is rejected by the codec protocol whitelist before network access.
- Track rendering checks: picture hiding, track mute, solo suppression, and audio-only clips are checked against decoded video pixels and PCM audio. Detach-audio and undo are exercised.
- Model coverage: rational overflow, Unicode/relative-path JSON roundtrip, schema 1 migration and schema 2 roundtrip, invalid time/source bounds, split/reverse/ripple semantics, exact forward/reverse trims, edge snapping, locked-track rejection, undo/redo and SRT roundtrip.
- Waveforms: split-byte PCM peak accumulation, extraction from actual generated audio, disk cache reuse, and changed-file fingerprint invalidation. Shortcut preferences: conflict/invalid-key rejection, remapping, persistence, reset, and failed-save rollback.
- QML startup smoke test: passed. A rendered demo window was captured and visually inspected; `docs/images/editor.png` is that actual application window.

The host has no usable audio device; audible playback was not manually evaluated. The standalone Qt test executable was used because the downloaded CTest runner crashed on this host. Windows CI uses the runner-provided CTest and repeats these tests, then checks the packaged executable without SDK paths. Consult the linked GitHub Actions run for its independent result.

See BUILDING.md for qualification work that remains. No performance, HDR, clean-machine, hardware-encoder or production-readiness claims are implied by these checks.
