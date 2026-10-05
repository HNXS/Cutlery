import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

FocusScope {
    id: root
    property var state: editor.state
    property real pixelsPerSecond: 48
    property real playbackFrame: -1
    property bool snapping: true
    property real snapGuide: -1
    readonly property int rowHeight: 86
    readonly property int labelWidth: 174
    property int renameIndex: -1
    property int markerIndex: -1
    readonly property bool dialogOpen: renameDialog.visible || markerDialog.visible
    property var draggingClip: null
    property real pointerX: 0
    property real pointerY: 0
    property bool dropHover: false
    property int dropTrack: -1
    property real dropRaw: 0
    property real dropFrame: 0
    property real dropDuration: 90
    property string dropAsset: ""
    property bool dropsEnabled: true
    readonly property bool dropValid: dropTrack >= 0 && dropTrack < state.tracks && !(editor.trackList[dropTrack] || {}).locked
    signal seekRequested(real frame)
    function trackAt(y) {
        return state.tracks - 1 - Math.floor((y + timeline.contentY) / rowHeight);
    }
    function positionFor(frame, track, id, length) {
        if (track < 0 || track >= state.tracks)
            return Math.max(0, Math.round(frame));
        if ((editor.trackList[track] || {}).magnetic) {
            const p = editor.placement(track, Math.round(frame), id);
            snapGuide = p;
            return p;
        }
        return snapped(frame, id, length, track);
    }
    function updateDrop() {
        dropTrack = trackAt(pointerY);
        dropRaw = Math.max(0, (pointerX + timeline.contentX) / pixelsPerSecond * state.fps);
        dropFrame = positionFor(dropRaw, dropTrack, "", dropDuration);
    }
    function cancelDrag() {
        if (draggingClip)
            draggingClip.operation = 0;
        clearDrag();
    }
    function clearDrag() {
        draggingClip = null;
        snapGuide = -1;
    }
    Timer {
        interval: 30
        repeat: true
        running: root.draggingClip !== null || root.dropHover
        onTriggered: {
            const dx = root.pointerX < 32 ? -14 : root.pointerX > timeline.width - 32 ? 14 : 0;
            const dy = root.pointerY < 20 ? -8 : root.pointerY > timeline.height - 20 ? 8 : 0;
            if (!dx && !dy)
                return;
            timeline.contentX = Math.max(0, Math.min(Math.max(0, timeline.contentWidth - timeline.width), timeline.contentX + dx));
            timeline.contentY = Math.max(0, Math.min(Math.max(0, timeline.contentHeight - timeline.height), timeline.contentY + dy));
            if (root.draggingClip)
                root.draggingClip.updateAt(root.pointerX + timeline.contentX, root.pointerY + timeline.contentY);
            else
                root.updateDrop();
        }
    }
    function fit() {
        pixelsPerSecond = Math.max(.25, Math.min(180, (timeline.width - 40) / Math.max(1, state.duration / state.fps)));
    }
    function zoom(factor) {
        pixelsPerSecond = Math.max(.25, Math.min(240, pixelsPerSecond * factor));
    }
    function reveal(frame) {
        const x = frame / state.fps * pixelsPerSecond;
        if (x < timeline.contentX)
            timeline.contentX = x;
        else if (x > timeline.contentX + timeline.width - 30)
            timeline.contentX = Math.max(0, x - timeline.width + 50);
    }
    function snapped(frame, id, length, track) {
        const raw = Math.round(frame);
        const result = snapping && (editor.trackList[track] || {}).snapping ? editor.snap(raw, Math.ceil(7 / pixelsPerSecond * state.fps), id, length || 0) : raw;
        snapGuide = result !== raw ? result : -1;
        return result;
    }
    component Tool: Button {
        implicitHeight: 29
        padding: 8
        background: Rectangle {
            color: parent.down ? "#36444d" : parent.hovered ? "#2e3c46" : "#232d36"
            radius: 5
            border.color: "#3a4853"
            opacity: parent.enabled ? 1 : .4
        }
        contentItem: Text {
            text: parent.text
            font: parent.font
            color: parent.enabled ? "#e5edf2" : "#6c7780"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
    Rectangle {
        anchors.fill: parent
        color: "#151b22"
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 10
            spacing: 8
            Label {
                text: "TIMELINE"
                font.pixelSize: 10
                font.letterSpacing: 1.3
                color: "#8c9aa8"
            }
            Tool {
                text: "+ Track"
                enabled: root.state.tracks < 64
                onClicked: editor.addTrack()
            }
            CheckBox {
                text: "Edge snap"
                checked: root.snapping
                onToggled: root.snapping = checked
                ToolTip.visible: hovered
                ToolTip.text: "Snap to clip edges, the playhead and timeline start"
            }
            Label {
                visible: root.state.analyzing
                text: "Reading thumbnails and waveforms…"
                color: "#8c9aa8"
                font.pixelSize: 10
            }
            Item {
                Layout.fillWidth: true
            }
            Tool {
                text: "Split"
                enabled: root.state.selectedId.length > 0 && !root.state.selected.locked
                onClicked: editor.split()
            }
            Tool {
                text: "Delete"
                enabled: root.state.selectedId.length > 0 && !root.state.selected.locked
                onClicked: editor.remove(false)
            }
            Tool {
                text: "Fit"
                onClicked: root.fit()
                ToolTip.visible: hovered
                ToolTip.text: "Fit the whole edit in the timeline"
            }
            Tool {
                text: "−"
                onClicked: root.zoom(1 / 1.4)
            }
            Slider {
                from: Math.log(.25)
                to: Math.log(240)
                value: Math.log(root.pixelsPerSecond)
                Layout.preferredWidth: 110
                onMoved: root.pixelsPerSecond = Math.exp(value)
            }
            Tool {
                text: "+"
                onClicked: root.zoom(1.4)
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 0
            Layout.preferredHeight: 27
            Layout.minimumHeight: 27
            Layout.maximumHeight: 27
            Layout.fillHeight: false
            Label {
                text: "  HIGHER TRACKS ON TOP"
                Layout.preferredWidth: root.labelWidth
                color: "#72808d"
                font.pixelSize: 8
                font.letterSpacing: .7
            }
            Item {
                id: ruler
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                readonly property real tick: root.pixelsPerSecond >= 40 ? 1 : root.pixelsPerSecond >= 8 ? 5 : root.pixelsPerSecond >= 2 ? 30 : 120
                readonly property int first: Math.floor(timeline.contentX / root.pixelsPerSecond / tick)
                Repeater {
                    model: Math.ceil(ruler.width / root.pixelsPerSecond / ruler.tick) + 2
                    Item {
                        required property int index
                        x: (ruler.first + index) * ruler.tick * root.pixelsPerSecond - timeline.contentX
                        height: ruler.height
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: 1
                            height: 6
                            color: "#657583"
                        }
                        Label {
                            x: 5
                            y: 3
                            text: {
                                const s = (ruler.first + parent.index) * ruler.tick;
                                return s < 60 ? s + "s" : Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0");
                            }
                            font.pixelSize: 9
                            color: "#8c9aa8"
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onPressed: function (mouse) {
                        root.forceActiveFocus();
                        root.seekRequested(Math.round((mouse.x + timeline.contentX) / root.pixelsPerSecond * root.state.fps));
                    }
                    onPositionChanged: function (mouse) {
                        if (pressed)
                            root.seekRequested(Math.round((mouse.x + timeline.contentX) / root.pixelsPerSecond * root.state.fps));
                    }
                }
                // In/out range: a band along the bottom of the ruler.
                Rectangle {
                    objectName: "inOutBand"
                    visible: root.state.inPoint >= 0 || root.state.outPoint >= 0
                    readonly property real from: Math.max(0, root.state.inPoint) / root.state.fps * root.pixelsPerSecond - timeline.contentX
                    readonly property real to: (root.state.outPoint >= 0 ? root.state.outPoint : root.state.duration) / root.state.fps * root.pixelsPerSecond - timeline.contentX
                    x: from
                    width: Math.max(2, to - from)
                    y: parent.height - 5
                    height: 5
                    color: "#5fa8ff"
                    opacity: 0.8
                }
                // Markers: click to jump, double-click to rename, right-click to remove.
                Repeater {
                    model: root.state.markers || []
                    Rectangle {
                        required property var modelData
                        required property int index
                        objectName: "marker-" + index
                        x: modelData.frame / root.state.fps * root.pixelsPerSecond - timeline.contentX - 5
                        y: 1
                        width: 11
                        height: 11
                        radius: 2
                        rotation: 45
                        color: modelData.color
                        border.color: "#14181d"
                        MouseArea {
                            id: markerMouse
                            anchors.fill: parent
                            anchors.margins: -3
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: function (mouse) {
                                if (mouse.button === Qt.RightButton)
                                    editor.removeMarker(parent.index);
                                else
                                    root.seekRequested(parent.modelData.frame);
                            }
                            onDoubleClicked: {
                                root.markerIndex = parent.index;
                                markerName.text = parent.modelData.name;
                                markerDialog.open();
                            }
                        }
                        ToolTip.visible: markerMouse.containsMouse
                        ToolTip.text: modelData.name + " · double-click to rename, right-click to remove"
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Item {
                Layout.preferredWidth: root.labelWidth
                Layout.fillHeight: true
                clip: true
                Column {
                    y: -timeline.contentY
                    width: parent.width
                    Repeater {
                        model: root.state.tracks
                        Rectangle {
                            required property int index
                            readonly property int trackIndex: root.state.tracks - 1 - index
                            property var track: editor.trackList[trackIndex] || ({})
                            width: root.labelWidth
                            height: root.rowHeight
                            color: track.locked ? "#27272e" : "#1c252e"
                            border.color: "#33404b"
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 2
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        text: parent.parent.parent.track.name || "Track"
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                        font.pixelSize: 11
                                        font.bold: true
                                        MouseArea {
                                            anchors.fill: parent
                                            onDoubleClicked: {
                                                root.renameIndex = trackIndex;
                                                trackName.text = track.name;
                                                renameDialog.open();
                                            }
                                        }
                                    }
                                    ToolButton {
                                        text: "⋯"
                                        implicitHeight: 22
                                        implicitWidth: 25
                                        onClicked: trackMenu.open()
                                        Menu {
                                            id: trackMenu
                                            MenuItem {
                                                text: "Rename track…"
                                                onTriggered: {
                                                    root.renameIndex = trackIndex;
                                                    trackName.text = track.name;
                                                    renameDialog.open();
                                                }
                                            }
                                            MenuItem {
                                                text: "Remove empty track"
                                                enabled: !track.locked && root.state.tracks > 1
                                                onTriggered: editor.removeTrack(trackIndex)
                                            }
                                        }
                                    }
                                }
                                Row {
                                    spacing: 3
                                    Repeater {
                                        model: [
                                            {
                                                key: "locked",
                                                label: "L",
                                                tip: "Lock editing"
                                            },
                                            {
                                                key: "muted",
                                                label: "M",
                                                tip: "Mute track audio"
                                            },
                                            {
                                                key: "solo",
                                                label: "S",
                                                tip: "Solo track audio"
                                            },
                                            {
                                                key: "hidden",
                                                label: "V",
                                                tip: "Hide track picture"
                                            }
                                        ]
                                        Button {
                                            required property var modelData
                                            width: 33
                                            height: 24
                                            text: modelData.label
                                            checkable: true
                                            checked: track[modelData.key] || false
                                            onClicked: editor.setTrack(trackIndex, modelData.key, checked)
                                            background: Rectangle {
                                                radius: 4
                                                color: parent.checked ? "#64d8bc" : "#303d47"
                                            }
                                            contentItem: Text {
                                                text: parent.text
                                                color: parent.checked ? "#10241f" : "#c3d0d9"
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                                font.pixelSize: 10
                                                font.bold: true
                                            }
                                            ToolTip.visible: hovered
                                            ToolTip.text: modelData.tip
                                        }
                                    }
                                }
                                Row {
                                    spacing: 4
                                    Repeater {
                                        model: [
                                            {
                                                key: "snapping",
                                                label: "Snap",
                                                tip: "Align edges while dragging on this track (Edge snap must be on)"
                                            },
                                            {
                                                key: "magnetic",
                                                label: "Magnet",
                                                tip: "Keep clips together from frame 0. Enabling closes existing gaps and overlaps; Undo restores them."
                                            }
                                        ]
                                        Button {
                                            required property var modelData
                                            objectName: modelData.key + "-" + trackIndex
                                            width: 69
                                            height: 22
                                            text: modelData.label
                                            checkable: true
                                            checked: track[modelData.key] || false
                                            enabled: modelData.key !== "magnetic" || !track.locked
                                            onClicked: editor.setTrack(trackIndex, modelData.key, checked)
                                            background: Rectangle {
                                                radius: 4
                                                color: parent.checked ? "#356457" : "#303d47"
                                                border.color: parent.checked ? "#64d8bc" : "#44525c"
                                            }
                                            ToolTip.visible: hovered
                                            ToolTip.text: modelData.tip
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Item {
                id: trackViewport
                objectName: "trackViewport"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                Flickable {
                    id: timeline
                    objectName: "timelineScroll"
                    anchors.fill: parent
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    contentWidth: Math.max(width, (Math.max(root.state.duration, root.draggingClip ? root.draggingClip.dragEnd : root.dropHover ? root.dropFrame + root.dropDuration : 0) / root.state.fps + 5) * root.pixelsPerSecond)
                    contentHeight: root.state.tracks * root.rowHeight
                    ScrollBar.horizontal: ScrollBar {}
                    ScrollBar.vertical: ScrollBar {}
                    Item {
                        id: body
                        width: timeline.contentWidth
                        height: timeline.contentHeight
                        objectName: "timelineBody"
                        Repeater {
                            model: root.state.tracks
                            Rectangle {
                                required property int index
                                y: index * root.rowHeight
                                width: body.width
                                height: root.rowHeight
                                color: index % 2 ? "#141c24" : "#17212a"
                                border.color: "#293540"
                            }
                        }
                        // Empty timeline: a click moves the playhead; dragging draws a rectangle that
                        // selects the clips it touches (Ctrl adds to the selection).
                        MouseArea {
                            id: emptyArea
                            objectName: "timelineEmpty"
                            anchors.fill: parent
                            property point origin
                            property bool banding: false
                            preventStealing: true
                            onPressed: function (mouse) {
                                root.forceActiveFocus();
                                origin = Qt.point(mouse.x, mouse.y);
                                banding = false;
                                root.seekRequested(Math.round(mouse.x / root.pixelsPerSecond * root.state.fps));
                            }
                            onPositionChanged: function (mouse) {
                                if (pressed && !banding && Math.abs(mouse.x - origin.x) + Math.abs(mouse.y - origin.y) > 8)
                                    banding = true;
                                if (banding) {
                                    band.x = Math.min(origin.x, mouse.x);
                                    band.y = Math.min(origin.y, mouse.y);
                                    band.width = Math.abs(mouse.x - origin.x);
                                    band.height = Math.abs(mouse.y - origin.y);
                                }
                            }
                            onReleased: function (mouse) {
                                if (!banding)
                                    return;
                                banding = false;
                                const from = Math.floor(band.x / root.pixelsPerSecond * root.state.fps);
                                const to = Math.ceil((band.x + band.width) / root.pixelsPerSecond * root.state.fps);
                                const high = root.state.tracks - 1 - Math.floor(band.y / root.rowHeight);
                                const low = root.state.tracks - 1 - Math.floor((band.y + band.height) / root.rowHeight);
                                editor.selectArea(from, Math.max(from + 1, to), low, high, (mouse.modifiers & Qt.ControlModifier) !== 0);
                            }
                        }
                        Rectangle {
                            id: band
                            objectName: "selectionBand"
                            visible: emptyArea.banding
                            z: 20
                            color: "#3364d8bc"
                            border.color: "#64d8bc"
                        }
                        Repeater {
                            model: editor.clips
                            Rectangle {
                                id: clipRect
                                required property var modelData
                                objectName: "clip-" + modelData.id
                                // 1 move, 2 start trim, 3 end trim; with Alt: 4 slip, 5 slide (Alt+Shift),
                                // 6 roll the end cut, 7 roll the start cut.
                                property int operation: 0
                                property real grabX: 0
                                property int editFrames: 0
                                property real grabOffset: 0
                                property real requestedStart: modelData.start
                                property real dragStart: modelData.start
                                property real dragEnd: modelData.start + modelData.duration
                                property int dragTrack: modelData.track
                                property var bounds: ({
                                        first: 0,
                                        last: 100000000
                                    })
                                property var wave: ({})
                                property var thumbs: ({})
                                readonly property real shownStart: operation ? dragStart : modelData.start
                                readonly property real shownEnd: operation ? dragEnd : modelData.start + modelData.duration
                                function refreshThumbs() {
                                    thumbs = modelData.title || modelData.effect || modelData.audio ? ({}) : editor.thumbnails(modelData.assetId);
                                }
                                // Strip tile for a filmstrip slot: the source frame at the slot's centre,
                                // following trims (including live trim drags), speed and reverse.
                                function tileFor(slot) {
                                    const local = (shownStart - modelData.start) / root.state.fps + (slot + .5) * filmstrip.tileWidth / root.pixelsPerSecond;
                                    const clipSeconds = modelData.duration / root.state.fps;
                                    const source = modelData.sourceIn + (modelData.reverse ? clipSeconds - local : local) * modelData.speed;
                                    return Math.max(0, Math.min(thumbs.count - 1, Math.floor(source / thumbs.interval)));
                                }
                                function refreshWave() {
                                    wave = modelData.hasAudio ? editor.waveform(modelData.assetId) : ({});
                                    waveform.requestPaint();
                                }
                                function begin(mode, mouse, area) {
                                    root.forceActiveFocus();
                                    // Ctrl+click adds or removes the clip from the selection.
                                    if (mouse.modifiers & Qt.ControlModifier) {
                                        editor.toggleSelect(modelData.id);
                                        return;
                                    }
                                    // A clip that is already selected keeps the others selected.
                                    if ((root.state.selectedIds || []).indexOf(modelData.id) < 0)
                                        editor.select(modelData.id);
                                    if (modelData.locked)
                                        return;
                                    if (mouse.modifiers & Qt.AltModifier)
                                        mode = mode === 1 ? (mouse.modifiers & Qt.ShiftModifier ? 5 : 4) : mode === 3 ? 6 : 7;
                                    operation = mode;
                                    editFrames = 0;
                                    grabX = area.mapToItem(body, mouse.x, mouse.y).x;
                                    dragStart = modelData.start;
                                    dragEnd = modelData.start + modelData.duration;
                                    dragTrack = modelData.track;
                                    requestedStart = modelData.start;
                                    root.draggingClip = clipRect;
                                    const point = area.mapToItem(timeline, mouse.x, mouse.y);
                                    root.pointerX = point.x;
                                    root.pointerY = point.y;
                                    bounds = editor.trimBounds(modelData.id);
                                    grabOffset = area.mapToItem(body, mouse.x, mouse.y).x - (mode === 3 ? dragEnd : dragStart) / root.state.fps * root.pixelsPerSecond;
                                }
                                function update(mouse, area) {
                                    if (!operation)
                                        return;
                                    const point = area.mapToItem(timeline, mouse.x, mouse.y);
                                    root.pointerX = point.x;
                                    root.pointerY = point.y;
                                    updateAt(point.x + timeline.contentX, point.y + timeline.contentY);
                                }
                                function updateAt(x, y) {
                                    if (operation >= 4) {
                                        editFrames = Math.round((x - grabX) / root.pixelsPerSecond * root.state.fps);
                                        const start = modelData.start, end = modelData.start + modelData.duration;
                                        dragStart = operation === 5 ? start + editFrames : operation === 7 ? Math.min(end - 1, start + editFrames) : start;
                                        dragEnd = operation === 5 ? end + editFrames : operation === 6 ? Math.max(start + 1, end + editFrames) : end;
                                        return;
                                    }
                                    if (operation === 1) {
                                        dragTrack = root.state.tracks - 1 - Math.floor(y / root.rowHeight);
                                        requestedStart = Math.max(0, Math.round((x - grabOffset) / root.pixelsPerSecond * root.state.fps));
                                        dragStart = root.positionFor(requestedStart, dragTrack, modelData.id, modelData.duration);
                                        dragEnd = dragStart + modelData.duration;
                                    } else if (operation === 2)
                                        dragStart = Math.max(bounds.first, Math.min(modelData.start + modelData.duration - 1, root.snapped((x - grabOffset) / root.pixelsPerSecond * root.state.fps, modelData.id, 0, modelData.track)));
                                    else if (operation === 3)
                                        dragEnd = Math.max(modelData.start + 1, Math.min(bounds.last, root.snapped((x - grabOffset) / root.pixelsPerSecond * root.state.fps, modelData.id, 0, modelData.track)));
                                }
                                function commit() {
                                    const id = modelData.id, mode = operation, start = dragStart, end = dragEnd, track = dragTrack;
                                    const frame = (editor.trackList[track] || {}).magnetic ? requestedStart : start;
                                    const frames = editFrames;
                                    operation = 0;
                                    root.clearDrag();
                                    if (mode === 4)
                                        // Dragging right shows earlier source, as if pulling the film along.
                                        editor.slipClip(id, -frames);
                                    else if (mode === 5)
                                        editor.slideClip(id, frames);
                                    else if (mode === 6)
                                        editor.rollCut(id, frames);
                                    else if (mode === 7) {
                                        const before = editor.clips.find(c => c.track === modelData.track && c.start + c.duration === modelData.start);
                                        if (before)
                                            editor.rollCut(before.id, frames);
                                    } else if (mode === 1 && track >= 0 && track < root.state.tracks && !(editor.trackList[track] || {}).locked)
                                        editor.moveClip(id, frame, track);
                                    else if (mode === 2 || mode === 3)
                                        editor.trimClip(id, start, end);
                                }
                                x: shownStart / root.state.fps * root.pixelsPerSecond
                                y: 4 + (root.state.tracks - 1 - (operation === 1 ? dragTrack : modelData.track)) * root.rowHeight
                                width: Math.max(8, (shownEnd - shownStart) / root.state.fps * root.pixelsPerSecond)
                                height: root.rowHeight - 8
                                radius: 5
                                clip: true
                                color: modelData.effect ? "#4d3f66" : modelData.title ? "#59453e" : modelData.audio ? "#28564c" : "#334a65"
                                opacity: modelData.locked ? .65 : 1
                                readonly property bool chosen: (root.state.selectedIds || []).indexOf(modelData.id) >= 0
                                border.width: chosen ? 2 : 1
                                border.color: operation === 1 && (dragTrack < 0 || dragTrack >= root.state.tracks || (editor.trackList[dragTrack] || {}).locked) ? "#ec947e" : root.state.selectedId === modelData.id ? "#64d8bc" : chosen ? "#b7f0e1" : "#6481a0"
                                Component.onCompleted: {
                                    refreshWave();
                                    refreshThumbs();
                                }
                                Component.onDestruction: {
                                    if (root.draggingClip === clipRect)
                                        root.clearDrag();
                                }
                                Connections {
                                    target: editor
                                    function onAnalysisChanged() {
                                        clipRect.refreshWave();
                                    }
                                    function onThumbnailsChanged() {
                                        clipRect.refreshThumbs();
                                    }
                                }
                                Item {
                                    id: filmstrip
                                    objectName: "filmstrip-" + clipRect.modelData.id
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    clip: true
                                    visible: clipRect.thumbs.status === "ready"
                                    readonly property real tileWidth: visible ? clipRect.thumbs.tileWidth * height / clipRect.thumbs.tileHeight : 0
                                    // Only tiles inside the visible part of the timeline are created.
                                    readonly property real visibleFrom: Math.max(0, timeline.contentX - clipRect.x)
                                    readonly property real visibleTo: Math.min(width, timeline.contentX + timeline.width - clipRect.x)
                                    readonly property int firstTile: tileWidth > 0 ? Math.floor(visibleFrom / tileWidth) : 0
                                    Repeater {
                                        model: filmstrip.tileWidth > 0 ? Math.max(0, Math.ceil(filmstrip.visibleTo / filmstrip.tileWidth) - filmstrip.firstTile) : 0
                                        Image {
                                            required property int index
                                            readonly property int slot: filmstrip.firstTile + index
                                            x: slot * filmstrip.tileWidth
                                            width: filmstrip.tileWidth
                                            height: filmstrip.height
                                            source: clipRect.thumbs.url
                                            sourceClipRect: Qt.rect(clipRect.tileFor(slot) * clipRect.thumbs.tileWidth, 0, clipRect.thumbs.tileWidth, clipRect.thumbs.tileHeight)
                                            fillMode: Image.PreserveAspectCrop
                                            asynchronous: true
                                        }
                                    }
                                    // Keeps the clip name and waveform legible over bright footage.
                                    Rectangle {
                                        anchors.fill: parent
                                        color: "#0b1016"
                                        opacity: clipRect.modelData.hidden ? .7 : .3
                                    }
                                }
                                Label {
                                    x: 10
                                    y: 7
                                    width: parent.width - 20
                                    elide: Text.ElideRight
                                    font.pixelSize: 11
                                    font.bold: true
                                    text: (clipRect.modelData.locked ? "[L] " : "") + (clipRect.modelData.linked ? "⛓ " : "") + (clipRect.modelData.grouped ? "▣ " : "") + clipRect.modelData.name
                                }
                                Label {
                                    objectName: "editLabel-" + clipRect.modelData.id
                                    visible: clipRect.operation >= 4
                                    anchors.right: parent.right
                                    anchors.rightMargin: 8
                                    y: 7
                                    font.pixelSize: 10
                                    color: "#ffd479"
                                    text: ["Slip", "Slide", "Roll", "Roll"][Math.max(0, clipRect.operation - 4)] + " " + (clipRect.editFrames > 0 ? "+" : "") + clipRect.editFrames + " f"
                                }
                                Label {
                                    x: 10
                                    y: 33
                                    visible: !clipRect.modelData.hasAudio || clipRect.wave.status !== "ready"
                                    text: ((clipRect.shownEnd - clipRect.shownStart) / root.state.fps).toFixed(2) + " s" + (clipRect.wave.status === "reading" ? " · waveform…" : "")
                                    color: "#bcc9d5"
                                    font.pixelSize: 9
                                }
                                Canvas {
                                    id: waveform
                                    x: Math.max(0, timeline.contentX - clipRect.x)
                                    y: 27
                                    width: Math.max(0, Math.min(clipRect.width - x, timeline.width))
                                    height: 26
                                    visible: clipRect.modelData.hasAudio && clipRect.wave.status === "ready"
                                    onXChanged: requestPaint()
                                    onWidthChanged: requestPaint()
                                    onPaint: {
                                        const ctx = getContext("2d");
                                        ctx.reset();
                                        const peaks = clipRect.wave.peaks || [];
                                        const step = clipRect.wave.step;
                                        if (!step || !peaks.length)
                                            return;
                                        ctx.fillStyle = clipRect.modelData.muted ? "#7c8b91" : "#81e6c9";
                                        const speed = clipRect.modelData.speed, duration = (clipRect.shownEnd - clipRect.shownStart) / root.state.fps;
                                        const delta = clipRect.modelData.reverse ? clipRect.modelData.start + clipRect.modelData.duration - clipRect.shownEnd : clipRect.shownStart - clipRect.modelData.start;
                                        const start = clipRect.modelData.sourceIn + delta / root.state.fps * speed;
                                        for (let px = 0; px < width; px += 2) {
                                            let a = (x + px) / root.pixelsPerSecond, b = (x + px + 2) / root.pixelsPerSecond;
                                            if (clipRect.modelData.reverse) {
                                                const prev = a;
                                                a = duration - b;
                                                b = duration - prev;
                                            }
                                            let lo = Math.max(0, Math.floor((start + a * speed) / step)), hi = Math.min(peaks.length - 1, Math.floor((start + b * speed) / step)), peak = 0;
                                            for (let i = lo; i <= hi; ++i)
                                                peak = Math.max(peak, peaks[i]);
                                            const h = Math.max(1, peak * height);
                                            ctx.fillRect(px, (height - h) / 2, 1.5, h);
                                        }
                                    }
                                    Connections {
                                        target: root
                                        function onPixelsPerSecondChanged() {
                                            waveform.requestPaint();
                                        }
                                    }
                                    Connections {
                                        target: clipRect
                                        function onShownStartChanged() {
                                            waveform.requestPaint();
                                        }
                                        function onShownEndChanged() {
                                            waveform.requestPaint();
                                        }
                                    }
                                }
                                MouseArea {
                                    id: moveArea
                                    anchors.fill: parent
                                    preventStealing: true
                                    cursorShape: clipRect.modelData.locked ? Qt.ForbiddenCursor : (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor)
                                    onPressed: function (mouse) {
                                        clipRect.begin(1, mouse, moveArea);
                                    }
                                    onPositionChanged: function (mouse) {
                                        if (pressed)
                                            clipRect.update(mouse, moveArea);
                                    }
                                    onReleased: clipRect.commit()
                                    onCanceled: {
                                        clipRect.operation = 0;
                                        root.clearDrag();
                                    }
                                    onDoubleClicked: root.seekRequested(clipRect.modelData.start)
                                }
                                // Keyframes: click to jump the playhead there.
                                Repeater {
                                    model: clipRect.modelData.keyframes
                                    Rectangle {
                                        required property var modelData
                                        objectName: "keyframe-" + clipRect.modelData.id + "-" + modelData
                                        x: (modelData - (clipRect.shownStart - clipRect.modelData.start)) / root.state.fps * root.pixelsPerSecond - width / 2
                                        y: clipRect.height - 13
                                        width: 9
                                        height: 9
                                        rotation: 45
                                        color: "#ffd479"
                                        border.color: "#0b1016"
                                        visible: x > -width && x < clipRect.width
                                        MouseArea {
                                            anchors.fill: parent
                                            anchors.margins: -3
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                root.forceActiveFocus();
                                                editor.select(clipRect.modelData.id);
                                                root.seekRequested(clipRect.modelData.start + parent.modelData);
                                            }
                                        }
                                    }
                                }
                                Repeater {
                                    model: 2
                                    Rectangle {
                                        required property int index
                                        objectName: (index === 0 ? "trimStart-" : "trimEnd-") + clipRect.modelData.id
                                        x: index === 0 ? 1 : clipRect.width - width - 1
                                        y: 3
                                        width: Math.min(9, clipRect.width / 3)
                                        height: clipRect.height - 6
                                        radius: 3
                                        color: trimArea.containsMouse ? "#64d8bc" : "transparent"
                                        visible: !clipRect.modelData.locked
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 2
                                            height: 16
                                            color: "#b2d2d3"
                                        }
                                        MouseArea {
                                            id: trimArea
                                            anchors.fill: parent
                                            preventStealing: true
                                            hoverEnabled: true
                                            cursorShape: Qt.SizeHorCursor
                                            onPressed: function (mouse) {
                                                clipRect.begin(index === 0 ? 2 : 3, mouse, trimArea);
                                            }
                                            onPositionChanged: function (mouse) {
                                                if (pressed)
                                                    clipRect.update(mouse, trimArea);
                                            }
                                            onReleased: clipRect.commit()
                                            onCanceled: {
                                                clipRect.operation = 0;
                                                root.clearDrag();
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        // Cuts between touching clips: "+" adds a dissolve; an existing transition
                        // shows its span across the cut. Clicking selects the incoming clip.
                        Repeater {
                            model: editor.clips
                            Item {
                                id: cut
                                required property var modelData
                                readonly property real cutX: modelData.start / root.state.fps * root.pixelsPerSecond
                                readonly property real rowY: 4 + (root.state.tracks - 1 - modelData.track) * root.rowHeight
                                readonly property bool active: modelData.transitionLength > 0
                                visible: modelData.canTransition && !modelData.audio && root.draggingClip === null
                                Rectangle {
                                    visible: cut.active
                                    x: cut.cutX - Math.floor(cut.modelData.transitionLength / 2) / root.state.fps * root.pixelsPerSecond
                                    y: cut.rowY
                                    width: Math.max(2, cut.modelData.transitionLength / root.state.fps * root.pixelsPerSecond)
                                    height: root.rowHeight - 8
                                    color: "#64d8bc"
                                    opacity: .22
                                    radius: 4
                                }
                                Rectangle {
                                    objectName: "transitionMarker-" + cut.modelData.id
                                    // On the top edge of the clips, so the edges below stay free for trims.
                                    x: cut.cutX - width / 2
                                    y: cut.rowY - height / 2
                                    width: 16
                                    height: 14
                                    radius: 4
                                    color: cut.active ? "#64d8bc" : markerMouse.containsMouse ? "#34434d" : "#202831"
                                    border.color: cut.active ? "#e7edf2" : "#6481a0"
                                    Label {
                                        anchors.centerIn: parent
                                        text: cut.active ? "⧓" : "+"
                                        color: cut.active ? "#0b1016" : "#e7edf2"
                                        font.pixelSize: cut.active ? 9 : 12
                                        font.bold: true
                                    }
                                    MouseArea {
                                        id: markerMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            root.forceActiveFocus();
                                            editor.select(cut.modelData.id);
                                            if (!cut.modelData.transition && !cut.modelData.locked)
                                                editor.setClip("transition", "fade");
                                        }
                                    }
                                    ToolTip.visible: markerMouse.containsMouse
                                    ToolTip.text: cut.active ? "Transition · edit it in the inspector" : "Add a dissolve at this cut"
                                }
                            }
                        }
                        Rectangle {
                            x: (root.playbackFrame >= 0 ? root.playbackFrame : root.state.playhead) / root.state.fps * root.pixelsPerSecond
                            width: 2
                            height: parent.height
                            color: "#64d8bc"
                            z: 10
                        }
                        // Markers and in/out points as thin lines across the tracks.
                        Repeater {
                            model: root.state.markers || []
                            Rectangle {
                                required property var modelData
                                x: modelData.frame / root.state.fps * root.pixelsPerSecond
                                width: 1
                                height: parent.height
                                color: modelData.color
                                opacity: 0.55
                                z: 9
                            }
                        }
                        Repeater {
                            model: [root.state.inPoint, root.state.outPoint]
                            Rectangle {
                                required property var modelData
                                visible: modelData >= 0
                                x: modelData / root.state.fps * root.pixelsPerSecond
                                width: 1
                                height: parent.height
                                color: "#5fa8ff"
                                z: 9
                            }
                        }
                        Rectangle {
                            visible: root.snapGuide >= 0
                            x: root.snapGuide / root.state.fps * root.pixelsPerSecond
                            width: 1
                            height: parent.height
                            color: "#ffd19c"
                            z: 11
                        }
                    }
                }
                DropArea {
                    id: trackDrop
                    anchors.fill: parent
                    enabled: root.dropsEnabled
                    function updateEvent(event) {
                        root.pointerX = event.x;
                        root.pointerY = event.y;
                        root.dropAsset = event.source && event.source.assetId ? event.source.assetId : "";
                        root.dropDuration = root.dropAsset.length ? Math.max(1, Math.floor(event.source.mediaDuration * root.state.fps)) : 3 * root.state.fps;
                        root.dropHover = true;
                        root.updateDrop();
                    }
                    onEntered: function (drag) {
                        drag.accepted = drag.hasUrls || !!(drag.source && drag.source.assetId);
                        if (drag.accepted)
                            updateEvent(drag);
                    }
                    onPositionChanged: function (drag) {
                        updateEvent(drag);
                    }
                    onExited: {
                        root.dropHover = false;
                        root.snapGuide = -1;
                    }
                    onDropped: function (drop) {
                        updateEvent(drop);
                        const track = root.dropTrack, asset = root.dropAsset;
                        const frame = (editor.trackList[track] || {}).magnetic ? Math.round(root.dropRaw) : root.dropFrame;
                        if (root.dropValid) {
                            if (asset.length)
                                Qt.callLater(function () {
                                    editor.insertAsset(asset, track, frame);
                                });
                            else if (drop.hasUrls)
                                editor.dropFiles(drop.urls, track, frame);
                            drop.accept(Qt.CopyAction);
                        } else
                            drop.accepted = false;
                        root.dropHover = false;
                        root.snapGuide = -1;
                        root.forceActiveFocus();
                    }
                }
                Rectangle {
                    visible: root.dropHover
                    x: root.dropFrame / root.state.fps * root.pixelsPerSecond - timeline.contentX
                    y: (root.state.tracks - 1 - root.dropTrack) * root.rowHeight - timeline.contentY + 3
                    width: Math.max(30, root.dropDuration / root.state.fps * root.pixelsPerSecond)
                    height: root.rowHeight - 6
                    radius: 5
                    color: root.dropValid ? "#553c8070" : "#556b3333"
                    border.width: 2
                    border.color: root.dropValid ? "#64d8bc" : "#ff9d89"
                    Label {
                        x: 8
                        y: 8
                        text: root.dropValid ? ((editor.trackList[root.dropTrack] || {}).magnetic ? "Insert here · Magnet" : "Drop here") : "Track locked"
                    }
                }
            }
        }
    }
    Dialog {
        id: renameDialog
        anchors.centerIn: parent
        title: "Rename track"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 320
        TextField {
            id: trackName
            width: parent.width
            maximumLength: 80
            selectByMouse: true
        }
        onAccepted: editor.setTrack(root.renameIndex, "name", trackName.text)
    }
    Dialog {
        id: markerDialog
        objectName: "markerDialog"
        anchors.centerIn: parent
        title: "Marker"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 320
        ColumnLayout {
            width: parent.width
            TextField {
                id: markerName
                objectName: "markerName"
                Layout.fillWidth: true
                maximumLength: 200
                selectByMouse: true
                onAccepted: markerDialog.accept()
            }
            RowLayout {
                Repeater {
                    model: ["#ffd23f", "#ff5a5f", "#64d8bc", "#5fa8ff", "#c38bff"]
                    Rectangle {
                        required property string modelData
                        width: 20
                        height: 20
                        radius: 10
                        color: modelData
                        border.width: ((root.state.markers || [])[root.markerIndex] || {}).color === modelData ? 3 : 1
                        border.color: "#e7edf2"
                        MouseArea {
                            anchors.fill: parent
                            onClicked: editor.setMarker(root.markerIndex, "color", parent.modelData)
                        }
                    }
                }
            }
        }
        onAccepted: editor.setMarker(root.markerIndex, "name", markerName.text)
    }
}
