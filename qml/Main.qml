import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQml.Models
import QtMultimedia

ApplicationWindow {
    id: win
    width: 1440
    height: 930
    minimumWidth: 1100
    minimumHeight: 760
    visible: true
    color: "#111419"
    title: (s.dirty ? "• " : "") + s.name + " — Cutlery"
    font.family: Qt.platform.os === "windows" ? "Segoe UI" : "DejaVu Sans"
    font.pixelSize: 12
    palette.window: "#111419"
    palette.windowText: "#e7edf2"
    palette.text: "#e7edf2"
    palette.base: "#151b22"
    palette.button: "#252e38"
    palette.buttonText: "#e7edf2"
    palette.highlight: "#64d8bc"
    palette.highlightedText: "#10241f"
    property var s: editor.state
    property var selection: s.selected
    property color mint: "#64d8bc"
    property color muted: "#8c9aa8"
    property real pixelsPerSecond: 48
    property int targetTrack: 0
    // The library media the export dialog converts on its own instead of exporting the timeline.
    property string convertAsset: ""
    // The next click on the viewer picks the selected clip's key colour.
    property bool pickingKey: false
    // Drawing a free mask: clicks on the canvas add its points.
    property bool drawingMask: false
    readonly property string maskClip: s ? s.selectedId : ""
    onMaskClipChanged: drawingMask = false
    // The asset panel's tabs (left): media, sound, text, stickers, effects, transitions, filters
    // and layouts.
    property string leftTab: "media"
    // Text to speech: the chosen voice (an id; the first German one, else the first, by default)
    // and speed.
    property string speechVoice: ""
    property real speechSpeed: 1
    readonly property string currentVoice: {
        const voices = (s.speech || {}).voices || [];
        if (voices.some(v => v.id === speechVoice))
            return speechVoice;
        const german = voices.find(v => v.language === "German");
        return german ? german.id : voices.length ? voices[0].id : "";
    }
    // The categories of every left tab, for the category column; "top" scrolls to the start.
    // The column keeps all of them and shows those of the tab on show, so switching tabs does
    // not destroy its buttons while the panel is laid out again.
    readonly property var allCategories: [
        { tab: "audio", name: "Sounds", target: "top" },
        { tab: "audio", name: "Speech", target: "cat-speech" }
    ].concat([...new Set(editor.sounds().map(x => x.category))].map(c => ({ tab: "audio", name: c, target: "cat-sound-" + c }))).concat([
        { tab: "stickers", name: "Shapes", target: "top" },
        { tab: "stickers", name: "Icons", target: "catIcons" },
        { tab: "stickers", name: "Brand kit", target: "catBrand" },
        { tab: "effects", name: "Areas", target: "top" },
        { tab: "effects", name: "Style effects", target: "catStyle" },
        { tab: "filters", name: "Looks", target: "top" },
        { tab: "filters", name: "LUTs", target: "catLuts" }
    ])
    readonly property var leftCategories: allCategories.filter(c => c.tab === leftTab && (leftTab !== "filters" || (s.lutLibrary || []).length > 0))
    // The first item under `root` with this objectName.
    function findByName(root, name) {
        if (!root)
            return null;
        if (root.objectName === name)
            return root;
        for (let i = 0; i < root.children.length; ++i) {
            const found = findByName(root.children[i], name);
            if (found)
                return found;
        }
        return null;
    }
    readonly property var leftTabList: [
        { id: "media", label: "Media", glyph: "▣" },
        { id: "audio", label: "Audio", glyph: "♪" },
        { id: "text", label: "Text", glyph: "T" },
        { id: "stickers", label: "Stickers", glyph: "★" },
        { id: "effects", label: "Effects", glyph: "✦" },
        { id: "transitions", label: "Transitions", glyph: "⋈" },
        { id: "filters", label: "Filters", glyph: "◐" },
        { id: "layouts", label: "Layouts", glyph: "▦" }
    ]
    // Looks: the colour and look settings at once (the first entry is the menu's prompt).
    readonly property var looks: [
        { label: "Apply a look…", values: null },
        { label: "Natural (reset)", values: {} },
        { label: "Warm", values: { temperature: .35, vibrance: .2 } },
        { label: "Cool", values: { temperature: -.35, vibrance: .1 } },
        { label: "Cinematic", values: { temperature: .1, contrast: 1.15, highlights: -.25, vibrance: .15, vignette: .35 } },
        { label: "Vintage", values: { temperature: .3, saturation: .75, shadows: .35, highlights: -.15, grain: .4, vignette: .4 } },
        { label: "Black & white", values: { saturation: 0, contrast: 1.2, grain: .2 } },
        { label: "Punchy", values: { contrast: 1.15, vibrance: .5, sharpen: .3 } },
        { label: "Dreamy", values: { glow: .5, highlights: .15, contrast: .9, temperature: .1 } }
    ]
    function applyLook(index) {
        const look = looks[index].values;
        if (!look)
            return;
        // Every look setting at once, in one undo step; the LUT stays.
        const values = { brightness: 0, contrast: 1, saturation: 1, temperature: 0, tint: 0, vibrance: 0, shadows: 0, highlights: 0, sharpen: 0, glow: 0, vignette: 0, grain: 0, curveMaster: "", curveRed: "", curveGreen: "", curveBlue: "", hslColors: "", hslHue: 0, hslSaturation: 0, hslLightness: 0 };
        for (const k in look)
            values[k] = look[k];
        editor.setClipValues(values);
    }
    readonly property var styleEffects: [
        { id: "", label: "No effect" }, { id: "shake", label: "Camera shake" }, { id: "glitch", label: "Glitch" },
        { id: "vhs", label: "VHS" }, { id: "film", label: "Old film" }, { id: "sketch", label: "Sketch" },
        { id: "poster", label: "Poster" }, { id: "fisheye", label: "Fisheye" }, { id: "mirror", label: "Mirror" }
    ]
    // The selected clip takes a look, style effect or transition from the asset panel.
    readonly property bool pictureSelected: selectionKind === "media"
    // Inspector pages: tabs across the top and sub-tabs below them, chosen by what is selected.
    // The choice is kept per kind of selection, so switching between clips keeps the page.
    readonly property string selectionKind: !s || !s.selectedId ? ""
        : selection.effect === "adjust" ? "adjust"
        : (selection.effect || "") !== "" ? "area"
        : (selection.graphic || "") !== "" ? "shape"
        : (selection.assetId || "") === "" ? "text"
        : selection.picture !== true ? "audio" : "media"
    readonly property var inspectorTabs: {
        const audio = { id: "audio", label: "Audio", subs: [{ id: "basic", label: "Basic" }, { id: "voice", label: "Voice" }, { id: "cleanup", label: "Clean-up" }] };
        const adjust = { id: "adjust", label: "Adjust", subs: [{ id: "basic", label: "Basic" }, { id: "hsl", label: "HSL" }, { id: "curves", label: "Curves" }, { id: "wheels", label: "Wheels" }, { id: "lut", label: "LUT" }] };
        const more = { id: "more", label: "More", subs: [] };
        const basic = { id: "basic", label: "Basic", subs: [] };
        const animation = { id: "animation", label: "Animation", subs: [] };
        switch (selectionKind) {
        case "media":
            return [{ id: "video", label: "Video", subs: [{ id: "basic", label: "Basic" }, { id: "cutout", label: "Cutout" }, { id: "mask", label: "Mask" }, { id: "canvas", label: "Canvas" }, { id: "enhance", label: "Enhance" }] }]
                .concat(selection.hasAudio === true ? [audio] : [])
                .concat([{ id: "speed", label: "Speed", subs: [] }, animation, adjust, { id: "effects", label: "Effects", subs: [] }, more]);
        case "audio":
            return [{ id: "basic", label: "Basic", subs: [] }, { id: "voice", label: "Voice", subs: [] }, { id: "cleanup", label: "Clean-up", subs: [] }, { id: "speed", label: "Speed", subs: [] }, more];
        case "text":
            return [{ id: "text", label: "Text", subs: [] }, basic, animation, more];
        case "shape":
            return [{ id: "shape", label: "Shape", subs: [] }, basic, animation, more];
        case "area":
            return [{ id: "effect", label: "Effect", subs: [] }, basic, more];
        case "adjust":
            return [adjust, basic, more];
        }
        return [];
    }
    property var inspectorChoice: ({})
    readonly property string inspectorTab: {
        const chosen = inspectorChoice[selectionKind] || "";
        return inspectorTabs.some(t => t.id === chosen) ? chosen : (inspectorTabs.length ? inspectorTabs[0].id : "");
    }
    readonly property var inspectorSubs: (inspectorTabs.find(t => t.id === inspectorTab) || { subs: [] }).subs
    readonly property string inspectorSub: {
        const chosen = inspectorChoice[selectionKind + "/" + inspectorTab] || "";
        return inspectorSubs.some(t => t.id === chosen) ? chosen : (inspectorSubs.length ? inspectorSubs[0].id : "");
    }
    function chooseInspector(tab, sub) {
        const choice = Object.assign({}, inspectorChoice);
        if (tab)
            choice[selectionKind] = tab;
        if (sub)
            choice[selectionKind + "/" + (tab || inspectorTab)] = sub;
        inspectorChoice = choice;
    }
    function onTab(tab) {
        return inspectorTab === tab;
    }
    // A page of the picture (Video tab, or Basic for titles, shapes and areas).
    function picPage(sub) {
        return (inspectorTab === "video" && inspectorSub === sub) || (sub === "basic" && inspectorTab === "basic" && selectionKind !== "audio");
    }
    // A page of the sound (its own tabs for sound clips, the Audio tab's sub-tabs otherwise).
    function audioPage(sub) {
        return (selectionKind === "audio" && inspectorTab === sub) || (inspectorTab === "audio" && inspectorSub === sub);
    }
    function adjustPage(sub) {
        return inspectorTab === "adjust" && inspectorSub === sub;
    }
    // Start, length and speed: the Speed tab, or Basic where there is none.
    readonly property bool timingPage: onTab("speed") || (onTab("basic") && ["text", "shape", "area", "adjust"].indexOf(selectionKind) >= 0)
    // Where each animatable or plain value of the clip is set.
    function propertyPage(key) {
        if (["volume", "pan", "fadeIn", "fadeOut"].indexOf(key) >= 0)
            return audioPage("basic");
        if (["exposure", "brightness", "contrast", "saturation"].indexOf(key) >= 0)
            return adjustPage("basic");
        if (key === "blur")
            return onTab("effects");
        if (key === "maskX" || key === "maskY")
            return picPage("mask");
        return picPage("basic");
    }
    function soundPage(key) {
        return audioPage(["reverb", "echo", "pitch"].indexOf(key) >= 0 ? "voice" : "cleanup");
    }
    property var exportChoice: ({
            format: "h264",
            quality: "high",
            height: 0,
            loudness: -14
        })
    // The canvas at the aspect w:h, keeping the shorter side (even sizes).
    function reframeTo(w, h) {
        const side = Math.min(win.s.width, win.s.height);
        if (w <= h)
            editor.reframe(side, Math.round(side * h / w / 2) * 2);
        else
            editor.reframe(Math.round(side * w / h / 2) * 2, side);
    }
    property string relinkAsset: "" // the library item to replace; else the selected clip's
    property bool queueExport: false // the export file dialog adds to the queue instead
    property string pendingAction: ""
    property bool allowClose: false
    property var libraryGesture: null
    property bool textEditing: activeFocusItem && typeof activeFocusItem.cursorPosition === "number"
    property bool showScopes: false
    // Set at launch without a project: the start screen shows until a project has content.
    property bool startScreen: false
    // Canvas formats offered for new projects.
    readonly property var projectFormats: [
        { w: 1920, h: 1080, label: "Full HD 16:9", hint: "YouTube, presentations" },
        { w: 3840, h: 2160, label: "4K UHD 16:9", hint: "Sharp masters" },
        { w: 1080, h: 1920, label: "Vertical 9:16", hint: "Shorts, Reels, TikTok" },
        { w: 1080, h: 1350, label: "Portrait 4:5", hint: "Instagram feed" },
        { w: 1080, h: 1080, label: "Square 1:1", hint: "Social posts" },
        { w: 1280, h: 720, label: "HD 16:9", hint: "Small and fast" }
    ]
    readonly property var frameRates: [
        { n: 24, d: 1, label: "24" }, { n: 25, d: 1, label: "25" }, { n: 30, d: 1, label: "30" },
        { n: 50, d: 1, label: "50" }, { n: 60, d: 1, label: "60" }, { n: 30000, d: 1001, label: "29.97" }
    ]
    property bool shortcutsBlocked: startPage.visible || preferencesDialog.visible || openDialog.visible || saveDialog.visible || importDialog.visible || exportDialog.visible || frameDialog.visible || audioFileDialog.visible || timelineFileDialog.visible || relinkDialog.visible || relinkFolderDialog.visible || srtOpen.visible || srtSave.visible || soundDialog.visible || folderDialog.visible || styleDialog.visible || layoutDialog.visible || rightsDialog.visible || templateDialog.visible || backupDialog.visible || discardDialog.visible || settings.visible || exportSettings.visible || about.visible || shortcutsDialog.visible || commandSearch.visible || timelinePanel.dialogOpen
    Shortcut {
        sequence: "Escape"
        enabled: (win.libraryGesture !== null && win.libraryGesture.dragging) || timelinePanel.draggingClip !== null
        onActivated: {
            if (win.libraryGesture)
                win.libraryGesture.cancelDrag();
            timelinePanel.cancelDrag();
        }
    }
    function goTo(frame) {
        editor.seek(frame);
        timelinePanel.reveal(s.playhead);
    }
    // The key combination bound to a command, for tooltips.
    function shortcut(command) {
        const b = shortcutSettings.bindings.find(b => b.id === command);
        return b && b.sequence ? b.sequence : "no shortcut";
    }
    function shortcutEnabled(command) {
        if ((libraryGesture && libraryGesture.dragging) || timelinePanel.draggingClip)
            return false;
        if (shortcutsBlocked)
            return false;
        if (textEditing)
            return ["new", "open", "import", "save", "saveAs", "export", "shortcuts"].indexOf(command) >= 0;
        if (["previousFrame", "nextFrame", "previousCut", "nextCut", "previousMarker", "nextMarker"].indexOf(command) >= 0 && activeFocusItem && (activeFocusItem instanceof Slider || activeFocusItem instanceof ComboBox || activeFocusItem instanceof SpinBox))
            return false;
        return true;
    }
    function command(id) {
        if (editor.playing && ["split", "trimStart", "trimEnd", "previousCut", "nextCut", "marker", "inPoint", "outPoint", "previousMarker", "nextMarker"].indexOf(id) >= 0)
            goTo(editor.playbackFrame);
        const c = s.selected, editable = s.selectedId.length > 0 && !c.locked;
        if (id === "new")
            guarded("new");
        else if (id === "open")
            guarded("open");
        else if (id === "import")
            importDialog.open();
        else if (id === "save")
            saveProject();
        else if (id === "saveAs")
            saveDialog.open();
        else if (id === "export" && s.duration > 0 && !s.busy)
            exportSettings.open();
        else if (id === "undo")
            editor.undo();
        else if (id === "redo")
            editor.redo();
        else if (id === "split" && editable)
            editor.split();
        else if (id === "duplicate" && editable)
            editor.duplicate();
        else if (id === "copy" && s.selectedId.length > 0)
            editor.copy();
        else if (id === "paste" && s.clipboard)
            editor.paste();
        else if (id === "pasteAttributes" && editable && s.clipboard)
            editor.pasteAttributes("all");
        else if (id === "delete" && editable)
            editor.remove(false);
        else if (id === "selectAll")
            editor.selectAll();
        else if (id === "group")
            editor.groupSelection();
        else if (id === "ungroup")
            editor.ungroupSelection();
        else if (id === "rippleDelete" && editable)
            editor.remove(true);
        else if (id === "trimStart" && editable && s.playhead > c.start && s.playhead < c.start + c.duration)
            editor.trimClip(c.id, s.playhead, c.start + c.duration);
        else if (id === "trimEnd" && editable && s.playhead > c.start && s.playhead < c.start + c.duration)
            editor.trimClip(c.id, c.start, s.playhead);
        else if (id === "detach" && editable && c.canDetach)
            editor.detachAudio();
        else if (id === "title")
            editor.addTitle();
        else if (id === "play" && s.duration > 0 && !s.busy)
            editor.togglePlayback();
        else if (id === "pause")
            editor.pause();
        else if (id === "shuttleForward" && s.duration > 0 && !s.busy)
            editor.shuttle(true);
        else if (id === "shuttleBack" && s.duration > 0 && !s.busy)
            editor.shuttle(false);
        else if (id === "previousFrame")
            goTo(editor.playbackFrame - 1);
        else if (id === "nextFrame")
            goTo(editor.playbackFrame + 1);
        else if (id === "previousCut")
            goTo(editor.adjacentCut(false));
        else if (id === "nextCut")
            goTo(editor.adjacentCut(true));
        else if (id === "marker")
            editor.toggleMarker();
        else if (id === "previousMarker" && editor.adjacentMarker(false) >= 0)
            goTo(editor.adjacentMarker(false));
        else if (id === "nextMarker" && editor.adjacentMarker(true) >= 0)
            goTo(editor.adjacentMarker(true));
        else if (id === "inPoint")
            editor.setInPoint();
        else if (id === "outPoint")
            editor.setOutPoint();
        else if (id === "clearInOut")
            editor.clearInOut();
        else if (id === "start")
            goTo(0);
        else if (id === "end")
            goTo(s.duration - 1);
        else if (id === "zoomIn")
            timelinePanel.zoom(1.4);
        else if (id === "zoomOut")
            timelinePanel.zoom(1 / 1.4);
        else if (id === "fit")
            timelinePanel.fit();
        else if (id === "snap")
            timelinePanel.snapping = !timelinePanel.snapping;
        else if (id === "addTrack")
            editor.addTrack();
        else if (id === "shortcuts")
            shortcutsDialog.open();
        else if (id === "commandSearch")
            commandSearch.open();
    }
    function clock(frame) {
        const fps = s.fps;
        const sec = Math.floor(frame / fps);
        return String(Math.floor(sec / 60)).padStart(2, "0") + ":" + String(sec % 60).padStart(2, "0") + ":" + String(Math.floor(frame % fps)).padStart(2, "0");
    }
    function guarded(action) {
        // New and opened projects go beside the shown one, which stays open.
        const besides = action === "new" || action === "open" || action === "recover" || action.startsWith("recent:") || action.startsWith("template:");
        if (!besides && (s.dirty || s.busy)) {
            pendingAction = action;
            discardDialog.open();
        } else
            runAction(action);
    }
    function runAction(action) {
        editor.pause();
        if (action === "new")
            editor.newProject();
        else if (action === "open")
            openDialog.open();
        else if (action === "recover")
            editor.recover();
        else if (action.startsWith("recent:"))
            editor.openRecent(action.substring(7));
        else if (action.startsWith("template:")) {
            if (editor.newFromTemplate(action.substring(9)))
                win.startScreen = false;
        }
        else if (action.startsWith("restore:"))
            editor.restoreBackup(action.substring(8));
        else if (action.startsWith("closeProject:"))
            editor.closeProject(Number(action.substring(13)));
        else if (action === "close") {
            allowClose = true;
            win.close();
        }
    }
    // Closes an open project, asking first when it has unsaved changes.
    function closeProjectAt(index) {
        const p = (s.projects || [])[index];
        if (!p)
            return;
        if (p.dirty) {
            pendingAction = "closeProject:" + index;
            discardDialog.open();
        } else
            editor.closeProject(index);
    }
    function saveProject() {
        if (s.path.length)
            editor.save();
        else
            saveDialog.open();
    }
    onClosing: function (close) {
        if (!allowClose && (s.anyDirty || s.busy)) {
            close.accepted = false;
            pendingAction = "close";
            discardDialog.open();
        }
    }
    component Action: Button {
        implicitHeight: 32
        padding: 12
        background: Rectangle {
            color: parent.down ? "#34434d" : parent.hovered ? "#2b3742" : "#202831"
            radius: 6
            // Keyboard focus shows as a light frame.
            border.width: parent.visualFocus ? 2 : 1
            border.color: parent.visualFocus ? "#e7edf2" : parent.highlighted ? "#64d8bc" : "#35404b"
            opacity: parent.enabled ? 1 : .4
        }
        contentItem: Text {
            text: parent.text
            color: parent.enabled ? "#e7edf2" : "#65707a"
            font: parent.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
    // Track motion for the selected clip: the button (Cancel while tracking), progress and the
    // result.
    component TrackMotion: ColumnLayout {
        id: tm
        property string prefix: ""  // of the object names
        property string mode: "clip" // "mask": the selected clip's free mask follows its video
        spacing: 4
        readonly property var track: win.s.track || ({})
        readonly property bool mine: track.clipId === win.s.selectedId && (track.kind || "clip") === mode
        readonly property bool busy: track.status === "tracking"
        RowLayout {
            Layout.fillWidth: true
            Action {
                objectName: tm.prefix + "trackMotion"
                Layout.fillWidth: true
                enabled: !tm.busy && win.selection.locked !== true
                text: tm.busy && tm.mine ? "Tracking… " + Math.round((tm.track.progress || 0) * 100) + "%" : tm.mode === "mask" ? "Track the mask" : "Track motion"
                Accessible.name: tm.mode === "mask" ? "Track the mask" : "Track motion"
                onClicked: tm.mode === "mask" ? editor.trackMask() : editor.trackMotion()
                ToolTip.visible: hovered
                ToolTip.text: tm.mode === "mask" ? "Draw the mask around something at the playhead, then track: the mask follows it through the clip, forwards and backwards, as Move X/Y keyframes" : "Place it over something in the video below at the playhead, then track: it follows that through the clip, forwards and backwards, as position keyframes"
            }
            Action {
                objectName: tm.prefix + "cancelTracking"
                visible: tm.busy && tm.mine
                text: "Cancel"
                onClicked: editor.cancelTracking()
            }
        }
        Label {
            objectName: tm.prefix + "trackStatus"
            Layout.fillWidth: true
            visible: tm.mine && (tm.track.status === "done" || tm.track.status === "failed")
            wrapMode: Text.Wrap
            font.pixelSize: 11
            color: tm.track.status === "failed" ? "#ec6f5a" : win.muted
            text: tm.track.status === "failed" ? "Could not follow it there; place it over something with detail"
                : tm.track.whole ? "Tracked ✓ · " + tm.track.keyframes + " keyframes; adjust them on the timeline if needed"
                : "Tracked from " + Number(tm.track.from).toFixed(1) + " s to " + Number(tm.track.to).toFixed(1) + " s of the clip, then lost; move the playhead there, place it again and track"
        }
    }
    // An AI processing option of a video clip: checkbox, progress, status and Run/Stop. The
    // task runs in the background once per media file and is cached.
    component AiOption: ColumnLayout {
        id: ai
        required property string task  // AiJobs task: "matte" or "upscale"
        required property string flag  // clip property switching the result on
        required property string infoKey // selection entry with the task status
        required property string label
        required property string runningText
        required property string doneText
        property string statusName: flag + "Status"
        property string runName: flag + "Run"
        readonly property var info: win.selection[infoKey] || ({})
        readonly property bool working: info.status === "running" || info.status === "queued"
        readonly property string missing: (win.s.aiMissing || {})[task] || ""
        Layout.fillWidth: true
        property bool shown: true
        visible: shown && win.selection[infoKey] !== undefined
        spacing: 4
        CheckBox {
            objectName: ai.flag
            text: ai.label
            checked: win.selection[ai.flag] || false
            enabled: ai.missing === "" || checked
            onToggled: {
                editor.setClip(ai.flag, checked)
                if (checked && ai.info.covered !== true && !ai.working)
                    editor.runAi(ai.task)
            }
        }
        ProgressBar {
            Layout.fillWidth: true
            visible: ai.working
            value: ai.info.progress || 0
        }
        RowLayout {
            Layout.fillWidth: true
            visible: win.selection[ai.flag] === true || ai.working
            Label {
                objectName: ai.statusName
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 11
                color: ai.info.status === "failed" ? "#ec6f5a" : win.muted
                text: ai.info.status === "queued" ? "Waiting for another AI job…"
                    : ai.working ? ai.runningText + " " + Math.round((ai.info.progress || 0) * 100) + "%"
                        + (ai.info.device === "gpu" ? " · GPU" : ai.info.device === "cpu" ? " · CPU (slow)" : "")
                    : ai.info.status === "failed" ? "Failed: " + (ai.info.error || "")
                    : ai.info.covered === true ? ai.doneText
                    : ai.missing === "" ? "Not processed for this range yet"
                    : ai.missing
            }
            Action {
                objectName: ai.runName
                visible: !ai.working && win.selection[ai.flag] === true && ai.info.covered !== true && ai.missing === ""
                text: "Run"
                padding: 6
                onClicked: editor.runAi(ai.task)
            }
            Action {
                visible: ai.working
                text: "Stop"
                padding: 6
                onClicked: editor.cancelAi()
            }
        }
    }
    // A section of the inspector: a bold title with, as needed, an on/off box (the body is
    // dimmed while off), a reset button and a fold arrow.
    component Section: ColumnLayout {
        id: section
        property string title
        property bool checkable: false
        property bool checked: true
        property bool resettable: false
        property bool expanded: true
        property string tip: ""
        // False for a page made only of sections: no title row and no rule.
        property bool header: true
        signal toggled(bool on)
        signal reset
        default property alias content: sectionBody.data
        Layout.fillWidth: true
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            visible: section.header
            spacing: 4
            CheckBox {
                objectName: section.objectName ? section.objectName + "-on" : ""
                Accessible.name: section.title
                visible: section.checkable
                checked: section.checked
                padding: 0
                enabled: win.selection.locked !== true
                onToggled: section.toggled(checked)
            }
            Label {
                text: section.title
                font.bold: true
                font.pixelSize: 12
                Layout.fillWidth: true
                elide: Text.ElideRight
                HoverHandler {
                    id: sectionHover
                }
                TapHandler {
                    onTapped: section.expanded = !section.expanded
                }
                ToolTip.visible: section.tip !== "" && sectionHover.hovered
                ToolTip.text: section.tip
            }
            ToolButton {
                objectName: section.objectName ? section.objectName + "-reset" : ""
                Accessible.name: "Reset " + section.title.toLowerCase()
                visible: section.resettable
                enabled: win.selection.locked !== true
                text: "↺"
                implicitWidth: 24
                implicitHeight: 22
                onClicked: section.reset()
                ToolTip.visible: hovered
                ToolTip.text: "Reset " + section.title.toLowerCase()
            }
            ToolButton {
                text: section.expanded ? "▾" : "▸"
                Accessible.name: (section.expanded ? "Fold " : "Unfold ") + section.title.toLowerCase()
                implicitWidth: 22
                implicitHeight: 22
                onClicked: section.expanded = !section.expanded
            }
        }
        ColumnLayout {
            id: sectionBody
            Layout.fillWidth: true
            spacing: 8
            visible: section.expanded || !section.header
            enabled: !section.checkable || section.checked
            opacity: enabled ? 1 : .45
        }
        Rule {
            visible: section.header
        }
    }
    // One value of the selected clip: its name, a number box with steppers, a keyframe diamond
    // for animatable values, and a slider. Each gesture is one undo step.
    component ValueRow: ColumnLayout {
        id: valueRow
        property string key
        property string label
        property real from: 0
        property real to: 1
        property real stepSize: .01
        property real defaultValue: 0
        property int decimals: 2
        property string unit: ""
        // The number shown is the value times this (100 for percent).
        property real shown: 1
        property bool animatable: false
        property string sliderName: ""
        property string tip: ""
        readonly property bool keyed: animatable && (win.selection.keyed || {})[key] === true
        readonly property bool animated: animatable && ((win.selection.keyframeCount || {})[key] || 0) > 0
        // Animated values show their value at the playhead.
        readonly property real value: animated ? Number((win.selection.animated || {})[key] ?? defaultValue) : Number(win.selection[key] ?? defaultValue)
        function commit(v) {
            editor.setClip(key, Math.max(from, Math.min(to, v)));
        }
        Layout.fillWidth: true
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            spacing: 2
            Label {
                text: valueRow.label
                color: valueRow.animated ? win.mint : Math.abs(valueRow.value - valueRow.defaultValue) > 1e-9 ? "#e7edf2" : win.muted
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            TextField {
                objectName: valueRow.sliderName ? valueRow.sliderName + "-box" : ""
                Accessible.name: valueRow.label
                implicitWidth: 62
                implicitHeight: 24
                padding: 4
                horizontalAlignment: Text.AlignRight
                font.pixelSize: 11
                selectByMouse: true
                enabled: win.selection.locked !== true
                text: (valueRow.value * valueRow.shown).toFixed(valueRow.decimals) + valueRow.unit
                onEditingFinished: {
                    const v = parseFloat(text);
                    if (isFinite(v))
                        valueRow.commit(v / valueRow.shown);
                }
                background: Rectangle {
                    radius: 4
                    color: "#11161c"
                    border.color: parent.activeFocus ? win.mint : "#2b333e"
                }
            }
            ColumnLayout {
                spacing: 0
                enabled: win.selection.locked !== true
                Repeater {
                    model: [1, -1]
                    ToolButton {
                        required property int modelData
                        text: modelData > 0 ? "▴" : "▾"
                        Accessible.name: (modelData > 0 ? "Increase " : "Decrease ") + valueRow.label.toLowerCase()
                        implicitWidth: 16
                        implicitHeight: 12
                        padding: 0
                        font.pixelSize: 9
                        autoRepeat: true
                        onClicked: valueRow.commit(valueRow.value + modelData * valueRow.stepSize)
                    }
                }
            }
            ToolButton {
                objectName: "keyframe-" + valueRow.key
                Accessible.name: (valueRow.keyed ? "Remove keyframe of " : "Add keyframe of ") + valueRow.label.toLowerCase()
                visible: valueRow.animatable
                enabled: win.selection.playheadInside === true && win.selection.locked !== true
                implicitWidth: 24
                implicitHeight: 22
                text: valueRow.keyed ? "◆" : "◇"
                palette.buttonText: valueRow.animated ? "#ffd479" : "#e7edf2"
                onClicked: editor.toggleKeyframe(valueRow.key)
                ToolTip.visible: hovered
                ToolTip.text: valueRow.keyed ? "Remove keyframe" : "Add keyframe at playhead"
            }
        }
        // The keyframe at the playhead: how it moves on to the next one.
        ComboBox {
            objectName: "keyframeEasing-" + valueRow.key
            Layout.fillWidth: true
            visible: valueRow.keyed
            readonly property var easings: ["smooth", "linear", "in", "out", "hold"]
            model: ["Ease in and out", "Linear", "Ease in (slow start)", "Ease out (slow end)", "Hold until the next keyframe"]
            currentIndex: Math.max(0, easings.indexOf((win.selection.keyEasing || {})[valueRow.key] || "smooth"))
            onActivated: editor.setKeyframeEasing(valueRow.key, easings[currentIndex])
            ToolTip.visible: hovered
            ToolTip.text: "How the value moves from this keyframe to the next"
        }
        Slider {
            objectName: valueRow.sliderName
            Accessible.name: valueRow.label
            Layout.fillWidth: true
            from: valueRow.from
            to: valueRow.to
            stepSize: valueRow.stepSize
            value: valueRow.value
            enabled: win.selection.locked !== true
            onPressedChanged: if (!pressed)
                valueRow.commit(value)
            onMoved: if (!pressed)
                valueRow.commit(value)
            ToolTip.visible: hovered && valueRow.tip !== ""
            ToolTip.text: valueRow.tip + (valueRow.tip ? ". " : "") + "Double-click to reset."
            TapHandler {
                acceptedButtons: Qt.LeftButton
                onDoubleTapped: valueRow.commit(valueRow.defaultValue)
            }
        }
    }
    // A section of values; reset puts all of them back to their defaults in one undo step.
    // Rows: { key, name, lo, hi, step, def, dec, unit, shown, anim, tip }.
    component ValueGroup: Section {
        id: group
        property var rows: []
        property string prefix: ""
        resettable: true
        onReset: {
            const values = {};
            for (const r of rows)
                values[r.key] = r.def ?? 0;
            editor.setClipValues(values);
        }
        Repeater {
            model: group.rows
            ValueRow {
                required property var modelData
                key: modelData.key
                label: modelData.name
                from: modelData.lo
                to: modelData.hi
                stepSize: modelData.step ?? .01
                defaultValue: modelData.def ?? 0
                decimals: modelData.dec ?? 2
                unit: modelData.unit ?? ""
                shown: modelData.shown ?? 1
                animatable: modelData.anim === true
                sliderName: modelData.obj ?? (group.prefix ? group.prefix + modelData.key : "")
                tip: modelData.tip ?? ""
            }
        }
    }
    // A tile in the asset panel: a large glyph or colour over a label; checked shows a mint
    // frame (e.g. the selected clip's current look).
    component Tile: AbstractButton {
        id: tile
        property string glyph: ""
        property color swatch: "#202831"
        implicitWidth: 78
        implicitHeight: 72
        hoverEnabled: true
        contentItem: ColumnLayout {
            spacing: 4
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                radius: 6
                color: tile.swatch
                border.width: tile.checked || tile.visualFocus ? 2 : 1
                border.color: tile.visualFocus ? "#e7edf2" : tile.checked ? win.mint : tile.hovered && tile.enabled ? "#6c8796" : "#35404b"
                Label {
                    anchors.centerIn: parent
                    text: tile.glyph
                    font.pixelSize: 20
                    color: tile.enabled ? "#e7edf2" : "#65707a"
                }
                Rectangle {
                    visible: tile.hovered && tile.enabled
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 3
                    width: 16
                    height: 16
                    radius: 8
                    color: win.mint
                    Label {
                        anchors.centerIn: parent
                        text: "+"
                        color: "#10241f"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                text: tile.text
                font.pixelSize: 10
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                color: tile.enabled ? (tile.checked ? win.mint : "#c7d0d8") : "#65707a"
            }
        }
        background: Item {}
        opacity: enabled ? 1 : .55
    }
    component Caption: Label {
        color: win.muted
        font.pixelSize: 10
        font.letterSpacing: 1.3
    }
    component Rule: Rectangle {
        Layout.fillWidth: true
        height: 1
        color: "#2b333e"
    }
    menuBar: MenuBar {
        Menu {
            title: "Project"
            MenuItem {
                text: "New"
                onTriggered: win.guarded("new")
            }
            MenuItem {
                objectName: "closeProjectItem"
                text: "Close project"
                onTriggered: win.closeProjectAt((win.s.projects || []).findIndex(p => p.current))
            }
            MenuItem {
                text: "Open…"
                onTriggered: win.guarded("open")
            }
            Menu {
                id: recentMenu
                objectName: "recentMenu"
                title: "Open recent"
                enabled: (win.s.recent || []).length > 0
                Instantiator {
                    model: win.s.recent || []
                    delegate: MenuItem {
                        required property var modelData
                        text: modelData.name + (modelData.exists ? "" : " (missing)")
                        enabled: modelData.exists
                        onTriggered: win.guarded("recent:" + modelData.path)
                        ToolTip.visible: hovered
                        ToolTip.text: modelData.path
                    }
                    onObjectAdded: (index, object) => recentMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => recentMenu.removeItem(object)
                }
            }
            MenuItem {
                text: "Save"
                onTriggered: win.saveProject()
            }
            MenuItem {
                text: "Save As…"
                onTriggered: saveDialog.open()
            }
            MenuItem {
                text: "Import image sequence…"
                onTriggered: sequenceFile.open()
            }
            MenuItem {
                objectName: "exportTimelineItem"
                text: "Export timeline for other editors (OTIO, EDL)…"
                enabled: win.s.duration > 0
                onTriggered: timelineFileDialog.open()
            }
            MenuItem {
                objectName: "exportFrameItem"
                text: "Export current frame as picture…"
                enabled: win.s.duration > 0
                onTriggered: frameDialog.open()
            }
            MenuItem {
                objectName: "collectProject"
                text: (win.s.collect || {}).status === "copying" ? "Collecting… " + Math.round(100 * (win.s.collect.progress || 0)) + "%" : "Collect project and media…"
                enabled: (win.s.collect || {}).status !== "copying"
                onTriggered: collectDialog.open()
            }
            // Editing proxies: small copies the preview plays instead of large videos.
            Menu {
                title: "Editing proxies"
                MenuItem {
                    objectName: "makeProxies"
                    text: "Make proxies for videos over 1080p"
                    onTriggered: editor.makeProxies([])
                }
                MenuItem {
                    objectName: "useProxies"
                    text: "Play the proxies in the preview"
                    checkable: true
                    checked: win.s.proxies.useProxies === true
                    onTriggered: editor.setUseProxies(checked)
                }
                MenuItem {
                    objectName: "cancelProxies"
                    text: "Stop making proxies"
                    enabled: !!win.s.proxies.making || win.s.proxies.queued > 0
                    onTriggered: editor.cancelProxies()
                }
                MenuItem {
                    objectName: "deleteProxies"
                    text: "Delete this project's proxies"
                    onTriggered: editor.deleteProxies()
                }
            }
            MenuSeparator {}
            MenuItem {
                text: "Project settings…"
                onTriggered: settings.open()
            }
            MenuItem {
                objectName: "preferencesItem"
                text: "Preferences…"
                onTriggered: preferencesDialog.open()
            }
            MenuSeparator {}
            MenuItem {
                objectName: "saveTemplate"
                text: "Save as template…"
                enabled: win.s.duration > 0
                onTriggered: templateDialog.open()
            }
            Menu {
                id: templateMenu
                title: "New from template"
                enabled: (win.s.templates || []).length > 0
                Instantiator {
                    model: win.s.templates || []
                    delegate: MenuItem {
                        required property var modelData
                        text: modelData.name
                        onTriggered: win.guarded("template:" + modelData.name)
                    }
                    onObjectAdded: (index, object) => templateMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => templateMenu.removeItem(object)
                }
            }
            Menu {
                id: removeTemplateMenu
                title: "Remove a template"
                enabled: (win.s.templates || []).length > 0
                Instantiator {
                    model: win.s.templates || []
                    delegate: MenuItem {
                        required property var modelData
                        text: modelData.name
                        onTriggered: editor.removeTemplate(modelData.name)
                    }
                    onObjectAdded: (index, object) => removeTemplateMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => removeTemplateMenu.removeItem(object)
                }
            }
            // A new canvas shape; pictures zoom to fill it and, with the AI pack, follow faces.
            Menu {
                id: reframeMenu
                title: "Reframe for…"
                enabled: win.s.duration > 0 && (win.s.reframe || {}).status !== "analysing"
                Instantiator {
                    model: [
                        { label: "Shorts, Reels, TikTok (9:16)", w: 9, h: 16 },
                        { label: "Instagram post (4:5)", w: 4, h: 5 },
                        { label: "Square (1:1)", w: 1, h: 1 },
                        { label: "Widescreen (16:9)", w: 16, h: 9 }
                    ]
                    delegate: MenuItem {
                        required property var modelData
                        objectName: "reframe-" + modelData.w + "x" + modelData.h
                        text: modelData.label
                        onTriggered: win.reframeTo(modelData.w, modelData.h)
                    }
                    onObjectAdded: (index, object) => reframeMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => reframeMenu.removeItem(object)
                }
            }
            MenuItem {
                objectName: "restoreVersion"
                text: "Restore an earlier version…"
                enabled: win.s.path.length > 0
                onTriggered: backupDialog.open()
            }
            MenuItem {
                text: "Recover autosave"
                enabled: win.s.hasRecovery
                onTriggered: win.guarded("recover")
            }
        }
        Menu {
            title: "Edit"
            MenuItem {
                text: "Undo"
                enabled: win.s.canUndo
                onTriggered: editor.undo()
            }
            MenuItem {
                text: "Redo"
                enabled: win.s.canRedo
                onTriggered: editor.redo()
            }
            MenuItem {
                objectName: "historyItem"
                text: "History…"
                enabled: win.s.canUndo || win.s.canRedo
                onTriggered: historyDialog.open()
            }
            MenuItem {
                text: "Select all clips"
                onTriggered: editor.selectAll()
            }
            MenuItem {
                objectName: "groupClips"
                text: "Group selected clips"
                enabled: (win.s.selectedIds || []).length > 1
                onTriggered: editor.groupSelection()
            }
            MenuItem {
                text: "Ungroup"
                enabled: (win.s.selectedIds || []).length > 0
                onTriggered: editor.ungroupSelection()
            }
            MenuSeparator {}
            MenuItem {
                objectName: "nestClips"
                text: "Nest selected clips"
                enabled: (win.s.selectedIds || []).length > 0
                onTriggered: editor.nestSelection()
            }
            MenuItem {
                text: "Open nested sequence"
                enabled: !!(win.selection && win.selection.nested)
                onTriggered: editor.openNested()
            }
            MenuItem {
                text: "Take nested sequence apart"
                enabled: !!(win.selection && win.selection.nested)
                onTriggered: editor.unnest()
            }
            MenuItem {
                text: "Back to the enclosing timeline"
                enabled: (win.s.nesting || []).length > 0
                onTriggered: editor.closeNested()
            }
            MenuSeparator {}
            MenuItem {
                text: "Split at playhead"
                enabled: win.s.selectedId.length > 0
                onTriggered: editor.split()
            }
            MenuItem {
                text: "Duplicate"
                enabled: win.s.selectedId.length > 0
                onTriggered: editor.duplicate()
            }
            MenuItem {
                text: "Delete and close gap on this track"
                enabled: win.s.selectedId.length > 0
                onTriggered: editor.remove(true)
            }
        }
        Menu {
            title: "Captions"
            MenuItem {
                text: "Generate captions (AI)…"
                onTriggered: captionDialog.open()
            }
            MenuItem {
                text: "Translate captions (AI)…"
                onTriggered: translateDialog.open()
            }
            MenuItem {
                text: "Import captions (SRT, VTT, ASS, TXT)…"
                onTriggered: srtOpen.open()
            }
            MenuItem {
                text: "Export titles/captions (SRT, VTT, ASS)…"
                onTriggered: srtSave.open()
            }
        }
        Menu {
            title: "Help"
            MenuItem {
                text: "Search commands…"
                onTriggered: commandSearch.open()
            }
            MenuItem {
                text: "Keyboard shortcuts…"
                onTriggered: shortcutsDialog.open()
            }
            MenuItem {
                objectName: "openLogFolder"
                text: "Open the log folder"
                onTriggered: Qt.openUrlExternally("file:///" + String(win.s.logPath || "").replace(/\\/g, "/").replace(/^\/+/, "").replace(/\/[^\/]*$/, ""))
            }
            MenuItem {
                text: "About this alpha"
                onTriggered: about.open()
            }
        }
    }
    // Command search: every menu command and keyboard command by name.
    Popup {
        id: commandSearch
        objectName: "commandSearch"
        parent: Overlay.overlay
        x: Math.round((win.width - width) / 2)
        y: 70
        width: Math.min(560, win.width - 40)
        height: Math.min(440, win.height - 120)
        modal: true
        focus: true
        property var entries: []
        property var found: []
        // Menu items, submenus included, then keyboard commands not already found by name.
        function collect() {
            const list = [];
            const seen = {};
            const plain = text => text.replace(/[….]+$/, "").trim().toLowerCase();
            function walk(menu, path) {
                for (let i = 0; i < menu.count; ++i) {
                    const item = menu.itemAt(i);
                    if (!item || !item.text)
                        continue;
                    if (item.subMenu) {
                        walk(item.subMenu, path + " › " + item.subMenu.title);
                        continue;
                    }
                    const key = plain(item.text);
                    if (seen[key])
                        continue;
                    seen[key] = true;
                    list.push({ label: item.text, where: path, keys: "", item: item, id: "" });
                }
            }
            for (let m = 0; m < win.menuBar.count; ++m) {
                const menu = win.menuBar.menuAt(m);
                walk(menu, menu.title);
            }
            for (const b of shortcutSettings.bindings) {
                const key = plain(b.label);
                if (seen[key])
                    continue;
                seen[key] = true;
                list.push({ label: b.label, where: b.category, keys: b.sequence, item: null, id: b.id });
            }
            entries = list;
        }
        function filter() {
            const words = searchField.text.toLowerCase().split(/\s+/).filter(w => w.length > 0);
            found = entries.filter(e => {
                const text = (e.label + " " + e.where).toLowerCase();
                return words.every(w => text.indexOf(w) >= 0) && (e.item === null || e.item.enabled);
            });
            results.currentIndex = found.length > 0 ? 0 : -1;
        }
        function run(index) {
            const e = found[index];
            if (!e)
                return;
            close();
            if (e.item)
                e.item.triggered();
            else
                win.command(e.id);
        }
        onAboutToShow: {
            searchField.text = "";
            collect();
            filter();
        }
        onOpened: searchField.forceActiveFocus()
        background: Rectangle {
            color: "#1b2129"
            border.color: "#3a4655"
            radius: 8
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            TextField {
                id: searchField
                objectName: "commandSearchField"
                Layout.fillWidth: true
                placeholderText: "Search commands, e.g. export frame"
                onTextChanged: commandSearch.filter()
                Keys.onDownPressed: results.currentIndex = Math.min(results.count - 1, results.currentIndex + 1)
                Keys.onUpPressed: results.currentIndex = Math.max(0, results.currentIndex - 1)
                Keys.onReturnPressed: commandSearch.run(results.currentIndex)
                Keys.onEnterPressed: commandSearch.run(results.currentIndex)
            }
            ListView {
                id: results
                objectName: "commandResults"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: commandSearch.found
                highlightMoveDuration: 0
                delegate: ItemDelegate {
                    required property var modelData
                    required property int index
                    width: ListView.view.width
                    highlighted: ListView.isCurrentItem
                    onClicked: commandSearch.run(index)
                    contentItem: RowLayout {
                        Label {
                            text: modelData.label
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        Label {
                            text: modelData.where
                            color: win.muted
                            font.pixelSize: 11
                        }
                        Label {
                            visible: modelData.keys.length > 0
                            text: modelData.keys
                            color: win.mint
                            font.pixelSize: 11
                        }
                    }
                }
            }
            Label {
                visible: results.count === 0
                text: "No command matches"
                color: win.muted
            }
        }
    }
    Instantiator {
        model: shortcutSettings.bindings
        delegate: Shortcut {
            required property var modelData
            sequence: modelData.sequence
            enabled: modelData.sequence.length > 0 && win.shortcutEnabled(modelData.id)
            onActivated: win.command(modelData.id)
        }
    }
    DropArea {
        id: fileImportDrop
        anchors.fill: parent
        enabled: !win.shortcutsBlocked
        onEntered: function (drag) {
            drag.accepted = drag.hasUrls;
        }
        onDropped: function (drop) {
            if (drop.hasUrls) {
                editor.importMedia(drop.urls);
                drop.acceptProposedAction();
            }
        }
    }
    Rectangle {
        parent: Overlay.overlay
        visible: fileImportDrop.containsDrag
        anchors.centerIn: parent
        width: 350
        height: 58
        radius: 10
        color: "#183b34"
        border.color: win.mint
        Label {
            anchors.centerIn: parent
            text: "Drop files to import into the media library"
        }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        // Top bar: name and save state in the middle, saving and exporting on the right.
        Rectangle {
            objectName: "topBar"
            Layout.fillWidth: true
            implicitHeight: 46
            color: "#12171d"
            Label {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: "CUTLERY"
                font.pixelSize: 15
                font.bold: true
                font.letterSpacing: 3
                color: win.mint
            }
            // Open projects, when there is more than one: a click shows one, × closes it.
            Row {
                objectName: "projectTabs"
                anchors.left: parent.left
                anchors.leftMargin: 140
                anchors.verticalCenter: parent.verticalCenter
                // Up to the project name in the middle; further tabs are cut off.
                width: Math.max(0, Math.min(implicitWidth, parent.width / 2 - 260))
                clip: true
                spacing: 4
                visible: (win.s.projects || []).length > 1
                Repeater {
                    model: win.s.projects || []
                    AbstractButton {
                        id: projectTab
                        required property var modelData
                        required property int index
                        objectName: "projectTab-" + index
                        width: Math.min(160, tabRow.implicitWidth + 16)
                        height: 30
                        hoverEnabled: true
                        onClicked: editor.switchProject(index)
                        ToolTip.visible: hovered && modelData.path !== ""
                        ToolTip.text: modelData.path
                        background: Rectangle {
                            radius: 6
                            color: projectTab.modelData.current ? "#26313b" : projectTab.hovered ? "#1c242c" : "transparent"
                            border.color: projectTab.modelData.current ? "#35404b" : "transparent"
                        }
                        contentItem: RowLayout {
                            id: tabRow
                            spacing: 4
                            Label {
                                Layout.fillWidth: true
                                Layout.maximumWidth: 120
                                leftPadding: 4
                                text: (projectTab.modelData.dirty ? "• " : "") + projectTab.modelData.name
                                elide: Text.ElideRight
                                font.pixelSize: 11
                                color: projectTab.modelData.current ? "#e7edf2" : win.muted
                            }
                            ToolButton {
                                objectName: "closeProjectTab-" + projectTab.index
                                text: "×"
                                implicitWidth: 18
                                implicitHeight: 18
                                padding: 0
                                font.pixelSize: 12
                                onClicked: win.closeProjectAt(projectTab.index)
                                ToolTip.visible: hovered
                                ToolTip.text: "Close " + projectTab.modelData.name
                            }
                        }
                    }
                }
            }
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 0
                Label {
                    objectName: "projectTitle"
                    Layout.alignment: Qt.AlignHCenter
                    text: (win.s.dirty ? "• " : "") + win.s.name
                    font.bold: true
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: win.s.width + " × " + win.s.height + "  ·  " + win.s.fps.toFixed(2).replace(/\.00$/, "") + " fps" + (win.s.dirty ? "  ·  not saved" : "")
                    color: win.muted
                    font.pixelSize: 10
                }
            }
            RowLayout {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8
                Action {
                    text: "Save"
                    implicitHeight: 30
                    onClicked: win.saveProject()
                    ToolTip.visible: hovered
                    ToolTip.text: "Save the project (" + win.shortcut("save") + ")"
                }
                Button {
                    objectName: "exportButton"
                    text: "⇪  Export"
                    implicitHeight: 30
                    enabled: win.s.duration > 0 && !win.s.busy
                    onClicked: exportSettings.open()
                    background: Rectangle {
                        radius: 6
                        color: parent.down ? "#4fbfa5" : parent.hovered ? "#7be3ca" : win.mint
                        opacity: parent.enabled ? 1 : .4
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#0f2620"
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                        rightPadding: 10
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            visible: win.s.error.length > 0
            implicitHeight: errorText.implicitHeight + 22
            color: "#482c2c"
            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                Label {
                    id: errorText
                    text: win.s.error
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: "#ffc2b7"
                }
                Action {
                    objectName: "retryExport"
                    visible: win.s.canRetryExport === true
                    text: "Try export again"
                    onClicked: {
                        editor.clearError();
                        editor.retryExport();
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: "Runs the same export again, e.g. after freeing disk space or reconnecting the drive"
                }
                Action {
                    text: "Dismiss"
                    onClicked: editor.clearError()
                }
            }
        }
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal
            handle: Rectangle {
                implicitWidth: 1
                color: "#303945"
            }
            Rectangle {
                SplitView.preferredWidth: 400
                SplitView.minimumWidth: 330
                color: "#171d24"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 14
                    // Icon tabs: what can be added to the timeline, by kind.
                    RowLayout {
                        id: leftTabs
                        objectName: "leftTabs"
                        Layout.fillWidth: true
                        Layout.leftMargin: -8
                        Layout.rightMargin: -8
                        spacing: 0
                        Repeater {
                            model: win.leftTabList
                            AbstractButton {
                                required property var modelData
                                objectName: "leftTab-" + modelData.id
                                Accessible.name: modelData.label
                                Layout.fillWidth: true
                                implicitHeight: 44
                                readonly property bool active: win.leftTab === modelData.id
                                onClicked: win.leftTab = modelData.id
                                hoverEnabled: true
                                contentItem: ColumnLayout {
                                    spacing: 1
                                    Label {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: parent.parent.modelData.glyph
                                        font.pixelSize: 16
                                        color: parent.parent.active ? win.mint : parent.parent.hovered ? "#e7edf2" : win.muted
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        horizontalAlignment: Text.AlignHCenter
                                        text: parent.parent.modelData.label
                                        font.pixelSize: 9
                                        elide: Text.ElideRight
                                        color: parent.parent.active ? win.mint : parent.parent.hovered ? "#e7edf2" : win.muted
                                    }
                                }
                                background: Item {}
                                ToolTip.visible: hovered
                                ToolTip.text: modelData.label + (modelData.id === "media" ? " (" + editor.assets.length + ")" : "")
                            }
                        }
                    }
                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: win.leftTab === "media" ? 0 : 1
                        ColumnLayout {
                            spacing: 12
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Action {
                                    text: win.s.importing ? "Reading media… " + (win.s.importRemaining || 0) + " left" : "+ Import media"
                                    Layout.fillWidth: true
                                    enabled: !win.s.importing
                                    onClicked: importDialog.open()
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Or drop files or whole folders here (a folder's media goes into a library folder of its name). Drag media onto any track; right-click it to sort it into folders."
                                }
                                Action {
                                    objectName: "cancelImport"
                                    visible: win.s.importing === true
                                    text: "Stop"
                                    onClicked: editor.cancelImport()
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Skip the files not read yet"
                                }
                            }
                            // What the library shows: everything, one kind of media, or a folder; then a
                            // name search within that.
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                ComboBox {
                                    id: libraryView
                                    objectName: "libraryView"
                                    Accessible.name: "Show in the library"
                                    Layout.fillWidth: true
                                    readonly property var kinds: [
                                        { key: "", label: "All media" },
                                        { key: "video", label: "Videos" },
                                        { key: "audio", label: "Audio" },
                                        { key: "image", label: "Images" }
                                    ]
                                    readonly property var folders: win.s.folders || []
                                    // The folder on show, or "" for the media kinds.
                                    readonly property string folder: currentIndex >= kinds.length ? folders[currentIndex - kinds.length] || "" : ""
                                    model: kinds.map(k => k.label).concat(folders.map(f => "▸ " + f))
                                    onFolderChanged: Qt.callLater(() => editor.setImportFolder(libraryView.folder))
                                    property string knownFolders: ""
                                    onFoldersChanged: {
                                        // Keep showing the folder after a rename or when one is added.
                                        const list = JSON.stringify(folders);
                                        if (list === knownFolders)
                                            return;
                                        knownFolders = list;
                                        // After the model has been rebuilt, which resets the choice.
                                        const keep = currentIndex;
                                        Qt.callLater(() => {
                                            const i = folders.indexOf(win.s.importFolder || "");
                                            currentIndex = i >= 0 ? kinds.length + i : keep < kinds.length ? keep : 0;
                                        });
                                    }
                                }
                                ToolButton {
                                    objectName: "addFolder"
                                    text: "+ Folder"
                                    onClicked: folderDialog.ask("", "")
                                    ToolTip.visible: hovered
                                    ToolTip.text: "New folder; media imported while it is on show goes into it"
                                }
                                ToolButton {
                                    text: "⋯"
                                    visible: libraryView.folder !== ""
                                    onClicked: folderMenu.popup()
                                    Menu {
                                        id: folderMenu
                                        MenuItem {
                                            text: "Rename folder…"
                                            onTriggered: folderDialog.ask(libraryView.folder, libraryView.folder)
                                        }
                                        MenuItem {
                                            text: "Delete folder (keeps its media)"
                                            onTriggered: editor.removeFolder(libraryView.folder)
                                        }
                                    }
                                }
                            }
                            // Missing media: find all of it at once in a folder.
                            Rectangle {
                                objectName: "missingMedia"
                                readonly property int count: editor.assets.filter(a => a.missing).length
                                visible: count > 0
                                Layout.fillWidth: true
                                implicitHeight: missingRow.implicitHeight + 16
                                radius: 7
                                color: "#3a2a24"
                                border.color: "#da886e"
                                ColumnLayout {
                                    id: missingRow
                                    anchors.fill: parent
                                    anchors.margins: 8
                                    spacing: 6
                                    Label {
                                        text: parent.parent.count === 1 ? "1 media file is missing." : parent.parent.count + " media files are missing."
                                        wrapMode: Text.Wrap
                                        Layout.fillWidth: true
                                    }
                                    Action {
                                        objectName: "findMissing"
                                        text: "Find in a folder…"
                                        Layout.fillWidth: true
                                        onClicked: relinkFolderDialog.open()
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Looks for files of the same names in the folder and the folders inside it"
                                    }
                                }
                            }
                            TextField {
                                id: librarySearch
                                objectName: "librarySearch"
                                Layout.fillWidth: true
                                placeholderText: "Search media, also by what is said"
                                selectByMouse: true
                            }
                            RowLayout {
                                Label {
                                    text: "Append to"
                                    color: win.muted
                                }
                                ComboBox {
                                    Accessible.name: "Append imported media to"
                                    model: editor.trackList
                                    textRole: "name"
                                    currentIndex: Math.min(win.targetTrack, win.s.tracks - 1)
                                    onActivated: win.targetTrack = currentIndex
                                    Layout.fillWidth: true
                                }
                            }
                            ListView {
                                id: mediaList
                                objectName: "mediaLibrary"
                                ScrollBar.vertical: ScrollBar {}
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                spacing: 8
                                // The library filtered by the view and the search words (all must match).
                                model: {
                                    const kind = libraryView.currentIndex >= 0 && libraryView.currentIndex < libraryView.kinds.length ? libraryView.kinds[libraryView.currentIndex].key : "";
                                    const folder = libraryView.folder;
                                    // Names, files, folders, sizes, rights and what is said (cached transcripts).
                                    const found = librarySearch.text.trim().length > 0 ? editor.searchMedia(librarySearch.text) : null;
                                    return editor.assets.filter(a => (folder === "" || a.folder === folder) && (kind === "" || a.kind === kind) && (!found || found[a.id] !== undefined))
                                        .map(a => Object.assign({ said: found ? found[a.id] : "" }, a));
                                }
                                delegate: Rectangle {
                                    id: mediaTile
                                    required property var modelData
                                    objectName: "asset-" + modelData.id
                                    property string assetId: modelData.id
                                    property real mediaDuration: modelData.seconds
                                    Component.onDestruction: {
                                        if (win.libraryGesture === mediaMouse)
                                            win.libraryGesture = null;
                                    }
                                    width: ListView.view.width
                                    height: 76
                                    radius: 7
                                    color: mediaMouse.containsMouse ? "#2a3640" : "#222b34"
                                    border.color: modelData.missing ? "#da886e" : "#34404c"
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 10
                                        Rectangle {
                                            id: poster
                                            property var strip: ({})
                                            function refresh() {
                                                strip = mediaTile.modelData.kind === "audio" ? ({}) : editor.thumbnails(mediaTile.modelData.id);
                                            }
                                            Component.onCompleted: refresh()
                                            Connections {
                                                target: editor
                                                function onThumbnailsChanged() {
                                                    poster.refresh();
                                                }
                                            }
                                            Layout.preferredWidth: poster.strip.status === "ready" ? 75 : 38
                                            Layout.preferredHeight: 42
                                            radius: 5
                                            clip: true
                                            color: modelData.kind === "audio" ? "#344c4e" : "#354255"
                                            // Poster frame: the tile from the middle of the media.
                                            Image {
                                                anchors.fill: parent
                                                visible: poster.strip.status === "ready"
                                                source: visible ? poster.strip.url : ""
                                                sourceClipRect: visible ? Qt.rect(Math.floor(poster.strip.count / 2) * poster.strip.tileWidth, 0, poster.strip.tileWidth, poster.strip.tileHeight) : Qt.rect(0, 0, 0, 0)
                                                fillMode: Image.PreserveAspectCrop
                                                asynchronous: true
                                            }
                                            // "Added": the media is used on the timeline.
                                            Rectangle {
                                                objectName: "added-" + mediaTile.modelData.id
                                                visible: mediaTile.modelData.used === true
                                                z: 1
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.bottom: parent.bottom
                                                height: 13
                                                color: "#cc12403a"
                                                Label {
                                                    anchors.centerIn: parent
                                                    text: "Added"
                                                    font.pixelSize: 8
                                                    font.bold: true
                                                    color: win.mint
                                                }
                                            }
                                            Label {
                                                anchors.centerIn: parent
                                                visible: poster.strip.status !== "ready"
                                                text: modelData.kind === "audio" ? "♫" : modelData.kind === "image" ? "▧" : "▶"
                                                color: win.mint
                                                font.pixelSize: 20
                                            }
                                        }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 5
                                            Label {
                                                text: modelData.name
                                                elide: Text.ElideMiddle
                                                Layout.fillWidth: true
                                                font.bold: true
                                            }
                                            Label {
                                                objectName: "said-" + modelData.id
                                                visible: !!modelData.said
                                                text: "“" + modelData.said + "”"
                                                font.pixelSize: 10
                                                font.italic: true
                                                color: win.mint
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                            Label {
                                                text: (modelData.missing ? "Missing • relink in inspector" : modelData.kind.toUpperCase() + "  ·  " + modelData.seconds.toFixed(1) + "s" + (modelData.folder && libraryView.folder === "" ? "  ·  ▸ " + modelData.folder : "")) + (modelData.proxy === "ready" ? "  ·  proxy" : modelData.proxy === "making" ? "  ·  proxy " + Math.round(100 * modelData.proxyProgress) + "%" : modelData.proxy === "queued" ? "  ·  proxy waiting" : "") + (modelData.rights === "personal" ? "  ·  ⚠ personal use" : modelData.rights === "unknown" ? "  ·  ⚠ rights unknown" : modelData.rights === "attribution" ? "  ·  credit needed" : "")
                                                font.pixelSize: 10
                                                color: win.muted
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }
                                    }
                                    Rectangle {
                                        id: libraryDrag
                                        parent: win.contentItem
                                        z: 1000
                                        width: 180
                                        height: 42
                                        radius: 7
                                        color: "#28564c"
                                        border.color: win.mint
                                        opacity: .92
                                        visible: mediaMouse.dragging
                                        Drag.active: mediaMouse.dragging
                                        Drag.source: mediaTile
                                        Drag.keys: ["cutlery/asset"]
                                        Drag.supportedActions: Qt.CopyAction
                                        Drag.proposedAction: Qt.CopyAction
                                        Drag.hotSpot.x: 12
                                        Drag.hotSpot.y: 12
                                        Label {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            text: mediaTile.modelData.name
                                            elide: Text.ElideRight
                                        }
                                    }
                                    MouseArea {
                                        id: mediaMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        preventStealing: true
                                        enabled: !mediaTile.modelData.missing
                                        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                        property bool dragging: false
                                        property bool cancelled: false
                                        property point pressedAt
                                        function cancelDrag() {
                                            cancelled = true;
                                            libraryDrag.Drag.cancel();
                                            dragging = false;
                                            win.libraryGesture = null;
                                        }
                                        onPressed: function (mouse) {
                                            const p = mapToItem(win.contentItem, mouse.x, mouse.y);
                                            pressedAt = p;
                                            cancelled = false;
                                            win.libraryGesture = mediaMouse;
                                            libraryDrag.x = p.x - 12;
                                            libraryDrag.y = p.y - 12;
                                        }
                                        onPositionChanged: function (mouse) {
                                            if (!pressed || cancelled)
                                                return;
                                            const p = mapToItem(win.contentItem, mouse.x, mouse.y);
                                            libraryDrag.x = p.x - 12;
                                            libraryDrag.y = p.y - 12;
                                            if (Math.abs(p.x - pressedAt.x) + Math.abs(p.y - pressedAt.y) > 6)
                                                dragging = true;
                                        }
                                        onReleased: {
                                            if (dragging)
                                                libraryDrag.Drag.drop();
                                            dragging = false;
                                            win.libraryGesture = null;
                                        }
                                        onCanceled: cancelDrag()
                                        onDoubleClicked: editor.addAsset(modelData.id, Math.min(win.targetTrack, win.s.tracks - 1))
                                    }
                                    // Right-click: move to a folder, or take unused media out of the library.
                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        onClicked: assetMenu.popup()
                                    }
                                    Menu {
                                        id: assetMenu
                                        objectName: "assetMenu-" + mediaTile.modelData.id
                                        Menu {
                                            id: moveMenu
                                            title: "Move to folder"
                                            enabled: (win.s.folders || []).length > 0
                                            MenuItem {
                                                text: "Top level"
                                                enabled: !!mediaTile.modelData.folder
                                                onTriggered: editor.moveToFolder([mediaTile.modelData.id], "")
                                            }
                                            Instantiator {
                                                model: win.s.folders || []
                                                delegate: MenuItem {
                                                    required property string modelData
                                                    text: modelData
                                                    enabled: modelData !== mediaTile.modelData.folder
                                                    onTriggered: editor.moveToFolder([mediaTile.modelData.id], modelData)
                                                }
                                                onObjectAdded: (index, object) => moveMenu.insertItem(index + 1, object)
                                                onObjectRemoved: (index, object) => moveMenu.removeItem(object)
                                            }
                                        }
                                        MenuItem {
                                            objectName: "convertMedia"
                                            text: "Convert or compress…"
                                            enabled: ["video", "audio"].indexOf(mediaTile.modelData.kind) >= 0 && !win.s.busy
                                            onTriggered: {
                                                win.convertAsset = mediaTile.modelData.id;
                                                exportSettings.open();
                                            }
                                        }
                                        MenuItem {
                                            objectName: "makeProxy"
                                            visible: mediaTile.modelData.kind === "video" && !mediaTile.modelData.nested
                                            height: visible ? implicitHeight : 0
                                            text: mediaTile.modelData.proxy === "ready" ? "Editing proxy made" : mediaTile.modelData.proxy !== "" ? "Making the editing proxy…" : "Make an editing proxy"
                                            enabled: mediaTile.modelData.proxy === "" && !mediaTile.modelData.missing
                                            onTriggered: editor.makeProxies([mediaTile.modelData.id])
                                            ToolTip.visible: hovered
                                            ToolTip.text: "A small copy (540 lines) that the preview plays instead, for smooth editing of large videos. Exports always use the original."
                                        }
                                        MenuItem {
                                            objectName: "overwriteAtPlayhead"
                                            text: "Overwrite at the playhead (track " + (Math.min(win.targetTrack, win.s.tracks - 1) + 1) + ")"
                                            onTriggered: editor.overwriteAsset(mediaTile.modelData.id, Math.min(win.targetTrack, win.s.tracks - 1), win.s.playhead)
                                        }
                                        MenuItem {
                                            text: "Replace with another file…"
                                            onTriggered: {
                                                win.relinkAsset = mediaTile.modelData.id;
                                                relinkDialog.open();
                                            }
                                        }
                                        MenuItem {
                                            objectName: "assetRights"
                                            text: "Usage rights…"
                                            onTriggered: rightsDialog.ask(mediaTile.modelData)
                                        }
                                        MenuItem {
                                            text: "New folder with this media…"
                                            onTriggered: folderDialog.ask("", "", mediaTile.modelData.id)
                                        }
                                        MenuItem {
                                            text: mediaTile.modelData.used ? "Remove from library (used on the timeline)" : "Remove from library"
                                            enabled: !mediaTile.modelData.used
                                            onTriggered: editor.removeAssets([mediaTile.modelData.id])
                                        }
                                    }
                                }
                                Label {
                                    anchors.centerIn: parent
                                    visible: editor.assets.length > 0 && mediaList.count === 0
                                    text: librarySearch.text.length > 0 ? "No media matches." : "Nothing here yet.\nImport media while this is on show,\nor right-click media to move it here."
                                    horizontalAlignment: Text.AlignHCenter
                                    color: win.muted
                                    lineHeight: 1.5
                                }
                                Label {
                                    anchors.centerIn: parent
                                    visible: editor.assets.length === 0
                                    text: "A blank canvas.\nBring your footage."
                                    horizontalAlignment: Text.AlignHCenter
                                    color: win.muted
                                    lineHeight: 1.5
                                }
                            }
                        }
                        // A narrow column of the tab's categories; a click scrolls to one.
                        RowLayout {
                            spacing: 6
                            ColumnLayout {
                                id: categoryColumn
                                objectName: "categoryColumn"
                                property string current: ""
                                Layout.preferredWidth: 78
                                Layout.maximumWidth: 78
                                Layout.fillWidth: false
                                Layout.alignment: Qt.AlignTop
                                visible: win.leftCategories.length > 1
                                spacing: 2
                                Connections {
                                    target: win
                                    function onLeftTabChanged() {
                                        categoryColumn.current = "";
                                    }
                                }
                                Repeater {
                                    model: win.allCategories
                                    AbstractButton {
                                        id: categoryButton
                                        required property var modelData
                                        required property int index
                                        readonly property bool active: categoryColumn.current === modelData.target || (categoryColumn.current === "" && modelData.target === "top")
                                        visible: modelData.tab === win.leftTab
                                        objectName: visible ? "category-" + modelData.target : ""
                                        Accessible.name: modelData.name
                                        Layout.fillWidth: true
                                        implicitHeight: 28
                                        hoverEnabled: true
                                        onClicked: {
                                            categoryColumn.current = modelData.target;
                                            const flick = addTabScroll.contentItem;
                                            const item = modelData.target === "top" ? null : win.findByName(addTabScroll.contentChildren[0], modelData.target);
                                            const y = item ? item.mapToItem(flick.contentItem, 0, 0).y - 4 : 0;
                                            flick.contentY = Math.max(0, Math.min(y, addTabScroll.contentChildren[0].height - flick.height));
                                        }
                                        contentItem: Label {
                                            text: categoryButton.modelData.name
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                            leftPadding: 8
                                            verticalAlignment: Text.AlignVCenter
                                            color: categoryButton.active ? "#e7edf2" : categoryButton.hovered ? "#c7d0d8" : win.muted
                                        }
                                        background: Rectangle {
                                            radius: 5
                                            color: categoryButton.active ? "#26313b" : "transparent"
                                        }
                                    }
                                }
                            }
                            // Titles, graphics, effect areas and sounds.
                            // Scrolls when the window is too low for all of it.
                            ScrollView {
                                id: addTabScroll
                                objectName: "addTabScroll"
                                Layout.fillWidth: true
                                // Not its content's width: that follows the scroll view's own width.
                                Layout.preferredWidth: 1
                                Layout.fillHeight: true
                                clip: true
                                contentWidth: availableWidth
                                ColumnLayout {
                                    width: parent.width
                                    spacing: 12
                                    RowLayout {
                                        visible: win.leftTab === "text" || win.leftTab === "audio"
                                        Layout.fillWidth: true
                                        Action {
                                            objectName: "addTitle"
                                            visible: win.leftTab === "text"
                                            text: "+ Add title"
                                            Layout.fillWidth: true
                                            onClicked: editor.addTitle()
                                        }
                                        Action {
                                            visible: win.leftTab === "text"
                                            objectName: "addCaption"
                                            text: "+ Caption"
                                            Layout.fillWidth: true
                                            onClicked: editor.addCaption()
                                            ToolTip.visible: hovered
                                            ToolTip.text: "A caption at the playhead on the caption track, 2 seconds or up to the next one"
                                        }
                                        // Sound effects: clicks, typing and swooshes for tutorials and screen videos.
                                        Action {
                                            visible: win.leftTab === "audio"
                                            objectName: "openSounds"
                                            text: "♪ Listen and more…"
                                            Layout.fillWidth: true
                                            onClicked: soundDialog.open()
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Listen to the sound effects, or put a whoosh on every transition"
                                        }
                                    }
                                    RowLayout {
                                        visible: win.leftTab === "text"
                                        Layout.fillWidth: true
                                        Action {
                                            objectName: "addLowerThird"
                                            text: "+ Lower third"
                                            Layout.fillWidth: true
                                            onClicked: editor.addTitleTemplate("lowerThird")
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Name and role in the lower left; slides in, fades out"
                                        }
                                        Action {
                                            objectName: "addTitleCard"
                                            text: "+ Title card"
                                            Layout.fillWidth: true
                                            onClicked: editor.addTitleTemplate("titleCard")
                                            ToolTip.visible: hovered
                                            ToolTip.text: "A large centred heading with a subtitle, e.g. for chapters"
                                        }
                                    }
                                    RowLayout {
                                        visible: win.leftTab === "text"
                                        Layout.fillWidth: true
                                        Action {
                                            objectName: "addLowerThirdRight"
                                            text: "+ Right third"
                                            Layout.fillWidth: true
                                            onClicked: editor.addTitleTemplate("lowerThirdRight")
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Name and role in the lower right; slides in from the right"
                                        }
                                        Action {
                                            objectName: "addBanner"
                                            text: "+ Banner"
                                            Layout.fillWidth: true
                                            onClicked: editor.addTitleTemplate("banner")
                                            ToolTip.visible: hovered
                                            ToolTip.text: "A band across the bottom with a headline and a line below, e.g. a call to action"
                                        }
                                        Action {
                                            objectName: "addQuote"
                                            text: "+ Quote"
                                            Layout.fillWidth: true
                                            onClicked: editor.addTitleTemplate("quote")
                                            ToolTip.visible: hovered
                                            ToolTip.text: "A quotation in the centre with a large quotation mark; the second line names who said it"
                                        }
                                    }
                                    RowLayout {
                                        visible: win.leftTab === "effects" || win.leftTab === "stickers"
                                        Layout.fillWidth: true
                                        Action {
                                            visible: win.leftTab === "effects"
                                            objectName: "addBlurArea"
                                            text: "+ Blur area"
                                            Layout.fillWidth: true
                                            onClicked: editor.addEffect("blur")
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Blurs whatever lower tracks show inside a rectangle, e.g. private data in a screen recording"
                                        }
                                        Action {
                                            visible: win.leftTab === "effects"
                                            objectName: "addMosaicArea"
                                            text: "+ Mosaic area"
                                            Layout.fillWidth: true
                                            onClicked: editor.addEffect("pixelate")
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Pixelates whatever lower tracks show inside a rectangle, e.g. a face"
                                        }
                                        // Shapes for tutorials and explainers.
                                        Action {
                                            visible: win.leftTab === "stickers"
                                            objectName: "addShape"
                                            text: "+ Shape ▾"
                                            Layout.fillWidth: true
                                            onClicked: shapeMenu.popup()
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Arrow, circle, speech bubble, box or line"
                                            Menu {
                                                id: shapeMenu
                                                MenuItem {
                                                    objectName: "addGraphic-arrow"
                                                    text: "➜  Arrow"
                                                    onTriggered: editor.addGraphic("arrow")
                                                }
                                                MenuItem {
                                                    objectName: "addGraphic-ellipse"
                                                    text: "◯  Circle"
                                                    onTriggered: editor.addGraphic("ellipse")
                                                }
                                                MenuItem {
                                                    objectName: "addGraphic-bubble"
                                                    text: "🗨  Speech bubble"
                                                    onTriggered: editor.addGraphic("bubble")
                                                }
                                                MenuItem {
                                                    objectName: "addGraphic-rectangle"
                                                    text: "▭  Box"
                                                    onTriggered: editor.addGraphic("rectangle")
                                                }
                                                MenuItem {
                                                    objectName: "addGraphic-line"
                                                    text: "―  Line"
                                                    onTriggered: editor.addGraphic("line")
                                                }
                                                MenuItem {
                                                    objectName: "addGraphic-badge"
                                                    text: "①  Numbered step (1, 2, 3 …)"
                                                    onTriggered: editor.addGraphic("badge")
                                                }
                                            }
                                        }
                                    }
                                    // Layouts: arrange the selected pictures (Ctrl+click several) at once.
                                    Caption {
                                        visible: win.leftTab === "layouts"
                                        text: "ARRANGE SELECTED"
                                    }
                                    CheckBox {
                                        visible: win.leftTab === "layouts"
                                        id: arrangeFill
                                        objectName: "arrangeFill"
                                        text: "Fill each area (crop)"
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Crops each picture to the shape of its area so there are no empty edges; off fits the whole picture inside"
                                    }
                                    GridLayout {
                                        visible: win.leftTab === "layouts"
                                        Layout.fillWidth: true
                                        columns: 2
                                        columnSpacing: 4
                                        rowSpacing: 4
                                        Repeater {
                                            model: [
                                                { id: "side", label: "▯▯ Side by side", tip: "Two pictures next to each other" },
                                                { id: "stack", label: "▭ Stacked", tip: "One above the other, e.g. for 9:16" },
                                                { id: "grid", label: "⊞ 2 × 2", tip: "Up to four pictures in a grid" },
                                                { id: "presenter", label: "◐ Presenter", tip: "Screen large on the left, the presenter round in the lower right" },
                                                { id: "pip-br", label: "▣ Picture in picture", tip: "The lower track full, the other small in the lower right corner" },
                                                { id: "full", label: "□ Full size", tip: "Back to full size" }
                                            ]
                                            Action {
                                                required property var modelData
                                                objectName: "arrange-" + modelData.id
                                                Layout.fillWidth: true
                                                text: modelData.label
                                                enabled: (win.s.selectedIds || []).length > (modelData.id === "full" ? 0 : 1)
                                                onClicked: editor.arrange(modelData.id, arrangeFill.checked)
                                                ToolTip.visible: hovered
                                                ToolTip.text: modelData.tip + ". Select the clips first (Ctrl+click)."
                                            }
                                        }
                                    }
                                    // Own layouts: the places of the selected pictures, kept for every project.
                                    RowLayout {
                                        visible: win.leftTab === "layouts"
                                        Layout.fillWidth: true
                                        ComboBox {
                                            id: layoutChoice
                                            objectName: "layoutChoice"
                                            Accessible.name: "Layout"
                                            Layout.fillWidth: true
                                            readonly property var layouts: win.s.layouts || []
                                            model: [layouts.length ? "My layouts…" : "No saved layouts"].concat(layouts.map(l => l.name + " (" + l.count + ")"))
                                            enabled: layouts.length > 0 && (win.s.selectedIds || []).length > 0
                                            onActivated: index => {
                                                if (index > 0)
                                                    editor.applyLayout(layouts[index - 1].name);
                                                currentIndex = 0;
                                            }
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Puts the selected pictures in the places of a saved layout, lowest track first"
                                        }
                                        ToolButton {
                                            objectName: "saveLayout"
                                            text: "Save…"
                                            enabled: (win.s.selectedIds || []).length > 0
                                            onClicked: layoutDialog.open()
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Keeps the places, sizes, crops and frames of the selected pictures as a layout for every project"
                                        }
                                        ToolButton {
                                            text: "⋯"
                                            visible: layoutChoice.layouts.length > 0
                                            onClicked: layoutMenu.popup()
                                            Menu {
                                                id: layoutMenu
                                                Instantiator {
                                                    model: layoutChoice.layouts
                                                    delegate: MenuItem {
                                                        required property var modelData
                                                        text: "Remove “" + modelData.name + "”"
                                                        onTriggered: editor.removeLayout(modelData.name)
                                                    }
                                                    onObjectAdded: (index, object) => layoutMenu.insertItem(index, object)
                                                    onObjectRemoved: (index, object) => layoutMenu.removeItem(object)
                                                }
                                            }
                                        }
                                    }
                                    // Icons for tutorials: a click goes on at the playhead, coloured and sized
                                    // like shapes.
                                    Caption {
                                        objectName: "catIcons"
                                        visible: win.leftTab === "stickers"
                                        text: "ICONS"
                                    }
                                    GridLayout {
                                        visible: win.leftTab === "stickers"
                                        Layout.fillWidth: true
                                        columns: 5
                                        columnSpacing: 4
                                        rowSpacing: 4
                                        Repeater {
                                            model: [
                                                { kind: "check", glyph: "✔", name: "Check mark" },
                                                { kind: "cross", glyph: "✖", name: "Cross" },
                                                { kind: "warning", glyph: "⚠", name: "Warning" },
                                                { kind: "info", glyph: "ℹ", name: "Info" },
                                                { kind: "star", glyph: "★", name: "Star" },
                                                { kind: "heart", glyph: "♥", name: "Heart" },
                                                { kind: "lightbulb", glyph: "💡", name: "Light bulb (tip)" },
                                                { kind: "cursor", glyph: "↖", name: "Mouse pointer" },
                                                { kind: "click", glyph: "✳", name: "Mouse click" },
                                                { kind: "play", glyph: "▶", name: "Play button" },
                                                { kind: "bell", glyph: "🔔", name: "Bell (e.g. notifications)" },
                                                { kind: "pin", glyph: "📍", name: "Location pin" },
                                                { kind: "clock", glyph: "🕒", name: "Clock" }
                                            ]
                                            ToolButton {
                                                required property var modelData
                                                objectName: "addIcon-" + modelData.kind
                                                Accessible.name: "Add icon: " + modelData.name
                                                Layout.fillWidth: true
                                                text: modelData.glyph
                                                font.pixelSize: 18
                                                onClicked: editor.addGraphic(modelData.kind)
                                                ToolTip.visible: hovered
                                                ToolTip.text: modelData.name
                                            }
                                        }
                                    }
                                    // Brand kit (every project): colours offered next to the colour settings,
                                    // and a logo put in a corner for the whole video.
                                    Caption {
                                        objectName: "catBrand"
                                        visible: win.leftTab === "stickers"
                                        text: "BRAND KIT"
                                    }
                                    Flow {
                                        visible: win.leftTab === "stickers"
                                        Layout.fillWidth: true
                                        spacing: 4
                                        Repeater {
                                            model: win.s.brandColors || []
                                            Rectangle {
                                                required property string modelData
                                                objectName: "brandColor-" + modelData
                                                width: 22
                                                height: 22
                                                radius: 11
                                                color: modelData
                                                border.width: 1
                                                border.color: "#6481a0"
                                                MouseArea {
                                                    anchors.fill: parent
                                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                    hoverEnabled: true
                                                    onClicked: mouse => {
                                                        if (mouse.button === Qt.RightButton)
                                                            editor.removeBrandColor(parent.modelData);
                                                    }
                                                    ToolTip.visible: containsMouse
                                                    ToolTip.text: parent.modelData + " (right-click to remove)"
                                                }
                                            }
                                        }
                                        Label {
                                            visible: (win.s.brandColors || []).length === 0
                                            text: "No brand colours yet"
                                            color: win.muted
                                            font.pixelSize: 11
                                        }
                                    }
                                    RowLayout {
                                        visible: win.leftTab === "stickers"
                                        Layout.fillWidth: true
                                        TextField {
                                            id: brandColorField
                                            objectName: "brandColorField"
                                            Layout.fillWidth: true
                                            placeholderText: "#rrggbb"
                                            onAccepted: {
                                                editor.addBrandColor(text);
                                                text = "";
                                            }
                                        }
                                        Action {
                                            objectName: "addBrandColor"
                                            text: "+ Colour"
                                            padding: 6
                                            onClicked: {
                                                editor.addBrandColor(brandColorField.text || win.selection.textColor || win.selection.fillColor || "");
                                                brandColorField.text = "";
                                            }
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Keeps the colour typed here (or the selected title's or shape's colour) for every project"
                                        }
                                    }
                                    RowLayout {
                                        visible: win.leftTab === "stickers"
                                        Layout.fillWidth: true
                                        Image {
                                            visible: !!win.s.brandLogo
                                            source: win.s.brandLogo ? "file:///" + win.s.brandLogo.replace(/^\/+/, "") : ""
                                            sourceSize.height: 28
                                            Layout.preferredHeight: 28
                                            Layout.preferredWidth: 56
                                            fillMode: Image.PreserveAspectFit
                                        }
                                        Action {
                                            objectName: "chooseBrandLogo"
                                            text: win.s.brandLogo ? "Change logo…" : "Choose logo…"
                                            Layout.fillWidth: true
                                            padding: 6
                                            onClicked: logoDialog.open()
                                        }
                                        Action {
                                            objectName: "addBrandLogo"
                                            text: "+ Logo ▾"
                                            padding: 6
                                            enabled: !!win.s.brandLogo
                                            onClicked: logoMenu.popup()
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Puts the logo small in a corner for the whole video, on its own track"
                                            Menu {
                                                id: logoMenu
                                                MenuItem {
                                                    objectName: "addBrandLogo-topLeft"
                                                    text: "◸  Top left"
                                                    onTriggered: editor.addBrandLogo("topLeft")
                                                }
                                                MenuItem {
                                                    objectName: "addBrandLogo-topRight"
                                                    text: "◹  Top right"
                                                    onTriggered: editor.addBrandLogo("topRight")
                                                }
                                                MenuItem {
                                                    objectName: "addBrandLogo-bottomLeft"
                                                    text: "◺  Bottom left"
                                                    onTriggered: editor.addBrandLogo("bottomLeft")
                                                }
                                                MenuItem {
                                                    objectName: "addBrandLogo-bottomRight"
                                                    text: "◿  Bottom right"
                                                    onTriggered: editor.addBrandLogo("bottomRight")
                                                }
                                            }
                                        }
                                    }
                                    Action {
                                        visible: win.leftTab === "effects"
                                        objectName: "addAdjustment"
                                        text: "+ Adjustment layer"
                                        Layout.fillWidth: true
                                        onClicked: editor.addEffect("adjust")
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Its colour and look change everything on the tracks below while it runs, e.g. one grade for a whole scene"
                                    }
                                    // Text: automatic captions from what is said.
                                    Action {
                                        objectName: "openAutoCaptions"
                                        visible: win.leftTab === "text"
                                        text: "Auto captions…"
                                        Layout.fillWidth: true
                                        onClicked: captionDialog.open()
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Captions from what is said in the timeline, made on this computer (AI pack)"
                                    }
                                    // Audio: the library's sound files, and recording.
                                    Action {
                                        objectName: "showAudioMedia"
                                        visible: win.leftTab === "audio"
                                        text: "Sound files in the library"
                                        Layout.fillWidth: true
                                        onClicked: {
                                            libraryView.currentIndex = 2;
                                            win.leftTab = "media";
                                        }
                                    }
                                    Label {
                                        visible: win.leftTab === "audio"
                                        Layout.fillWidth: true
                                        wrapMode: Text.Wrap
                                        color: win.muted
                                        font.pixelSize: 11
                                        text: "Record a voice-over with ● Voice-over under the player; it lands at the playhead."
                                    }
                                    // Text to speech (AI pack): typed text, spoken at the playhead.
                                    ColumnLayout {
                                        id: speechPanel
                                        objectName: "speechPanel"
                                        visible: win.leftTab === "audio"
                                        Layout.fillWidth: true
                                        spacing: 6
                                        readonly property var speech: win.s.speech || ({})
                                        readonly property bool ready: (speech.missing || "") === ""
                                        readonly property bool busy: speech.status === "speaking"
                                        Caption {
                                            objectName: "cat-speech"
                                            text: "TEXT TO SPEECH"
                                        }
                                        Label {
                                            objectName: "speechMissing"
                                            visible: !speechPanel.ready
                                            Layout.fillWidth: true
                                            wrapMode: Text.Wrap
                                            color: win.muted
                                            font.pixelSize: 11
                                            text: (speechPanel.speech.missing || "") + " Download it next to Cutlery.exe."
                                        }
                                        TextArea {
                                            id: speechText
                                            objectName: "speechText"
                                            Accessible.name: "Text to speak"
                                            enabled: speechPanel.ready
                                            placeholderText: "Type what should be said…"
                                            wrapMode: TextEdit.Wrap
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 70
                                            selectByMouse: true
                                            background: Rectangle {
                                                color: "#10161c"
                                                radius: 5
                                                border.color: speechText.activeFocus ? "#64d8bc" : "#35404b"
                                            }
                                        }
                                        ComboBox {
                                            objectName: "speechVoice"
                                            Accessible.name: "Voice"
                                            visible: speechPanel.ready
                                            Layout.fillWidth: true
                                            readonly property var voices: speechPanel.speech.voices || []
                                            model: voices.map(v => v.name + " · " + v.language)
                                            currentIndex: Math.max(0, voices.findIndex(v => v.id === win.currentVoice))
                                            onActivated: win.speechVoice = voices[currentIndex].id
                                        }
                                        RowLayout {
                                            visible: speechPanel.ready
                                            Layout.fillWidth: true
                                            Label {
                                                text: "Speed"
                                                color: win.muted
                                            }
                                            Slider {
                                                objectName: "speechSpeed"
                                                Accessible.name: "Speaking speed"
                                                Layout.fillWidth: true
                                                from: .5
                                                to: 2
                                                stepSize: .05
                                                value: win.speechSpeed
                                                onMoved: win.speechSpeed = value
                                            }
                                            Label {
                                                text: Math.round(win.speechSpeed * 100) + "%"
                                                font.pixelSize: 11
                                                Layout.preferredWidth: 36
                                            }
                                        }
                                        Action {
                                            objectName: "speakText"
                                            Layout.fillWidth: true
                                            enabled: speechPanel.ready && !speechPanel.busy && speechText.text.trim() !== ""
                                            text: speechPanel.busy ? "Speaking…" : "Add speech at the playhead"
                                            onClicked: editor.speak(speechText.text, win.currentVoice, win.speechSpeed)
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Speaks the text with the chosen voice on this computer and adds it at the playhead on a free track"
                                        }
                                    }
                                    // Sound effects by category: a click adds one at the playhead. Made
                                    // once and hidden on other tabs: destroying the tiles while the panel
                                    // is laid out again for another tab can crash Qt's layouts.
                                    Repeater {
                                        model: {
                                            const groups = [];
                                            for (const sound of editor.sounds()) {
                                                let g = groups.find(x => x.category === sound.category);
                                                if (!g) {
                                                    g = { category: sound.category, sounds: [] };
                                                    groups.push(g);
                                                }
                                                g.sounds.push(sound);
                                            }
                                            return groups;
                                        }
                                        ColumnLayout {
                                            id: soundGroup
                                            required property var modelData
                                            visible: win.leftTab === "audio"
                                            Layout.fillWidth: true
                                            spacing: 6
                                            Caption {
                                                objectName: "cat-sound-" + soundGroup.modelData.category
                                                text: soundGroup.modelData.category.toUpperCase()
                                            }
                                            Flow {
                                                Layout.fillWidth: true
                                                spacing: 6
                                                Repeater {
                                                    model: soundGroup.modelData.sounds
                                                    Tile {
                                                        required property var modelData
                                                        objectName: "soundTile-" + modelData.id
                                                        text: modelData.name
                                                        glyph: /click|mouse/i.test(modelData.category + modelData.name) ? "⌖" : /typ|key/i.test(modelData.category + modelData.name) ? "⌨" : /whoosh|swoosh|swish/i.test(modelData.category + modelData.name) ? "≋" : "♪"
                                                        swatch: "#1f3340"
                                                        implicitWidth: 112
                                                        onClicked: editor.addSound(modelData.id)
                                                        ToolTip.visible: hovered
                                                        ToolTip.text: modelData.name + " · " + Number(modelData.seconds).toFixed(1) + " s · " + modelData.licence + "\nAdds it at the playhead."
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    // Effects, transitions and looks for the selected clip, as tiles.
                                    Caption {
                                        objectName: "catStyle"
                                        visible: win.leftTab === "effects"
                                        text: "STYLE EFFECT · SELECTED CLIP"
                                    }
                                    Flow {
                                        visible: win.leftTab === "effects"
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Repeater {
                                            model: win.styleEffects
                                            Tile {
                                                required property var modelData
                                                objectName: "fxTile-" + (modelData.id || "none")
                                                text: modelData.label
                                                glyph: modelData.id ? "✦" : "⊘"
                                                checked: win.pictureSelected && (win.selection.fx || "") === modelData.id
                                                enabled: win.pictureSelected && win.selection.locked !== true
                                                onClicked: editor.setClip("fx", modelData.id)
                                            }
                                        }
                                    }
                                    Caption {
                                        visible: win.leftTab === "transitions"
                                        text: "TRANSITION INTO THE SELECTED CLIP"
                                    }
                                    Flow {
                                        visible: win.leftTab === "transitions"
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Repeater {
                                            model: [{ id: "", label: "None (cut)" }].concat(editor.transitionTypes())
                                            Tile {
                                                required property var modelData
                                                objectName: "transitionTile-" + (modelData.id || "none")
                                                text: modelData.label
                                                glyph: modelData.id ? "⋈" : "|"
                                                checked: win.selection.canTransition === true && (win.selection.transition || "") === modelData.id
                                                enabled: win.selection.canTransition === true && win.selection.locked !== true
                                                onClicked: editor.setClip("transition", modelData.id)
                                            }
                                        }
                                    }
                                    Caption {
                                        visible: win.leftTab === "filters"
                                        text: "LOOK · SELECTED CLIP"
                                    }
                                    Flow {
                                        visible: win.leftTab === "filters"
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Repeater {
                                            model: win.looks.slice(1)
                                            Tile {
                                                required property var modelData
                                                required property int index
                                                objectName: "lookTile-" + index
                                                text: modelData.label
                                                swatch: ["#2a3038", "#6b4a2a", "#2a4a6b", "#3a3346", "#5e4b33", "#3c3c3c", "#5a2f3a", "#4a3f5e"][index] || "#202831"
                                                glyph: "◐"
                                                enabled: (win.pictureSelected || win.selectionKind === "adjust") && win.selection.locked !== true
                                                onClicked: win.applyLook(index + 1)
                                            }
                                        }
                                    }
                                    Caption {
                                        visible: win.leftTab === "filters"
                                        objectName: "catLuts"
                                        text: "LUT LIBRARY"
                                    }
                                    Flow {
                                        visible: win.leftTab === "filters" && (win.s.lutLibrary || []).length > 0
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Repeater {
                                            model: win.s.lutLibrary || []
                                            Tile {
                                                required property var modelData
                                                text: modelData.name
                                                glyph: "▤"
                                                checked: (win.pictureSelected || win.selectionKind === "adjust") && win.selection.lutName === modelData.name
                                                enabled: (win.pictureSelected || win.selectionKind === "adjust") && win.selection.locked !== true
                                                onClicked: editor.setClip("lut", modelData.path)
                                            }
                                        }
                                    }
                                    Action {
                                        visible: win.leftTab === "filters"
                                        text: "+ Add a LUT file to the library"
                                        Layout.fillWidth: true
                                        onClicked: lutLibraryDialog.open()
                                    }
                                    Label {
                                        visible: ["effects", "transitions", "filters"].indexOf(win.leftTab) >= 0
                                        Layout.fillWidth: true
                                        wrapMode: Text.Wrap
                                        color: win.muted
                                        font.pixelSize: 11
                                        text: win.leftTab === "transitions" ? (win.selection.canTransition === true ? "Fine-tune the length in the inspector under Animation." : "Select a clip that directly follows another on its track.") : (win.pictureSelected || (win.leftTab === "filters" && win.selectionKind === "adjust") ? "Applies to " + (win.selection.name || "the selected clip") + "; fine-tune it in the inspector." : "Select a video or picture on the timeline first.")
                                    }
                                }
                            }
                        }
                    }
                    Caption {
                        text: "LOCAL FILES. YOUR STORY."
                        font.pixelSize: 9
                    }
                }
            }
            Rectangle {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 420
                color: "#10151b"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            text: "Player"
                            font.bold: true
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Label {
                            text: editor.playing ? "Playing" : "Paused · exact frame"
                            color: win.muted
                            font.pixelSize: 10
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        // Video scopes over the top-right corner of the viewer: the preview
                        // still when paused, the frame on screen four times a second while
                        // playing.
                        Rectangle {
                            id: scopes
                            objectName: "scopes"
                            property bool live: false
                            property int serial: 0
                            visible: win.showScopes && win.s.duration > 0
                            z: 10
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.margins: 6
                            width: scopeImage.implicitWidth + 12
                            height: scopeImage.implicitHeight + scopeKind.height + 18
                            color: "#e6101418"
                            radius: 6
                            border.color: "#2e3741"
                            Timer {
                                interval: 250
                                repeat: true
                                running: scopes.visible && editor.playing
                                onTriggered: if (editor.captureScopeFrame()) {
                                    scopes.live = true;
                                    scopes.serial++;
                                }
                            }
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 6
                                RowLayout {
                                    Layout.fillWidth: true
                                    ComboBox {
                                        id: scopeKind
                                        objectName: "scopeKind"
                                        Layout.fillWidth: true
                                        implicitHeight: 26
                                        font.pixelSize: 11
                                        readonly property var kinds: ["histogram", "waveform", "vectorscope"]
                                        model: ["Histogram", "Waveform", "Vectorscope"]
                                    }
                                    Action {
                                        text: "✕"
                                        padding: 4
                                        onClicked: win.showScopes = false
                                    }
                                }
                                Image {
                                    id: scopeImage
                                    objectName: "scopeImage"
                                    cache: false
                                    // The still is used again as soon as playback stops.
                                    readonly property bool useLive: scopes.live && editor.playing
                                    source: scopes.visible ? "image://frames/scope/" + scopeKind.kinds[scopeKind.currentIndex] + "/" + (useLive ? "live" : "still") + "/" + scopes.serial + "-" + win.s.previewUrl : ""
                                }
                            }
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: Math.min(parent.width, parent.height * win.s.width / win.s.height)
                            height: width * win.s.height / win.s.width
                            color: "#07090c"
                            border.color: "#2e3741"
                            // Preview stills and live playback share this surface, so pausing
                            // keeps the last played frame until the exact still replaces it.
                            VideoOutput {
                                id: videoOutput
                                objectName: "viewer"
                                anchors.fill: parent
                                fillMode: VideoOutput.PreserveAspectFit
                                visible: win.s.duration > 0
                                Component.onCompleted: editor.setVideoSink(videoSink)
                            }
                            // Picking the key colour: a click on the canvas takes the selected clip's
                            // colour there.
                            MouseArea {
                                objectName: "keyPicker"
                                anchors.fill: parent
                                z: 10
                                visible: win.pickingKey && win.s.selectedId.length > 0
                                cursorShape: Qt.CrossCursor
                                onClicked: function (mouse) {
                                    win.pickingKey = false;
                                    editor.pickKeyColor(mouse.x / width, mouse.y / height);
                                }
                            }
                            // Drawing a free mask: each click adds a point; the outline so far
                            // is shown over the picture.
                            MouseArea {
                                objectName: "maskDrawer"
                                anchors.fill: parent
                                z: 11
                                visible: win.drawingMask && win.s.selectedId.length > 0
                                cursorShape: Qt.CrossCursor
                                onClicked: function (mouse) {
                                    editor.addMaskPoint(mouse.x / width, mouse.y / height);
                                }
                            }
                            Canvas {
                                id: maskOutline
                                objectName: "maskOutline"
                                anchors.fill: parent
                                z: 11
                                visible: win.drawingMask && win.s.selectedId.length > 0
                                readonly property var points: win.selection.maskOutline || []
                                onPointsChanged: requestPaint()
                                onVisibleChanged: requestPaint()
                                onPaint: {
                                    const ctx = getContext("2d");
                                    ctx.reset();
                                    if (points.length === 0)
                                        return;
                                    ctx.lineWidth = 2;
                                    ctx.strokeStyle = "#64d8bc";
                                    ctx.beginPath();
                                    ctx.moveTo(points[0].x * width, points[0].y * height);
                                    for (let i = 1; i < points.length; ++i)
                                        ctx.lineTo(points[i].x * width, points[i].y * height);
                                    if (points.length >= 3)
                                        ctx.closePath();
                                    ctx.stroke();
                                    ctx.fillStyle = "#ffd479";
                                    for (let i = 0; i < points.length; ++i) {
                                        ctx.beginPath();
                                        ctx.arc(points[i].x * width, points[i].y * height, i === 0 ? 5 : 3.5, 0, 2 * Math.PI);
                                        ctx.fill();
                                    }
                                }
                            }
                            // Selected clip on the canvas: drag inside to move, drag a corner to
                            // resize around the centre. One undo step on release; animated
                            // properties get a keyframe at the playhead.
                            Item {
                                id: transformBox
                                objectName: "transformBox"
                                readonly property var bounds: {
                                    win.s.revision;
                                    win.s.playhead;
                                    return win.s.selectedId.length > 0 ? editor.clipBounds(win.s.selectedId) : ({});
                                }
                                readonly property real canvasWidth: parent.width
                                readonly property real canvasHeight: parent.height
                                property real dragX: 0
                                property real dragY: 0
                                property real dragScale: 1
                                visible: bounds.inside === true && !editor.playing && win.selection.locked !== true && win.s.duration > 0
                                x: ((bounds.x || 0) + dragX) * canvasWidth + (bounds.width || 0) * canvasWidth * (1 - dragScale) / 2
                                y: ((bounds.y || 0) + dragY) * canvasHeight + (bounds.height || 0) * canvasHeight * (1 - dragScale) / 2
                                width: (bounds.width || 0) * canvasWidth * dragScale
                                height: (bounds.height || 0) * canvasHeight * dragScale
                                function commit() {
                                    const now = win.selection.animated || {};
                                    const values = {};
                                    if (dragX !== 0 || dragY !== 0) {
                                        values.x = Math.max(-2, Math.min(2, now.x + dragX));
                                        values.y = Math.max(-2, Math.min(2, now.y + dragY));
                                    }
                                    if (dragScale !== 1)
                                        values.scale = Math.max(.1, Math.min(3, now.scale * dragScale));
                                    dragX = 0;
                                    dragY = 0;
                                    dragScale = 1;
                                    if (Object.keys(values).length > 0)
                                        editor.setClipValues(values);
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    color: "transparent"
                                    border.color: win.mint
                                    border.width: 2
                                }
                                MouseArea {
                                    id: moveHandle
                                    objectName: "transformMove"
                                    anchors.fill: parent
                                    cursorShape: Qt.SizeAllCursor
                                    property point origin
                                    onPressed: function (mouse) {
                                        origin = mapToItem(transformBox.parent, mouse.x, mouse.y);
                                    }
                                    onPositionChanged: function (mouse) {
                                        const point = mapToItem(transformBox.parent, mouse.x, mouse.y);
                                        transformBox.dragX = (point.x - origin.x) / transformBox.canvasWidth;
                                        transformBox.dragY = (point.y - origin.y) / transformBox.canvasHeight;
                                    }
                                    onReleased: transformBox.commit()
                                    onCanceled: transformBox.commit()
                                }
                                Repeater {
                                    model: 4
                                    Rectangle {
                                        required property int index
                                        objectName: "transformCorner" + index
                                        x: (index % 2) * transformBox.width - width / 2
                                        y: (index < 2 ? 0 : 1) * transformBox.height - height / 2
                                        width: 12
                                        height: 12
                                        radius: 3
                                        color: win.mint
                                        border.color: "#0b1016"
                                        MouseArea {
                                            anchors.fill: parent
                                            anchors.margins: -4
                                            cursorShape: index === 0 || index === 3 ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                                            property point centre
                                            property real reach: 1
                                            onPressed: function (mouse) {
                                                const b = transformBox.bounds;
                                                centre = Qt.point((b.x + b.width / 2) * transformBox.canvasWidth, (b.y + b.height / 2) * transformBox.canvasHeight);
                                                const point = mapToItem(transformBox.parent, mouse.x, mouse.y);
                                                reach = Math.max(4, Math.hypot(point.x - centre.x, point.y - centre.y));
                                            }
                                            onPositionChanged: function (mouse) {
                                                const point = mapToItem(transformBox.parent, mouse.x, mouse.y);
                                                transformBox.dragScale = Math.max(.05, Math.hypot(point.x - centre.x, point.y - centre.y) / reach);
                                            }
                                            onReleased: transformBox.commit()
                                            onCanceled: transformBox.commit()
                                        }
                                    }
                                }
                            }
                            ColumnLayout {
                                anchors.centerIn: parent
                                visible: win.s.duration === 0
                                spacing: 14
                                Label {
                                    text: "Every story starts with a cut."
                                    font.pixelSize: 20
                                    font.bold: true
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Label {
                                    text: "Import footage or add a title to begin."
                                    color: win.muted
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Action {
                                    text: "Import media"
                                    Layout.alignment: Qt.AlignHCenter
                                    onClicked: importDialog.open()
                                }
                            }
                        }
                    }
                    // Player bar: time on the left, playback in the middle, recording, scopes and
                    // the frame's shape on the right.
                    Item {
                        objectName: "playerBar"
                        Layout.fillWidth: true
                        implicitHeight: 36
                        RowLayout {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 10
                            Label {
                                objectName: "playerTime"
                                text: win.clock(editor.playbackFrame)
                                font.family: "Consolas"
                                color: win.mint
                            }
                            Label {
                                text: win.clock(win.s.duration)
                                font.family: "Consolas"
                                color: win.muted
                            }
                        Label {
                            objectName: "playbackRate"
                            visible: editor.playbackRate !== 1 && (editor.playing || editor.playbackRate < 0)
                            text: (editor.playbackRate < 0 ? "◀◀ " : "▶▶ ") + Math.abs(editor.playbackRate) + "×"
                            color: "#ffd479"
                            font.bold: true
                        }
                        // Peak meter for the left and right channel, −60 to 0 dBFS.
                        Column {
                            objectName: "levelMeter"
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 2
                            ToolTip.visible: meterHover.hovered
                            ToolTip.text: "Peak level while playing. Keep the loudest parts in the yellow; red means close to clipping. Measure the whole mix in the export dialog."
                            HoverHandler { id: meterHover }
                            Repeater {
                                model: 2
                                Rectangle {
                                    required property int index
                                    readonly property real db: editor.levels.length > index ? editor.levels[index] : -90
                                    width: 90
                                    height: 5
                                    radius: 2
                                    color: "#26302d"
                                    Rectangle {
                                        height: parent.height
                                        radius: 2
                                        width: parent.width * Math.max(0, Math.min(1, (parent.db + 60) / 60))
                                        color: parent.db > -1 ? "#e5534b" : parent.db > -9 ? "#e3b341" : win.mint
                                    }
                                }
                            }
                        }
                        }
                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 4
                            Repeater {
                                model: [
                                    { id: "previousFrame", glyph: "⏮", tip: "One frame back" },
                                    { id: "play", glyph: "", tip: "Play or pause (Space plays live from the playhead)" },
                                    { id: "nextFrame", glyph: "⏭", tip: "One frame on" }
                                ]
                                AbstractButton {
                                    required property var modelData
                                    objectName: "player-" + modelData.id
                                    Accessible.name: modelData.id === "play" ? (editor.playing ? "Pause" : "Play") : modelData.tip
                                    implicitWidth: modelData.id === "play" ? 40 : 30
                                    implicitHeight: modelData.id === "play" ? 34 : 28
                                    hoverEnabled: true
                                    enabled: modelData.id !== "play" || (win.s.duration > 0 && !win.s.busy)
                                    onClicked: modelData.id === "play" ? editor.togglePlayback() : win.command(modelData.id)
                                    contentItem: Text {
                                        text: parent.modelData.id === "play" ? (editor.playing ? "❚❚" : "▶") : parent.modelData.glyph
                                        font.pixelSize: parent.modelData.id === "play" ? 18 : 14
                                        color: !parent.enabled ? "#56616b" : parent.hovered ? "#ffffff" : "#d7e0e7"
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        radius: height / 2
                                        color: parent.hovered && parent.enabled ? "#26313a" : "transparent"
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.text: modelData.tip
                                }
                            }
                        }
                        RowLayout {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 6
                            Action {
                                objectName: "voiceOver"
                                readonly property var voice: win.s.voiceOver || ({})
                                text: voice.recording ? "■ Stop " + Number(voice.seconds || 0).toFixed(0) + " s" : "● Voice-over"
                                palette.buttonText: voice.recording ? "#ff6b6b" : "#e7edf2"
                                enabled: voice.recording || (voice.available === true && !win.s.busy)
                                onClicked: voice.recording ? editor.stopVoiceOver() : editor.startVoiceOver()
                                ToolTip.visible: hovered
                                ToolTip.text: voice.available === true ? "Record narration from the microphone while the timeline plays from the playhead. Use headphones so the recording does not pick up the timeline." : "No microphone found"
                            }
                            Action {
                                objectName: "toggleProxies"
                                visible: editor.assets.some(a => a.proxy === "ready" || a.proxy === "making")
                                text: win.s.proxies.making ? "Proxy " + Math.round(100 * (win.s.proxies.progress || 0)) + "%" : "Proxy"
                                highlighted: win.s.proxies.useProxies === true
                                padding: 4
                                onClicked: editor.setUseProxies(!win.s.proxies.useProxies)
                                ToolTip.visible: hovered
                                ToolTip.text: (win.s.proxies.useProxies ? "The preview plays the small editing copies. Click to see the originals." : "The preview plays the originals. Click to use the small editing copies.") + " Exports always use the originals."
                            }
                            Action {
                                objectName: "toggleScopes"
                                text: "Scopes"
                                highlighted: win.showScopes
                                padding: 4
                                onClicked: win.showScopes = !win.showScopes
                                ToolTip.visible: hovered
                                ToolTip.text: "Histogram, waveform and vectorscope of the picture, to judge exposure and colour"
                            }
                            Label {
                                objectName: "playerRatio"
                                readonly property int divisor: {
                                    let a = win.s.width, b = win.s.height;
                                    while (b) {
                                        const t = b;
                                        b = a % b;
                                        a = t;
                                    }
                                    return a || 1;
                                }
                                text: win.s.width / divisor > 40 ? win.s.width + "×" + win.s.height : (win.s.width / divisor) + ":" + (win.s.height / divisor)
                                color: win.muted
                                font.pixelSize: 11
                                ToolTip.visible: ratioHover.hovered
                                ToolTip.text: "The frame's shape; change it under Project → Reframe for…"
                                HoverHandler {
                                    id: ratioHover
                                }
                            }
                        }
                    }
                }
            }
            Rectangle {
                objectName: "inspector"
                SplitView.preferredWidth: 330
                SplitView.minimumWidth: 270
                color: "#171d24"
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    // Tabs for what is selected; sub-tabs below for the bigger ones.
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 34
                        color: "#1c232b"
                        visible: win.inspectorTabs.length > 0
                        ListView {
                            id: inspectorTabRow
                            objectName: "inspectorTabs"
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            orientation: ListView.Horizontal
                            clip: true
                            spacing: 2
                            model: win.inspectorTabs
                            delegate: AbstractButton {
                                required property var modelData
                                objectName: "inspectorTab-" + modelData.id
                                Accessible.name: modelData.label
                                height: inspectorTabRow.height
                                implicitWidth: tabLabel.implicitWidth + 16
                                readonly property bool active: win.inspectorTab === modelData.id
                                onClicked: win.chooseInspector(modelData.id, "")
                                contentItem: Label {
                                    id: tabLabel
                                    text: parent.modelData.label
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 12
                                    font.bold: parent.active
                                    color: parent.active ? win.mint : (parent.hovered ? "#e7edf2" : win.muted)
                                }
                                background: Rectangle {
                                    color: "transparent"
                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        width: parent.width - 12
                                        height: 2
                                        radius: 1
                                        color: win.mint
                                        visible: parent.parent.active
                                    }
                                }
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        Layout.topMargin: 8
                        implicitHeight: 26
                        radius: 5
                        color: "#11161c"
                        visible: win.inspectorSubs.length > 0
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 2
                            spacing: 2
                            Repeater {
                                model: win.inspectorSubs
                                AbstractButton {
                                    required property var modelData
                                    objectName: "inspectorSub-" + modelData.id
                                    Accessible.name: modelData.label
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    readonly property bool active: win.inspectorSub === modelData.id
                                    onClicked: win.chooseInspector(win.inspectorTab, modelData.id)
                                    contentItem: Label {
                                        text: parent.modelData.label
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        color: parent.active ? "#e7edf2" : win.muted
                                    }
                                    background: Rectangle {
                                        radius: 4
                                        color: parent.active ? "#2f3a45" : "transparent"
                                    }
                                }
                            }
                        }
                    }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 14
                    clip: true
                    contentWidth: availableWidth
                    ColumnLayout {
                        width: parent.width
                        spacing: 12
                        Label {
                            objectName: "inspectorTitle"
                            visible: win.s.selectedId.length > 0
                            text: win.selection.name || ""
                            font.pixelSize: 14
                            font.bold: true
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        // Nothing selected: the project's details.
                        ColumnLayout {
                            objectName: "projectDetails"
                            visible: !win.s.selectedId.length
                            Layout.fillWidth: true
                            spacing: 8
                            Caption {
                                text: "PROJECT"
                            }
                            Repeater {
                                model: [
                                    { key: "Name", value: win.s.name || "Untitled" },
                                    { key: "Saved", value: win.s.path || "Not saved yet" },
                                    { key: "Canvas", value: win.s.width + " × " + win.s.height },
                                    { key: "Frame rate", value: Number(win.s.fps).toFixed(2).replace(/\.00$/, "") + " fps" },
                                    { key: "Length", value: win.clock(win.s.duration || 0) }
                                ]
                                RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Label {
                                        text: modelData.key
                                        color: win.muted
                                        Layout.preferredWidth: 80
                                        Layout.alignment: Qt.AlignTop
                                    }
                                    Label {
                                        text: modelData.value
                                        Layout.fillWidth: true
                                        wrapMode: Text.WrapAnywhere
                                    }
                                }
                            }
                            Rule {}
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                text: "Select a clip on the timeline to edit it: its settings appear here in tabs."
                                color: win.muted
                                font.pixelSize: 11
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Action {
                                    objectName: "projectPreferences"
                                    text: "Preferences…"
                                    onClicked: preferencesDialog.open()
                                }
                            }
                        }
                        Label {
                            visible: win.selection.locked || false
                            text: "Track locked · unlock L in the timeline"
                            color: "#edbd92"
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                        ColumnLayout {
                            visible: win.s.selectedId.length > 0
                            enabled: !win.selection.locked
                            Layout.fillWidth: true
                            spacing: 12
                            Label {
                                visible: win.timingPage
                                text: "Timing • values in frames"
                                color: win.muted
                                font.pixelSize: 10
                            }
                            Repeater {
                                model: [
                                    {
                                        key: "start",
                                        name: "Start frame"
                                    },
                                    {
                                        key: "duration",
                                        name: "Length (frames)"
                                    },
                                    {
                                        key: "sourceIn",
                                        name: "Source in (sec)"
                                    },
                                    {
                                        key: "speed",
                                        name: "Speed (0.1–100×)"
                                    }
                                ]
                                RowLayout {
                                    visible: win.timingPage
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Label {
                                        text: modelData.name
                                        Layout.fillWidth: true
                                        color: win.muted
                                    }
                                    TextField {
                                        Accessible.name: modelData.name
                                        Layout.preferredWidth: 85
                                        text: String(win.selection[modelData.key] ?? 0)
                                        selectByMouse: true
                                        onEditingFinished: {
                                            if (isFinite(Number(text)))
                                                editor.setClip(modelData.key, Number(text));
                                        }
                                    }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                visible: (win.selection.video === true && (win.selection.speed ?? 1) < 1) && (win.onTab("speed"))
                                Label {
                                    text: "Slow motion"
                                    color: win.muted
                                    Layout.fillWidth: true
                                }
                                ComboBox {
                                    objectName: "slowMotion"
                                    Layout.preferredWidth: 170
                                    readonly property var modes: ["", "blend", "flow"]
                                    model: ["Repeat frames", "Blend frames", "Optical flow (slow)"]
                                    currentIndex: Math.max(0, modes.indexOf(win.selection.slowMotion || ""))
                                    onActivated: index => editor.setClip("slowMotion", modes[index])
                                    ToolTip.visible: hovered
                                    ToolTip.text: "How in-between frames are made when the clip plays slower than it was filmed. Optical flow is smoothest but renders slowly."
                                }
                            }
                            // Speed ramps: the clip cut into parts that speed up and slow down.
                            ComboBox {
                                objectName: "speedRamp"
                                Accessible.name: "Speed ramp"
                                Layout.fillWidth: true
                                visible: (win.selection.video === true || (!!win.selection.assetId && win.selection.picture !== true)) && (win.onTab("speed"))
                                enabled: win.selection.locked !== true
                                readonly property var presets: ["montage", "hero", "bullet", "jumpCut", "flashIn", "flashOut"]
                                model: ["Speed ramp…", "Montage (fast, slow, fast)", "Hero (slow moment)", "Bullet (long slow moment)", "Jump cut (slow, then fast)", "Flash in (fast, then normal)", "Flash out (speeds up)"]
                                onActivated: index => {
                                    if (index > 0)
                                        editor.speedRamp(presets[index - 1]);
                                    currentIndex = 0;
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "Cuts the clip (and its sound) into parts that play faster and slower; the parts stay editable"
                            }
                            ComboBox {
                                visible: win.onTab("more")
                                Accessible.name: "Track of the clip"
                                Layout.fillWidth: true
                                model: editor.trackList
                                textRole: "name"
                                currentIndex: win.selection.track ?? 0
                                onActivated: editor.setClip("track", currentIndex)
                            }
                            Rule { visible: win.timingPage }
                            RowLayout {
                                visible: win.picPage("basic") || win.audioPage("basic")
                                Layout.fillWidth: true
                                Label {
                                    Layout.fillWidth: true
                                    text: win.selection.playheadInside ? "◇ sets a keyframe at the playhead" : "Move the playhead into the clip to animate"
                                    color: win.muted
                                    font.pixelSize: 10
                                    wrapMode: Text.Wrap
                                }
                                Action {
                                    objectName: "previousKeyframe"
                                    Accessible.name: "Previous keyframe"
                                    text: "◀◆"
                                    padding: 6
                                    onClicked: win.goTo(editor.adjacentKeyframe(false))
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Previous keyframe"
                                }
                                Action {
                                    objectName: "nextKeyframe"
                                    Accessible.name: "Next keyframe"
                                    text: "◆▶"
                                    padding: 6
                                    onClicked: win.goTo(editor.adjacentKeyframe(true))
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Next keyframe"
                                }
                            }
                            ValueGroup {
                                objectName: "transformSection"
                                title: "Transform"
                                prefix: "prop-"
                                visible: win.picPage("basic")
                                rows: [
                                    { key: "scale", name: "Scale", lo: .1, hi: 5, step: .01, def: 1, dec: 0, unit: "%", shown: 100, anim: true },
                                    { key: "x", name: "Position X", lo: -1, hi: 1, step: .01, anim: true, tip: "Left (−) or right (+), in frame widths" },
                                    { key: "y", name: "Position Y", lo: -1, hi: 1, step: .01, anim: true, tip: "Up (−) or down (+), in frame heights" },
                                    { key: "rotation", name: "Rotation", lo: -180, hi: 180, step: 1, dec: 0, unit: "°", anim: true }
                                ]
                            }
                            // Overlays on a video follow something in it.
                            TrackMotion {
                                Layout.fillWidth: true
                                visible: win.picPage("basic") && win.selectionKind !== "area" && win.s.selectionTrackable === true
                            }
                            ValueGroup {
                                objectName: "opacitySection"
                                title: "Opacity"
                                prefix: "prop-"
                                visible: win.picPage("basic") && win.selectionKind !== "adjust"
                                rows: [
                                    { key: "opacity", name: "Opacity", lo: 0, hi: 1, def: 1, dec: 0, unit: "%", shown: 100, anim: true }
                                ]
                            }
                            ValueGroup {
                                objectName: "cropSection"
                                title: "Crop"
                                prefix: "prop-"
                                visible: win.picPage("basic") && win.selectionKind === "media"
                                rows: [
                                    { key: "crop", name: "All edges", lo: 0, hi: .45, dec: 0, unit: "%", shown: 100 },
                                    { key: "cropLeft", name: "Left", lo: 0, hi: .9, dec: 0, unit: "%", shown: 100 },
                                    { key: "cropRight", name: "Right", lo: 0, hi: .9, dec: 0, unit: "%", shown: 100 },
                                    { key: "cropTop", name: "Top", lo: 0, hi: .9, dec: 0, unit: "%", shown: 100 },
                                    { key: "cropBottom", name: "Bottom", lo: 0, hi: .9, dec: 0, unit: "%", shown: 100 }
                                ]
                            }
                            ValueGroup {
                                objectName: "lightSection"
                                title: "Light"
                                prefix: "prop-"
                                visible: win.adjustPage("basic")
                                rows: [
                                    { key: "exposure", name: "Exposure", lo: -3, hi: 3, step: .05, unit: " EV", tip: "In stops, like a camera" },
                                    { key: "brightness", name: "Brightness", lo: -.5, hi: .5, step: .01 },
                                    { key: "contrast", name: "Contrast", lo: .1, hi: 3, step: .01, def: 1 },
                                    { key: "saturation", name: "Saturation", lo: 0, hi: 3, step: .01, def: 1 }
                                ]
                            }
                            ValueGroup {
                                objectName: "blurSection"
                                title: "Blur"
                                prefix: "prop-"
                                visible: win.onTab("effects")
                                rows: [
                                    { key: "blur", name: "Blur", lo: 0, hi: 1, dec: 0, unit: "%", shown: 100 }
                                ]
                            }
                            ValueGroup {
                                objectName: "volumeSection"
                                title: "Volume and fades"
                                prefix: "prop-"
                                visible: win.audioPage("basic")
                                rows: [
                                    { key: "volume", name: "Volume", lo: 0, hi: 4, step: .01, def: 1, dec: 0, unit: "%", shown: 100, anim: true },
                                    { key: "pan", name: "Balance", lo: -1, hi: 1, step: .05, anim: true, tip: "Left (−) or right (+)" },
                                    { key: "fadeIn", name: "Fade in", lo: 0, hi: 5, step: .1, dec: 1, unit: " s" },
                                    { key: "fadeOut", name: "Fade out", lo: 0, hi: 5, step: .1, dec: 1, unit: " s" }
                                ]
                            }
                            Section {
                                title: "Transition from the previous clip"
                                Layout.fillWidth: true
                                visible: (win.selection.audioOnly !== true) && (win.onTab("animation"))
                                Label {
                                    Layout.fillWidth: true
                                    visible: win.selection.canTransition !== true
                                    text: "Place this clip directly after another clip on the same track to add a transition."
                                    wrapMode: Text.Wrap
                                    color: win.muted
                                    font.pixelSize: 10
                                }
                                ComboBox {
                                    id: transitionType
                                    objectName: "transitionType"
                                    Layout.fillWidth: true
                                    visible: win.selection.canTransition === true
                                    readonly property var entries: [{
                                            id: "",
                                            label: "None (straight cut)"
                                        }].concat(editor.transitionTypes())
                                    model: entries
                                    textRole: "label"
                                    currentIndex: Math.max(0, entries.findIndex(e => e.id === (win.selection.transition || "")))
                                    onActivated: editor.setClip("transition", entries[currentIndex].id)
                                }
                                ValueRow {
                                    visible: win.selection.canTransition === true && (win.selection.transition || "") !== ""
                                    key: "transitionFrames"
                                    label: "Duration" + ((win.selection.transitionLength || 0) < (win.selection.transitionFrames || 0) ? " (clip limit " + ((win.selection.transitionLength || 0) / win.s.fps).toFixed(2) + " s)" : "")
                                    from: 2
                                    to: Math.round(3 * win.s.fps)
                                    stepSize: 1
                                    defaultValue: Math.round(.5 * win.s.fps)
                                    shown: 1 / win.s.fps
                                    decimals: 2
                                    unit: " s"
                                    sliderName: "transitionDuration"
                                    tip: "How long the transition takes"
                                    function commit(v) {
                                        editor.setClip("transitionFrames", Math.max(2, Math.min(to, Math.round(v))));
                                    }
                                }
                                Rule {}
                            }
                            // Blur or mosaic area: what it does and how strongly. Move and resize it
                            // in the preview; its position can be keyframed.
                            // Adjustment layer: its look (below) applies to every track under it.
                            Section {
                                title: "Adjustment layer"
                                Layout.fillWidth: true
                                visible: (win.selection.effect === "adjust") && (win.onTab("basic"))
                                spacing: 4
                                Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    color: win.muted
                                    font.pixelSize: 11
                                    text: "The colour and look below change everything on the tracks under this clip while it runs."
                                }
                                RowLayout {
                                    Label {
                                        text: "Strength"
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: Math.round((win.selection.opacity ?? 1) * 100) + "%"
                                    }
                                }
                                Slider {
                                    objectName: "adjustStrength"
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 1
                                    stepSize: .01
                                    value: win.selection.opacity ?? 1
                                    enabled: win.selection.locked !== true
                                    onPressedChanged: if (!pressed)
                                        editor.setClip("opacity", value)
                                    onMoved: if (!pressed)
                                        editor.setClip("opacity", value)
                                }
                            }
                            Section {
                                title: "Blur or mosaic area"
                                Layout.fillWidth: true
                                visible: ((win.selection.effect || "") !== "" && win.selection.effect !== "adjust") && (win.onTab("effect"))
                                spacing: 4
                                ComboBox {
                                    objectName: "effectType"
                                    Layout.fillWidth: true
                                    readonly property var types: ["blur", "pixelate"]
                                    model: ["Blur", "Mosaic (pixelate)"]
                                    currentIndex: Math.max(0, types.indexOf(win.selection.effect || "blur"))
                                    onActivated: editor.setClip("effect", types[currentIndex])
                                }
                                RowLayout {
                                    Label {
                                        text: "Strength"
                                        color: win.muted
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: Math.round((win.selection.effectStrength || 0) * 100) + "%"
                                        font.pixelSize: 10
                                    }
                                }
                                Slider {
                                    objectName: "effectStrength"
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 1
                                    stepSize: .01
                                    value: win.selection.effectStrength ?? .6
                                    onPressedChanged: if (!pressed)
                                        editor.setClip("effectStrength", value)
                                    onMoved: if (!pressed)
                                        editor.setClip("effectStrength", value)
                                }
                                ComboBox {
                                    objectName: "effectShape"
                                    Layout.fillWidth: true
                                    readonly property var shapes: ["rect", "ellipse"]
                                    model: ["Rectangle", "Ellipse"]
                                    currentIndex: Math.max(0, shapes.indexOf(win.selection.effectShape || "rect"))
                                    onActivated: editor.setClip("effectShape", shapes[currentIndex])
                                }
                                RowLayout {
                                    Label {
                                        text: "Soft edge"
                                        color: win.muted
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        text: Math.round((win.selection.feather || 0) * 200) + "%"
                                        font.pixelSize: 10
                                    }
                                }
                                Slider {
                                    objectName: "effectFeather"
                                    Layout.fillWidth: true
                                    from: 0
                                    to: .5
                                    stepSize: .01
                                    value: win.selection.feather ?? 0
                                    onPressedChanged: if (!pressed)
                                        editor.setClip("feather", value)
                                    onMoved: if (!pressed)
                                        editor.setClip("feather", value)
                                }
                                Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    font.pixelSize: 11
                                    color: win.muted
                                    text: "Drag the frame in the preview to place it and its corners to resize. Track motion makes it follow whatever it covers, or let it follow a face. The area affects all tracks below it."
                                }
                                Action {
                                    objectName: "followFace"
                                    Layout.fillWidth: true
                                    readonly property var follow: win.s.follow || ({})
                                    readonly property bool busy: follow.status === "analysing" && follow.clipId === win.s.selectedId
                                    enabled: !busy && win.selection.locked !== true && ((win.s.aiMissing || {}).faces || "") === ""
                                    text: busy ? "Finding faces…" : "Follow a face (AI)"
                                    onClicked: editor.followFace()
                                    ToolTip.visible: hovered
                                    ToolTip.text: ((win.s.aiMissing || {}).faces || "") !== "" ? win.s.aiMissing.faces : "Finds the faces in the video below and keyframes the area onto the one nearest to it, sized to cover it"
                                }
                                Label {
                                    objectName: "followStatus"
                                    Layout.fillWidth: true
                                    visible: (win.s.follow || {}).clipId === win.s.selectedId && (win.s.follow.status === "done" || win.s.follow.status === "failed")
                                    font.pixelSize: 11
                                    color: (win.s.follow || {}).status === "failed" ? "#ec6f5a" : win.muted
                                    text: (win.s.follow || {}).status === "done" ? "Following a face ✓ · move the playhead to check, adjust keyframes if needed" : "No face found under this area"
                                }
                                TrackMotion {
                                    prefix: "area-"
                                    Layout.fillWidth: true
                                }
                                Rule {}
                            }
                            // Shape: kind, colours, outline and size. Move, resize and rotate it in
                            // the preview like any overlay.
                            Section {
                                title: "Shape"
                                objectName: "graphicSection"
                                Layout.fillWidth: true
                                visible: ((win.selection.graphic || "") !== "") && (win.onTab("shape"))
                                spacing: 6
                                ComboBox {
                                    objectName: "graphicKind"
                                    Layout.fillWidth: true
                                    readonly property var kinds: ["arrow", "ellipse", "bubble", "rectangle", "line", "badge"]
                                    model: ["Arrow", "Circle / ellipse", "Speech bubble", "Box", "Line", "Numbered step"]
                                    currentIndex: Math.max(0, kinds.indexOf(win.selection.graphic || "arrow"))
                                    onActivated: index => editor.setClip("graphic", kinds[index])
                                }
                                Section {
                                    objectName: "graphicColours"
                                    title: "Colour"
                                    spacing: 6
                                    Repeater {
                                        model: [
                                            { key: "fillColor", name: "Fill", colors: ["#ffd23f", "#ff5a5f", "#64d8bc", "#5fa8ff", "#ffffff", "#14181d", "#00000000"] },
                                            { key: "strokeColor", name: "Outline", colors: ["#000000", "#ffffff", "#ff5a5f", "#ffd23f"] }
                                        ]
                                        RowLayout {
                                            id: graphicColours
                                            required property var modelData
                                            Label {
                                                text: graphicColours.modelData.name
                                                color: win.muted
                                                Layout.preferredWidth: 60
                                            }
                                            Repeater {
                                                model: (win.s.brandColors || []).concat(graphicColours.modelData.colors)
                                                Rectangle {
                                                    required property string modelData
                                                    width: 18
                                                    height: 18
                                                    radius: 9
                                                    color: modelData
                                                    border.width: win.selection[graphicColours.modelData.key] === modelData ? 3 : 1
                                                    border.color: win.selection[graphicColours.modelData.key] === modelData ? win.mint : "#6481a0"
                                                    Label {
                                                        anchors.centerIn: parent
                                                        visible: parent.modelData === "#00000000"
                                                        text: "∅"
                                                        font.pixelSize: 11
                                                    }
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        onClicked: editor.setClip(graphicColours.modelData.key, parent.modelData)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                ValueGroup {
                                    objectName: "graphicSize"
                                    title: "Size and outline"
                                    rows: [
                                        { key: "graphicWidth", name: "Width", lo: 0.01, hi: 1, def: 0.3, shown: 100, dec: 0, unit: "%", obj: "graphic-graphicWidth" },
                                        { key: "graphicHeight", name: "Height", lo: 0.005, hi: 1, def: 0.2, shown: 100, dec: 0, unit: "%", obj: "graphic-graphicHeight" },
                                        { key: "stroke", name: "Outline width", lo: 0, hi: 0.05, step: .001, def: 0, shown: 100, dec: 1, unit: "%", obj: "graphic-stroke" }
                                    ]
                                }
                                Rule {}
                            }
                            Section {
                                title: (win.selection.graphic || "") !== "" ? "Text in the shape" : "Text"
                                Layout.fillWidth: true
                                visible: (win.selection.assetId === "" && (win.selection.effect || "") === "" && ["arrow", "line"].indexOf(win.selection.graphic || "") < 0) && (win.onTab("text") || win.onTab("shape"))
                                TextArea {
                                    id: titleText
                                    objectName: "titleText"
                                    Accessible.name: "Text"
                                    property string editId: ""
                                    function sync() {
                                        if (editId !== win.s.selectedId || !activeFocus) {
                                            editId = win.s.selectedId;
                                            text = win.selection.text || "";
                                        }
                                    }
                                    Component.onCompleted: sync()
                                    Connections {
                                        target: editor
                                        function onChanged() {
                                            if (titleText.editId !== win.s.selectedId)
                                                titleText.sync();
                                        }
                                        function onProjectChanged() {
                                            titleText.sync();
                                        }
                                    }
                                    wrapMode: TextEdit.Wrap
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 85
                                    selectByMouse: true
                                    background: Rectangle {
                                        color: "#10161c"
                                        radius: 5
                                        border.color: "#35404b"
                                    }
                                }
                                Action {
                                    text: "Apply text"
                                    Layout.fillWidth: true
                                    onClicked: editor.setClip("text", titleText.text)
                                }
                                Action {
                                    objectName: "readAloud"
                                    Layout.fillWidth: true
                                    readonly property var speech: win.s.speech || ({})
                                    enabled: (speech.missing || "") === "" && speech.status !== "speaking" && win.currentVoice !== ""
                                    text: speech.status === "speaking" ? "Speaking…" : "Read aloud"
                                    onClicked: editor.speakSelection(win.currentVoice, win.speechSpeed)
                                    ToolTip.visible: hovered
                                    ToolTip.text: (speech.missing || "") !== "" ? speech.missing : "Speaks the selected titles at their starts with the voice and speed chosen under Audio › Text to speech"
                                }
                                Section {
                                    objectName: "titleLayout"
                                    title: "Style"
                                    spacing: 6
                                    // Title templates: the first line is the name or heading, the
                                    // next lines the role or subtitle.
                                    ComboBox {
                                        objectName: "titleStyle"
                                        Accessible.name: "Title style"
                                        Layout.fillWidth: true
                                        visible: (win.selection.captionStyle || "") === ""
                                        readonly property var styles: ["", "lowerThird", "lowerThirdLine", "lowerThirdRight", "titleCard", "banner", "quote"]
                                        model: ["Plain title", "Lower third (plate)", "Lower third (line)", "Lower third (right)", "Title card", "Banner across the bottom", "Quote"]
                                        currentIndex: Math.max(0, styles.indexOf(win.selection.titleStyle || ""))
                                        onActivated: editor.setClip("titleStyle", styles[currentIndex])
                                    }
                                    // Plain titles can build up character by character or word by word.
                                    RowLayout {
                                        Layout.fillWidth: true
                                        visible: (win.selection.titleStyle || "") === "" && (win.selection.captionStyle || "") === "" && !win.selection.graphic
                                        ComboBox {
                                            objectName: "textAnimation"
                                            Accessible.name: "Text animation"
                                            Layout.fillWidth: true
                                            readonly property var kinds: ["", "typewriter", "words", "rise", "pop", "fly", "drop", "spin", "fade"]
                                            model: ["Appears at once", "Typewriter", "Word by word", "Letters rise", "Letters pop up", "Letters fly in", "Letters drop and bounce", "Letters spin in", "Letters fade in"]
                                            currentIndex: Math.max(0, kinds.indexOf(win.selection.textAnimation || ""))
                                            onActivated: editor.setClip("textAnimation", kinds[currentIndex])
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Lets the text build up from the start of the clip, letter by letter or word by word"
                                        }
                                        SpinBox {
                                            objectName: "textAnimationTime"
                                            visible: !!win.selection.textAnimation
                                            from: 1
                                            to: 300
                                            value: Math.round((win.selection.textAnimationTime ?? 1.5) * 10)
                                            editable: true
                                            textFromValue: (v) => (v / 10).toFixed(1) + " s"
                                            valueFromText: (t) => Math.round(parseFloat(t) * 10)
                                            onValueModified: editor.setClip("textAnimationTime", value / 10)
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Time until the whole text is shown"
                                        }
                                    }
                                    RowLayout {
                                        visible: (win.selection.titleStyle || "") !== ""
                                        Label {
                                            text: "Accent"
                                            color: win.muted
                                            Layout.fillWidth: true
                                        }
                                        Repeater {
                                            model: ["#64d8bc", "#ffd23f", "#ec6f5a", "#5fa8ff", "#c38bff", "#ffffff"]
                                            Rectangle {
                                                required property string modelData
                                                width: 20
                                                height: 20
                                                radius: 10
                                                color: modelData
                                                border.width: win.selection.accentColor === modelData ? 3 : 1
                                                border.color: win.selection.accentColor === modelData ? win.mint : "#6481a0"
                                                MouseArea {
                                                    anchors.fill: parent
                                                    onClicked: editor.setClip("accentColor", parent.modelData)
                                                }
                                            }
                                        }
                                    }
                                    CheckBox {
                                        objectName: "titleSlide"
                                        visible: (win.selection.titleStyle || "").startsWith("lowerThird")
                                        text: win.selection.titleStyle === "lowerThirdRight" ? "Slide in from the right" : "Slide in from the left"
                                        checked: win.selection.titleSlide !== false
                                        onToggled: editor.setClip("titleSlide", checked)
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: (win.selection.titleStyle || "") !== ""
                                        wrapMode: Text.Wrap
                                        font.pixelSize: 11
                                        color: win.muted
                                        text: "First line: name or heading. Next lines: role or subtitle. Fade in/out sets how it appears; lower thirds also slide in."
                                    }
                                    // Captions with word timing (automatic captions) can highlight
                                    // the spoken word. Editing the words keeps the timing only while
                                    // the number of words stays the same.
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        visible: win.selection.timedWords === true || (win.selection.captionStyle || "") !== ""
                                        spacing: 4
                                        ComboBox {
                                            objectName: "captionStyle"
                                            Layout.fillWidth: true
                                            readonly property var styles: ["", "karaoke", "word"]
                                            model: ["Plain caption", "Karaoke: highlight the spoken word", "One word at a time"]
                                            currentIndex: Math.max(0, styles.indexOf(win.selection.captionStyle || ""))
                                            onActivated: editor.setClip("captionStyle", styles[currentIndex])
                                        }
                                        RowLayout {
                                            visible: (win.selection.captionStyle || "") !== ""
                                            Label {
                                                text: "Highlight"
                                                color: win.muted
                                                Layout.fillWidth: true
                                            }
                                            Repeater {
                                                model: ["#ffd23f", "#64d8bc", "#ff6fae", "#5fa8ff", "#ffffff"]
                                                Rectangle {
                                                    required property string modelData
                                                    width: 20
                                                    height: 20
                                                    radius: 10
                                                    color: modelData
                                                    border.width: win.selection.highlightColor === modelData ? 3 : 1
                                                    border.color: win.selection.highlightColor === modelData ? win.mint : "#6481a0"
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        onClicked: editor.setClip("highlightColor", parent.modelData)
                                                    }
                                                }
                                            }
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            visible: win.selection.timedWords !== true
                                            wrapMode: Text.Wrap
                                            font.pixelSize: 11
                                            color: win.muted
                                            text: "The word count changed, so this caption shows plainly. Generate captions again for word timing."
                                        }
                                    }
                                }
                                // Typography: font, size, weight and alignment.
                                Section {
                                    objectName: "fontSection"
                                    title: "Font"
                                    spacing: 6
                                    RowLayout {
                                        Layout.fillWidth: true
                                        ComboBox {
                                            id: fontBox
                                            objectName: "fontFamily"
                                            Accessible.name: "Font"
                                            Layout.fillWidth: true
                                            editable: true
                                            // Read again when the favourites change, which reorders the list.
                                            property var families: (win.s.fontFavorites || []).length >= 0 ? editor.fontFamilies() : []
                                            model: families
                                            currentIndex: families.indexOf(win.selection.fontFamily || "Arial")
                                            onActivated: index => editor.setClip("fontFamily", families[index])
                                            onAccepted: {
                                                const i = families.findIndex(f => f.toLowerCase() === editText.toLowerCase());
                                                if (i >= 0)
                                                    editor.setClip("fontFamily", families[i]);
                                            }
                                            delegate: ItemDelegate {
                                                required property string modelData
                                                required property int index
                                                width: ListView.view ? ListView.view.width : 200
                                                text: ((win.s.fontFavorites || []).indexOf(modelData) >= 0 ? "★ " : "") + modelData
                                                font.family: modelData
                                                highlighted: fontBox.highlightedIndex === index
                                            }
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Type to find a font. Fonts you add are kept in Cutlery's data folder."
                                        }
                                        ToolButton {
                                            objectName: "favoriteFont"
                                            Accessible.name: starred ? "Remove font from favourites" : "Add font to favourites"
                                            readonly property bool starred: (win.s.fontFavorites || []).indexOf(win.selection.fontFamily || "") >= 0
                                            text: starred ? "★" : "☆"
                                            onClicked: editor.toggleFontFavorite(win.selection.fontFamily || "")
                                            ToolTip.visible: hovered
                                            ToolTip.text: starred ? "Remove this font from the favourites" : "Keep this font at the top of the list (every project)"
                                        }
                                        Action {
                                            objectName: "addFont"
                                            text: "+ Font"
                                            padding: 6
                                            onClicked: fontFileDialog.open()
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Add a .ttf or .otf font file"
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 4
                                        Repeater {
                                            model: [
                                                { key: "bold", label: "B" },
                                                { key: "italic", label: "I" }
                                            ]
                                            Button {
                                                required property var modelData
                                                objectName: "text-" + modelData.key
                                                text: modelData.label
                                                checkable: true
                                                checked: win.selection[modelData.key] === true
                                                font.bold: modelData.key === "bold"
                                                font.italic: modelData.key === "italic"
                                                implicitWidth: 34
                                                onClicked: editor.setClip(modelData.key, checked)
                                            }
                                        }
                                        Item { Layout.fillWidth: true }
                                        Repeater {
                                            model: [
                                                { key: "left", label: "⯇ Left" },
                                                { key: "center", label: "Centre" },
                                                { key: "right", label: "Right ⯈" }
                                            ]
                                            Button {
                                                required property var modelData
                                                objectName: "align-" + modelData.key
                                                text: modelData.label
                                                checkable: true
                                                checked: (win.selection.align || "center") === modelData.key
                                                padding: 4
                                                onClicked: editor.setClip("align", modelData.key)
                                            }
                                        }
                                    }
                                    ValueRow {
                                        key: "fontSize"
                                        label: "Size"
                                        from: 8
                                        to: 500
                                        stepSize: 1
                                        decimals: 0
                                        defaultValue: 72
                                        sliderName: "fontSize"
                                    }
                                }
                                Section {
                                    objectName: "textColours"
                                    title: "Colour"
                                    spacing: 6
                                    RowLayout {
                                        Layout.fillWidth: true
                                        TextField {
                                            Layout.fillWidth: true
                                            text: win.selection.textColor || "#ffffff"
                                            placeholderText: "Text colour (#rrggbb)"
                                            onEditingFinished: editor.setClip("textColor", text)
                                        }
                                        // A second colour turns the letters into a top-to-bottom gradient.
                                        TextField {
                                            objectName: "gradientColor"
                                            Layout.fillWidth: true
                                            visible: !win.selection.titleStyle && !win.selection.captionStyle
                                            text: win.selection.gradientColor || ""
                                            placeholderText: "Gradient to (#rrggbb)"
                                            onEditingFinished: editor.setClip("gradientColor", text.trim())
                                            ToolTip.visible: hovered
                                            ToolTip.text: "The letters fade from the text colour at the top to this colour at the bottom. Leave empty for one colour."
                                        }
                                    }
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 4
                                        visible: (win.s.brandColors || []).length > 0
                                        Repeater {
                                            model: win.s.brandColors || []
                                            Rectangle {
                                                required property string modelData
                                                objectName: "brandTextColor-" + modelData
                                                width: 18
                                                height: 18
                                                radius: 9
                                                color: modelData
                                                border.width: win.selection.textColor === modelData ? 3 : 1
                                                border.color: win.selection.textColor === modelData ? win.mint : "#6481a0"
                                                MouseArea {
                                                    anchors.fill: parent
                                                    onClicked: editor.setClip("textColor", parent.modelData)
                                                }
                                            }
                                        }
                                    }
                                    // Words between asterisks (*like this*) take this colour in plain titles.
                                    RowLayout {
                                        Layout.fillWidth: true
                                        visible: (win.selection.captionStyle || "") === "" && (win.selection.titleStyle || "") === "" && win.selection.text !== undefined
                                        Label {
                                            text: "*Highlight*"
                                            color: (win.selection.text || "").indexOf("*") >= 0 ? win.mint : win.muted
                                            Layout.fillWidth: true
                                            ToolTip.visible: highlightHover.hovered
                                            ToolTip.text: "Put asterisks around words in the text, *like this*, to show them in this colour"
                                            HoverHandler { id: highlightHover }
                                        }
                                        Repeater {
                                            model: ["#ffd23f", "#64d8bc", "#ff6fae", "#5fa8ff", "#ff5a5f"].concat(win.s.brandColors || [])
                                            Rectangle {
                                                required property string modelData
                                                objectName: "titleHighlight-" + modelData
                                                width: 18
                                                height: 18
                                                radius: 9
                                                color: modelData
                                                border.width: win.selection.highlightColor === modelData ? 3 : 1
                                                border.color: win.selection.highlightColor === modelData ? win.mint : "#6481a0"
                                                MouseArea {
                                                    anchors.fill: parent
                                                    onClicked: editor.setClip("highlightColor", parent.modelData)
                                                }
                                            }
                                        }
                                    }
                                }
                                Section {
                                    objectName: "savedStyles"
                                    title: "Saved styles"
                                    spacing: 6
                                    // Saved text styles (for every project): apply to the selected titles,
                                    // or save this title's look under a name.
                                    RowLayout {
                                        Layout.fillWidth: true
                                        ComboBox {
                                            id: styleChoice
                                            objectName: "textStyle"
                                            Accessible.name: "Saved text style"
                                            Layout.fillWidth: true
                                            readonly property var styles: win.s.textStyles || []
                                            model: [styles.length ? "Apply a style…" : "No saved styles"].concat(styles.map(st => st.name))
                                            enabled: styles.length > 0 && win.selection.locked !== true
                                            onActivated: index => {
                                                if (index > 0)
                                                    editor.applyTextStyle(styles[index - 1].name);
                                                currentIndex = 0;
                                            }
                                        }
                                        ToolButton {
                                            objectName: "saveTextStyle"
                                            text: "Save…"
                                            enabled: !!win.selection.text
                                            onClicked: styleDialog.open()
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Keep this title's font, colours and effects as a style for every project"
                                        }
                                        ToolButton {
                                            text: "⋯"
                                            visible: styleChoice.styles.length > 0
                                            onClicked: styleMenu.popup()
                                            Menu {
                                                id: styleMenu
                                                title: "Remove a style"
                                                Instantiator {
                                                    model: styleChoice.styles
                                                    delegate: MenuItem {
                                                        required property var modelData
                                                        text: "Remove “" + modelData.name + "”"
                                                        onTriggered: editor.removeTextStyle(modelData.name)
                                                    }
                                                    onObjectAdded: (index, object) => styleMenu.insertItem(index, object)
                                                    onObjectRemoved: (index, object) => styleMenu.removeItem(object)
                                                }
                                            }
                                        }
                                    }
                                }
                                ValueGroup {
                                    objectName: "textSpacing"
                                    title: "Spacing"
                                    prefix: "text-"
                                    rows: [
                                        { key: "letterSpacing", name: "Letter spacing", lo: -0.1, hi: 0.5, def: 0, shown: 100, dec: 0, unit: "%" },
                                        { key: "lineSpacing", name: "Line spacing", lo: 0.7, hi: 3, def: 1, dec: 2, unit: "×" }
                                    ]
                                }
                                ValueGroup {
                                    objectName: "textEffects"
                                    title: "Outline, shadow and glow"
                                    prefix: "text-"
                                    rows: [
                                        { key: "outline", name: "Outline", lo: 0, hi: 0.25, def: 0, shown: 100, dec: 0, unit: "%" },
                                        { key: "textShadow", name: "Shadow", lo: 0, hi: 1, def: 1, shown: 100, dec: 0, unit: "%" },
                                        { key: "textGlow", name: "Glow", lo: 0, hi: 1, def: 0, shown: 100, dec: 0, unit: "%" },
                                        { key: "background", name: "Background box", lo: 0, hi: 1, def: 0, shown: 100, dec: 0, unit: "%" }
                                    ]
                                    // Outline, glow and box colours.
                                    Repeater {
                                        model: [
                                            { key: "outlineColor", name: "Outline colour", colours: ["#000000", "#ffffff", "#ffd23f"] },
                                            { key: "textGlowColor", name: "Glow colour", colours: ["#ffd23f", "#ffffff", "#ff4fd8"] },
                                            { key: "backgroundColor", name: "Box colour", colours: ["#000000", "#ffffff", "#64d8bc"] }
                                        ]
                                        RowLayout {
                                            id: textColourRow
                                            required property var modelData
                                            Layout.fillWidth: true
                                            Label {
                                                text: textColourRow.modelData.name
                                                color: win.muted
                                                Layout.fillWidth: true
                                            }
                                            Repeater {
                                                model: textColourRow.modelData.colours
                                                Rectangle {
                                                    required property string modelData
                                                    objectName: textColourRow.modelData.key + "-" + modelData
                                                    width: 18
                                                    height: 18
                                                    radius: 9
                                                    color: modelData
                                                    border.width: win.selection[textColourRow.modelData.key] === modelData ? 3 : 1
                                                    border.color: win.selection[textColourRow.modelData.key] === modelData ? win.mint : "#6481a0"
                                                    MouseArea {
                                                        anchors.fill: parent
                                                        enabled: win.selection.locked !== true
                                                        onClicked: editor.setClip(textColourRow.modelData.key, parent.modelData)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            // Presenter overlays: corner placement, shape, border, shadow, green screen.
                            Section {
                                title: ({ basic: "Placement", mask: "Shape and frame", cutout: "Cut out", canvas: "Background" })[win.inspectorSub] || "Picture"
                                header: !win.picPage("mask")
                                Layout.fillWidth: true
                                visible: (win.selection.audioOnly !== true && (win.selection.effect || "") === "" && editor.clipBounds(win.s.selectedId).width !== undefined) && (win.picPage("basic") || win.picPage("mask") || win.picPage("cutout") || win.picPage("canvas"))
                                spacing: 6
                                RowLayout {
                                    visible: win.picPage("basic")
                                    Layout.fillWidth: true
                                    Repeater {
                                        model: [
                                            { id: "topLeft", label: "↖" },
                                            { id: "topRight", label: "↗" },
                                            { id: "bottomLeft", label: "↙" },
                                            { id: "bottomRight", label: "↘" },
                                            { id: "full", label: "⛶" }
                                        ]
                                        Action {
                                            required property var modelData
                                            objectName: "place-" + modelData.id
                                            Accessible.name: modelData.id === "full" ? "Full frame" : "Picture-in-picture " + ({ topLeft: "top left", topRight: "top right", bottomLeft: "bottom left", bottomRight: "bottom right" })[modelData.id]
                                            text: modelData.label
                                            padding: 6
                                            Layout.fillWidth: true
                                            enabled: win.selection.locked !== true
                                            onClicked: editor.placeClip(modelData.id)
                                            ToolTip.visible: hovered
                                            ToolTip.text: modelData.id === "full" ? "Full frame" : "Picture-in-picture in this corner"
                                        }
                                    }
                                }
                                // Corner pin: move the picture's corners, e.g. into a screen in a photo.
                                Section {
                                    objectName: "cornerPin"
                                    title: "Perspective"
                                    tip: "Moves the corners: places the picture into a four-sided shape, e.g. onto a screen or a sign in another picture"
                                    visible: (!!win.selection.assetId && win.selection.picture === true) && (win.picPage("basic"))
                                    checkable: true
                                    checked: (win.selection.cornerPin || []).length === 8
                                    expanded: checked
                                    resettable: checked
                                    onToggled: on => editor.setClip("cornerPin", on ? [0, 0, 1, 0, 0, 1, 1, 1] : [])
                                    onReset: editor.setClip("cornerPin", [0, 0, 1, 0, 0, 1, 1, 1])
                                    spacing: 0
                                    Repeater {
                                        model: [
                                            { label: "Top left", i: 0 },
                                            { label: "Top right", i: 2 },
                                            { label: "Bottom left", i: 4 },
                                            { label: "Bottom right", i: 6 }
                                        ]
                                        delegate: Item {
                                            id: pinCorner
                                            required property var modelData
                                            Layout.fillWidth: true
                                            implicitHeight: pinRow.implicitHeight
                                            RowLayout {
                                                id: pinRow
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                Label {
                                                    text: pinCorner.modelData.label
                                                    color: win.muted
                                                    font.pixelSize: 11
                                                    Layout.preferredWidth: 72
                                                }
                                                Repeater {
                                                    model: 2
                                                    Slider {
                                                        required property int index
                                                        objectName: "cornerPin-" + (pinCorner.modelData.i + index)
                                                        Layout.fillWidth: true
                                                        from: 0
                                                        to: 1
                                                        stepSize: .005
                                                        value: (win.selection.cornerPin || [0, 0, 1, 0, 0, 1, 1, 1])[pinCorner.modelData.i + index] ?? 0
                                                        enabled: win.selection.locked !== true
                                                        function commit() {
                                                            const pin = (win.selection.cornerPin || [0, 0, 1, 0, 0, 1, 1, 1]).slice();
                                                            pin[pinCorner.modelData.i + index] = value;
                                                            editor.setClip("cornerPin", pin);
                                                        }
                                                        onPressedChanged: if (!pressed)
                                                            commit()
                                                        onMoved: if (!pressed)
                                                            commit()
                                                        ToolTip.visible: hovered
                                                        ToolTip.text: (index === 0 ? "Across" : "Down") + " (fraction of the picture)"
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                // 3D tilt: lean or turn the picture, seen in perspective.
                                ValueGroup {
                                    objectName: "tilt"
                                    title: "3D tilt"
                                    visible: (!!win.selection.assetId && win.selection.picture === true) && (win.picPage("basic"))
                                    rows: [
                                        { key: "tiltX", name: "Lean back", lo: -70, hi: 70, step: 1, dec: 0, unit: "°", obj: "tilt-tiltX", tip: "Leans the top away (or, below zero, towards you)" },
                                        { key: "tiltY", name: "Turn", lo: -70, hi: 70, step: 1, dec: 0, unit: "°", obj: "tilt-tiltY", tip: "Turns the right side away (or, below zero, towards you)" }
                                    ]
                                }
                                // Canvas fill: what shows around a picture that does not fill the frame,
                                // e.g. a portrait video in a landscape project.
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: (!!win.selection.assetId && win.selection.picture === true) && (win.picPage("canvas"))
                                    Label {
                                        text: "Background"
                                        color: win.selection.canvasFill ? win.mint : win.muted
                                        Layout.preferredWidth: 80
                                    }
                                    ComboBox {
                                        id: canvasFill
                                        objectName: "canvasFill"
                                        Accessible.name: "Background"
                                        Layout.fillWidth: true
                                        enabled: win.selection.locked !== true
                                        readonly property var fills: ["", "blur", "#000000", "#ffffff"].concat((win.s.brandColors || []).filter(c => c !== "#000000" && c !== "#ffffff"))
                                        readonly property string current: win.selection.canvasFill || ""
                                        model: ["None (tracks below)", "Blurred picture", "Black", "White"].concat(fills.slice(4)).concat(fills.indexOf(current) < 0 ? [current] : [])
                                        currentIndex: fills.indexOf(current) >= 0 ? fills.indexOf(current) : fills.length
                                        onActivated: index => {
                                            if (index < fills.length)
                                                editor.setClip("canvasFill", fills[index]);
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Fills the frame around the picture: the picture itself blurred, or a colour (brand colours are listed too)"
                                    }
                                    TextField {
                                        objectName: "canvasFillColor"
                                        Layout.preferredWidth: 80
                                        placeholderText: "#rrggbb"
                                        text: win.selection.canvasFill && win.selection.canvasFill !== "blur" ? win.selection.canvasFill : ""
                                        onEditingFinished: if (text.trim() !== (win.selection.canvasFill || ""))
                                            editor.setClip("canvasFill", text.trim())
                                    }
                                }
                                // Anchor: the point that zoom and rotation keep in place, e.g. a corner for
                                // a zoom into that corner.
                                RowLayout {
                                    visible: win.picPage("basic")
                                    Layout.fillWidth: true
                                    Label {
                                        text: "Anchor"
                                        color: (win.selection.anchorX ?? .5) !== .5 || (win.selection.anchorY ?? .5) !== .5 ? win.mint : win.muted
                                        Layout.fillWidth: true
                                    }
                                    Grid {
                                        objectName: "anchorGrid"
                                        columns: 3
                                        spacing: 2
                                        Repeater {
                                            model: 9
                                            Rectangle {
                                                required property int index
                                                readonly property real ax: (index % 3) / 2
                                                readonly property real ay: Math.floor(index / 3) / 2
                                                readonly property bool active: Math.abs((win.selection.anchorX ?? .5) - ax) < .01 && Math.abs((win.selection.anchorY ?? .5) - ay) < .01
                                                objectName: "anchor-" + index
                                                width: 16
                                                height: 12
                                                radius: 2
                                                color: active ? win.mint : "#2b3742"
                                                border.color: "#35404b"
                                                TapHandler {
                                                    enabled: win.selection.locked !== true
                                                    onTapped: editor.setClipValues({ anchorX: parent.ax, anchorY: parent.ay })
                                                }
                                                HoverHandler { id: anchorHover }
                                                ToolTip.visible: anchorHover.hovered
                                                ToolTip.text: "Zoom and rotate around this point"
                                            }
                                        }
                                    }
                                }
                                Section {
                                    objectName: "shapeSection"
                                    title: "Shape"
                                    visible: win.picPage("mask")
                                    resettable: (win.selection.shape || "rect") !== "rect"
                                    onReset: editor.setClip("shape", "rect")
                                    ComboBox {
                                        id: overlayShape
                                        objectName: "overlayShape"
                                        Accessible.name: "Shape"
                                        Layout.fillWidth: true
                                        readonly property var shapes: ["rect", "rounded", "circle"]
                                        model: ["Rectangle", "Rounded corners", "Circle"]
                                        currentIndex: Math.max(0, shapes.indexOf(win.selection.shape || "rect"))
                                        onActivated: editor.setClip("shape", shapes[currentIndex])
                                    }
                                }
                                // Free mask: points clicked on the preview.
                                Section {
                                    objectName: "freeMask"
                                    title: "Free mask"
                                    tip: "Keeps only what is inside an outline you click on the preview"
                                    visible: win.picPage("mask")
                                    spacing: 4
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Action {
                                            objectName: "drawMask"
                                            Layout.fillWidth: true
                                            enabled: win.selection.locked !== true
                                            text: win.drawingMask ? "Done (" + (win.selection.maskOutline || []).length + " points)" : (win.selection.mask ? "Edit mask" : "Draw mask")
                                            onClicked: win.drawingMask = !win.drawingMask
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Click around what should stay visible in the preview, point by point (at least 3); everything outside is hidden. Works with the soft edge."
                                        }
                                        Action {
                                            objectName: "removeMaskPoint"
                                            visible: win.drawingMask
                                            text: "↶ Point"
                                            enabled: (win.selection.maskOutline || []).length > 0
                                            onClicked: editor.removeMaskPoint()
                                        }
                                        Action {
                                            objectName: "clearMask"
                                            visible: !!win.selection.mask || win.drawingMask
                                            text: "Remove"
                                            onClicked: {
                                                editor.clearMask();
                                                win.drawingMask = false;
                                            }
                                        }
                                    }
                                    RowLayout {
                                        visible: !!win.selection.mask
                                        CheckBox {
                                            objectName: "maskSmooth"
                                            text: "Smooth curve"
                                            checked: win.selection.maskSmooth === true
                                            onToggled: editor.setClip("maskSmooth", checked)
                                            ToolTip.visible: hovered
                                            ToolTip.text: "A rounded curve through the points instead of straight lines"
                                        }
                                        CheckBox {
                                            objectName: "maskInvert"
                                            text: "Invert"
                                            checked: win.selection.maskInvert === true
                                            onToggled: editor.setClip("maskInvert", checked)
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Hides what is inside the mask instead"
                                        }
                                    }
                                }
                                // Where the free mask sits; keyframed, or tracked to follow
                                // something in the video.
                                ValueGroup {
                                    objectName: "maskPosition"
                                    title: "Mask position"
                                    visible: win.picPage("mask") && !!win.selection.mask
                                    prefix: "mask-"
                                    rows: [
                                        { key: "maskX", name: "Move X", lo: -1, hi: 1, step: .01, dec: 0, unit: "%", shown: 100, anim: true, tip: "Left (−) or right (+), in picture widths" },
                                        { key: "maskY", name: "Move Y", lo: -1, hi: 1, step: .01, dec: 0, unit: "%", shown: 100, anim: true, tip: "Up (−) or down (+), in picture heights" }
                                    ]
                                    TrackMotion {
                                        prefix: "mask-"
                                        mode: "mask"
                                        Layout.fillWidth: true
                                        visible: win.selection.video === true
                                    }
                                }
                                ValueGroup {
                                    objectName: "frameSection"
                                    title: "Edge and frame"
                                    visible: win.picPage("mask")
                                    prefix: "style-"
                                    rows: [
                                        { key: "radius", name: "Corner radius", lo: 0, hi: .5, dec: 0, unit: "%", shown: 100 },
                                        { key: "feather", name: "Soft edge", lo: 0, hi: .5, dec: 0, unit: "%", shown: 100, tip: "Fades the picture out towards its edge" },
                                        { key: "border", name: "Border", lo: 0, hi: .03, step: .001, dec: 1, unit: "%", shown: 100, tip: "A ring around the picture, in percent of the frame height" },
                                        { key: "shadow", name: "Shadow", lo: 0, hi: 1, step: .05, dec: 0, unit: "%", shown: 100 }
                                    ].filter(r => r.key !== "radius" || win.selection.shape === "rounded")
                                    RowLayout {
                                        visible: (win.selection.border || 0) > 0
                                        Label {
                                            text: "Border colour"
                                            color: win.muted
                                            Layout.fillWidth: true
                                        }
                                        Repeater {
                                            model: ["#ffffff", "#000000", "#64d8bc", "#ffd479", "#ec6f5a"]
                                            Rectangle {
                                                required property string modelData
                                                width: 20
                                                height: 20
                                                radius: 10
                                                color: modelData
                                                border.width: win.selection.borderColor === modelData ? 3 : 1
                                                border.color: win.selection.borderColor === modelData ? win.mint : "#6481a0"
                                                MouseArea {
                                                    anchors.fill: parent
                                                    onClicked: editor.setClip("borderColor", parent.modelData)
                                                }
                                            }
                                        }
                                    }
                                }
                                AiOption {
                                    shown: win.picPage("cutout")
                                    task: "matte"
                                    flag: "aiCutout"
                                    infoKey: "cutout"
                                    label: "Remove background (AI)"
                                    runningText: "Finding the speaker…"
                                    doneText: "Speaker found ✓"
                                    statusName: "cutoutStatus"
                                    runName: "cutoutAnalyze"
                                }
                                Section {
                                    objectName: "chromaKey"
                                    title: "Green or blue screen"
                                    tip: "Removes a green or blue screen behind the picture"
                                    visible: win.picPage("cutout")
                                    checkable: true
                                    checked: win.selection.chromaKey === true
                                    expanded: checked
                                    resettable: checked
                                    onToggled: on => editor.setClip("chromaKey", on)
                                    onReset: editor.setClipValues({ keySimilarity: .25, keyBlend: .08 })
                                    spacing: 4
                                    RowLayout {
                                        Label {
                                            text: "Screen colour"
                                            color: win.muted
                                            Layout.fillWidth: true
                                        }
                                        Repeater {
                                            model: ["#00ff00", "#00b140", "#0047bb"]
                                            Rectangle {
                                                required property string modelData
                                                width: 20
                                                height: 20
                                                radius: 10
                                                color: modelData
                                                border.width: (win.selection.keyColor || "") === modelData ? 3 : 1
                                                border.color: (win.selection.keyColor || "") === modelData ? win.mint : "#6481a0"
                                                MouseArea {
                                                    anchors.fill: parent
                                                    onClicked: editor.setClip("keyColor", parent.modelData)
                                                }
                                            }
                                        }
                                        Rectangle {
                                            // A picked colour that is none of the presets.
                                            visible: ["#00ff00", "#00b140", "#0047bb"].indexOf(win.selection.keyColor || "") < 0
                                            width: 20
                                            height: 20
                                            radius: 10
                                            color: win.selection.keyColor || "transparent"
                                            border.width: 3
                                            border.color: win.mint
                                        }
                                        Action {
                                            objectName: "pickKeyColor"
                                            text: win.pickingKey ? "Click the screen…" : "Pick"
                                            padding: 6
                                            onClicked: win.pickingKey = !win.pickingKey
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Click the green or blue screen in the viewer to key out exactly that colour"
                                        }
                                    }
                                    ValueRow {
                                        key: "keySimilarity"
                                        label: "Tolerance"
                                        from: .01
                                        to: .6
                                        defaultValue: .25
                                        sliderName: "keySimilarity"
                                        tip: "How far a colour may be from the screen colour and still be removed"
                                    }
                                    ValueRow {
                                        key: "keyBlend"
                                        label: "Edge softness"
                                        to: .4
                                        defaultValue: .08
                                        sliderName: "keyBlend"
                                    }
                                }
                                Section {
                                    objectName: "brightnessKey"
                                    title: "Remove by brightness"
                                    visible: win.picPage("cutout")
                                    resettable: (win.selection.lumaKey || "") !== ""
                                    onReset: editor.setClipValues({ lumaTolerance: .1, lumaSoftness: .05 })
                                    spacing: 4
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: "Make transparent"
                                            color: win.muted
                                            Layout.fillWidth: true
                                        }
                                        ComboBox {
                                            objectName: "lumaKey"
                                            Accessible.name: "Remove by brightness"
                                            model: [{ id: "", label: "Off" }, { id: "dark", label: "Black" }, { id: "light", label: "White" }]
                                            textRole: "label"
                                            valueRole: "id"
                                            currentIndex: Math.max(0, ["", "dark", "light"].indexOf(win.selection.lumaKey || ""))
                                            onActivated: editor.setClip("lumaKey", currentValue)
                                            ToolTip.visible: hovered
                                            ToolTip.text: "Makes black (or white) parts transparent, e.g. for fire, smoke or light effects on black"
                                        }
                                    }
                                    ValueRow {
                                        visible: (win.selection.lumaKey || "") !== ""
                                        key: "lumaTolerance"
                                        label: "Tolerance"
                                        to: .6
                                        from: .01
                                        defaultValue: .1
                                        sliderName: (win.selection.lumaKey || "") !== "" ? "lumaTolerance" : ""
                                    }
                                    ValueRow {
                                        visible: (win.selection.lumaKey || "") !== ""
                                        key: "lumaSoftness"
                                        label: "Edge softness"
                                        to: .5
                                        defaultValue: .05
                                        sliderName: (win.selection.lumaKey || "") !== "" ? "lumaSoftness" : ""
                                    }
                                }
                                RowLayout {
                                    visible: win.picPage("basic")
                                    Layout.fillWidth: true
                                    Label {
                                        text: "Blend mode"
                                        color: win.muted
                                        Layout.fillWidth: true
                                    }
                                    ComboBox {
                                        objectName: "blendMode"
                                        Accessible.name: "Blend mode"
                                        model: editor.blendModes()
                                        textRole: "label"
                                        valueRole: "id"
                                        currentIndex: Math.max(0, indexOfValue(win.selection.blendMode || ""))
                                        onActivated: editor.setClip("blendMode", currentValue)
                                        ToolTip.visible: hovered
                                        ToolTip.text: "How the picture mixes with the tracks below it"
                                    }
                                }
                                Rule {}
                            }
                            Caption {
                                visible: win.onTab("more")
                                text: "PICTURE & SOUND"
                            }
                            Action {
                                objectName: "removePauses"
                                Layout.fillWidth: true
                                visible: (win.selection.hasAudio === true && win.selection.reverse !== true) && (win.onTab("more"))
                                enabled: win.selection.locked !== true
                                text: "Remove pauses…"
                                onClicked: pauseDialog.open()
                            }
                            // Text-based editing: the clip's words; click to choose, double-click
                            // to jump there, then cut the chosen words or every "äh" and "ähm".
                            ColumnLayout {
                                id: wordEditor
                                objectName: "wordEditor"
                                Layout.fillWidth: true
                                spacing: 6
                                readonly property var t: win.s.transcript || ({})
                                readonly property var words: t.words || []
                                // The words as text, so the choice resets only when they change.
                                readonly property string wordsKey: (t.clipId || "") + ":" + words.map(w => w.start + w.text).join("|")
                                property var chosen: []
                                visible: (win.selection.hasAudio === true && win.selection.reverse !== true) && (win.onTab("more"))
                                onWordsKeyChanged: chosen = []
                                function toggle(i) {
                                    const next = chosen.slice();
                                    const at = next.indexOf(i);
                                    if (at >= 0)
                                        next.splice(at, 1);
                                    else
                                        next.push(i);
                                    chosen = next;
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: wordEditor.t.status !== "ready" && wordEditor.t.status !== "running"
                                    ComboBox {
                                        id: wordLanguage
                                        objectName: "wordLanguage"
                                        Layout.fillWidth: true
                                        model: captionDialog.languages.map(l => l.label)
                                        currentIndex: Math.max(0, captionDialog.languages.findIndex(l => l.id === (wordEditor.t.language || "auto")))
                                    }
                                    Action {
                                        objectName: "transcribeClip"
                                        text: "Edit by text"
                                        enabled: wordEditor.t.status !== "unavailable"
                                        onClicked: editor.transcribeClip(captionDialog.languages[wordLanguage.currentIndex].id)
                                        ToolTip.visible: hovered
                                        ToolTip.text: wordEditor.t.status === "unavailable" ? wordEditor.t.missing : "Writes down what is said in this clip; then cut words by choosing them"
                                    }
                                }
                                ProgressBar {
                                    Layout.fillWidth: true
                                    visible: wordEditor.t.status === "running"
                                    value: wordEditor.t.progress || 0
                                }
                                ScrollView {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Math.min(wordFlow.implicitHeight + 4, 220)
                                    visible: wordEditor.t.status === "ready" && wordEditor.words.length > 0
                                    clip: true
                                    contentWidth: availableWidth
                                    Flow {
                                        id: wordFlow
                                        width: parent.width
                                        spacing: 3
                                        Repeater {
                                            // A count, not the list: chips are kept while the playhead moves.
                                            model: wordEditor.words.length
                                            delegate: Rectangle {
                                                id: wordChip
                                                required property int index
                                                readonly property var modelData: wordEditor.words[index] || ({})
                                                objectName: "word-" + index
                                                readonly property bool chosen: wordEditor.chosen.indexOf(index) >= 0
                                                readonly property bool current: win.s.playhead >= modelData.start && win.s.playhead < modelData.end
                                                width: wordText.implicitWidth + 8
                                                height: wordText.implicitHeight + 4
                                                radius: 3
                                                color: chosen ? "#7a3b33" : current ? "#28564c" : modelData.filler ? "#4a3e22" : "transparent"
                                                signal clicked
                                                signal doubleClicked
                                                onClicked: wordEditor.toggle(index)
                                                onDoubleClicked: editor.seek(modelData.start)
                                                Label {
                                                    id: wordText
                                                    anchors.centerIn: parent
                                                    text: wordChip.modelData.text
                                                    font.strikeout: wordChip.chosen
                                                    color: wordChip.modelData.filler ? "#e5c07b" : palette.text
                                                }
                                                MouseArea {
                                                    anchors.fill: parent
                                                    onClicked: wordChip.clicked()
                                                    onDoubleClicked: wordChip.doubleClicked()
                                                }
                                            }
                                        }
                                    }
                                }
                                Label {
                                    visible: wordEditor.t.status === "ready" && wordEditor.words.length === 0
                                    text: "No speech found in this clip."
                                    color: win.muted
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: wordEditor.t.status === "ready" && wordEditor.words.length > 0
                                    Action {
                                        objectName: "cutWords"
                                        Layout.fillWidth: true
                                        enabled: wordEditor.chosen.length > 0 && win.selection.locked !== true
                                        text: wordEditor.chosen.length ? "Cut " + wordEditor.chosen.length + (wordEditor.chosen.length === 1 ? " word" : " words") : "Click words to cut"
                                        onClicked: editor.cutWords(wordEditor.chosen)
                                    }
                                    Action {
                                        objectName: "removeFillers"
                                        Layout.fillWidth: true
                                        enabled: (wordEditor.t.fillers || 0) > 0 && win.selection.locked !== true
                                        text: "Remove " + (wordEditor.t.fillers || 0) + " “äh”"
                                        onClicked: editor.removeFillers()
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Cuts every hesitation sound (äh, ähm, hm, um, uh) the transcript contains"
                                    }
                                }
                            }
                            // Beat markers for cutting to music; clips snap to them.
                            RowLayout {
                                Layout.fillWidth: true
                                visible: (win.selection.hasAudio === true && win.selection.reverse !== true) && (win.onTab("more"))
                                Action {
                                    objectName: "markBeats"
                                    Layout.fillWidth: true
                                    readonly property bool finding: (win.s.beats || {}).status === "finding"
                                    enabled: !finding
                                    text: finding ? "Finding beats…" : "Mark the beats"
                                    onClicked: editor.markBeats([1, 2, 4][beatEvery.currentIndex])
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Puts a marker on the beats of the music; clips snap to markers. Undo removes them."
                                }
                                ComboBox {
                                    id: beatEvery
                                    objectName: "beatEvery"
                                    Layout.preferredWidth: 110
                                    model: ["every beat", "every 2nd", "every 4th"]
                                }
                            }
                            // Cutting on the markers (beats or your own).
                            RowLayout {
                                Layout.fillWidth: true
                                visible: ((win.s.markers || []).length > 0) && (win.onTab("more"))
                                Action {
                                    objectName: "splitAtMarkers"
                                    Layout.fillWidth: true
                                    enabled: win.selection.locked !== true
                                    text: "Split at markers"
                                    onClicked: editor.splitAtMarkers()
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Cuts the clip at every marker inside it, e.g. on every beat"
                                }
                                Action {
                                    objectName: "fitToMarkers"
                                    Layout.fillWidth: true
                                    enabled: win.selection.locked !== true
                                    text: "Cut on the beat"
                                    onClicked: editor.fitToMarkers()
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Puts the selected clips (Ctrl+click several) one after another, each ending on the marker nearest to its end"
                                }
                            }
                            // Phone and screen recordings often have a variable frame rate.
                            ColumnLayout {
                                objectName: "variableRate"
                                Layout.fillWidth: true
                                visible: (win.selection.variableRate === true) && (win.onTab("speed"))
                                Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    font.pixelSize: 11
                                    color: "#ffd479"
                                    text: "Variable frame rate (typical of phone and screen recordings). Cutlery plays it by timestamps; if picture and sound drift, convert it."
                                }
                                Action {
                                    objectName: "conformFrameRate"
                                    Layout.fillWidth: true
                                    readonly property bool converting: (win.s.conform || {}).status === "converting"
                                    enabled: !win.s.busy && win.selection.locked !== true
                                    text: converting ? "Converting… " + Math.round(100 * (win.s.conform.progress || 0)) + "%" : "Convert to constant frame rate"
                                    onClicked: editor.conformFrameRate()
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Makes an editing copy (ProRes) with evenly spaced frames in Cutlery's data folder and switches the media to it. The original stays untouched."
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                visible: (win.selection.video === true) && (win.onTab("speed"))
                                Action {
                                    objectName: "freezeFrame"
                                    Layout.fillWidth: true
                                    enabled: win.selection.locked !== true && win.selection.playheadInside === true
                                    text: "Freeze frame here"
                                    onClicked: editor.freezeFrame(freezeSeconds.value / 10)
                                    ToolTip.visible: hovered
                                    ToolTip.text: win.selection.playheadInside ? "Holds the picture at the playhead for the time beside; the rest of the clip continues afterwards" : "Move the playhead into the clip first"
                                }
                                SpinBox {
                                    id: freezeSeconds
                                    objectName: "freezeSeconds"
                                    Accessible.name: "Freeze frame length"
                                    from: 1
                                    to: 600
                                    value: 20
                                    stepSize: 5
                                    editable: true
                                    textFromValue: v => (v / 10).toFixed(1) + " s"
                                    valueFromText: t => Math.round(parseFloat(t) * 10)
                                    ToolTip.visible: hovered
                                    ToolTip.text: "How long the frozen picture lasts (0.1–60 s)"
                                }
                            }
                            Action {
                                objectName: "splitScenes"
                                Layout.fillWidth: true
                                visible: (win.selection.video === true && win.selection.reverse !== true) && (win.onTab("more"))
                                readonly property bool finding: (win.s.scenes || {}).status === "finding"
                                enabled: win.selection.locked !== true && !finding
                                text: finding ? "Finding scene changes…" : "Split at scene changes"
                                onClicked: editor.splitAtScenes(0.5)
                                ToolTip.visible: hovered
                                ToolTip.text: "Cuts the clip into its shots, e.g. a long recording or a downloaded video. Undo restores it."
                            }
                            AiOption {
                                shown: win.picPage("enhance")
                                task: "eyecontact"
                                flag: "eyeContact"
                                infoKey: "eyeContactInfo"
                                label: "Eye contact (AI): look into the camera"
                                runningText: "Correcting the gaze…"
                                doneText: "Eye contact ready ✓ · untick to compare"
                            }
                            AiOption {
                                shown: win.picPage("enhance")
                                task: "upscale"
                                flag: "aiUpscale"
                                infoKey: "upscale"
                                visible: win.picPage("enhance") && (win.selection.upscaleHeight || 0) > 0
                                label: "Enhance resolution (AI, up to " + (win.selection.upscaleHeight || 0) + "p)"
                                runningText: "Upscaling…"
                                doneText: "Sharper picture ready ✓"
                            }
                            // Ready-made motions: keyframes for the selected clips in one step.
                            ComboBox {
                                objectName: "motionPreset"
                                Accessible.name: "Motion"
                                Layout.fillWidth: true
                                visible: (win.selection.audioOnly !== true && ((win.selection.assetId || "") === "" || win.selection.picture === true)) && (win.onTab("animation"))
                                enabled: win.selection.locked !== true
                                readonly property var presets: ["", "popIn", "popOut", "slideLeft", "slideUp", "pulse", "wiggle", "none"]
                                model: ["Add a motion…", "Pop in", "Pop out at the end", "Slide in from the left", "Slide up into place", "Pulse", "Wiggle", "Remove motion"]
                                onActivated: {
                                    if (currentIndex > 0)
                                        editor.applyMotion(presets[currentIndex]);
                                    currentIndex = 0;
                                }
                                ToolTip.visible: hovered
                                ToolTip.text: "Animates the selected clips with keyframes you can change afterwards"
                            }
                            // Sound: clean-up, tone and dynamics of the clip's audio.
                            Section {
                                title: ({ basic: "Sound", voice: "Voice", cleanup: "Presets and tools" })[win.selectionKind === "audio" ? win.inspectorTab : win.inspectorSub] || "Sound"
                                objectName: "soundSection"
                                visible: (win.selection.hasAudio === true) && (win.audioPage("basic") || win.audioPage("voice") || win.audioPage("cleanup"))
                                Layout.fillWidth: true
                                spacing: 6
                                Action {
                                    visible: win.audioPage("basic")
                                    objectName: "extractAudio"
                                    Layout.fillWidth: true
                                    text: "Save sound as file…"
                                    enabled: !win.s.busy && win.selection.muted !== true
                                    onClicked: audioFileDialog.open()
                                    ToolTip.visible: hovered
                                    ToolTip.text: "The clip's sound as you hear it (trim, speed, volume, sound tools), without the other clips, as WAV, MP3 or M4A"
                                }
                                ComboBox {
                                    visible: win.audioPage("cleanup")
                                    objectName: "soundPreset"
                                    Layout.fillWidth: true
                                    enabled: win.selection.locked !== true
                                    readonly property var presets: [
                                        { label: "Apply a sound preset…", values: null },
                                        { label: "Natural (reset)", values: {} },
                                        { label: "Clear voice", values: { lowCut: 80, denoise: .4, gate: .2, eqMid: 3, deess: .3, compressor: .5 } },
                                        { label: "Warm podcast voice", values: { lowCut: 60, denoise: .3, eqLow: 3, eqMid: 2, eqHigh: -1, deess: .4, compressor: .6 } },
                                        { label: "Noisy room", values: { lowCut: 120, denoise: .8, gate: .5, eqMid: 2, compressor: .4 } },
                                        { label: "Echoing room", values: { lowCut: 80, denoise: .3, dereverb: .7, eqMid: 2, compressor: .4 } },
                                        { label: "Phone call", values: { lowCut: 300, eqLow: -12, eqMid: 6, eqHigh: -12, compressor: .7 } },
                                        { label: "Music: more punch", values: { eqLow: 4, eqHigh: 3, compressor: .3 } }
                                    ]
                                    model: presets.map(p => p.label)
                                    onActivated: index => {
                                        const preset = presets[index].values;
                                        if (preset) {
                                            const values = { eqLow: 0, eqMid: 0, eqHigh: 0, lowCut: 0, compressor: 0, gate: 0, denoise: 0, dereverb: 0, deess: 0, reverb: 0, echo: 0 };
                                            for (const k in preset)
                                                values[k] = preset[k];
                                            editor.setClipValues(values);
                                        }
                                        currentIndex = 0;
                                    }
                                }
                                // Voice and music apart (AI): the clip plays the voice alone or
                                // everything but the voice, once the sound is separated.
                                ColumnLayout {
                                    id: stemsBox
                                    objectName: "stemsBox"
                                    visible: win.audioPage("cleanup") && win.selection.stemsInfo !== undefined
                                    Layout.fillWidth: true
                                    spacing: 4
                                    readonly property var info: win.selection.stemsInfo || ({})
                                    readonly property bool working: info.status === "running" || info.status === "queued"
                                    readonly property string missing: (win.s.aiMissing || {}).separate || ""
                                    readonly property bool on: (win.selection.stems || "") !== ""
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: "Voice and music"
                                            color: stemsBox.on ? win.mint : win.muted
                                            Layout.preferredWidth: 105
                                        }
                                        ComboBox {
                                            objectName: "stems"
                                            Accessible.name: "Voice and music"
                                            Layout.fillWidth: true
                                            enabled: win.selection.locked !== true && (stemsBox.missing === "" || stemsBox.on)
                                            readonly property var kinds: ["", "voice", "music"]
                                            model: ["As recorded", "Voice only (AI)", "Without the voice (AI)"]
                                            currentIndex: Math.max(0, kinds.indexOf(win.selection.stems || ""))
                                            onActivated: index => {
                                                editor.setClip("stems", kinds[index]);
                                                if (kinds[index] !== "" && stemsBox.info.covered !== true && !stemsBox.working)
                                                    editor.runAi("separate");
                                            }
                                            ToolTip.visible: hovered
                                            ToolTip.text: stemsBox.missing !== "" ? stemsBox.missing : "Separates the voice from music and background on this computer: keep only the speech or singing, or only the music (karaoke)"
                                        }
                                    }
                                    ProgressBar {
                                        Layout.fillWidth: true
                                        visible: stemsBox.working
                                        value: stemsBox.info.progress || 0
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        visible: stemsBox.on || stemsBox.working
                                        Label {
                                            objectName: "stemsStatus"
                                            Layout.fillWidth: true
                                            wrapMode: Text.Wrap
                                            font.pixelSize: 11
                                            color: stemsBox.info.status === "failed" ? "#ec6f5a" : win.muted
                                            text: stemsBox.info.status === "queued" ? "Waiting for another AI job…"
                                                : stemsBox.working ? "Separating voice and music… " + Math.round((stemsBox.info.progress || 0) * 100) + "%"
                                                    + (stemsBox.info.device === "gpu" ? " · GPU" : stemsBox.info.device === "cpu" ? " · CPU (slow)" : "")
                                                : stemsBox.info.status === "failed" ? "Failed: " + (stemsBox.info.error || "")
                                                : stemsBox.info.covered === true ? "Separated ✓"
                                                : stemsBox.missing === "" ? "Not separated for this range yet; it plays as recorded"
                                                : stemsBox.missing
                                        }
                                        Action {
                                            objectName: "stemsRun"
                                            visible: !stemsBox.working && stemsBox.on && stemsBox.info.covered !== true && stemsBox.missing === ""
                                            text: "Run"
                                            padding: 6
                                            onClicked: editor.runAi("separate")
                                        }
                                        Action {
                                            visible: stemsBox.working
                                            text: "Stop"
                                            padding: 6
                                            onClicked: editor.cancelAi()
                                        }
                                    }
                                }
                                ValueGroup {
                                    objectName: "cleanupSection"
                                    title: "Clean-up"
                                    visible: win.audioPage("cleanup")
                                    prefix: "sound-"
                                    rows: [
                                        { key: "lowCut", name: "Low cut", lo: 0, hi: 300, step: 5, dec: 0, unit: " Hz", tip: "Removes rumble, hum and wind below this frequency" },
                                        { key: "denoise", name: "Noise reduction", lo: 0, hi: 1, dec: 0, unit: "%", shown: 100, tip: "Reduces steady hiss and hum" },
                                        { key: "dereverb", name: "Less room echo", lo: 0, hi: 1, dec: 0, unit: "%", shown: 100, tip: "Turns down the echo of the room that trails each word (hall, bare rooms)" },
                                        { key: "gate", name: "Noise gate", lo: 0, hi: 1, dec: 0, unit: "%", shown: 100, tip: "Lowers the sound between phrases" }
                                    ]
                                }
                                ValueGroup {
                                    objectName: "toneSection"
                                    title: "Tone"
                                    visible: win.audioPage("cleanup")
                                    prefix: "sound-"
                                    rows: [
                                        { key: "eqLow", name: "Bass", lo: -12, hi: 12, step: .5, dec: 1, unit: " dB", tip: "Below 100 Hz" },
                                        { key: "eqMid", name: "Presence", lo: -12, hi: 12, step: .5, dec: 1, unit: " dB", tip: "Around 2.5 kHz, where speech is clear" },
                                        { key: "eqHigh", name: "Treble", lo: -12, hi: 12, step: .5, dec: 1, unit: " dB", tip: "Above 8 kHz" },
                                        { key: "deess", name: "De-esser", lo: 0, hi: 1, dec: 0, unit: "%", shown: 100, tip: "Softens sharp S sounds" }
                                    ]
                                }
                                ValueGroup {
                                    objectName: "dynamicsSection"
                                    title: "Dynamics"
                                    visible: win.audioPage("cleanup")
                                    prefix: "sound-"
                                    rows: [
                                        { key: "compressor", name: "Compressor", lo: 0, hi: 1, dec: 0, unit: "%", shown: 100, tip: "Evens out loud and quiet parts" }
                                    ]
                                }
                                ValueGroup {
                                    objectName: "roomSection"
                                    title: "Room and pitch"
                                    visible: win.audioPage("voice")
                                    prefix: "sound-"
                                    rows: [
                                        { key: "reverb", name: "Reverb", lo: 0, hi: 1, dec: 0, unit: "%", shown: 100, tip: "The sound of a room" },
                                        { key: "echo", name: "Echo", lo: 0, hi: 1, dec: 0, unit: "%", shown: 100, tip: "Repeats a third of a second apart" },
                                        { key: "pitch", name: "Pitch", lo: -12, hi: 12, step: .5, dec: 1, unit: " st", tip: "Higher or lower voice at the same speed, in semitones" }
                                    ]
                                }
                                RowLayout {
                                    visible: win.audioPage("voice")
                                    Layout.fillWidth: true
                                    Label {
                                        text: "Voice"
                                        color: win.selection.voice ? win.mint : win.muted
                                        Layout.preferredWidth: 105
                                    }
                                    ComboBox {
                                        objectName: "voice"
                                        Layout.fillWidth: true
                                        enabled: win.selection.locked !== true
                                        readonly property var voices: ["", "robot", "telephone", "megaphone", "alien", "chipmunk", "monster"]
                                        model: ["Normal", "Robot", "Telephone", "Megaphone", "Alien", "Chipmunk", "Monster"]
                                        currentIndex: Math.max(0, voices.indexOf(win.selection.voice || ""))
                                        onActivated: index => editor.setClip("voice", voices[index])
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Changes how the voice sounds; works with the pitch above"
                                    }
                                }
                                // Measured sound tools: even loudness across clips, noise learned here.
                                RowLayout {
                                    visible: win.audioPage("cleanup")
                                    Layout.fillWidth: true
                                    readonly property var measure: win.s.soundMeasure || ({})
                                    Action {
                                        objectName: "evenLoudness"
                                        Layout.fillWidth: true
                                        enabled: win.selection.locked !== true && parent.measure.status !== "measuring"
                                        text: parent.measure.status === "measuring" && parent.measure.task === "loudness" ? "Measuring…" : "Even loudness"
                                        onClicked: editor.evenLoudness(-16)
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Measures each selected clip (Ctrl+click several) and sets its volume so all play equally loud (−16 LUFS, like podcasts and online video)"
                                    }
                                    Action {
                                        objectName: "learnNoise"
                                        Layout.fillWidth: true
                                        enabled: win.selection.locked !== true && win.selection.playheadInside === true && parent.measure.status !== "measuring"
                                        text: parent.measure.status === "measuring" && parent.measure.task === "noise" ? "Listening…" : "Learn noise here"
                                        onClicked: editor.learnNoise()
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Put the playhead where only background noise is heard: noise reduction then works against exactly that noise" + ((win.selection.noiseFloor || 0) !== 0 ? " (learned: " + win.selection.noiseFloor + " dB)" : "")
                                    }
                                }
                            }
                            // Colour and look of the clip's picture.
                            Section {
                                title: ({ basic: "Look", hsl: "Selective colour", curves: "Curves", wheels: "Colour wheels", lut: "LUT" })[win.inspectorSub] || "Colour"
                                objectName: "lookSection"
                                visible: (win.selection.picture === true) && (win.onTab("adjust"))
                                Layout.fillWidth: true
                                spacing: 6
                                ComboBox {
                                    visible: win.adjustPage("basic")
                                    id: lookPreset
                                    objectName: "lookPreset"
                                    Accessible.name: "Look"
                                    Layout.fillWidth: true
                                    enabled: win.selection.locked !== true
                                    model: win.looks.map(l => l.label)
                                    onActivated: index => {
                                        win.applyLook(index);
                                        currentIndex = 0;
                                    }
                                }
                                Action {
                                    visible: win.adjustPage("basic")
                                    objectName: "autoColour"
                                    Layout.fillWidth: true
                                    readonly property bool measuring: (win.s.autoColour || {}).status === "measuring"
                                    enabled: !measuring && win.selection.locked !== true
                                    text: measuring ? "Measuring…" : "Auto colour"
                                    onClicked: editor.autoColour()
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Measures the clip and sets brightness, contrast, temperature and tint for a full range and neutral greys"
                                }
                                ValueGroup {
                                    objectName: "colourSection"
                                    title: "Colour"
                                    visible: win.adjustPage("basic")
                                    prefix: "look-"
                                    rows: [
                                        { key: "temperature", name: "Temperature", lo: -1, hi: 1, tip: "Warmer (right) or cooler (left) light" },
                                        { key: "tint", name: "Tint", lo: -1, hi: 1, tip: "Magenta (right) or green (left)" },
                                        { key: "vibrance", name: "Vibrance", lo: -1, hi: 1, tip: "Saturates muted colours more than strong ones; skin stays natural" }
                                    ]
                                }
                                ValueGroup {
                                    objectName: "tonesSection"
                                    title: "Tones"
                                    visible: win.adjustPage("basic")
                                    prefix: "look-"
                                    rows: [
                                        { key: "shadows", name: "Shadows", lo: -1, hi: 1, tip: "Lift or deepen the dark parts" },
                                        { key: "highlights", name: "Highlights", lo: -1, hi: 1, tip: "Recover or brighten the bright parts" },
                                        { key: "whites", name: "Whites", lo: -1, hi: 1, tip: "Where the brightest tones end: up makes them clip sooner, down softens them" },
                                        { key: "blacks", name: "Blacks", lo: -1, hi: 1, tip: "Where the darkest tones end: down deepens them, up fades them like film" }
                                    ]
                                }
                                ValueGroup {
                                    objectName: "detailsSection"
                                    title: "Details"
                                    visible: win.adjustPage("basic")
                                    prefix: "look-"
                                    rows: [
                                        { key: "sharpen", name: "Sharpen", lo: 0, hi: 1, dec: 0, unit: "%", shown: 100, tip: "Contrast-adaptive sharpening" },
                                        { key: "glow", name: "Glow", lo: 0, hi: 1, dec: 0, unit: "%", shown: 100, tip: "A soft glow around bright areas" },
                                        { key: "vignette", name: "Vignette", lo: 0, hi: 1, dec: 0, unit: "%", shown: 100, tip: "Darker corners draw the eye to the centre" },
                                        { key: "grain", name: "Film grain", lo: 0, hi: 1, dec: 0, unit: "%", shown: 100, tip: "Moving grain like film" }
                                    ]
                                }
                                // Tone curves: drag points, click to add one, double-click to remove it.
                                RowLayout {
                                    visible: win.adjustPage("curves")
                                    Layout.fillWidth: true
                                    Label {
                                        text: "Curves"
                                        color: ["curveMaster", "curveRed", "curveGreen", "curveBlue"].some(k => !!win.selection[k]) ? win.mint : win.muted
                                        Layout.fillWidth: true
                                    }
                                    Repeater {
                                        model: [{ key: "curveMaster", label: "All", colour: "#d8dee9" }, { key: "curveRed", label: "R", colour: "#e06c75" }, { key: "curveGreen", label: "G", colour: "#98c379" }, { key: "curveBlue", label: "B", colour: "#61afef" }]
                                        ToolButton {
                                            required property var modelData
                                            objectName: "curveChannel-" + modelData.key
                                            text: modelData.label
                                            checkable: true
                                            checked: curveEditor.key === modelData.key
                                            onClicked: curveEditor.key = modelData.key
                                            contentItem: Label {
                                                text: parent.text
                                                color: parent.modelData.colour
                                                font.bold: parent.checked
                                                horizontalAlignment: Text.AlignHCenter
                                            }
                                        }
                                    }
                                    ToolButton {
                                        text: "↺"
                                        Accessible.name: "Reset curve"
                                        enabled: !!win.selection[curveEditor.key] && win.selection.locked !== true
                                        onClicked: editor.setClip(curveEditor.key, "")
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Reset this curve"
                                    }
                                }
                                Canvas {
                                    visible: win.adjustPage("curves")
                                    id: curveEditor
                                    objectName: "curveEditor"
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: width
                                    property string key: "curveMaster"
                                    // Points as [x, y] in 0..1, from the clip or the straight line.
                                    readonly property var stored: {
                                        const text = win.selection[key] || "";
                                        const list = text.split(" ").filter(p => p.length).map(p => p.split("/").map(Number));
                                        return list.length >= 2 ? list : [[0, 0], [1, 1]];
                                    }
                                    property var points: stored
                                    property int dragging: -1
                                    onStoredChanged: { points = stored; requestPaint(); }
                                    onKeyChanged: requestPaint()
                                    onWidthChanged: requestPaint()
                                    readonly property color lineColour: ({ curveMaster: "#d8dee9", curveRed: "#e06c75", curveGreen: "#98c379", curveBlue: "#61afef" })[key]
                                    function commit() {
                                        const sorted = points.slice().sort((a, b) => a[0] - b[0]);
                                        const unique = sorted.filter((p, i) => i === 0 || p[0] > sorted[i - 1][0] + 0.001);
                                        const straight = unique.length === 2 && unique[0][0] === 0 && unique[0][1] === 0 && unique[1][0] === 1 && unique[1][1] === 1;
                                        editor.setClip(key, straight ? "" : unique.map(p => p[0].toFixed(3) + "/" + p[1].toFixed(3)).join(" "));
                                    }
                                    function nearest(mx, my) {
                                        for (let i = 0; i < points.length; ++i)
                                            if (Math.abs(points[i][0] * width - mx) < 9 && Math.abs((1 - points[i][1]) * height - my) < 9)
                                                return i;
                                        return -1;
                                    }
                                    onPaint: {
                                        const g = getContext("2d");
                                        g.reset();
                                        g.fillStyle = "#151b22";
                                        g.fillRect(0, 0, width, height);
                                        g.strokeStyle = "#2b333e";
                                        g.lineWidth = 1;
                                        for (let i = 1; i < 4; ++i) {
                                            g.beginPath();
                                            g.moveTo(i * width / 4, 0);
                                            g.lineTo(i * width / 4, height);
                                            g.moveTo(0, i * height / 4);
                                            g.lineTo(width, i * height / 4);
                                            g.stroke();
                                        }
                                        const sorted = points.slice().sort((a, b) => a[0] - b[0]);
                                        g.strokeStyle = lineColour;
                                        g.lineWidth = 2;
                                        g.beginPath();
                                        g.moveTo(0, (1 - sorted[0][1]) * height);
                                        for (const p of sorted)
                                            g.lineTo(p[0] * width, (1 - p[1]) * height);
                                        g.lineTo(width, (1 - sorted[sorted.length - 1][1]) * height);
                                        g.stroke();
                                        g.fillStyle = lineColour;
                                        for (const p of sorted)
                                            g.fillRect(p[0] * width - 3.5, (1 - p[1]) * height - 3.5, 7, 7);
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        enabled: win.selection.locked !== true
                                        preventStealing: true
                                        function at(mouse) {
                                            return [Math.max(0, Math.min(1, mouse.x / width)), Math.max(0, Math.min(1, 1 - mouse.y / height))];
                                        }
                                        onPressed: mouse => {
                                            let i = curveEditor.nearest(mouse.x, mouse.y);
                                            if (i < 0 && curveEditor.points.length < 16) {
                                                curveEditor.points = curveEditor.points.concat([at(mouse)]);
                                                i = curveEditor.points.length - 1;
                                            }
                                            curveEditor.dragging = i;
                                            curveEditor.requestPaint();
                                        }
                                        onPositionChanged: mouse => {
                                            if (curveEditor.dragging < 0)
                                                return;
                                            const next = curveEditor.points.slice();
                                            next[curveEditor.dragging] = at(mouse);
                                            curveEditor.points = next;
                                            curveEditor.requestPaint();
                                        }
                                        onReleased: {
                                            if (curveEditor.dragging >= 0)
                                                curveEditor.commit();
                                            curveEditor.dragging = -1;
                                        }
                                        onDoubleClicked: mouse => {
                                            const i = curveEditor.nearest(mouse.x, mouse.y);
                                            if (i >= 0 && curveEditor.points.length > 2) {
                                                const next = curveEditor.points.slice();
                                                next.splice(i, 1);
                                                curveEditor.points = next;
                                                curveEditor.commit();
                                            }
                                        }
                                    }
                                }
                                // Selective colour: change only some colours (all when none is chosen).
                                RowLayout {
                                    visible: win.adjustPage("hsl")
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Label {
                                        text: "Colours"
                                        color: (win.selection.hslHue || win.selection.hslSaturation || win.selection.hslLightness) ? win.mint : win.muted
                                        Layout.fillWidth: true
                                    }
                                    Repeater {
                                        model: [{ id: "r", colour: "#e05050" }, { id: "y", colour: "#e0c040" }, { id: "g", colour: "#50b050" }, { id: "c", colour: "#40c0c0" }, { id: "b", colour: "#4070e0" }, { id: "m", colour: "#c050c0" }]
                                        Rectangle {
                                            id: hslSwatch
                                            required property var modelData
                                            objectName: "hslColour-" + modelData.id
                                            readonly property var chosen: (win.selection.hslColors || "").split(" ").filter(x => x.length)
                                            readonly property bool on: chosen.indexOf(modelData.id) >= 0
                                            width: 20
                                            height: 20
                                            radius: 10
                                            color: modelData.colour
                                            opacity: on || chosen.length === 0 ? 1 : 0.35
                                            border.width: on ? 2 : 0
                                            border.color: "white"
                                            signal clicked
                                            onClicked: {
                                                const next = on ? chosen.filter(x => x !== modelData.id) : chosen.concat([modelData.id]);
                                                editor.setClip("hslColors", next.join(" "));
                                            }
                                            MouseArea {
                                                anchors.fill: parent
                                                enabled: win.selection.locked !== true
                                                onClicked: hslSwatch.clicked()
                                            }
                                        }
                                    }
                                }
                                Repeater {
                                    model: [
                                        { key: "hslHue", name: "Hue shift", lo: -180, hi: 180, step: 1 },
                                        { key: "hslSaturation", name: "Colour saturation", lo: -1, hi: 1, step: .01 },
                                        { key: "hslLightness", name: "Colour lightness", lo: -1, hi: 1, step: .01 }
                                    ]
                                    ColumnLayout {
                                        visible: win.adjustPage("hsl")
                                        id: hslRow
                                        required property var modelData
                                        Layout.fillWidth: true
                                        spacing: 0
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Label {
                                                text: hslRow.modelData.name
                                                color: Number(win.selection[hslRow.modelData.key] || 0) !== 0 ? win.mint : win.muted
                                                Layout.fillWidth: true
                                            }
                                            Label {
                                                text: Number(win.selection[hslRow.modelData.key] || 0).toFixed(hslRow.modelData.step < 1 ? 2 : 0)
                                                font.pixelSize: 10
                                            }
                                        }
                                        Slider {
                                            objectName: "look-" + hslRow.modelData.key
                                            Accessible.name: hslRow.modelData.name
                                            Layout.fillWidth: true
                                            from: hslRow.modelData.lo
                                            to: hslRow.modelData.hi
                                            stepSize: hslRow.modelData.step
                                            value: Number(win.selection[hslRow.modelData.key] || 0)
                                            enabled: win.selection.locked !== true
                                            onPressedChanged: if (!pressed)
                                                editor.setClip(hslRow.modelData.key, value)
                                            onMoved: if (!pressed)
                                                editor.setClip(hslRow.modelData.key, value)
                                            TapHandler {
                                                acceptedButtons: Qt.LeftButton
                                                onDoubleTapped: editor.setClip(hslRow.modelData.key, 0)
                                            }
                                        }
                                    }
                                }
                                RowLayout {
                                    visible: win.adjustPage("lut")
                                    Layout.fillWidth: true
                                    Label {
                                        objectName: "lutName"
                                        Layout.fillWidth: true
                                        elide: Text.ElideMiddle
                                        color: win.selection.lutMissing ? "#e5534b" : win.selection.lut ? win.mint : win.muted
                                        text: !win.selection.lut ? "No LUT" : win.selection.lutMissing ? "LUT missing: " + win.selection.lutName : "LUT: " + win.selection.lutName
                                    }
                                    Action {
                                        objectName: "chooseLut"
                                        text: win.selection.lut ? "Change…" : "Load LUT…"
                                        padding: 6
                                        enabled: win.selection.locked !== true
                                        onClicked: lutDialog.open()
                                        ToolTip.visible: hovered
                                        ToolTip.text: "A 3D colour lookup table (.cube or .3dl), e.g. a camera log conversion or a film look"
                                    }
                                    Action {
                                        text: "✕"
                                        padding: 6
                                        visible: !!win.selection.lut
                                        onClicked: editor.setClip("lut", "")
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Remove the LUT"
                                    }
                                }
                                // Colour wheels: drag the point towards a colour to push it into the
                                // shadows, midtones or highlights; double-click resets.
                                RowLayout {
                                    visible: win.adjustPage("wheels")
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Repeater {
                                        model: [
                                            { key: "lift", name: "Shadows" },
                                            { key: "gamma", name: "Midtones" },
                                            { key: "gain", name: "Highlights" }
                                        ]
                                        ColumnLayout {
                                            id: wheel
                                            required property var modelData
                                            readonly property real wx: win.selection[modelData.key + "X"] || 0
                                            readonly property real wy: win.selection[modelData.key + "Y"] || 0
                                            Layout.fillWidth: true
                                            spacing: 2
                                            Item {
                                                objectName: "wheel-" + wheel.modelData.key
                                                Layout.alignment: Qt.AlignHCenter
                                                implicitWidth: 64
                                                implicitHeight: 64
                                                // While dragging, the point follows the mouse; otherwise the clip's values.
                                                property bool dragging: false
                                                property real mouseX: 0
                                                property real mouseY: 0
                                                readonly property real dragX: dragging ? mouseX : wheel.wx
                                                readonly property real dragY: dragging ? mouseY : wheel.wy
                                                function set(x, y) {
                                                    const r = Math.hypot(x, y);
                                                    const k = r > 1 ? 1 / r : 1;
                                                    const values = {};
                                                    values[wheel.modelData.key + "X"] = Math.round(x * k * 1000) / 1000;
                                                    values[wheel.modelData.key + "Y"] = Math.round(y * k * 1000) / 1000;
                                                    editor.setClipValues(values);
                                                }
                                                Canvas {
                                                    anchors.fill: parent
                                                    onPaint: {
                                                        const ctx = getContext("2d");
                                                        ctx.reset();
                                                        // Red at the right, green up-left, blue down-left.
                                                        const g = ctx.createConicalGradient(width / 2, height / 2, 0);
                                                        g.addColorStop(0, "#c04040");
                                                        g.addColorStop(1 / 3, "#40c040");
                                                        g.addColorStop(2 / 3, "#4040c0");
                                                        g.addColorStop(1, "#c04040");
                                                        ctx.fillStyle = g;
                                                        ctx.beginPath();
                                                        ctx.arc(width / 2, height / 2, width / 2 - 1, 0, 2 * Math.PI);
                                                        ctx.fill();
                                                        const r = ctx.createRadialGradient(width / 2, height / 2, 0, width / 2, height / 2, width / 2);
                                                        r.addColorStop(0, "#ff808080");
                                                        r.addColorStop(1, "#00808080");
                                                        ctx.fillStyle = r;
                                                        ctx.fill();
                                                    }
                                                }
                                                Rectangle {
                                                    width: 10
                                                    height: 10
                                                    radius: 5
                                                    color: "transparent"
                                                    border.color: "white"
                                                    border.width: 2
                                                    x: parent.width / 2 + parent.dragX * (parent.width / 2 - 2) - width / 2
                                                    y: parent.height / 2 - parent.dragY * (parent.height / 2 - 2) - height / 2
                                                }
                                                MouseArea {
                                                    anchors.fill: parent
                                                    enabled: win.selection.locked !== true
                                                    function at(mouse) {
                                                        parent.mouseX = Math.max(-1, Math.min(1, (mouse.x - width / 2) / (width / 2 - 2)));
                                                        parent.mouseY = Math.max(-1, Math.min(1, (height / 2 - mouse.y) / (height / 2 - 2)));
                                                        parent.dragging = true;
                                                    }
                                                    onPressed: mouse => at(mouse)
                                                    onPositionChanged: mouse => at(mouse)
                                                    onReleased: {
                                                        parent.set(parent.mouseX, parent.mouseY);
                                                        parent.dragging = false;
                                                    }
                                                    onDoubleClicked: parent.set(0, 0)
                                                }
                                            }
                                            Label {
                                                Layout.alignment: Qt.AlignHCenter
                                                text: wheel.modelData.name
                                                font.pixelSize: 10
                                                color: wheel.wx !== 0 || wheel.wy !== 0 ? win.mint : win.muted
                                            }
                                        }
                                    }
                                }
                                // LUT library: the LUTs kept in Cutlery's data folder, one pick away.
                                RowLayout {
                                    visible: win.adjustPage("lut")
                                    Layout.fillWidth: true
                                    ComboBox {
                                        objectName: "lutLibrary"
                                        Accessible.name: "LUT"
                                        Layout.fillWidth: true
                                        readonly property var luts: win.s.lutLibrary || []
                                        model: [luts.length ? "LUT library…" : "LUT library is empty"].concat(luts.map(l => l.name))
                                        enabled: luts.length > 0 && win.selection.locked !== true
                                        onActivated: index => {
                                            if (index > 0)
                                                editor.setClip("lut", luts[index - 1].path);
                                            currentIndex = 0;
                                        }
                                    }
                                    Action {
                                        objectName: "addLutToLibrary"
                                        text: "+ Library"
                                        padding: 6
                                        onClicked: lutLibraryDialog.open()
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Copy .cube or .3dl files into the library, for every project"
                                    }
                                }
                                RowLayout {
                                    visible: (!!win.selection.lut) && (win.adjustPage("lut"))
                                    Layout.fillWidth: true
                                    Label { text: "LUT strength"; color: win.muted }
                                    Slider {
                                        objectName: "lutStrength"
                                        Layout.fillWidth: true
                                        from: 0
                                        to: 1
                                        stepSize: .01
                                        value: win.selection.lutStrength ?? 1
                                        onPressedChanged: if (!pressed)
                                            editor.setClip("lutStrength", value)
                                        onMoved: if (!pressed)
                                            editor.setClip("lutStrength", value)
                                    }
                                }
                            }
                            // Style effects and camera movement of the clip's picture.
                            Section {
                                title: win.onTab("effects") ? "Style effect" : "Enhance"
                                objectName: "effectsSection"
                                visible: (win.selection.picture === true) && (win.onTab("effects") || win.picPage("enhance"))
                                Layout.fillWidth: true
                                spacing: 6
                                RowLayout {
                                    visible: win.onTab("effects")
                                    Layout.fillWidth: true
                                    ComboBox {
                                        id: fxChoice
                                        objectName: "fxChoice"
                                        Accessible.name: "Style effect"
                                        Layout.fillWidth: true
                                        enabled: win.selection.locked !== true
                                        readonly property var keys: ["", "shake", "glitch", "vhs", "film", "sketch", "poster", "fisheye", "mirror"]
                                        model: ["No effect", "Camera shake", "Glitch", "VHS", "Old film", "Sketch", "Poster", "Fisheye", "Mirror"]
                                        currentIndex: Math.max(0, keys.indexOf(win.selection.fx || ""))
                                        onActivated: index => editor.setClip("fx", keys[index])
                                        ToolTip.visible: hovered
                                        ToolTip.text: "A style effect on the clip's picture"
                                    }
                                    Slider {
                                        objectName: "fxStrength"
                                        visible: !!win.selection.fx
                                        Layout.preferredWidth: 90
                                        from: 0
                                        to: 1
                                        stepSize: .01
                                        value: win.selection.fxStrength ?? .5
                                        enabled: win.selection.locked !== true
                                        onPressedChanged: if (!pressed)
                                            editor.setClip("fxStrength", value)
                                        onMoved: if (!pressed)
                                            editor.setClip("fxStrength", value)
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Effect strength"
                                    }
                                }
                                ValueGroup {
                                    objectName: "motionBlurSection"
                                    title: "Motion blur"
                                    visible: win.selection.video === true && win.onTab("effects")
                                    rows: [
                                        { key: "motionBlur", obj: "motionBlur", name: "Amount", lo: 0, hi: 1, dec: 0, unit: "%", shown: 100, tip: "Smears fast movement across frames" }
                                    ]
                                }
                                Section {
                                    objectName: "stabilize"
                                    title: "Stabilize"
                                    tip: "Smooths a shaky hand-held camera; the edges are filled in"
                                    visible: win.selection.video === true && win.picPage("enhance")
                                    checkable: true
                                    checked: win.selection.stabilize === true
                                    onToggled: on => editor.setClip("stabilize", on)
                                    resettable: true
                                    onReset: editor.setClipValues({ stabilizeStrength: .33, stabilizeZoom: false })
                                    ValueRow {
                                        key: "stabilizeStrength"
                                        label: "Strength"
                                        sliderName: "stabilizeStrength"
                                        stepSize: .05
                                        defaultValue: .33
                                        decimals: 0
                                        unit: "%"
                                        shown: 100
                                        tip: "How much shake is evened out; stronger may also smooth intended camera moves"
                                    }
                                    CheckBox {
                                        objectName: "stabilizeZoom"
                                        text: "Zoom in so no filled-in edge shows"
                                        checked: win.selection.stabilizeZoom === true
                                        onToggled: editor.setClip("stabilizeZoom", checked)
                                    }
                                }
                                ValueGroup {
                                    objectName: "noiseSection"
                                    title: "Noise and flicker"
                                    visible: win.selection.video === true && win.picPage("enhance")
                                    rows: [
                                        { key: "videoDenoise", obj: "videoDenoise", name: "Video noise", lo: 0, hi: 1, step: .05, dec: 0, unit: "%", shown: 100, tip: "Calms grain and noise in dark or low-light video by averaging neighbouring frames" },
                                        { key: "deflicker", obj: "deflicker", name: "Flicker", lo: 0, hi: 1, step: .05, dec: 0, unit: "%", shown: 100, tip: "Evens out brightness that pulses from frame to frame (lamps, time-lapses); stronger compares more frames" }
                                    ]
                                }
                                ValueGroup {
                                    objectName: "lensSection"
                                    title: "Lens"
                                    visible: !!win.selection.assetId && win.selection.picture === true && win.picPage("enhance")
                                    rows: [
                                        { key: "lensCorrection", obj: "lensCorrection", name: "Lens distortion", lo: -1, hi: 1, step: .05, dec: 0, unit: "%", shown: 100, tip: "Above 0 straightens lines that a wide-angle or action camera bends outwards; below 0 straightens lines bent inwards" }
                                    ]
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: (win.selection.video === true && !!win.selection.hdr) && (win.picPage("enhance"))
                                    Label {
                                        text: "HDR"
                                        color: win.muted
                                        Layout.fillWidth: true
                                    }
                                    ComboBox {
                                        objectName: "toneMap"
                                        Layout.preferredWidth: 170
                                        enabled: win.selection.locked !== true
                                        readonly property var modes: ["", "bright", "off"]
                                        model: ["Natural", "Bright", "Unconverted"]
                                        currentIndex: Math.max(0, modes.indexOf(win.selection.toneMap || ""))
                                        onActivated: index => editor.setClip("toneMap", modes[index])
                                        ToolTip.visible: hovered
                                        ToolTip.text: "This video is HDR (" + (win.selection.hdr === "hlg" ? "HLG" : "HDR10") + "). Its bright highlights are fitted into the normal picture: natural keeps contrast, bright keeps more brightness."
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: (win.selection.video === true) && (win.picPage("enhance"))
                                    Label {
                                        text: "Source colours"
                                        color: win.muted
                                        Layout.fillWidth: true
                                    }
                                    ComboBox {
                                        objectName: "colorRange"
                                        Accessible.name: "Colour range of the source"
                                        Layout.preferredWidth: 110
                                        enabled: win.selection.locked !== true
                                        readonly property var modes: ["", "tv", "pc"]
                                        model: ["Range: file", "Limited", "Full"]
                                        currentIndex: Math.max(0, modes.indexOf(win.selection.colorRange || ""))
                                        onActivated: index => editor.setClip("colorRange", modes[index])
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Fixes a video whose blacks look grey and washed out (choose Full) or crushed and too contrasty (choose Limited)"
                                    }
                                    ComboBox {
                                        objectName: "colorMatrix"
                                        Accessible.name: "Colour matrix of the source"
                                        Layout.preferredWidth: 110
                                        enabled: win.selection.locked !== true
                                        readonly property var modes: ["", "bt601", "bt709", "bt2020"]
                                        model: ["Colours: file", "BT.601", "BT.709", "BT.2020"]
                                        currentIndex: Math.max(0, modes.indexOf(win.selection.colorMatrix || ""))
                                        onActivated: index => editor.setClip("colorMatrix", modes[index])
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Fixes slightly wrong colours (skin too orange or green) when the file names the wrong colour standard: BT.601 for old SD video, BT.709 for HD"
                                    }
                                }
                            }
                            RowLayout {
                                visible: win.onTab("speed") || win.picPage("basic")
                                CheckBox {
                                    text: "Reverse"
                                    visible: win.onTab("speed")
                                    checked: win.selection.reverse || false
                                    onToggled: editor.setClip("reverse", checked)
                                }
                                CheckBox {
                                    text: "Flip"
                                    visible: win.picPage("basic")
                                    checked: win.selection.flip || false
                                    onToggled: editor.setClip("flip", checked)
                                }
                                CheckBox {
                                    objectName: "flipVertical"
                                    text: "Upside down"
                                    visible: win.picPage("basic")
                                    checked: win.selection.flipVertical || false
                                    onToggled: editor.setClip("flipVertical", checked)
                                }
                            }
                            RowLayout {
                                visible: win.audioPage("basic") || win.picPage("basic")
                                CheckBox {
                                    text: "Mute"
                                    visible: win.audioPage("basic")
                                    checked: win.selection.muted || false
                                    onToggled: editor.setClip("muted", checked)
                                }
                                CheckBox {
                                    text: "Hide"
                                    visible: win.picPage("basic")
                                    checked: win.selection.hidden || false
                                    onToggled: editor.setClip("hidden", checked)
                                }
                            }
                            RowLayout {
                                visible: win.onTab("more")
                                Layout.fillWidth: true
                                Action {
                                    objectName: "copyClip"
                                    text: "Copy"
                                    padding: 6
                                    onClicked: editor.copy()
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Copy this clip (" + win.shortcut("copy") + "); paste it at the playhead with " + win.shortcut("paste")
                                }
                                Action {
                                    objectName: "pasteLook"
                                    text: "Paste look"
                                    padding: 6
                                    enabled: !!win.s.clipboard && win.selection.locked !== true
                                    onClicked: editor.pasteAttributes("look")
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Colour, effects and LUT from " + (win.s.clipboard || "the copied clip")
                                }
                                Action {
                                    objectName: "pasteAttributes"
                                    text: "Paste all"
                                    padding: 6
                                    enabled: !!win.s.clipboard && win.selection.locked !== true
                                    onClicked: editor.pasteAttributes("all")
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Look, position and size, keyframes, shape, keying, volume and fades from " + (win.s.clipboard || "the copied clip") + " (" + win.shortcut("pasteAttributes") + ")"
                                }
                            }
                            Action {
                                text: "Detach audio to new track"
                                visible: (win.selection.canDetach || false) && (win.onTab("more"))
                                Layout.fillWidth: true
                                onClicked: editor.detachAudio()
                            }
                            Action {
                                objectName: "unlinkClip"
                                text: "Unlink picture and sound"
                                visible: ((win.selection.linkedCount || 0) > 0) && (win.onTab("more"))
                                enabled: win.selection.locked !== true
                                Layout.fillWidth: true
                                onClicked: editor.unlinkClip()
                                ToolTip.visible: hovered
                                ToolTip.text: "Linked clips move and trim together. Unlink to edit them separately."
                            }
                            Action {
                                text: "Relink source media…"
                                visible: ((win.selection.assetId || "").length > 0) && (win.onTab("more"))
                                Layout.fillWidth: true
                                onClicked: relinkDialog.open()
                            }
                        }
                    }
                }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#34404b"
        }
        // Inside a nested sequence: where we are, and the way back.
        Rectangle {
            objectName: "nestingBar"
            Layout.fillWidth: true
            visible: (win.s.nesting || []).length > 0
            implicitHeight: 36
            color: "#28403a"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10
                Action {
                    objectName: "closeNested"
                    text: "← Back"
                    onClicked: editor.closeNested()
                }
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                    text: "Main timeline  ›  " + (win.s.nesting || []).join("  ›  ") + "  —  changes apply when you go back"
                }
            }
        }
        Timeline {
            id: timelinePanel
            objectName: "timelinePanel"
            Layout.fillWidth: true
            Layout.preferredHeight: 360
            dropsEnabled: !win.shortcutsBlocked
            state: win.s
            playbackFrame: editor.playing ? editor.playbackFrame : -1
            onSeekRequested: function (frame) {
                win.goTo(frame);
            }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 32
            color: "#202831"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 12
                Label {
                    text: win.s.status
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                    font.pixelSize: 10
                    color: win.muted
                }
                ProgressBar {
                    visible: win.s.busy
                    value: win.s.progress
                    Layout.preferredWidth: 160
                }
                Button {
                    text: "Cancel"
                    visible: win.s.busy
                    onClicked: editor.cancelJob()
                    implicitHeight: 25
                }
                Label {
                    text: "0.5.0 ALPHA"
                    font.pixelSize: 9
                    font.letterSpacing: 1
                    color: win.mint
                }
            }
        }
    }
    // The start screen: a new project in a chosen format, a recent project, or the autosave.
    Rectangle {
        id: startPage
        objectName: "startScreen"
        anchors.fill: parent
        z: 50
        visible: win.startScreen && win.s.duration === 0 && editor.assets.length === 0 && !win.s.path
        color: "#0f1419"
        readonly property var prefs: win.s.preferences || ({})
        function begin(format) {
            editor.configure(format.w, format.h, prefs.fpsN || 30, prefs.fpsD || 1);
            win.startScreen = false;
        }
        MouseArea {
            anchors.fill: parent // keeps clicks off the editor below
        }
        // Left: the app and the places to go; right: new projects, recent ones and templates.
        Rectangle {
            id: startSide
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 210
            color: "#12171d"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 8
                Label {
                    text: "CUTLERY"
                    font.pixelSize: 18
                    font.bold: true
                    font.letterSpacing: 3
                    color: win.mint
                }
                Label {
                    text: "Local video editor"
                    color: win.muted
                    font.pixelSize: 11
                }
                Item {
                    implicitHeight: 14
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 32
                    radius: 6
                    color: "#222b34"
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        text: "⌂  Home"
                        color: win.mint
                    }
                }
                Action {
                    Layout.fillWidth: true
                    text: "Open project…"
                    onClicked: win.guarded("open")
                }
                Action {
                    objectName: "startRecover"
                    Layout.fillWidth: true
                    visible: win.s.hasRecovery
                    text: "Recover autosave"
                    onClicked: win.guarded("recover")
                }
                Action {
                    Layout.fillWidth: true
                    text: "Preferences…"
                    onClicked: preferencesDialog.open()
                }
                Item {
                    Layout.fillHeight: true
                }
                CheckBox {
                    objectName: "startScreenAgain"
                    text: "Show at start"
                    checked: startPage.prefs.startScreen !== false
                    onToggled: editor.setPreferences({ startScreen: checked })
                }
                Action {
                    objectName: "startEmpty"
                    Layout.fillWidth: true
                    text: "Skip to the editor"
                    onClicked: win.startScreen = false
                }
            }
        }
        ScrollView {
            anchors.left: startSide.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                x: 36
                y: 28
                width: Math.min(parent.width - 72, 900)
                spacing: 16
                // New project in the shape last used (or the first), one click.
                Rectangle {
                    objectName: "startCreate"
                    Layout.fillWidth: true
                    implicitHeight: 96
                    radius: 10
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop {
                            position: 0
                            color: "#1f7a68"
                        }
                        GradientStop {
                            position: 1
                            color: "#3b3f80"
                        }
                    }
                    readonly property var format: win.projectFormats.find(f => f.w === startPage.prefs.width && f.h === startPage.prefs.height) || win.projectFormats[0]
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 22
                        spacing: 16
                        Label {
                            text: "＋"
                            font.pixelSize: 34
                            color: "white"
                        }
                        ColumnLayout {
                            spacing: 2
                            Label {
                                text: "New project"
                                font.pixelSize: 22
                                font.bold: true
                                color: "white"
                            }
                            Label {
                                text: parent.parent.parent.format.label + " · " + parent.parent.parent.format.w + " × " + parent.parent.parent.format.h + " — or choose another shape below"
                                color: "#d8f3ec"
                            }
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: startPage.begin(parent.format)
                    }
                }
                Label {
                    text: "Shape of the video (you can change it later under Project → Reframe for…)"
                    color: win.muted
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    columnSpacing: 12
                    rowSpacing: 12
                    Repeater {
                        model: win.projectFormats
                        delegate: Rectangle {
                            id: formatTile
                            required property var modelData
                            required property int index
                            objectName: "startFormat-" + index
                            signal clicked
                            onClicked: startPage.begin(modelData)
                            Layout.fillWidth: true
                            Layout.preferredHeight: 92
                            radius: 8
                            color: tileMouse.containsMouse ? "#2a3640" : "#1d252e"
                            border.color: modelData.w === startPage.prefs.width && modelData.h === startPage.prefs.height ? win.mint : "#34404c"
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 12
                                // The canvas shape, to scale.
                                Item {
                                    Layout.preferredWidth: 48
                                    Layout.preferredHeight: 48
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 44 * Math.min(1, formatTile.modelData.w / formatTile.modelData.h)
                                        height: 44 * Math.min(1, formatTile.modelData.h / formatTile.modelData.w)
                                        radius: 3
                                        color: "transparent"
                                        border.color: win.mint
                                        border.width: 2
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3
                                    Label {
                                        text: formatTile.modelData.label
                                        font.bold: true
                                    }
                                    Label {
                                        text: formatTile.modelData.w + " × " + formatTile.modelData.h + " · " + formatTile.modelData.hint
                                        color: win.muted
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                            MouseArea {
                                id: tileMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: formatTile.clicked()
                            }
                        }
                    }
                }
                // After a session that did not end normally: what to do now, and where the log is.
                Rectangle {
                    objectName: "uncleanExitNotice"
                    visible: win.s.uncleanExit === true
                    Layout.fillWidth: true
                    implicitHeight: noticeRow.implicitHeight + 16
                    radius: 6
                    color: "#3a2f1c"
                    border.color: "#e5c07b"
                    RowLayout {
                        id: noticeRow
                        anchors.fill: parent
                        anchors.margins: 8
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            text: "Cutlery did not close normally last time." + (win.s.hasRecovery ? " Your unsaved work can be recovered with “Recover autosave”." : "") + " The log may tell why: " + (win.s.logPath || "")
                        }
                        Action {
                            text: "Open log folder"
                            onClicked: Qt.openUrlExternally("file:///" + String(win.s.logPath || "").replace(/\\/g, "/").replace(/^\/+/, "").replace(/\/[^\/]*$/, ""))
                        }
                        ToolButton {
                            objectName: "dismissUncleanExit"
                            text: "✕"
                            onClicked: editor.dismissUncleanExit()
                        }
                    }
                }
                Label {
                    visible: (win.s.recent || []).length > 0
                    text: "Projects"
                    font.bold: true
                    font.pixelSize: 15
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 10
                    Repeater {
                        model: (win.s.recent || []).slice(0, 12)
                        delegate: AbstractButton {
                            required property var modelData
                            required property int index
                            objectName: "startRecent-" + index
                            Accessible.name: "Open " + modelData.name + (modelData.exists ? "" : " (missing)")
                            width: 168
                            height: 112
                            padding: 8
                            enabled: modelData.exists
                            hoverEnabled: true
                            onClicked: win.guarded("recent:" + modelData.path)
                            background: Rectangle {
                                radius: 8
                                color: parent.hovered ? "#2a3640" : "#1d252e"
                                border.color: parent.hovered ? win.mint : "#34404c"
                            }
                            contentItem: ColumnLayout {
                                spacing: 4
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 48
                                    radius: 5
                                    color: "#0f141a"
                                    Label {
                                        anchors.centerIn: parent
                                        text: "▶"
                                        color: "#4c5966"
                                        font.pixelSize: 18
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: parent.parent.modelData.name + (parent.parent.modelData.exists ? "" : " (missing)")
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: parent.parent.modelData.path
                                    color: win.muted
                                    font.pixelSize: 10
                                    elide: Text.ElideMiddle
                                }
                            }
                            ToolTip.visible: hovered
                            ToolTip.text: modelData.path
                        }
                    }
                }
                Label {
                    visible: (win.s.templates || []).length > 0
                    text: "From a template"
                    color: win.muted
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: (win.s.templates || []).length > 0
                    Repeater {
                        model: win.s.templates || []
                        delegate: Action {
                            required property var modelData
                            required property int index
                            objectName: "startTemplate-" + index
                            text: modelData.name
                            onClicked: win.guarded("template:" + modelData.name)
                        }
                    }
                }
            }
        }
    }
    FileDialog {
        id: importDialog
        title: "Import local media"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Media files (*.mp4 *.mov *.mkv *.webm *.avi *.m4v *.mts *.m2ts *.ts *.mpg *.mpeg *.vob *.mxf *.dv *.wmv *.flv *.3gp *.mp3 *.wav *.m4a *.aac *.flac *.ogg *.opus *.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff *.gif *.svg *.avif *.heic *.heif)", "All files (*)"]
        onAccepted: editor.importMedia(selectedFiles)
    }
    FileDialog {
        id: openDialog
        title: "Open project"
        nameFilters: ["Cutlery project (*.cutlery)"]
        onAccepted: editor.openProject(selectedFile)
    }
    FileDialog {
        id: saveDialog
        title: "Save project"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "cutlery"
        nameFilters: ["Cutlery project (*.cutlery)"]
        onAccepted: editor.save(selectedFile)
    }
    FileDialog {
        id: timelineFileDialog
        title: "Export the timeline for another editor"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "otio"
        nameFilters: ["OpenTimelineIO (*.otio)", "CMX 3600 EDL (*.edl)"]
        onAccepted: editor.exportTimeline(selectedFile)
    }
    FileDialog {
        id: audioFileDialog
        title: "Save the clip's sound"
        fileMode: FileDialog.SaveFile
        defaultSuffix: ["wav", "mp3", "m4a"][Math.max(0, selectedNameFilter.index)]
        nameFilters: ["WAV sound (*.wav)", "MP3 sound (*.mp3)", "AAC sound (*.m4a)"]
        onAccepted: editor.extractAudio(selectedFile)
    }
    FileDialog {
        id: frameDialog
        title: "Save the picture at the playhead"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "png"
        nameFilters: ["PNG picture (*.png)", "JPEG picture (*.jpg *.jpeg)"]
        onAccepted: editor.exportFrame(selectedFile)
    }
    FileDialog {
        id: exportDialog
        title: "Export — choose a new filename"
        fileMode: FileDialog.SaveFile
        readonly property string extension: editor.exportPreview(win.exportChoice).extension || "mp4"
        defaultSuffix: extension
        nameFilters: [extension.toUpperCase() + " (*." + extension + ")"]
        onRejected: win.convertAsset = ""
        onAccepted: {
            if (win.convertAsset.length > 0) {
                editor.convertAsset(win.convertAsset, selectedFile, win.exportChoice);
                win.convertAsset = "";
            } else if (win.queueExport)
                editor.queueExport(selectedFile, win.exportChoice);
            else
                editor.exportWith(selectedFile, win.exportChoice);
        }
    }
    FileDialog {
        id: relinkDialog
        title: "Choose replacement media"
        onAccepted: editor.relink(win.relinkAsset || win.selection.assetId, selectedFile)
        onVisibleChanged: if (!visible)
            win.relinkAsset = ""
    }
    // Collect: copies the project and everything it uses into a new folder, e.g. to archive it or
    // move it to another computer.
    FolderDialog {
        id: relinkFolderDialog
        title: "Find missing media in this folder"
        onAccepted: editor.relinkFolder(selectedFolder)
    }
    FolderDialog {
        id: collectDialog
        title: "Collect project into an empty folder"
        onAccepted: editor.collectProject(selectedFolder)
    }
    FileDialog {
        id: sequenceFile
        title: "Choose any image of the numbered sequence"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.tif *.tiff *.bmp *.webp *.exr *.dpx)"]
        onAccepted: {
            sequenceDialog.file = selectedFile;
            sequenceDialog.info = editor.imageSequenceAt(selectedFile);
            sequenceDialog.open();
        }
    }
    Dialog {
        id: sequenceDialog
        objectName: "sequenceDialog"
        property url file
        property var info: ({})
        anchors.centerIn: parent
        title: "Import image sequence"
        modal: true
        width: 380
        standardButtons: info.count ? Dialog.Ok | Dialog.Cancel : Dialog.Close
        ColumnLayout {
            width: parent.width
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: sequenceDialog.info.count ? sequenceDialog.info.count + " images, numbered from " + sequenceDialog.info.start + ". They become one video clip (transparency is kept)." : "This image is not part of a numbered sequence (e.g. shot_0001.png, shot_0002.png …)."
            }
            RowLayout {
                visible: !!sequenceDialog.info.count
                Label { text: "Frames per second" }
                SpinBox {
                    id: sequenceRate
                    objectName: "sequenceRate"
                    from: 1
                    to: 120
                    editable: true
                    value: Math.round(win.s.fps || 30)
                }
            }
        }
        onAccepted: if (info.count)
            editor.importImageSequence(file, sequenceRate.value)
    }
    FileDialog {
        id: fontFileDialog
        title: "Add a font"
        nameFilters: ["Fonts (*.ttf *.otf *.ttc)"]
        onAccepted: {
            const family = editor.addFont(selectedFile);
            if (family.length > 0) {
                fontBox.families = editor.fontFamilies();
                editor.setClip("fontFamily", family);
            }
        }
    }
    FileDialog {
        id: creditsDialog
        title: "Save credits"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "txt"
        nameFilters: ["Text (*.txt)"]
        onAccepted: editor.exportCredits(selectedFile)
    }
    FileDialog {
        id: lutLibraryDialog
        title: "Add LUTs to the library"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["3D LUTs (*.cube *.3dl)", "All files (*)"]
        onAccepted: {
            for (const f of selectedFiles)
                editor.addLutToLibrary(f);
        }
    }
    FileDialog {
        id: logoDialog
        title: "Choose the brand logo"
        nameFilters: ["Pictures (*.png *.jpg *.jpeg *.webp *.bmp)", "All files (*)"]
        onAccepted: editor.setBrandLogo(selectedFile)
    }
    FileDialog {
        id: lutDialog
        title: "Choose a LUT"
        nameFilters: ["3D LUTs (*.cube *.3dl)", "All files (*)"]
        onAccepted: editor.setClip("lut", selectedFile)
    }
    FileDialog {
        id: srtOpen
        title: "Import captions"
        nameFilters: ["Captions (*.srt *.vtt *.ass *.ssa *.txt)", "SubRip (*.srt)", "WebVTT (*.vtt)", "SubStation Alpha (*.ass *.ssa)", "Plain text, one caption per line (*.txt)"]
        onAccepted: editor.importSrt(selectedFile)
    }
    FileDialog {
        id: srtSave
        title: "Export captions"
        fileMode: FileDialog.SaveFile
        defaultSuffix: ["srt", "vtt", "ass"][Math.max(0, selectedNameFilter.index)]
        nameFilters: ["SubRip (*.srt)", "WebVTT for the web (*.vtt)", "Styled ASS subtitles (*.ass)"]
        onAccepted: editor.exportSrt(selectedFile)
    }
    // Remove pauses: silence detection on the selected clip's sound, then one ripple edit.
    // Earlier saved versions of the open project, newest first.
    Dialog {
        id: backupDialog
        objectName: "backupDialog"
        anchors.centerIn: parent
        title: "Restore an earlier version"
        modal: true
        width: 420
        property var list: []
        onOpened: {
            list = editor.backups();
            backupList.currentIndex = list.length > 0 ? 0 : -1;
        }
        footer: DialogButtonBox {
            Button {
                objectName: "restoreBackup"
                text: "Restore"
                enabled: backupList.currentIndex >= 0
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: {
                    const file = backupDialog.list[backupList.currentIndex].file;
                    backupDialog.close();
                    win.guarded("restore:" + file);
                }
            }
            Button {
                text: "Close"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: backupDialog.close()
            }
        }
        ColumnLayout {
            anchors.fill: parent
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: win.muted
                text: backupDialog.list.length > 0 ? "Each save keeps the version before it (the last 20). Restoring keeps the current file as a version too." : "No earlier versions yet: they appear after the project is saved again."
            }
            ListView {
                id: backupList
                objectName: "backupList"
                Layout.fillWidth: true
                implicitHeight: 240
                clip: true
                model: backupDialog.list
                delegate: ItemDelegate {
                    required property var modelData
                    required property int index
                    width: backupList.width
                    highlighted: ListView.isCurrentItem
                    text: modelData.time + "   ·   " + Math.max(1, Math.round(modelData.bytes / 1024)) + " KB"
                    onClicked: backupList.currentIndex = index
                }
            }
        }
    }
    // App-wide settings: the format of new projects, picture length, earlier versions kept and
    // the start screen.
    Dialog {
        id: preferencesDialog
        objectName: "preferencesDialog"
        anchors.centerIn: parent
        title: "Preferences"
        modal: true
        width: 400
        standardButtons: Dialog.Ok | Dialog.Cancel
        readonly property var prefs: win.s.preferences || ({})
        onAboutToShow: {
            prefFormat.currentIndex = Math.max(0, win.projectFormats.findIndex(f => f.w === prefs.width && f.h === prefs.height));
            prefRate.currentIndex = Math.max(0, win.frameRates.findIndex(r => r.n === prefs.fpsN && r.d === prefs.fpsD));
            prefStill.value = Math.round((prefs.stillSeconds || 5) * 10);
            prefBackups.value = prefs.backups ?? 20;
            prefStart.checked = prefs.startScreen !== false;
            prefProxies.checked = prefs.proxiesOnImport === true;
            prefCache.value = prefs.cacheGB ?? 20;
            prefUndo.value = prefs.undoSteps ?? 60;
            cacheUsage = editor.cacheUsage();
        }
        property var cacheUsage: ({})
        GridLayout {
            anchors.fill: parent
            columns: 2
            columnSpacing: 12
            rowSpacing: 10
            Label { text: "New projects" }
            ComboBox {
                id: prefFormat
                objectName: "prefFormat"
                Layout.fillWidth: true
                model: win.projectFormats.map(f => f.label + " · " + f.w + " × " + f.h)
            }
            Label { text: "Frame rate" }
            ComboBox {
                id: prefRate
                objectName: "prefRate"
                Layout.fillWidth: true
                model: win.frameRates.map(r => r.label + " fps")
            }
            Label { text: "Pictures last" }
            SpinBox {
                id: prefStill
                objectName: "prefStill"
                from: 5
                to: 600
                editable: true
                textFromValue: (v) => (v / 10).toFixed(1) + " s"
                valueFromText: (t) => Math.round(parseFloat(t) * 10)
                ToolTip.visible: hovered
                ToolTip.text: "Length of a still image when it is added to the timeline"
            }
            Label { text: "Earlier versions" }
            SpinBox {
                id: prefBackups
                objectName: "prefBackups"
                from: 0
                to: 100
                ToolTip.visible: hovered
                ToolTip.text: "Copies kept per project when you save (Project → Restore an earlier version); 0 keeps none"
            }
            CheckBox {
                id: prefStart
                objectName: "prefStart"
                Layout.columnSpan: 2
                text: "Show the start screen when Cutlery opens"
            }
            CheckBox {
                id: prefProxies
                objectName: "prefProxies"
                Layout.columnSpan: 2
                text: "Make editing proxies for imported videos over 1080p"
                ToolTip.visible: hovered
                ToolTip.text: "Small copies the preview plays instead, made in the background; exports use the originals"
            }
            Label { text: "Undo steps" }
            SpinBox {
                id: prefUndo
                objectName: "prefUndo"
                from: 10
                to: 500
                stepSize: 10
                editable: true
                ToolTip.visible: hovered
                ToolTip.text: "How many edits Undo can go back; more steps use more memory with long projects"
            }
            Label { text: "Cache limit" }
            SpinBox {
                id: prefCache
                objectName: "prefCache"
                from: 1
                to: 2000
                editable: true
                textFromValue: (v) => v + " GB"
                valueFromText: (t) => parseInt(t)
                ToolTip.visible: hovered
                ToolTip.text: "Waveforms, thumbnails and nested sequences are kept up to this size; the oldest go first when Cutlery starts"
            }
            Label {
                objectName: "cacheUsage"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: win.muted
                font.pixelSize: 11
                text: "Cache: " + ((preferencesDialog.cacheUsage.bytes || 0) / 1048576).toFixed(0) + " MB in " + (preferencesDialog.cacheUsage.files || 0) + " files" + (preferencesDialog.cacheUsage.clearAtStart ? " · emptied at the next start" : "")
            }
            Action {
                objectName: "clearCache"
                text: "Empty at next start"
                enabled: !preferencesDialog.cacheUsage.clearAtStart
                onClicked: {
                    editor.clearCacheAtStart();
                    preferencesDialog.cacheUsage = editor.cacheUsage();
                }
                ToolTip.visible: hovered
                ToolTip.text: "Frees the disk space; Cutlery makes these files again when needed"
            }
        }
        onAccepted: {
            const f = win.projectFormats[prefFormat.currentIndex], r = win.frameRates[prefRate.currentIndex];
            editor.setPreferences({ width: f.w, height: f.h, fpsN: r.n, fpsD: r.d, stillSeconds: prefStill.value / 10, backups: prefBackups.value, startScreen: prefStart.checked, proxiesOnImport: prefProxies.checked, cacheGB: prefCache.value, undoSteps: prefUndo.value });
        }
    }
    // Names a template made from the current project.
    Dialog {
        id: templateDialog
        objectName: "templateDialog"
        anchors.centerIn: parent
        modal: true
        title: "Save as template"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAboutToShow: {
            templateName.text = "";
            templateName.forceActiveFocus();
        }
        ColumnLayout {
            width: 320
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: win.muted
                text: "New projects can start from it (Project → New from template). Replace its media with right-click → Replace with another file."
            }
            TextField {
                id: templateName
                objectName: "templateName"
                Layout.fillWidth: true
                maximumLength: 60
                placeholderText: "Template name, e.g. Tutorial intro"
                onAccepted: templateDialog.accept()
            }
        }
        onAccepted: editor.saveTemplate(templateName.text)
    }
    // Names a text style to keep.
    Dialog {
        id: styleDialog
        objectName: "styleDialog"
        anchors.centerIn: parent
        modal: true
        title: "Save text style"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAboutToShow: {
            styleName.text = "";
            styleName.forceActiveFocus();
        }
        TextField {
            id: styleName
            objectName: "styleName"
            width: 280
            maximumLength: 60
            placeholderText: "Style name, e.g. Channel title"
            onAccepted: styleDialog.accept()
        }
        onAccepted: editor.saveTextStyle(styleName.text)
    }
    Dialog {
        id: layoutDialog
        objectName: "layoutDialog"
        anchors.centerIn: parent
        modal: true
        title: "Save layout"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAboutToShow: {
            layoutName.text = "";
            layoutName.forceActiveFocus();
        }
        TextField {
            id: layoutName
            objectName: "layoutName"
            width: 280
            maximumLength: 60
            placeholderText: "Layout name, e.g. Interview split"
            onAccepted: layoutDialog.accept()
        }
        onAccepted: editor.saveLayout(layoutName.text)
    }
    // Names a new folder (optionally moving one medium into it) or renames one.
    Dialog {
        id: folderDialog
        objectName: "folderDialog"
        property string renaming: ""
        property string assetId: ""
        function ask(from, name, asset) {
            renaming = from;
            assetId = asset || "";
            folderName.text = name;
            open();
            folderName.forceActiveFocus();
        }
        anchors.centerIn: parent
        modal: true
        title: renaming ? "Rename folder" : "New folder"
        standardButtons: Dialog.Ok | Dialog.Cancel
        TextField {
            id: folderName
            objectName: "folderName"
            width: 280
            placeholderText: "Folder name"
            maximumLength: 80
            onAccepted: folderDialog.accept()
        }
        onAccepted: {
            const name = folderName.text.trim();
            if (renaming) {
                editor.renameFolder(renaming, name);
                return;
            }
            editor.addFolder(name);
            if (assetId && (win.s.folders || []).indexOf(name) >= 0)
                editor.moveToFolder([assetId], name);
        }
    }
    // Usage rights of a medium and the credit line it needs (kept with the project).
    Dialog {
        id: rightsDialog
        objectName: "rightsDialog"
        property string assetId: ""
        readonly property var kinds: [
            { id: "", label: "Not recorded" },
            { id: "own", label: "My own" },
            { id: "free", label: "Free for any use (public domain, CC0)" },
            { id: "attribution", label: "Free with a credit (e.g. CC BY)" },
            { id: "licensed", label: "Bought or licensed" },
            { id: "personal", label: "Personal use only (not commercial)" },
            { id: "unknown", label: "Unknown" }
        ]
        function ask(asset) {
            assetId = asset.id;
            title = "Usage rights: " + asset.name;
            rightsKind.currentIndex = Math.max(0, kinds.findIndex(k => k.id === (asset.rights || "")));
            rightsCredit.text = asset.credit || "";
            open();
        }
        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            width: 340
            ComboBox {
                id: rightsKind
                objectName: "rightsKind"
                Layout.fillWidth: true
                model: rightsDialog.kinds.map(k => k.label)
            }
            TextField {
                id: rightsCredit
                objectName: "rightsCredit"
                Layout.fillWidth: true
                maximumLength: 500
                placeholderText: "Credit line or source, e.g. Music: Jane Doe (CC BY 4.0)"
                onAccepted: rightsDialog.accept()
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 11
                color: win.muted
                text: "Kept with the project. The export points out media for personal use only or of unknown rights, and can save the credit lines as a text file."
            }
        }
        onAccepted: editor.setAssetRights([assetId], kinds[rightsKind.currentIndex].id, rightsCredit.text)
    }
    // The sound effects library: listen, add at the playhead, or a swoosh on every transition.
    Dialog {
        id: soundDialog
        objectName: "soundDialog"
        anchors.centerIn: parent
        title: "Sound effects"
        modal: true
        width: 460
        property string playing: ""
        onOpened: {
            soundModel.clear();
            for (const sound of editor.sounds())
                soundModel.append({ soundId: sound.id, name: sound.name, category: sound.category, seconds: sound.seconds, licence: sound.licence, source: sound.source, builtIn: sound.builtIn });
        }
        ListModel {
            id: soundModel
        }
        onClosed: {
            if (player)
                player.stop();
            playing = "";
        }
        // Created on the first listen, so the audio system starts only when needed.
        property var player: null
        Component {
            id: soundPlayerComponent
            MediaPlayer {
                audioOutput: AudioOutput {}
                onPlaybackStateChanged: if (playbackState === MediaPlayer.StoppedState)
                    soundDialog.playing = ""
            }
        }
        footer: DialogButtonBox {
            Button {
                text: "Close"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: soundDialog.close()
            }
        }
        ListView {
            id: soundList
            objectName: "soundList"
            implicitHeight: 360
            width: parent.width
            clip: true
            model: soundModel
            section.property: "category"
            section.delegate: Caption {
                required property string section
                text: section.toUpperCase()
                topPadding: 8
            }
            delegate: RowLayout {
                id: soundRow
                required property string soundId
                required property string category
                required property string name
                required property double seconds
                required property string licence
                required property string source
                required property bool builtIn
                width: soundList.width
                spacing: 6
                Action {
                    objectName: "listen-" + soundRow.soundId
                    text: soundDialog.playing === soundRow.soundId ? "■" : "▶"
                    padding: 6
                    onClicked: {
                        if (soundDialog.player)
                            soundDialog.player.stop();
                        if (soundDialog.playing === soundRow.soundId) {
                            soundDialog.playing = "";
                            return;
                        }
                        const url = editor.soundFile(soundRow.soundId);
                        if (url.toString().length > 0) {
                            if (!soundDialog.player)
                                soundDialog.player = soundPlayerComponent.createObject(soundDialog);
                            soundDialog.player.source = url;
                            soundDialog.playing = soundRow.soundId;
                            soundDialog.player.play();
                        }
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: "Listen"
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: soundRow.name + "  ·  " + Number(soundRow.seconds).toFixed(1) + " s"
                    }
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: soundRow.licence + (soundRow.builtIn ? "" : " · " + soundRow.source)
                        color: win.muted
                        font.pixelSize: 10
                        ToolTip.visible: licenceHover.hovered && truncated
                        ToolTip.text: text
                        HoverHandler { id: licenceHover }
                    }
                }
                // Whooshes go onto every transition at once, e.g. where the full-screen video
                // changes to the presenter layout.
                Action {
                    objectName: "transitionSound-" + soundRow.soundId
                    visible: soundRow.category === "Transitions"
                    text: "⇄ All"
                    padding: 8
                    onClicked: {
                        editor.addSoundAtTransitions(soundRow.soundId);
                        if (!win.s.error)
                            soundDialog.close();
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: "Adds this sound at every transition between two clips, loudest at the cut"
                }
                Action {
                    objectName: "addSound-" + soundRow.soundId
                    text: "Add"
                    padding: 8
                    onClicked: {
                        editor.addSound(soundRow.soundId);
                        soundDialog.close();
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: "Adds the sound at the playhead"
                }
            }
        }
    }
    Dialog {
        id: pauseDialog
        objectName: "pauseDialog"
        anchors.centerIn: parent
        title: "Remove pauses"
        modal: true
        width: 420
        readonly property var state: win.s.pauses || ({})
        footer: DialogButtonBox {
            Button {
                objectName: "pauseFind"
                text: "Find pauses"
                enabled: pauseDialog.state.status !== "finding"
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: editor.findPauses(pauseThreshold.value, pauseLength.value)
            }
            Button {
                objectName: "pauseRemove"
                text: "Remove " + (pauseDialog.state.count || 0)
                enabled: pauseDialog.state.status === "ready" && (pauseDialog.state.count || 0) > 0
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: {
                    editor.removePauses();
                    pauseDialog.close();
                }
            }
            Button {
                text: "Close"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: pauseDialog.close()
            }
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 6
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: "Cuts out the quiet moments between sentences and closes the gaps on this clip's track. Detached audio of the clip is cut the same way. Undo restores everything."
            }
            Label {
                text: "Quieter than  " + pauseThreshold.value.toFixed(0) + " dB"
                color: win.muted
            }
            Slider {
                id: pauseThreshold
                objectName: "pauseThreshold"
                Layout.fillWidth: true
                from: -60
                to: -20
                stepSize: 1
                value: -40
            }
            Label {
                text: "For at least  " + pauseLength.value.toFixed(1) + " s"
                color: win.muted
            }
            Slider {
                id: pauseLength
                objectName: "pauseLength"
                Layout.fillWidth: true
                from: .3
                to: 3
                stepSize: .1
                value: .7
            }
            Label {
                objectName: "pauseStatus"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: pauseDialog.state.status === "failed" ? "#ec6f5a" : win.muted
                text: pauseDialog.state.status === "finding" ? "Listening…"
                    : pauseDialog.state.status === "failed" ? "Could not read the clip's sound."
                    : pauseDialog.state.status === "stale" ? "The clip changed. Find pauses again."
                    : pauseDialog.state.status === "ready" ? (pauseDialog.state.count > 0 ? pauseDialog.state.count + (pauseDialog.state.count === 1 ? " pause, " : " pauses, ") + Number(pauseDialog.state.seconds).toFixed(1) + " s in total. A short gap is kept around speech." : "No pauses found. Try a higher threshold or shorter pauses.")
                    : "Raise the threshold if background noise hides the pauses."
            }
        }
    }
    // Caption translation: German to English or English to German, on this computer.
    Dialog {
        id: translateDialog
        objectName: "translateDialog"
        anchors.centerIn: parent
        title: "Translate captions"
        modal: true
        width: 420
        readonly property var state: win.s.translation || ({})
        readonly property string target: translateTarget.currentIndex === 0 ? "en" : "de"
        readonly property string missing: (state.missing || {})[target] || ""
        readonly property bool busy: state.status === "translating"
        footer: DialogButtonBox {
            Button {
                objectName: "translateStart"
                text: "Translate"
                enabled: translateDialog.missing === "" && !translateDialog.busy
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: editor.translateCaptions(translateDialog.target)
            }
            Button {
                text: translateDialog.busy ? "Stop" : "Close"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: translateDialog.busy ? editor.cancelTranslation() : translateDialog.close()
            }
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: "Translates the selected titles, or else every caption on the “AI captions” track, and replaces their text (one undo step). Everything stays on this computer."
            }
            ComboBox {
                id: translateTarget
                objectName: "translateTarget"
                Accessible.name: "Translate into"
                Layout.fillWidth: true
                model: ["German → English", "English → German"]
            }
            ProgressBar {
                Layout.fillWidth: true
                visible: translateDialog.busy
                value: translateDialog.state.progress || 0
            }
            Label {
                objectName: "translateStatus"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 11
                color: translateDialog.state.status === "failed" || translateDialog.missing !== "" ? "#ec6f5a" : win.muted
                text: translateDialog.missing !== "" ? translateDialog.missing + " Download the AI pack next to Cutlery.exe."
                    : translateDialog.busy ? "Translating… " + Math.round((translateDialog.state.progress || 0) * 100) + "%"
                    : translateDialog.state.status === "done" ? "Translated " + translateDialog.state.count + " captions ✓"
                    : translateDialog.state.status === "failed" ? "Translation failed" : ""
            }
        }
    }
    // Automatic captions: speech recognition on the audible clips, placed on their own track.
    Dialog {
        id: captionDialog
        objectName: "captionDialog"
        anchors.centerIn: parent
        title: "Generate captions"
        modal: true
        width: 420
        readonly property var state: win.s.captions || ({})
        readonly property string missing: (win.s.aiMissing || {}).transcribe || ""
        readonly property var languages: [
            { id: "auto", label: "Detect automatically" },
            { id: "de", label: "Deutsch" },
            { id: "en", label: "English" },
            { id: "fr", label: "Français" },
            { id: "es", label: "Español" },
            { id: "it", label: "Italiano" },
            { id: "nl", label: "Nederlands" },
            { id: "pl", label: "Polski" },
            { id: "pt", label: "Português" },
            { id: "tr", label: "Türkçe" }
        ]
        footer: DialogButtonBox {
            Button {
                objectName: "captionStart"
                text: "Generate"
                enabled: captionDialog.missing === "" && captionDialog.state.running !== true
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: editor.generateCaptions(captionDialog.languages[captionLanguage.currentIndex].id, captionStyleChoice.styles[captionStyleChoice.currentIndex], captionChars.value, captionLines.currentIndex + 1)
            }
            Button {
                text: captionDialog.state.running === true ? "Stop" : "Close"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: {
                    if (captionDialog.state.running === true)
                        editor.cancelAi()
                    else
                        captionDialog.close()
                }
            }
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: "Cutlery listens to every audible clip on the timeline and writes captions on the “AI captions” track. Running it again replaces that track. Everything stays on this computer."
            }
            Label {
                text: "Spoken language"
                color: win.muted
            }
            ComboBox {
                id: captionLanguage
                objectName: "captionLanguage"
                Layout.fillWidth: true
                model: captionDialog.languages.map(l => l.label)
            }
            Label {
                text: "Style"
                color: win.muted
            }
            ComboBox {
                id: captionStyleChoice
                objectName: "captionStyleChoice"
                Layout.fillWidth: true
                readonly property var styles: ["karaoke", "", "word"]
                model: ["Karaoke: highlight the spoken word", "Plain captions", "One word at a time (big)"]
            }
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "Characters per line"
                    color: win.muted
                }
                SpinBox {
                    id: captionChars
                    objectName: "captionChars"
                    from: 20
                    to: 80
                    value: 42
                    editable: true
                }
                ComboBox {
                    id: captionLines
                    objectName: "captionLines"
                    Layout.fillWidth: true
                    enabled: captionStyleChoice.currentIndex === 1
                    model: ["One line", "Two lines"]
                    ToolTip.visible: hovered
                    ToolTip.text: "Two lines hold more text per caption; for plain captions"
                }
            }
            ProgressBar {
                Layout.fillWidth: true
                visible: captionDialog.state.running === true
                value: captionDialog.state.progress || 0
            }
            Label {
                objectName: "captionStatus"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: captionDialog.missing !== "" ? "#ec6f5a" : win.muted
                text: captionDialog.missing !== "" ? captionDialog.missing
                    : captionDialog.state.running === true ? "Recognising speech… " + Math.round((captionDialog.state.progress || 0) * 100) + "%"
                    : win.s.status
            }
        }
    }
    // The edit history: every step of the shown timeline; a click goes back or forward to it.
    Dialog {
        id: historyDialog
        objectName: "historyDialog"
        anchors.centerIn: parent
        title: "History"
        modal: true
        width: 380
        standardButtons: Dialog.Close
        property var steps: []
        function refresh() {
            steps = editor.history();
        }
        onAboutToShow: refresh()
        Connections {
            target: editor
            enabled: historyDialog.visible
            function onProjectChanged() {
                historyDialog.refresh();
            }
        }
        ListView {
            id: historyList
            objectName: "historyList"
            implicitHeight: Math.min(420, contentHeight)
            width: parent.width
            clip: true
            model: historyDialog.steps
            ScrollBar.vertical: ScrollBar {}
            onCountChanged: positionViewAtIndex(historyDialog.steps.findIndex(x => x.offset === 0), ListView.Contain)
            delegate: ItemDelegate {
                id: step
                required property var modelData
                required property int index
                objectName: "historyStep-" + index
                width: ListView.view.width
                height: 30
                onClicked: editor.goToHistory(modelData.offset)
                background: Rectangle {
                    radius: 4
                    color: step.modelData.offset === 0 ? "#26313b" : step.hovered ? "#1c242c" : "transparent"
                }
                contentItem: RowLayout {
                    Label {
                        Layout.fillWidth: true
                        text: step.modelData.label
                        elide: Text.ElideRight
                        color: step.modelData.offset > 0 ? win.muted : "#e7edf2"
                        font.italic: step.modelData.offset > 0
                    }
                    Label {
                        text: step.modelData.offset === 0 ? "now" : step.modelData.offset > 0 ? "undone" : ""
                        color: step.modelData.offset === 0 ? win.mint : win.muted
                        font.pixelSize: 10
                    }
                }
            }
        }
    }
    Dialog {
        id: discardDialog
        anchors.centerIn: parent
        title: "Unsaved work / active render"
        modal: true
        width: 420
        standardButtons: Dialog.Discard | Dialog.Cancel
        Label {
            width: parent.width
            text: win.pendingAction.startsWith("closeProject:") ? "Close this project without saving its changes?" : win.pendingAction === "close" && win.s.anyDirty && !win.s.dirty ? "Another open project has unsaved changes. Quit without saving them? Cancel to switch to it and save." : "Discard unsaved changes and continue? An active render will be cancelled. Cancel to save your project first."
            wrapMode: Text.Wrap
        }
        onDiscarded: win.runAction(win.pendingAction)
    }
    Dialog {
        id: settings
        objectName: "projectSettings"
        anchors.centerIn: parent
        title: "Project settings"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 380
        readonly property var sizes: [[1920, 1080], [1080, 1920], [1080, 1080], [1280, 720], [3840, 2160], [1080, 1350]]
        readonly property var rates: [[24000, 1001], [24, 1], [25, 1], [30000, 1001], [30, 1], [50, 1], [60000, 1001], [60, 1]]
        // The current canvas and rate are chosen when the dialog opens; a size or rate not in the
        // lists is offered as it is.
        property var currentSize: [win.s.width, win.s.height]
        property var currentRate: [win.s.fpsN, win.s.fpsD]
        readonly property var sizeChoices: sizes.some(z => z[0] === currentSize[0] && z[1] === currentSize[1]) ? sizes : [currentSize].concat(sizes)
        readonly property var rateChoices: rates.some(r => r[0] * currentRate[1] === currentRate[0] * r[1]) ? rates : [currentRate].concat(rates)
        onAboutToShow: {
            currentSize = [win.s.width, win.s.height];
            currentRate = [win.s.fpsN, win.s.fpsD];
            canvas.currentIndex = Math.max(0, sizeChoices.findIndex(z => z[0] === currentSize[0] && z[1] === currentSize[1]));
            frameRate.currentIndex = Math.max(0, rateChoices.findIndex(r => r[0] * currentRate[1] === currentRate[0] * r[1]));
        }
        ColumnLayout {
            anchors.fill: parent
            Label {
                text: "Canvas"
            }
            ComboBox {
                id: canvas
                objectName: "projectCanvas"
                Layout.fillWidth: true
                model: settings.sizeChoices.map(z => z[0] + " × " + z[1] + " · " + (z[0] > z[1] ? "landscape" : z[0] < z[1] ? "portrait" : "square"))
            }
            Label {
                text: "Frame rate"
            }
            ComboBox {
                id: frameRate
                objectName: "projectFrameRate"
                Layout.fillWidth: true
                model: settings.rateChoices.map(r => r[1] === 1 ? String(r[0]) : (r[0] / r[1]).toFixed(3).replace(/0+$/, "") + " (" + r[0] + "/" + r[1] + ")")
            }
            Label {
                Layout.fillWidth: true
                visible: win.s.duration > 0
                wrapMode: Text.Wrap
                color: win.muted
                font.pixelSize: 11
                text: "Clips, keyframes, markers and captions keep their times at a new frame rate (to the nearest frame). Undo puts it back."
            }
        }
        onAccepted: {
            const size = sizeChoices[canvas.currentIndex], rate = rateChoices[frameRate.currentIndex];
            editor.configure(size[0], size[1], rate[0], rate[1]);
        }
    }
    Dialog {
        id: exportSettings
        objectName: "exportSettings"
        anchors.centerIn: parent
        title: win.convertAsset.length > 0 ? "Convert or compress media" : "Export video"
        onRejected: win.convertAsset = ""
        modal: true
        width: 480
        footer: DialogButtonBox {
            Button {
                objectName: "exportNow"
                text: "Export…"
                enabled: !win.s.busy
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: exportSettings.choose(false)
            }
            Button {
                objectName: "addToQueue"
                visible: win.convertAsset.length === 0
                text: "Add to queue…"
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                onClicked: exportSettings.choose(true)
            }
            Button {
                text: "Close"
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }
        function choose(queue) {
            win.exportChoice = Object.assign({}, current, { range: exportRange.visible && exportRange.currentIndex === 1 ? "inout" : "all" });
            win.queueExport = queue;
            exportDialog.open();
            if (!queue)
                close();
        }
        readonly property var formats: [
            { id: "h264", label: "H.264 · MP4 (plays everywhere)" },
            { id: "hevc", label: "HEVC / H.265 · MP4 (smaller files)" },
            { id: "av1", label: "AV1 · MP4 (smallest, modern devices)" },
            { id: "vp9", label: "VP9 · WebM (web)" },
            { id: "prores", label: "ProRes 422 HQ · MOV (editing master, large)" },
            { id: "prores4444", label: "ProRes 4444 with transparency · MOV (overlays for other editors)" },
            { id: "mpeg4", label: "MPEG-4 Part 2 · MP4 (legacy, always available)" },
            { id: "gif", label: "Animated GIF (no sound; best at 480p or less)" },
            { id: "png", label: "PNG picture sequence with transparency (a folder, no sound)" },
            { id: "mp3", label: "Audio only · MP3" },
            { id: "m4a", label: "Audio only · AAC (M4A)" },
            { id: "wav", label: "Audio only · WAV (uncompressed)" }
        ]
        readonly property var qualities: [
            { id: "max", label: "Maximum" },
            { id: "high", label: "High" },
            { id: "balanced", label: "Balanced" },
            { id: "small", label: "Small file" }
        ]
        readonly property var heights: [0, 720, 1080, 1440, 2160]
        readonly property var deepFormats: ["hevc", "av1", "vp9", "prores"]
        readonly property var dynamicRanges: [
            { id: "", label: "8-bit (standard)" },
            { id: "10bit", label: "10-bit (smoother gradients)" },
            { id: "pq", label: "HDR10 (PQ, BT.2020)" },
            { id: "hlg", label: "HDR HLG (BT.2020)" }
        ]
        readonly property var frameRates: [
            { value: 0, label: "Project" },
            { value: 23.976, label: "23.976" }, { value: 24, label: "24" }, { value: 25, label: "25" },
            { value: 29.97, label: "29.97" }, { value: 30, label: "30" }, { value: 50, label: "50" },
            { value: 59.94, label: "59.94" }, { value: 60, label: "60" }
        ]
        readonly property var bitrates: [0, 2000, 4000, 8000, 12000, 16000, 25000, 40000, 60000, 100000]
        readonly property var captionFiles: [
            { id: "", label: "None" },
            { id: "srt", label: "SRT beside the video" },
            { id: "vtt", label: "WebVTT beside the video" }
        ]
        readonly property var soundFormats: [
            { channels: 2, sampleRate: 48000, label: "Stereo · 48 kHz" },
            { channels: 2, sampleRate: 44100, label: "Stereo · 44.1 kHz (CD, some music services)" },
            { channels: 1, sampleRate: 48000, label: "Mono · 48 kHz (speech, podcasts)" },
            { channels: 1, sampleRate: 44100, label: "Mono · 44.1 kHz" },
            { channels: 6, sampleRate: 48000, label: "5.1 surround · 48 kHz (place tracks with their ⋯ menu)" }
        ]
        readonly property var loudnessTargets: [
            { value: 0, label: "Keep as mixed" },
            { value: -14, label: "YouTube & streaming (−14 LUFS)" },
            { value: -16, label: "Podcast, Apple (−16 LUFS)" },
            { value: -23, label: "TV broadcast, EBU R128 (−23 LUFS)" }
        ]
        // Presets fill the fields below; any manual change makes the choice "Custom".
        readonly property var presets: [
            { label: "Custom", settings: null },
            { label: "YouTube (best quality, 4K upload)", settings: { format: "h264", quality: "max", height: 2160, loudness: -14 } },
            { label: "Share quickly (small H.264 1080p)", settings: { format: "h264", quality: "small", height: 1080, loudness: -14 } },
            { label: "Archive (AV1, maximum quality)", settings: { format: "av1", quality: "max", height: 0, loudness: 0 } },
            { label: "Editing master (ProRes 422 HQ)", settings: { format: "prores", quality: "max", height: 0, loudness: 0 } }
        ]
        property var current: ({ format: "h264", quality: "high", height: 0, loudness: -14 })
        readonly property var preview: editor.exportPreview(current)
        function apply(settings) {
            current = settings;
            exportFormat.currentIndex = formats.findIndex(f => f.id === settings.format);
            exportQuality.currentIndex = qualities.findIndex(q => q.id === settings.quality);
            exportHeight.currentIndex = Math.max(0, heights.indexOf(settings.height));
            exportLoudness.currentIndex = Math.max(0, loudnessTargets.findIndex(l => l.value === (settings.loudness || 0)));
            exportFps.currentIndex = Math.max(0, frameRates.findIndex(r => r.value === (settings.fps || 0)));
            exportBitrate.currentIndex = Math.max(0, bitrates.indexOf(settings.bitrate || 0));
            exportCaptions.currentIndex = Math.max(0, captionFiles.findIndex(f => f.id === (settings.captions || "")));
            exportSound.currentIndex = Math.max(0, soundFormats.findIndex(f => f.channels === (settings.channels || 2) && f.sampleRate === (settings.sampleRate || 48000)));
            exportDynamicRange.currentIndex = Math.max(0, dynamicRanges.findIndex(d => d.id === (settings.dynamicRange || "")));
        }
        function changed() {
            current = {
                format: formats[exportFormat.currentIndex].id,
                quality: qualities[exportQuality.currentIndex].id,
                height: heights[exportHeight.currentIndex],
                loudness: loudnessTargets[exportLoudness.currentIndex].value,
                fps: frameRates[exportFps.currentIndex].value,
                bitrate: bitrates[exportBitrate.currentIndex],
                captions: captionFiles[exportCaptions.currentIndex].id,
                channels: soundFormats[exportSound.currentIndex].channels,
                sampleRate: soundFormats[exportSound.currentIndex].sampleRate,
                dynamicRange: deepFormats.indexOf(formats[exportFormat.currentIndex].id) >= 0 ? dynamicRanges[exportDynamicRange.currentIndex].id : ""
            };
            const match = presets.findIndex(p => p.settings && p.settings.format === current.format && p.settings.quality === current.quality && p.settings.height === current.height && p.settings.loudness === current.loudness && current.fps === 0 && current.bitrate === 0 && current.channels === 2 && current.sampleRate === 48000 && current.captions === "" && current.dynamicRange === "");
            exportPreset.currentIndex = Math.max(0, match);
        }
        onAboutToShow: apply(win.exportChoice)
        GridLayout {
            anchors.fill: parent
            columns: 2
            columnSpacing: 12
            rowSpacing: 10
            Label {
                Layout.columnSpan: 2
                text: "Project " + win.s.width + " × " + win.s.height + " · " + win.s.fps.toFixed(2) + " fps · " + (win.s.duration / win.s.fps).toFixed(2) + " s"
                color: win.muted
            }
            Label { text: "Preset" }
            ComboBox {
                id: exportPreset
                objectName: "exportPreset"
                Layout.fillWidth: true
                model: exportSettings.presets
                textRole: "label"
                onActivated: if (exportSettings.presets[currentIndex].settings)
                    exportSettings.apply(exportSettings.presets[currentIndex].settings)
            }
            Label {
                text: "Range"
                visible: exportRange.visible
            }
            ComboBox {
                id: exportRange
                objectName: "exportRange"
                Layout.fillWidth: true
                visible: win.s.inPoint >= 0 || win.s.outPoint >= 0
                model: ["Whole timeline", "In to out (" + win.clock(Math.max(0, win.s.inPoint)) + " – " + win.clock(win.s.outPoint >= 0 ? win.s.outPoint : win.s.duration) + ")"]
                currentIndex: 1
            }
            Label { text: "Format" }
            ComboBox {
                id: exportFormat
                objectName: "exportFormat"
                Layout.fillWidth: true
                model: exportSettings.formats
                textRole: "label"
                onActivated: exportSettings.changed()
            }
            Label { text: "Quality" }
            ComboBox {
                id: exportQuality
                objectName: "exportQuality"
                Layout.fillWidth: true
                model: exportSettings.qualities
                textRole: "label"
                onActivated: exportSettings.changed()
            }
            Label { text: "Resolution" }
            ComboBox {
                id: exportHeight
                objectName: "exportHeight"
                Layout.fillWidth: true
                model: exportSettings.heights.map(h => h === 0 ? "Project (" + win.s.width + " × " + win.s.height + ")" : h === 2160 ? "4K (2160p)" : h + "p")
                enabled: !exportSettings.preview.audio
                onActivated: exportSettings.changed()
            }
            Label { text: "Colour" }
            ComboBox {
                id: exportDynamicRange
                objectName: "exportDynamicRange"
                Layout.fillWidth: true
                model: exportSettings.dynamicRanges
                textRole: "label"
                enabled: exportSettings.deepFormats.indexOf(exportSettings.current.format) >= 0
                onActivated: exportSettings.changed()
                ToolTip.visible: hovered
                ToolTip.text: "10-bit avoids banding in skies and gradients. HDR marks the video for HDR screens; the picture keeps its look with white at 203 nits. Needs HEVC, AV1, VP9 or ProRes 422."
            }
            Label { text: "Frame rate" }
            ComboBox {
                id: exportFps
                objectName: "exportFps"
                Layout.fillWidth: true
                model: exportSettings.frameRates.map(r => r.value === 0 ? "Project (" + win.s.fps.toFixed(2).replace(/\.00$/, "") + " fps)" : r.label + " fps")
                enabled: !exportSettings.preview.audio && exportSettings.current.format !== "gif"
                onActivated: exportSettings.changed()
                ToolTip.visible: hovered
                ToolTip.text: "Another rate repeats or drops frames of the edit to reach it"
            }
            Label { text: "Bitrate" }
            ComboBox {
                id: exportBitrate
                objectName: "exportBitrate"
                Layout.fillWidth: true
                model: exportSettings.bitrates.map(b => b === 0 ? "By quality" : (b / 1000) + " Mbit/s")
                enabled: !exportSettings.preview.audio && ["gif", "prores", "prores4444", "png"].indexOf(exportSettings.current.format) < 0
                onActivated: exportSettings.changed()
                ToolTip.visible: hovered
                ToolTip.text: "A fixed average bitrate (peaks up to 1.5×) instead of the quality setting, e.g. for platforms with an upload limit"
            }
            Label { text: "Captions" }
            ComboBox {
                id: exportCaptions
                objectName: "exportCaptions"
                Layout.fillWidth: true
                model: exportSettings.captionFiles
                textRole: "label"
                onActivated: exportSettings.changed()
                ToolTip.visible: hovered
                ToolTip.text: "Also saves the titles and captions as a subtitle file named like the video, for YouTube or players that switch subtitles on and off"
            }
            Label { text: "Sound" }
            ComboBox {
                id: exportSound
                objectName: "exportSound"
                Layout.fillWidth: true
                model: exportSettings.soundFormats
                textRole: "label"
                enabled: ["gif", "png"].indexOf(exportSettings.current.format) < 0
                onActivated: exportSettings.changed()
            }
            Label { text: "Loudness" }
            ComboBox {
                id: exportLoudness
                objectName: "exportLoudness"
                Layout.fillWidth: true
                model: exportSettings.loudnessTargets
                textRole: "label"
                onActivated: exportSettings.changed()
                ToolTip.visible: hovered
                ToolTip.text: "Measures the whole mix first and sets one gain, so the video plays as loud as others on the platform. Peaks are limited 1 dB below full scale."
            }
            Action {
                objectName: "measureLoudness"
                text: win.s.loudness && win.s.loudness.status === "measuring" ? "Measuring…" : "Measure mix"
                enabled: win.s.duration > 0 && !(win.s.loudness && win.s.loudness.status === "measuring")
                onClicked: editor.analyzeLoudness()
            }
            Label {
                objectName: "loudnessResult"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 11
                readonly property var l: win.s.loudness || ({})
                readonly property int target: exportSettings.loudnessTargets[exportLoudness.currentIndex].value
                color: l.status === "failed" ? "#e5534b" : win.muted
                text: l.status === "measuring" ? "Measuring the whole mix…"
                    : l.status === "failed" ? "The mix could not be measured."
                    : l.status === "ready" || l.status === "stale"
                        ? l.integrated.toFixed(1) + " LUFS · true peak " + l.peak.toFixed(1) + " dBTP"
                          + (target !== 0 ? " · export changes it by " + ((target - l.integrated) >= 0 ? "+" : "") + (target - l.integrated).toFixed(1) + " dB" : "")
                          + (l.status === "stale" ? " (before your last edit)" : "")
                    : "Integrated loudness of the whole mix, as platforms measure it."
            }
            Label {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: win.muted
                font.pixelSize: 11
                text: exportSettings.current.format === "png" ? "Output: numbered PNG pictures (name_00001.png, …) in a new folder named like the file you choose, " + (exportSettings.preview.width || 0) + " × " + (exportSettings.preview.height || 0) + ", transparent where the timeline is empty." : exportSettings.preview.audio ? "Output: the timeline's sound only, 48 kHz stereo · ." + exportSettings.preview.extension + (exportSettings.preview.extension === "wav" ? " (24-bit at Maximum quality, otherwise 16-bit)" : "") + "." : "Output " + (exportSettings.preview.width || 0) + " × " + (exportSettings.preview.height || 0) + " · ." + (exportSettings.preview.extension || "mp4") + ". Cutlery uses your graphics card's encoder (NVIDIA, AMD or Intel) when available, otherwise Windows' encoder; AV1, VP9 and ProRes also work in software. Higher resolutions re-render each source at that size with sharp Lanczos scaling, so 4K sources stay 4K."
            }
            // Usage rights of the media on the timeline: a note before publishing, and the credits.
            Label {
                id: rightsNote
                objectName: "rightsNote"
                Layout.columnSpan: 2
                Layout.fillWidth: true
                visible: win.convertAsset.length === 0 && exportSettings.visible && text.length > 0
                wrapMode: Text.Wrap
                font.pixelSize: 11
                color: "#e5c07b"
                readonly property var r: exportSettings.visible ? editor.rightsCheck() : ({})
                text: [
                    (r.personal || []).length ? "⚠ For personal use only: " + r.personal.join(", ") + "." : "",
                    (r.unknown || []).length ? "⚠ Rights unknown: " + r.unknown.join(", ") + "." : "",
                    (r.credits || []).length ? (r.credits.length + " medi" + (r.credits.length === 1 ? "um needs" : "a need") + " a credit.") : ""
                ].filter(t => t.length).join(" ")
            }
            Action {
                objectName: "saveCredits"
                Layout.columnSpan: 2
                visible: rightsNote.visible && (rightsNote.r.credits || []).length > 0
                text: "Save credits…"
                onClicked: creditsDialog.open()
                ToolTip.visible: hovered
                ToolTip.text: "Saves the credit lines of the media on the timeline as a text file, e.g. for the video description"
            }
            // The export queue: each job renders the timeline as it was when it was added, one
            // after the other, so you can queue several formats and keep editing.
            Label {
                Layout.columnSpan: 2
                visible: win.s.exportQueue.length > 0
                text: "Export queue" + (win.s.queuePaused ? " (paused after a cancelled export)" : "")
                font.bold: true
            }
            Repeater {
                model: win.s.exportQueue
                delegate: RowLayout {
                    required property var modelData
                    required property int index
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    objectName: "queued-" + index
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                        text: modelData.file + "  ·  " + modelData.label
                    }
                    Label {
                        text: ({ waiting: "Waiting", exporting: "Exporting " + Math.round(win.s.progress * 100) + " %", done: "Done", failed: "Failed", cancelled: "Cancelled" })[modelData.status] || modelData.status
                        color: modelData.status === "failed" || modelData.status === "cancelled" ? "#e06c75" : modelData.status === "done" ? "#98c379" : win.muted
                    }
                    ToolButton {
                        objectName: "removeQueued-" + index
                        text: "✕"
                        enabled: modelData.status !== "exporting"
                        onClicked: editor.removeQueued(index)
                        ToolTip.visible: hovered
                        ToolTip.text: "Remove from the queue"
                    }
                }
            }
            Button {
                Layout.columnSpan: 2
                objectName: "startQueue"
                visible: win.s.queuePaused && win.s.exportQueue.some(q => q.status === "waiting")
                text: "Continue the queue"
                onClicked: editor.startQueue()
            }
        }
    }
    Dialog {
        id: shortcutsDialog
        anchors.centerIn: parent
        title: "Keyboard shortcuts"
        modal: true
        width: 680
        height: Math.min(680, win.height - 80)
        standardButtons: Dialog.Close
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            Label {
                text: "Click a key field to type a combination (e.g. Ctrl+B or Space), then Apply. Clear the field to disable a command. Editing shortcuts leave text fields alone."
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                color: win.muted
            }
            Label {
                visible: shortcutSettings.error.length > 0
                text: shortcutSettings.error
                color: "#ffc2b7"
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Action {
                text: "Restore defaults"
                onClicked: shortcutSettings.reset()
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    spacing: 6
                    Repeater {
                        model: shortcutSettings.bindings
                        RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Label {
                                text: modelData.category
                                color: win.muted
                                font.pixelSize: 10
                                Layout.preferredWidth: 75
                            }
                            Label {
                                text: modelData.label
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                            }
                            TextField {
                                id: keyField
                                text: modelData.sequence
                                Layout.preferredWidth: 130
                                selectByMouse: true
                                onAccepted: shortcutSettings.assign(modelData.id, text)
                            }
                            Action {
                                text: "Apply"
                                onClicked: shortcutSettings.assign(modelData.id, keyField.text)
                            }
                        }
                    }
                }
            }
        }
    }
    Dialog {
        id: about
        anchors.centerIn: parent
        title: "Cutlery · 0.5.0 alpha"
        modal: true
        width: 490
        standardButtons: Dialog.Ok
        Label {
            width: parent.width
            text: "A local desktop editor built around a shared FFmpeg render graph.\n\nImport media, arrange up to 64 tracks, drag clip edges to trim, add titles/captions, adjust picture and sound, play, and export. Higher tracks appear above lower tracks.\n\nPlay or Space starts live playback from the playhead; edits made while playing apply immediately. Track buttons lock edits, mute or solo audio, and hide picture. See Help → Keyboard shortcuts to customize keys. This alpha does not yet include a real-time D3D11 engine, automatic captions, keyframes, masks, tracking, or the full design roadmap.\n\nProject files reference your original media. Keep those files alongside the project. Recovery data: " + win.s.dataPath
            wrapMode: Text.Wrap
            lineHeight: 1.3
        }
    }
}
