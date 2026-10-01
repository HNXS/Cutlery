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
    property var exportChoice: ({
            format: "h264",
            quality: "high",
            height: 0,
            loudness: -14
        })
    property string pendingAction: ""
    property bool allowClose: false
    property var libraryGesture: null
    property bool textEditing: activeFocusItem && typeof activeFocusItem.cursorPosition === "number"
    property bool shortcutsBlocked: openDialog.visible || saveDialog.visible || importDialog.visible || exportDialog.visible || relinkDialog.visible || srtOpen.visible || srtSave.visible || discardDialog.visible || settings.visible || exportSettings.visible || about.visible || shortcutsDialog.visible || timelinePanel.dialogOpen
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
    function shortcutEnabled(command) {
        if ((libraryGesture && libraryGesture.dragging) || timelinePanel.draggingClip)
            return false;
        if (shortcutsBlocked)
            return false;
        if (textEditing)
            return ["new", "open", "import", "save", "saveAs", "export", "shortcuts"].indexOf(command) >= 0;
        if (["previousFrame", "nextFrame", "previousCut", "nextCut"].indexOf(command) >= 0 && activeFocusItem && (activeFocusItem instanceof Slider || activeFocusItem instanceof ComboBox || activeFocusItem instanceof SpinBox))
            return false;
        return true;
    }
    function command(id) {
        if (editor.playing && ["split", "trimStart", "trimEnd", "previousCut", "nextCut"].indexOf(id) >= 0)
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
        else if (id === "delete" && editable)
            editor.remove(false);
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
        else if (id === "previousFrame")
            goTo(editor.playbackFrame - 1);
        else if (id === "nextFrame")
            goTo(editor.playbackFrame + 1);
        else if (id === "previousCut")
            goTo(editor.adjacentCut(false));
        else if (id === "nextCut")
            goTo(editor.adjacentCut(true));
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
    }
    function clock(frame) {
        const fps = s.fps;
        const sec = Math.floor(frame / fps);
        return String(Math.floor(sec / 60)).padStart(2, "0") + ":" + String(sec % 60).padStart(2, "0") + ":" + String(Math.floor(frame % fps)).padStart(2, "0");
    }
    function guarded(action) {
        if (s.dirty || s.busy) {
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
        else if (action === "close") {
            allowClose = true;
            win.close();
        }
    }
    function saveProject() {
        if (s.path.length)
            editor.save();
        else
            saveDialog.open();
    }
    onClosing: function (close) {
        if (!allowClose && (s.dirty || s.busy)) {
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
            border.color: "#35404b"
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
        visible: win.selection[infoKey] !== undefined
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
                text: "Open…"
                onTriggered: win.guarded("open")
            }
            MenuItem {
                text: "Save"
                onTriggered: win.saveProject()
            }
            MenuItem {
                text: "Save As…"
                onTriggered: saveDialog.open()
            }
            MenuSeparator {}
            MenuItem {
                text: "Project settings…"
                onTriggered: settings.open()
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
                text: "Import SRT…"
                onTriggered: srtOpen.open()
            }
            MenuItem {
                text: "Export titles/captions as SRT…"
                onTriggered: srtSave.open()
            }
        }
        Menu {
            title: "Help"
            MenuItem {
                text: "Keyboard shortcuts…"
                onTriggered: shortcutsDialog.open()
            }
            MenuItem {
                text: "About this alpha"
                onTriggered: about.open()
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
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 64
            color: "#171d24"
            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12
                Label {
                    text: "CUTLERY"
                    font.pixelSize: 21
                    font.bold: true
                    font.letterSpacing: 3
                    color: win.mint
                }
                Rectangle {
                    width: 1
                    Layout.fillHeight: true
                    color: "#34404a"
                }
                ColumnLayout {
                    spacing: 2
                    Label {
                        text: win.s.name
                        font.bold: true
                    }
                    Label {
                        text: win.s.width + " × " + win.s.height + "  /  " + win.s.fps.toFixed(2) + " fps"
                        color: win.muted
                        font.pixelSize: 10
                    }
                }
                Item {
                    Layout.fillWidth: true
                }
                Action {
                    text: "Undo"
                    enabled: win.s.canUndo
                    onClicked: editor.undo()
                }
                Action {
                    text: "Redo"
                    enabled: win.s.canRedo
                    onClicked: editor.redo()
                }
                Action {
                    text: "Save project"
                    onClicked: win.saveProject()
                }
                Action {
                    text: "Export video ↗"
                    enabled: win.s.duration > 0 && !win.s.busy
                    onClicked: exportSettings.open()
                    palette.buttonText: win.mint
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
                SplitView.preferredWidth: 240
                SplitView.minimumWidth: 190
                color: "#171d24"
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 14
                    RowLayout {
                        Layout.fillWidth: true
                        Caption {
                            text: "MEDIA LIBRARY"
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Label {
                            text: editor.assets.length
                            color: win.muted
                        }
                    }
                    Action {
                        text: win.s.importing ? "Reading media…" : "+ Import media"
                        Layout.fillWidth: true
                        enabled: !win.s.importing
                        onClicked: importDialog.open()
                    }
                    Label {
                        text: "Drag media onto any track.\nDrop files here to import them."
                        color: win.muted
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    RowLayout {
                        Label {
                            text: "Append to"
                            color: win.muted
                        }
                        ComboBox {
                            model: editor.trackList
                            textRole: "name"
                            currentIndex: Math.min(win.targetTrack, win.s.tracks - 1)
                            onActivated: win.targetTrack = currentIndex
                            Layout.fillWidth: true
                        }
                    }
                    ListView {
                        objectName: "mediaLibrary"
                        ScrollBar.vertical: ScrollBar {}
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 8
                        model: editor.assets
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
                                        text: modelData.missing ? "Missing • relink in inspector" : modelData.kind.toUpperCase() + "  ·  " + modelData.seconds.toFixed(1) + "s"
                                        font.pixelSize: 10
                                        color: win.muted
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
                    Rule {}
                    Action {
                        text: "+ Add title"
                        Layout.fillWidth: true
                        onClicked: editor.addTitle()
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
                        Caption {
                            text: "PROJECT MONITOR"
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Label {
                            text: editor.playing ? "LIVE PLAYBACK" : "PREVIEW FRAME"
                            color: win.muted
                            font.pixelSize: 9
                            font.letterSpacing: 1
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
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
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 10
                        Action {
                            text: "−1"
                            onClicked: win.command("previousFrame")
                        }
                        Action {
                            text: editor.playing ? "Pause" : "Play"
                            enabled: win.s.duration > 0 && !win.s.busy
                            onClicked: editor.togglePlayback()
                        }
                        Action {
                            text: "+1"
                            onClicked: win.command("nextFrame")
                        }
                        Label {
                            text: win.clock(editor.playbackFrame) + " / " + win.clock(win.s.duration)
                            font.family: "Consolas"
                            color: win.mint
                        }
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Space plays live from the playhead"
                        color: win.muted
                        font.pixelSize: 10
                    }
                }
            }
            Rectangle {
                SplitView.preferredWidth: 272
                SplitView.minimumWidth: 235
                color: "#171d24"
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 16
                    clip: true
                    contentWidth: availableWidth
                    ColumnLayout {
                        width: parent.width
                        spacing: 12
                        Caption {
                            text: "CLIP INSPECTOR"
                        }
                        Label {
                            text: win.selection.name || "Select a clip"
                            font.pixelSize: 17
                            font.bold: true
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                        }
                        Label {
                            visible: !win.s.selectedId.length
                            text: "Select a timeline clip to trim,\ntransform or adjust its sound."
                            color: win.muted
                            lineHeight: 1.5
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
                                        name: "Speed (0.25–4×)"
                                    }
                                ]
                                RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Label {
                                        text: modelData.name
                                        Layout.fillWidth: true
                                        color: win.muted
                                    }
                                    TextField {
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
                            ComboBox {
                                Layout.fillWidth: true
                                model: editor.trackList
                                textRole: "name"
                                currentIndex: win.selection.track ?? 0
                                onActivated: editor.setClip("track", currentIndex)
                            }
                            Rule {}
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: win.selection.audioOnly !== true
                                Caption {
                                    text: "TRANSITION FROM PREVIOUS CLIP"
                                }
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
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    visible: win.selection.canTransition === true && (win.selection.transition || "") !== ""
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: "Duration"
                                            color: win.muted
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            text: ((win.selection.transitionLength || 0) / win.s.fps).toFixed(2) + " s" + ((win.selection.transitionLength || 0) < (win.selection.transitionFrames || 0) ? " (clip limit)" : "")
                                            font.pixelSize: 10
                                        }
                                    }
                                    Slider {
                                        objectName: "transitionDuration"
                                        Layout.fillWidth: true
                                        from: .1
                                        to: 3
                                        stepSize: .05
                                        value: (win.selection.transitionFrames || 0) / win.s.fps
                                        onPressedChanged: if (!pressed)
                                            editor.setClip("transitionFrames", Math.max(2, Math.round(value * win.s.fps)))
                                        onMoved: if (!pressed)
                                            editor.setClip("transitionFrames", Math.max(2, Math.round(value * win.s.fps)))
                                    }
                                }
                                Rule {}
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: win.selection.assetId === ""
                                Caption {
                                    text: "TITLE / CAPTION"
                                }
                                TextArea {
                                    id: titleText
                                    objectName: "titleText"
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
                                RowLayout {
                                    Label {
                                        text: "Font size"
                                        Layout.fillWidth: true
                                        color: win.muted
                                    }
                                    SpinBox {
                                        from: 8
                                        to: 500
                                        value: win.selection.fontSize || 72
                                        editable: true
                                        onValueModified: editor.setClip("fontSize", value)
                                    }
                                }
                                TextField {
                                    Layout.fillWidth: true
                                    text: win.selection.textColor || "#ffffff"
                                    placeholderText: "Text colour (#rrggbb)"
                                    onEditingFinished: editor.setClip("textColor", text)
                                }
                            }
                            // Presenter overlays: corner placement, shape, border, shadow, green screen.
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: win.selection.audioOnly !== true && editor.clipBounds(win.s.selectedId).width !== undefined
                                spacing: 6
                                Caption {
                                    text: "PRESENTER OVERLAY"
                                }
                                RowLayout {
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
                                ComboBox {
                                    id: overlayShape
                                    objectName: "overlayShape"
                                    Layout.fillWidth: true
                                    readonly property var shapes: ["rect", "rounded", "circle"]
                                    model: ["Rectangle", "Rounded corners", "Circle"]
                                    currentIndex: Math.max(0, shapes.indexOf(win.selection.shape || "rect"))
                                    onActivated: editor.setClip("shape", shapes[currentIndex])
                                }
                                Repeater {
                                    model: [
                                        { key: "radius", name: "Corner radius", lo: 0, hi: .5, step: .01, show: "rounded" },
                                        { key: "border", name: "Border", lo: 0, hi: .03, step: .001, show: "" },
                                        { key: "shadow", name: "Shadow", lo: 0, hi: 1, step: .05, show: "" }
                                    ]
                                    ColumnLayout {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        spacing: 0
                                        visible: modelData.show === "" || win.selection.shape === modelData.show
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Label {
                                                text: modelData.name
                                                color: win.muted
                                                Layout.fillWidth: true
                                            }
                                            Label {
                                                text: Number(win.selection[modelData.key] ?? 0).toFixed(modelData.key === "border" ? 3 : 2)
                                                font.pixelSize: 10
                                            }
                                        }
                                        Slider {
                                            objectName: "style-" + modelData.key
                                            Layout.fillWidth: true
                                            from: modelData.lo
                                            to: modelData.hi
                                            stepSize: modelData.step
                                            value: win.selection[modelData.key] ?? 0
                                            onPressedChanged: if (!pressed)
                                                editor.setClip(modelData.key, value)
                                            onMoved: if (!pressed)
                                                editor.setClip(modelData.key, value)
                                        }
                                    }
                                }
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
                                AiOption {
                                    task: "matte"
                                    flag: "aiCutout"
                                    infoKey: "cutout"
                                    label: "Remove background (AI)"
                                    runningText: "Finding the speaker…"
                                    doneText: "Speaker found ✓"
                                    statusName: "cutoutStatus"
                                    runName: "cutoutAnalyze"
                                }
                                CheckBox {
                                    objectName: "chromaKey"
                                    text: "Remove green/blue screen"
                                    checked: win.selection.chromaKey || false
                                    onToggled: editor.setClip("chromaKey", checked)
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    visible: win.selection.chromaKey === true
                                    spacing: 0
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
                                    }
                                    Repeater {
                                        model: [
                                            { key: "keySimilarity", name: "Tolerance", lo: .01, hi: .6 },
                                            { key: "keyBlend", name: "Edge softness", lo: 0, hi: .4 }
                                        ]
                                        ColumnLayout {
                                            required property var modelData
                                            Layout.fillWidth: true
                                            spacing: 0
                                            Label {
                                                text: modelData.name + "  " + Number(win.selection[modelData.key] ?? 0).toFixed(2)
                                                color: win.muted
                                            }
                                            Slider {
                                                Layout.fillWidth: true
                                                from: modelData.lo
                                                to: modelData.hi
                                                stepSize: .01
                                                value: win.selection[modelData.key] ?? 0
                                                onPressedChanged: if (!pressed)
                                                    editor.setClip(modelData.key, value)
                                                onMoved: if (!pressed)
                                                    editor.setClip(modelData.key, value)
                                            }
                                        }
                                    }
                                }
                                Rule {}
                            }
                            Caption {
                                text: "PICTURE & SOUND"
                            }
                            Action {
                                objectName: "removePauses"
                                Layout.fillWidth: true
                                visible: win.selection.hasAudio === true && win.selection.reverse !== true
                                enabled: win.selection.locked !== true
                                text: "Remove pauses…"
                                onClicked: pauseDialog.open()
                            }
                            AiOption {
                                task: "upscale"
                                flag: "aiUpscale"
                                infoKey: "upscale"
                                visible: (win.selection.upscaleHeight || 0) > 0
                                label: "Enhance resolution (AI, up to " + (win.selection.upscaleHeight || 0) + "p)"
                                runningText: "Upscaling…"
                                doneText: "Sharper picture ready ✓"
                            }
                            RowLayout {
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
                                    text: "◀◆"
                                    padding: 6
                                    onClicked: win.goTo(editor.adjacentKeyframe(false))
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Previous keyframe"
                                }
                                Action {
                                    objectName: "nextKeyframe"
                                    text: "◆▶"
                                    padding: 6
                                    onClicked: win.goTo(editor.adjacentKeyframe(true))
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Next keyframe"
                                }
                            }
                            Repeater {
                                model: [
                                    {
                                        key: "scale",
                                        name: "Scale",
                                        lo: .1,
                                        hi: 3,
                                        step: .01
                                    },
                                    {
                                        key: "x",
                                        name: "Horizontal position",
                                        lo: -1,
                                        hi: 1,
                                        step: .01
                                    },
                                    {
                                        key: "y",
                                        name: "Vertical position",
                                        lo: -1,
                                        hi: 1,
                                        step: .01
                                    },
                                    {
                                        key: "rotation",
                                        name: "Rotation",
                                        lo: -180,
                                        hi: 180,
                                        step: 1
                                    },
                                    {
                                        key: "crop",
                                        name: "Crop all edges",
                                        lo: 0,
                                        hi: .45,
                                        step: .01
                                    },
                                    {
                                        key: "opacity",
                                        name: "Opacity",
                                        lo: 0,
                                        hi: 1,
                                        step: .01
                                    },
                                    {
                                        key: "brightness",
                                        name: "Brightness",
                                        lo: -.5,
                                        hi: .5,
                                        step: .01
                                    },
                                    {
                                        key: "contrast",
                                        name: "Contrast",
                                        lo: .1,
                                        hi: 3,
                                        step: .01
                                    },
                                    {
                                        key: "saturation",
                                        name: "Saturation",
                                        lo: 0,
                                        hi: 3,
                                        step: .01
                                    },
                                    {
                                        key: "volume",
                                        name: "Volume",
                                        lo: 0,
                                        hi: 4,
                                        step: .01
                                    },
                                    {
                                        key: "fadeIn",
                                        name: "Fade in (sec)",
                                        lo: 0,
                                        hi: 5,
                                        step: .1
                                    },
                                    {
                                        key: "fadeOut",
                                        name: "Fade out (sec)",
                                        lo: 0,
                                        hi: 5,
                                        step: .1
                                    }
                                ]
                                ColumnLayout {
                                    id: propertyRow
                                    required property var modelData
                                    readonly property bool animatable: ["scale", "x", "y", "rotation", "opacity", "volume"].indexOf(modelData.key) >= 0
                                    readonly property bool animated: animatable && ((win.selection.keyframeCount || {})[modelData.key] || 0) > 0
                                    // Animated properties show their value at the playhead.
                                    readonly property real current: animated ? win.selection.animated[modelData.key] : Number(win.selection[modelData.key] ?? 0)
                                    Layout.fillWidth: true
                                    spacing: 0
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: modelData.name
                                            color: propertyRow.animated ? win.mint : win.muted
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            text: propertyRow.current.toFixed(2)
                                            font.pixelSize: 10
                                        }
                                        ToolButton {
                                            objectName: "keyframe-" + modelData.key
                                            visible: propertyRow.animatable
                                            enabled: win.selection.playheadInside === true && win.selection.locked !== true
                                            implicitWidth: 24
                                            implicitHeight: 22
                                            text: (win.selection.keyed || {})[modelData.key] ? "◆" : "◇"
                                            palette.buttonText: propertyRow.animated ? "#ffd479" : "#e7edf2"
                                            onClicked: editor.toggleKeyframe(modelData.key)
                                            ToolTip.visible: hovered
                                            ToolTip.text: (win.selection.keyed || {})[modelData.key] ? "Remove keyframe" : "Add keyframe at playhead"
                                        }
                                    }
                                    Slider {
                                        Layout.fillWidth: true
                                        from: modelData.lo
                                        to: modelData.hi
                                        stepSize: modelData.step
                                        value: propertyRow.current
                                        onPressedChanged: if (!pressed)
                                            editor.setClip(modelData.key, value)
                                        onMoved: if (!pressed)
                                            editor.setClip(modelData.key, value)
                                    }
                                }
                            }
                            RowLayout {
                                CheckBox {
                                    text: "Reverse"
                                    checked: win.selection.reverse || false
                                    onToggled: editor.setClip("reverse", checked)
                                }
                                CheckBox {
                                    text: "Flip"
                                    checked: win.selection.flip || false
                                    onToggled: editor.setClip("flip", checked)
                                }
                            }
                            RowLayout {
                                CheckBox {
                                    text: "Mute"
                                    checked: win.selection.muted || false
                                    onToggled: editor.setClip("muted", checked)
                                }
                                CheckBox {
                                    text: "Hide"
                                    checked: win.selection.hidden || false
                                    onToggled: editor.setClip("hidden", checked)
                                }
                            }
                            Action {
                                text: "Detach audio to new track"
                                visible: win.selection.canDetach || false
                                Layout.fillWidth: true
                                onClicked: editor.detachAudio()
                            }
                            Action {
                                text: "Relink source media…"
                                visible: (win.selection.assetId || "").length > 0
                                Layout.fillWidth: true
                                onClicked: relinkDialog.open()
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
                    text: "0.4.0 ALPHA"
                    font.pixelSize: 9
                    font.letterSpacing: 1
                    color: win.mint
                }
            }
        }
    }
    FileDialog {
        id: importDialog
        title: "Import local media"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Media files (*.mp4 *.mov *.mkv *.webm *.avi *.mp3 *.wav *.m4a *.aac *.flac *.ogg *.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff)", "All files (*)"]
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
        id: exportDialog
        title: "Export — choose a new filename"
        fileMode: FileDialog.SaveFile
        readonly property string extension: editor.exportPreview(win.exportChoice).extension || "mp4"
        defaultSuffix: extension
        nameFilters: [extension.toUpperCase() + " (*." + extension + ")"]
        onAccepted: editor.exportWith(selectedFile, win.exportChoice)
    }
    FileDialog {
        id: relinkDialog
        title: "Choose replacement media"
        onAccepted: editor.relink(win.selection.assetId, selectedFile)
    }
    FileDialog {
        id: srtOpen
        title: "Import captions"
        nameFilters: ["SubRip captions (*.srt)"]
        onAccepted: editor.importSrt(selectedFile)
    }
    FileDialog {
        id: srtSave
        title: "Export captions"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "srt"
        nameFilters: ["SubRip captions (*.srt)"]
        onAccepted: editor.exportSrt(selectedFile)
    }
    // Remove pauses: silence detection on the selected clip's sound, then one ripple edit.
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
                onClicked: editor.generateCaptions(captionDialog.languages[captionLanguage.currentIndex].id)
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
    Dialog {
        id: discardDialog
        anchors.centerIn: parent
        title: "Unsaved work / active render"
        modal: true
        width: 420
        standardButtons: Dialog.Discard | Dialog.Cancel
        Label {
            width: parent.width
            text: "Discard unsaved changes and continue? An active render will be cancelled. Cancel to save your project first."
            wrapMode: Text.Wrap
        }
        onDiscarded: win.runAction(win.pendingAction)
    }
    Dialog {
        id: settings
        anchors.centerIn: parent
        title: "Project settings"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 360
        ColumnLayout {
            anchors.fill: parent
            Label {
                text: "Canvas"
            }
            ComboBox {
                id: canvas
                Layout.fillWidth: true
                model: ["1920 × 1080 · landscape", "1080 × 1920 · portrait", "1080 × 1080 · square", "1280 × 720 · landscape"]
            }
            Label {
                text: "Frame rate (set before adding clips)"
                color: win.muted
            }
            ComboBox {
                id: frameRate
                enabled: win.s.duration === 0
                Layout.fillWidth: true
                model: ["24", "25", "30", "50", "60", "29.97 (30000/1001)"]
                currentIndex: 2
            }
        }
        onAccepted: {
            const dims = [[1920, 1080], [1080, 1920], [1080, 1080], [1280, 720]][canvas.currentIndex];
            const rate = [24, 25, 30, 50, 60, 30000][frameRate.currentIndex];
            editor.configure(dims[0], dims[1], win.s.duration > 0 ? win.s.fpsN : rate, win.s.duration > 0 ? win.s.fpsD : (frameRate.currentIndex === 5 ? 1001 : 1));
        }
    }
    Dialog {
        id: exportSettings
        objectName: "exportSettings"
        anchors.centerIn: parent
        title: "Export video"
        modal: true
        width: 480
        standardButtons: Dialog.Ok | Dialog.Cancel
        readonly property var formats: [
            { id: "h264", label: "H.264 · MP4 (plays everywhere)" },
            { id: "hevc", label: "HEVC / H.265 · MP4 (smaller files)" },
            { id: "av1", label: "AV1 · MP4 (smallest, modern devices)" },
            { id: "vp9", label: "VP9 · WebM (web)" },
            { id: "prores", label: "ProRes 422 HQ · MOV (editing master, large)" },
            { id: "mpeg4", label: "MPEG-4 Part 2 · MP4 (legacy, always available)" }
        ]
        readonly property var qualities: [
            { id: "max", label: "Maximum" },
            { id: "high", label: "High" },
            { id: "balanced", label: "Balanced" },
            { id: "small", label: "Small file" }
        ]
        readonly property var heights: [0, 720, 1080, 1440, 2160]
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
        }
        function changed() {
            current = {
                format: formats[exportFormat.currentIndex].id,
                quality: qualities[exportQuality.currentIndex].id,
                height: heights[exportHeight.currentIndex],
                loudness: loudnessTargets[exportLoudness.currentIndex].value
            };
            const match = presets.findIndex(p => p.settings && p.settings.format === current.format && p.settings.quality === current.quality && p.settings.height === current.height && p.settings.loudness === current.loudness);
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
            Label {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: win.muted
                font.pixelSize: 11
                text: "Output " + (exportSettings.preview.width || 0) + " × " + (exportSettings.preview.height || 0) + " · ." + (exportSettings.preview.extension || "mp4") + ". Cutlery uses your graphics card's encoder (NVIDIA, AMD or Intel) when available, otherwise Windows' encoder; AV1, VP9 and ProRes also work in software. Higher resolutions re-render each source at that size with sharp Lanczos scaling, so 4K sources stay 4K."
            }
        }
        onAccepted: {
            win.exportChoice = current;
            exportDialog.open();
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
        title: "Cutlery · 0.4.0 alpha"
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
