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
    property string exportProfile: "mpeg4"
    property string pendingAction: ""
    property bool allowClose: false
    property bool showPlayback: false
    property bool playWhenReady: false
    property var libraryGesture: null
    property bool textEditing: activeFocusItem && typeof activeFocusItem.cursorPosition === "number"
    property bool shortcutsBlocked: openDialog.visible || saveDialog.visible || importDialog.visible || exportDialog.visible || relinkDialog.visible || srtOpen.visible || srtSave.visible || discardDialog.visible || settings.visible || exportSettings.visible || about.visible || shortcutsDialog.visible || playbackError.visible || timelinePanel.dialogOpen
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
        player.pause();
        showPlayback = false;
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
        if (showPlayback && ["split", "trimStart", "trimEnd", "previousCut", "nextCut"].indexOf(id) >= 0)
            goTo(Math.floor(player.position * s.fps / 1000));
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
            play();
        else if (id === "pause")
            goTo(showPlayback ? Math.floor(player.position * s.fps / 1000) : s.playhead);
        else if (id === "render" && s.duration > 0 && !s.busy)
            editor.renderPlayback();
        else if (id === "previousFrame")
            goTo((showPlayback ? Math.floor(player.position * s.fps / 1000) : s.playhead) - 1);
        else if (id === "nextFrame")
            goTo((showPlayback ? Math.floor(player.position * s.fps / 1000) : s.playhead) + 1);
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
        player.stop();
        showPlayback = false;
        playWhenReady = false;
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
    function play() {
        if (!s.playbackUrl.length) {
            playWhenReady = true;
            editor.renderPlayback();
            return;
        }
        showPlayback = true;
        if (player.playbackState === MediaPlayer.PlayingState) {
            player.pause();
            editor.seek(Math.floor(player.position * s.fps / 1000));
        } else {
            player.position = Math.round(s.playhead / s.fps * 1000);
            player.play();
        }
    }
    onClosing: function (close) {
        if (!allowClose && (s.dirty || s.busy)) {
            close.accepted = false;
            pendingAction = "close";
            discardDialog.open();
        }
    }
    Connections {
        target: editor
        function onChanged() {
            if (!win.s.playbackUrl.length) {
                player.stop();
                win.showPlayback = false;
            }
            if (win.playWhenReady && win.s.playbackUrl.length) {
                win.playWhenReady = false;
                Qt.callLater(win.play);
            } else if (win.playWhenReady && !win.s.busy)
                win.playWhenReady = false;
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
                                    width: 38
                                    height: 42
                                    radius: 5
                                    color: modelData.kind === "audio" ? "#344c4e" : "#354255"
                                    Label {
                                        anchors.centerIn: parent
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
                            text: win.showPlayback ? "CACHED PLAYBACK" : "RENDERED FRAME"
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
                            Image {
                                anchors.fill: parent
                                source: win.s.previewUrl
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                visible: !win.showPlayback && source.toString().length > 0
                            }
                            VideoOutput {
                                id: videoOutput
                                anchors.fill: parent
                                fillMode: VideoOutput.PreserveAspectFit
                                visible: win.showPlayback
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
                            text: player.playbackState === MediaPlayer.PlayingState ? "Pause" : win.s.playbackUrl.length ? "Play" : "Render & play"
                            enabled: win.s.duration > 0 && !win.s.busy
                            onClicked: win.play()
                        }
                        Action {
                            text: "+1"
                            onClicked: win.command("nextFrame")
                        }
                        Label {
                            text: win.clock(win.showPlayback ? Math.floor(player.position * win.s.fps / 1000) : win.s.playhead) + " / " + win.clock(win.s.duration)
                            font.family: "Consolas"
                            color: win.mint
                        }
                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        Action {
                            text: "Render playback"
                            enabled: win.s.duration > 0 && !win.s.busy
                            onClicked: editor.renderPlayback()
                        }
                        Label {
                            text: "Space renders if needed, then plays"
                            color: win.muted
                            font.pixelSize: 10
                        }
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
                            Caption {
                                text: "PICTURE & SOUND"
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
                                    required property var modelData
                                    Layout.fillWidth: true
                                    spacing: 0
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: modelData.name
                                            color: win.muted
                                            Layout.fillWidth: true
                                        }
                                        Label {
                                            text: Number(win.selection[modelData.key] ?? 0).toFixed(2)
                                            font.pixelSize: 10
                                        }
                                    }
                                    Slider {
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
            playbackFrame: win.showPlayback ? player.position / 1000 * win.s.fps : -1
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
                    text: "0.3.0 ALPHA"
                    font.pixelSize: 9
                    font.letterSpacing: 1
                    color: win.mint
                }
            }
        }
    }
    MediaPlayer {
        id: player
        objectName: "previewPlayer"
        source: win.s.playbackUrl
        audioOutput: AudioOutput {}
        videoOutput: videoOutput
        onErrorOccurred: function (error, errorString) {
            playbackError.text = errorString;
            playbackError.open();
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
        defaultSuffix: win.exportProfile === "webm" ? "webm" : "mp4"
        nameFilters: win.exportProfile === "webm" ? ["WebM (*.webm)"] : ["MP4 (*.mp4)"]
        onAccepted: editor.exportVideo(selectedFile, win.exportProfile)
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
        anchors.centerIn: parent
        title: "Export video"
        modal: true
        width: 460
        standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Label {
                text: win.s.width + " × " + win.s.height + " · " + win.s.fps.toFixed(2) + " fps · " + (win.s.duration / win.s.fps).toFixed(2) + " seconds"
            }
            ComboBox {
                id: codec
                Layout.fillWidth: true
                model: ["MP4 · MPEG-4 + AAC (portable default)", "WebM · VP9 + Opus", "MP4 · H.264 via Windows Media Foundation"]
                currentIndex: 0
            }
            Label {
                text: "H.264 requires an available Windows encoder. This alpha exports SDR video and stereo audio. It renders an immutable snapshot of your current timeline."
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                color: win.muted
            }
        }
        onAccepted: {
            win.exportProfile = ["mpeg4", "webm", "h264"][codec.currentIndex];
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
        title: "Cutlery · 0.3.0 alpha"
        modal: true
        width: 490
        standardButtons: Dialog.Ok
        Label {
            width: parent.width
            text: "A local desktop editor built around a shared FFmpeg render graph.\n\nImport media, arrange up to 64 tracks, drag clip edges to trim, add titles/captions, adjust picture and sound, render playback, and export. Higher tracks appear above lower tracks.\n\nPlayback is cached; Play or Space renders automatically when needed. Track buttons lock edits, mute or solo audio, and hide picture. See Help → Keyboard shortcuts to customize keys. This alpha does not yet include a real-time D3D11 engine, automatic captions, keyframes, masks, tracking, or the full design roadmap.\n\nProject files reference your original media. Keep those files alongside the project. Recovery data: " + win.s.dataPath
            wrapMode: Text.Wrap
            lineHeight: 1.3
        }
    }
    MessageDialog {
        id: playbackError
        title: "Playback error"
    }
}
