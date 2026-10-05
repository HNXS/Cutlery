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
    property bool showScopes: false
    property bool shortcutsBlocked: openDialog.visible || saveDialog.visible || importDialog.visible || exportDialog.visible || relinkDialog.visible || srtOpen.visible || srtSave.visible || soundDialog.visible || backupDialog.visible || discardDialog.visible || settings.visible || exportSettings.visible || about.visible || shortcutsDialog.visible || timelinePanel.dialogOpen
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
        else if (action.startsWith("recent:"))
            editor.openRecent(action.substring(7));
        else if (action.startsWith("restore:"))
            editor.restoreBackup(action.substring(8));
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
            border.color: parent.highlighted ? "#64d8bc" : "#35404b"
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
                objectName: "collectProject"
                text: (win.s.collect || {}).status === "copying" ? "Collecting… " + Math.round(100 * (win.s.collect.progress || 0)) + "%" : "Collect project and media…"
                enabled: (win.s.collect || {}).status !== "copying"
                onTriggered: collectDialog.open()
            }
            MenuSeparator {}
            MenuItem {
                text: "Project settings…"
                onTriggered: settings.open()
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
                text: "Import captions (SRT, VTT, ASS)…"
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
                    RowLayout {
                        Layout.fillWidth: true
                        Action {
                            text: "+ Add title"
                            Layout.fillWidth: true
                            onClicked: editor.addTitle()
                        }
                        // Sound effects: clicks, typing and swooshes for tutorials and screen videos.
                        Action {
                            objectName: "openSounds"
                            text: "♪ Sounds…"
                            Layout.fillWidth: true
                            onClicked: soundDialog.open()
                            ToolTip.visible: hovered
                            ToolTip.text: "Sound effects: mouse clicks, keyboard typing and whooshes, free to use"
                        }
                    }
                    RowLayout {
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
                        Layout.fillWidth: true
                        Action {
                            objectName: "addBlurArea"
                            text: "+ Blur area"
                            Layout.fillWidth: true
                            onClicked: editor.addEffect("blur")
                            ToolTip.visible: hovered
                            ToolTip.text: "Blurs whatever lower tracks show inside a rectangle, e.g. private data in a screen recording"
                        }
                        Action {
                            objectName: "addMosaicArea"
                            text: "+ Mosaic area"
                            Layout.fillWidth: true
                            onClicked: editor.addEffect("pixelate")
                            ToolTip.visible: hovered
                            ToolTip.text: "Pixelates whatever lower tracks show inside a rectangle, e.g. a face"
                        }
                        // Shapes for tutorials and explainers.
                        Action {
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
                        Label {
                            objectName: "playbackRate"
                            visible: editor.playbackRate !== 1 && (editor.playing || editor.playbackRate < 0)
                            text: (editor.playbackRate < 0 ? "◀◀ " : "▶▶ ") + Math.abs(editor.playbackRate) + "×"
                            color: "#ffd479"
                            font.bold: true
                        }
                        Label {
                            text: win.clock(editor.playbackFrame) + " / " + win.clock(win.s.duration)
                            font.family: "Consolas"
                            color: win.mint
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
                                        name: "Speed (0.1–10×)"
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
                            RowLayout {
                                Layout.fillWidth: true
                                visible: win.selection.video === true && (win.selection.speed ?? 1) < 1
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
                            // Blur or mosaic area: what it does and how strongly. Move and resize it
                            // in the preview; its position can be keyframed.
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: (win.selection.effect || "") !== ""
                                spacing: 4
                                Caption {
                                    text: "BLUR / MOSAIC AREA"
                                }
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
                                Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    font.pixelSize: 11
                                    color: win.muted
                                    text: "Drag the frame in the preview to place it and its corners to resize. Keyframe X/Y below to follow something moving, or let it follow a face. The area affects all tracks below it."
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
                                Rule {}
                            }
                            // Shape: kind, colours, outline and size. Move, resize and rotate it in
                            // the preview like any overlay.
                            ColumnLayout {
                                objectName: "graphicSection"
                                Layout.fillWidth: true
                                visible: (win.selection.graphic || "") !== ""
                                spacing: 6
                                Caption { text: "SHAPE" }
                                ComboBox {
                                    objectName: "graphicKind"
                                    Layout.fillWidth: true
                                    readonly property var kinds: ["arrow", "ellipse", "bubble", "rectangle", "line"]
                                    model: ["Arrow", "Circle / ellipse", "Speech bubble", "Box", "Line"]
                                    currentIndex: Math.max(0, kinds.indexOf(win.selection.graphic || "arrow"))
                                    onActivated: index => editor.setClip("graphic", kinds[index])
                                }
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
                                            model: graphicColours.modelData.colors
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
                                Repeater {
                                    model: [
                                        { key: "stroke", name: "Outline width", lo: 0, hi: 0.05 },
                                        { key: "graphicWidth", name: "Width", lo: 0.01, hi: 1 },
                                        { key: "graphicHeight", name: "Height", lo: 0.005, hi: 1 }
                                    ]
                                    RowLayout {
                                        id: graphicRow
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Label {
                                            text: graphicRow.modelData.name
                                            color: win.muted
                                            Layout.preferredWidth: 95
                                        }
                                        Slider {
                                            objectName: "graphic-" + graphicRow.modelData.key
                                            Layout.fillWidth: true
                                            from: graphicRow.modelData.lo
                                            to: graphicRow.modelData.hi
                                            value: win.selection[graphicRow.modelData.key] ?? 0
                                            onPressedChanged: if (!pressed)
                                                editor.setClip(graphicRow.modelData.key, value)
                                            onMoved: if (!pressed)
                                                editor.setClip(graphicRow.modelData.key, value)
                                        }
                                    }
                                }
                                Rule {}
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: win.selection.assetId === "" && (win.selection.effect || "") === "" && ["arrow", "line"].indexOf(win.selection.graphic || "") < 0
                                Caption {
                                    text: (win.selection.graphic || "") !== "" ? "TEXT IN SHAPE" : "TITLE / CAPTION"
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
                                // Title templates: the first line is the name or heading, the
                                // next lines the role or subtitle.
                                ComboBox {
                                    objectName: "titleStyle"
                                    Layout.fillWidth: true
                                    visible: (win.selection.captionStyle || "") === ""
                                    readonly property var styles: ["", "lowerThird", "lowerThirdLine", "titleCard"]
                                    model: ["Plain title", "Lower third (plate)", "Lower third (line)", "Title card"]
                                    currentIndex: Math.max(0, styles.indexOf(win.selection.titleStyle || ""))
                                    onActivated: editor.setClip("titleStyle", styles[currentIndex])
                                }
                                // Plain titles can build up character by character or word by word.
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: (win.selection.titleStyle || "") === "" && (win.selection.captionStyle || "") === "" && !win.selection.graphic
                                    ComboBox {
                                        objectName: "textAnimation"
                                        Layout.fillWidth: true
                                        readonly property var kinds: ["", "typewriter", "words"]
                                        model: ["Appears at once", "Typewriter", "Word by word"]
                                        currentIndex: Math.max(0, kinds.indexOf(win.selection.textAnimation || ""))
                                        onActivated: editor.setClip("textAnimation", kinds[currentIndex])
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Lets the text build up from the start of the clip"
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
                                // Typography: font, weight, alignment, spacing, outline, shadow, box.
                                RowLayout {
                                    Layout.fillWidth: true
                                    ComboBox {
                                        id: fontBox
                                        objectName: "fontFamily"
                                        Layout.fillWidth: true
                                        editable: true
                                        property var families: editor.fontFamilies()
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
                                            text: modelData
                                            font.family: modelData
                                            highlighted: fontBox.highlightedIndex === index
                                        }
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Type to find a font. Fonts you add are kept in Cutlery's data folder."
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
                                Repeater {
                                    model: [
                                        { key: "letterSpacing", name: "Letter spacing", lo: -0.1, hi: 0.5, def: 0 },
                                        { key: "lineSpacing", name: "Line spacing", lo: 0.7, hi: 3, def: 1 },
                                        { key: "outline", name: "Outline", lo: 0, hi: 0.25, def: 0 },
                                        { key: "textShadow", name: "Shadow", lo: 0, hi: 1, def: 1 },
                                        { key: "background", name: "Background box", lo: 0, hi: 1, def: 0 }
                                    ]
                                    RowLayout {
                                        id: textStyleRow
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Label {
                                            text: textStyleRow.modelData.name
                                            color: win.muted
                                            Layout.preferredWidth: 95
                                        }
                                        Slider {
                                            objectName: "text-" + textStyleRow.modelData.key
                                            Layout.fillWidth: true
                                            from: textStyleRow.modelData.lo
                                            to: textStyleRow.modelData.hi
                                            stepSize: .01
                                            value: win.selection[textStyleRow.modelData.key] ?? textStyleRow.modelData.def
                                            onPressedChanged: if (!pressed)
                                                editor.setClip(textStyleRow.modelData.key, value)
                                            onMoved: if (!pressed)
                                                editor.setClip(textStyleRow.modelData.key, value)
                                        }
                                        // Outline and box colours.
                                        Repeater {
                                            model: textStyleRow.modelData.key === "outline" ? ["#000000", "#ffffff", "#ffd23f"] : textStyleRow.modelData.key === "background" ? ["#000000", "#ffffff", "#64d8bc"] : []
                                            Rectangle {
                                                required property string modelData
                                                readonly property string colorKey: textStyleRow.modelData.key === "outline" ? "outlineColor" : "backgroundColor"
                                                width: 16
                                                height: 16
                                                radius: 8
                                                color: modelData
                                                border.width: win.selection[colorKey] === modelData ? 3 : 1
                                                border.color: win.selection[colorKey] === modelData ? win.mint : "#6481a0"
                                                MouseArea {
                                                    anchors.fill: parent
                                                    onClicked: editor.setClip(parent.colorKey, parent.modelData)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            // Presenter overlays: corner placement, shape, border, shadow, green screen.
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: win.selection.audioOnly !== true && (win.selection.effect || "") === "" && editor.clipBounds(win.s.selectedId).width !== undefined
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
                                // Anchor: the point that zoom and rotation keep in place, e.g. a corner for
                                // a zoom into that corner.
                                RowLayout {
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
                            // Beat markers for cutting to music; clips snap to them.
                            RowLayout {
                                Layout.fillWidth: true
                                visible: win.selection.hasAudio === true && win.selection.reverse !== true
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
                            // Phone and screen recordings often have a variable frame rate.
                            ColumnLayout {
                                objectName: "variableRate"
                                Layout.fillWidth: true
                                visible: win.selection.variableRate === true
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
                            Action {
                                objectName: "freezeFrame"
                                Layout.fillWidth: true
                                visible: win.selection.video === true && win.selection.reverse !== true
                                enabled: win.selection.locked !== true && win.selection.playheadInside === true
                                text: "Freeze frame here (2 s)"
                                onClicked: editor.freezeFrame(2)
                                ToolTip.visible: hovered
                                ToolTip.text: win.selection.playheadInside ? "Holds the picture at the playhead for 2 seconds; the rest of the clip continues afterwards" : "Move the playhead into the clip first"
                            }
                            Action {
                                objectName: "splitScenes"
                                Layout.fillWidth: true
                                visible: win.selection.video === true && win.selection.reverse !== true
                                readonly property bool finding: (win.s.scenes || {}).status === "finding"
                                enabled: win.selection.locked !== true && !finding
                                text: finding ? "Finding scene changes…" : "Split at scene changes"
                                onClicked: editor.splitAtScenes(0.5)
                                ToolTip.visible: hovered
                                ToolTip.text: "Cuts the clip into its shots, e.g. a long recording or a downloaded video. Undo restores it."
                            }
                            AiOption {
                                task: "eyecontact"
                                flag: "eyeContact"
                                infoKey: "eyeContactInfo"
                                label: "Eye contact (AI): look into the camera"
                                runningText: "Correcting the gaze…"
                                doneText: "Eye contact ready ✓ · untick to compare"
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
                                        key: "blur",
                                        name: "Blur",
                                        lo: 0,
                                        hi: 1,
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
                                        key: "pan",
                                        name: "Pan (left − / right +)",
                                        lo: -1,
                                        hi: 1,
                                        step: .05
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
                                    readonly property bool animatable: ["scale", "x", "y", "rotation", "opacity", "volume", "pan"].indexOf(modelData.key) >= 0
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
                            // Sound: clean-up, tone and dynamics of the clip's audio.
                            ColumnLayout {
                                objectName: "soundSection"
                                visible: win.selection.hasAudio === true
                                Layout.fillWidth: true
                                spacing: 6
                                Rule {}
                                Caption { text: "SOUND" }
                                ComboBox {
                                    objectName: "soundPreset"
                                    Layout.fillWidth: true
                                    enabled: win.selection.locked !== true
                                    readonly property var presets: [
                                        { label: "Apply a sound preset…", values: null },
                                        { label: "Natural (reset)", values: {} },
                                        { label: "Clear voice", values: { lowCut: 80, denoise: .4, gate: .2, eqMid: 3, deess: .3, compressor: .5 } },
                                        { label: "Warm podcast voice", values: { lowCut: 60, denoise: .3, eqLow: 3, eqMid: 2, eqHigh: -1, deess: .4, compressor: .6 } },
                                        { label: "Noisy room", values: { lowCut: 120, denoise: .8, gate: .5, eqMid: 2, compressor: .4 } },
                                        { label: "Phone call", values: { lowCut: 300, eqLow: -12, eqMid: 6, eqHigh: -12, compressor: .7 } },
                                        { label: "Music: more punch", values: { eqLow: 4, eqHigh: 3, compressor: .3 } }
                                    ]
                                    model: presets.map(p => p.label)
                                    onActivated: index => {
                                        const preset = presets[index].values;
                                        if (preset) {
                                            const values = { eqLow: 0, eqMid: 0, eqHigh: 0, lowCut: 0, compressor: 0, gate: 0, denoise: 0, deess: 0, reverb: 0, echo: 0 };
                                            for (const k in preset)
                                                values[k] = preset[k];
                                            editor.setClipValues(values);
                                        }
                                        currentIndex = 0;
                                    }
                                }
                                Repeater {
                                    model: [
                                        { key: "lowCut", name: "Low cut (Hz)", lo: 0, hi: 300, step: 5, tip: "Removes rumble, hum and wind below this frequency" },
                                        { key: "denoise", name: "Noise reduction", lo: 0, hi: 1, step: .01, tip: "Reduces steady hiss and hum" },
                                        { key: "gate", name: "Noise gate", lo: 0, hi: 1, step: .01, tip: "Lowers the sound between phrases" },
                                        { key: "eqLow", name: "Bass (dB)", lo: -12, hi: 12, step: .5, tip: "Below 100 Hz" },
                                        { key: "eqMid", name: "Presence (dB)", lo: -12, hi: 12, step: .5, tip: "Around 2.5 kHz, where speech is clear" },
                                        { key: "eqHigh", name: "Treble (dB)", lo: -12, hi: 12, step: .5, tip: "Above 8 kHz" },
                                        { key: "deess", name: "De-esser", lo: 0, hi: 1, step: .01, tip: "Softens sharp S sounds" },
                                        { key: "compressor", name: "Compressor", lo: 0, hi: 1, step: .01, tip: "Evens out loud and quiet parts" },
                                        { key: "reverb", name: "Reverb", lo: 0, hi: 1, step: .01, tip: "The sound of a room" },
                                        { key: "echo", name: "Echo", lo: 0, hi: 1, step: .01, tip: "Repeats a third of a second apart" }
                                    ]
                                    RowLayout {
                                        id: soundRow
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Label {
                                            text: soundRow.modelData.name
                                            color: Number(win.selection[soundRow.modelData.key] || 0) !== 0 ? win.mint : win.muted
                                            Layout.preferredWidth: 105
                                        }
                                        Slider {
                                            objectName: "sound-" + soundRow.modelData.key
                                            Layout.fillWidth: true
                                            from: soundRow.modelData.lo
                                            to: soundRow.modelData.hi
                                            stepSize: soundRow.modelData.step
                                            value: Number(win.selection[soundRow.modelData.key] || 0)
                                            enabled: win.selection.locked !== true
                                            onPressedChanged: if (!pressed)
                                                editor.setClip(soundRow.modelData.key, value)
                                            onMoved: if (!pressed)
                                                editor.setClip(soundRow.modelData.key, value)
                                            ToolTip.visible: hovered
                                            ToolTip.text: soundRow.modelData.tip
                                        }
                                        Label {
                                            text: Number(win.selection[soundRow.modelData.key] || 0).toFixed(soundRow.modelData.hi > 1 ? 0 : 2)
                                            font.pixelSize: 10
                                            Layout.preferredWidth: 28
                                        }
                                    }
                                }
                            }
                            // Colour and look of the clip's picture.
                            ColumnLayout {
                                objectName: "lookSection"
                                visible: win.selection.picture === true
                                Layout.fillWidth: true
                                spacing: 6
                                Rule {}
                                Caption { text: "COLOUR & LOOK" }
                                ComboBox {
                                    id: lookPreset
                                    objectName: "lookPreset"
                                    Layout.fillWidth: true
                                    enabled: win.selection.locked !== true
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
                                    model: looks.map(l => l.label)
                                    onActivated: index => {
                                        const look = looks[index].values;
                                        if (look) {
                                            // Every look setting at once, in one undo step; the LUT stays.
                                            const values = { brightness: 0, contrast: 1, saturation: 1, temperature: 0, tint: 0, vibrance: 0, shadows: 0, highlights: 0, sharpen: 0, glow: 0, vignette: 0, grain: 0 };
                                            for (const k in look)
                                                values[k] = look[k];
                                            editor.setClipValues(values);
                                        }
                                        currentIndex = 0;
                                    }
                                }
                                Repeater {
                                    model: [
                                        { key: "temperature", name: "Temperature", lo: -1, hi: 1, tip: "Warmer (right) or cooler (left) light" },
                                        { key: "tint", name: "Tint", lo: -1, hi: 1, tip: "Magenta (right) or green (left)" },
                                        { key: "vibrance", name: "Vibrance", lo: -1, hi: 1, tip: "Saturates muted colours more than strong ones; skin stays natural" },
                                        { key: "shadows", name: "Shadows", lo: -1, hi: 1, tip: "Lift or deepen the dark parts" },
                                        { key: "highlights", name: "Highlights", lo: -1, hi: 1, tip: "Recover or brighten the bright parts" },
                                        { key: "sharpen", name: "Sharpen", lo: 0, hi: 1, tip: "Contrast-adaptive sharpening" },
                                        { key: "glow", name: "Glow", lo: 0, hi: 1, tip: "A soft glow around bright areas" },
                                        { key: "vignette", name: "Vignette", lo: 0, hi: 1, tip: "Darker corners draw the eye to the centre" },
                                        { key: "grain", name: "Film grain", lo: 0, hi: 1, tip: "Moving grain like film" }
                                    ]
                                    ColumnLayout {
                                        id: lookRow
                                        required property var modelData
                                        Layout.fillWidth: true
                                        spacing: 0
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Label {
                                                text: lookRow.modelData.name
                                                color: Number(win.selection[lookRow.modelData.key] || 0) !== 0 ? win.mint : win.muted
                                                Layout.fillWidth: true
                                            }
                                            Label {
                                                text: Number(win.selection[lookRow.modelData.key] || 0).toFixed(2)
                                                font.pixelSize: 10
                                            }
                                        }
                                        Slider {
                                            objectName: "look-" + lookRow.modelData.key
                                            Layout.fillWidth: true
                                            from: lookRow.modelData.lo
                                            to: lookRow.modelData.hi
                                            stepSize: .01
                                            value: Number(win.selection[lookRow.modelData.key] || 0)
                                            enabled: win.selection.locked !== true
                                            onPressedChanged: if (!pressed)
                                                editor.setClip(lookRow.modelData.key, value)
                                            onMoved: if (!pressed)
                                                editor.setClip(lookRow.modelData.key, value)
                                            ToolTip.visible: hovered
                                            ToolTip.text: lookRow.modelData.tip + ". Double-click to reset."
                                            TapHandler {
                                                acceptedButtons: Qt.LeftButton
                                                onDoubleTapped: editor.setClip(lookRow.modelData.key, 0)
                                            }
                                        }
                                    }
                                }
                                RowLayout {
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
                                RowLayout {
                                    visible: !!win.selection.lut
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
                            ColumnLayout {
                                objectName: "effectsSection"
                                visible: win.selection.picture === true
                                Layout.fillWidth: true
                                spacing: 6
                                Rule {}
                                Caption { text: "EFFECTS" }
                                RowLayout {
                                    Layout.fillWidth: true
                                    ComboBox {
                                        id: fxChoice
                                        objectName: "fxChoice"
                                        Layout.fillWidth: true
                                        enabled: win.selection.locked !== true
                                        readonly property var keys: ["", "shake", "glitch", "vhs", "film"]
                                        model: ["No effect", "Camera shake", "Glitch", "VHS", "Old film"]
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
                                RowLayout {
                                    visible: win.selection.video === true
                                    Layout.fillWidth: true
                                    Label {
                                        text: "Motion blur"
                                        color: Number(win.selection.motionBlur || 0) > 0 ? win.mint : win.muted
                                        Layout.preferredWidth: 105
                                    }
                                    Slider {
                                        objectName: "motionBlur"
                                        Layout.fillWidth: true
                                        from: 0
                                        to: 1
                                        stepSize: .01
                                        value: Number(win.selection.motionBlur || 0)
                                        enabled: win.selection.locked !== true
                                        onPressedChanged: if (!pressed)
                                            editor.setClip("motionBlur", value)
                                        onMoved: if (!pressed)
                                            editor.setClip("motionBlur", value)
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Smears fast movement across frames"
                                    }
                                }
                                CheckBox {
                                    objectName: "stabilize"
                                    visible: win.selection.video === true
                                    text: "Stabilize"
                                    checked: win.selection.stabilize === true
                                    enabled: win.selection.locked !== true
                                    onToggled: editor.setClip("stabilize", checked)
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Smooths a shaky hand-held camera; the edges are filled in"
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
                            RowLayout {
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
                                visible: win.selection.canDetach || false
                                Layout.fillWidth: true
                                onClicked: editor.detachAudio()
                            }
                            Action {
                                objectName: "unlinkClip"
                                text: "Unlink picture and sound"
                                visible: (win.selection.linkedCount || 0) > 0
                                enabled: win.selection.locked !== true
                                Layout.fillWidth: true
                                onClicked: editor.unlinkClip()
                                ToolTip.visible: hovered
                                ToolTip.text: "Linked clips move and trim together. Unlink to edit them separately."
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
                    text: "0.5.0 ALPHA"
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
        nameFilters: ["Media files (*.mp4 *.mov *.mkv *.webm *.avi *.mp3 *.wav *.m4a *.aac *.flac *.ogg *.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff *.gif *.svg)", "All files (*)"]
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
    // Collect: copies the project and everything it uses into a new folder, e.g. to archive it or
    // move it to another computer.
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
        id: lutDialog
        title: "Choose a LUT"
        nameFilters: ["3D LUTs (*.cube *.3dl)", "All files (*)"]
        onAccepted: editor.setClip("lut", selectedFile)
    }
    FileDialog {
        id: srtOpen
        title: "Import captions"
        nameFilters: ["Captions (*.srt *.vtt *.ass *.ssa)", "SubRip (*.srt)", "WebVTT (*.vtt)", "SubStation Alpha (*.ass *.ssa)"]
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
                onClicked: editor.generateCaptions(captionDialog.languages[captionLanguage.currentIndex].id, captionStyleChoice.styles[captionStyleChoice.currentIndex])
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
            { id: "mpeg4", label: "MPEG-4 Part 2 · MP4 (legacy, always available)" },
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
                text: exportSettings.preview.audio ? "Output: the timeline's sound only, 48 kHz stereo · ." + exportSettings.preview.extension + (exportSettings.preview.extension === "wav" ? " (24-bit at Maximum quality, otherwise 16-bit)" : "") + "." : "Output " + (exportSettings.preview.width || 0) + " × " + (exportSettings.preview.height || 0) + " · ." + (exportSettings.preview.extension || "mp4") + ". Cutlery uses your graphics card's encoder (NVIDIA, AMD or Intel) when available, otherwise Windows' encoder; AV1, VP9 and ProRes also work in software. Higher resolutions re-render each source at that size with sharp Lanczos scaling, so 4K sources stay 4K."
            }
        }
        onAccepted: {
            win.exportChoice = Object.assign({}, current, { range: exportRange.visible && exportRange.currentIndex === 1 ? "inout" : "all" });
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
