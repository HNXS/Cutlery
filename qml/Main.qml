import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia

ApplicationWindow {
    id: win
    width: 1440; height: 930; minimumWidth: 1100; minimumHeight: 760
    visible: true
    color: "#111419"
    title: (s.dirty ? "• " : "") + s.name + " — Cutlery"
    font.family: Qt.platform.os === "windows" ? "Segoe UI" : "DejaVu Sans"
    font.pixelSize: 12
    palette.window: "#111419"; palette.windowText: "#e7edf2"; palette.text: "#e7edf2"
    palette.base: "#151b22"; palette.button: "#252e38"; palette.buttonText: "#e7edf2"
    palette.highlight: "#64d8bc"; palette.highlightedText: "#10241f"
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
    function clock(frame) {
        const fps = s.fps; const sec = Math.floor(frame / fps)
        return String(Math.floor(sec/60)).padStart(2,"0") + ":" + String(sec%60).padStart(2,"0") + ":" + String(Math.floor(frame%fps)).padStart(2,"0")
    }
    function guarded(action) { if(s.dirty) { pendingAction=action; discardDialog.open() } else runAction(action) }
    function runAction(action) {
        player.stop(); showPlayback=false
        if(action==="new") editor.newProject()
        else if(action==="open") openDialog.open()
        else if(action==="recover") editor.recover()
        else if(action==="close") { allowClose=true; win.close() }
    }
    function saveProject() { if(s.path.length) editor.save(); else saveDialog.open() }
    function play() {
        if(!s.playbackUrl.length) { editor.renderPlayback(); return }
        showPlayback=true
        if(player.playbackState===MediaPlayer.PlayingState) { player.pause(); editor.seek(Math.floor(player.position*s.fps/1000)) }
        else { player.position=Math.round(s.playhead/s.fps*1000); player.play() }
    }
    onClosing: function(close) {
        if(!allowClose && (s.dirty || s.busy)) { close.accepted=false; pendingAction="close"; discardDialog.open() }
    }
    Connections {
        target: editor
        function onChanged() {
            if(!win.s.playbackUrl.length) { player.stop(); win.showPlayback=false }
        }
    }
    component Action: Button {
        implicitHeight: 32
        padding: 12
        background: Rectangle { color: parent.down ? "#34434d" : parent.hovered ? "#2b3742" : "#202831"; radius: 6; border.color: "#35404b"; opacity: parent.enabled ? 1 : .4 }
        contentItem: Text { text: parent.text; color: parent.enabled ? "#e7edf2" : "#65707a"; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
    }
    component Caption: Label { color: win.muted; font.pixelSize: 10; font.letterSpacing: 1.3 }
    component Rule: Rectangle { Layout.fillWidth: true; height: 1; color: "#2b333e" }
    menuBar: MenuBar {
        Menu { title: "Project"
            MenuItem { text: "New"; onTriggered: win.guarded("new") }
            MenuItem { text: "Open…"; onTriggered: win.guarded("open") }
            MenuItem { text: "Save"; onTriggered: win.saveProject() }
            MenuItem { text: "Save As…"; onTriggered: saveDialog.open() }
            MenuSeparator {}
            MenuItem { text: "Project settings…"; onTriggered: settings.open() }
            MenuItem { text: "Recover autosave"; enabled: win.s.hasRecovery; onTriggered: win.guarded("recover") }
        }
        Menu { title: "Edit"
            MenuItem { text: "Undo"; enabled: win.s.canUndo; onTriggered: editor.undo() }
            MenuItem { text: "Redo"; enabled: win.s.canRedo; onTriggered: editor.redo() }
            MenuItem { text: "Split at playhead"; enabled: win.s.selectedId.length>0; onTriggered: editor.split() }
            MenuItem { text: "Duplicate"; enabled: win.s.selectedId.length>0; onTriggered: editor.duplicate() }
            MenuItem { text: "Delete and close gap on this track"; enabled: win.s.selectedId.length>0; onTriggered: editor.remove(true) }
        }
        Menu { title: "Captions"
            MenuItem { text: "Import SRT…"; onTriggered: srtOpen.open() }
            MenuItem { text: "Export titles/captions as SRT…"; onTriggered: srtSave.open() }
        }
        Menu { title: "Help"
            MenuItem { text: "About this alpha"; onTriggered: about.open() }
        }
    }
    Shortcut { sequences: [StandardKey.Save]; onActivated: win.saveProject() }
    Shortcut { sequences: [StandardKey.Open]; onActivated: win.guarded("open") }
    Shortcut { sequences: [StandardKey.Undo]; onActivated: editor.undo() }
    Shortcut { sequences: [StandardKey.Redo]; onActivated: editor.redo() }
    ColumnLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 64; color: "#171d24"
            RowLayout {
                anchors.fill: parent; anchors.margins: 16; spacing: 12
                Label { text: "CUTLERY"; font.pixelSize: 21; font.bold: true; font.letterSpacing: 3; color: win.mint }
                Rectangle { width: 1; Layout.fillHeight: true; color: "#34404a" }
                ColumnLayout { spacing: 2
                    Label { text: win.s.name; font.bold: true }
                    Label { text: win.s.width+" × "+win.s.height+"  /  "+win.s.fps.toFixed(2)+" fps"; color: win.muted; font.pixelSize: 10 }
                }
                Item { Layout.fillWidth: true }
                Action { text: "Undo"; enabled: win.s.canUndo; onClicked: editor.undo() }
                Action { text: "Redo"; enabled: win.s.canRedo; onClicked: editor.redo() }
                Action { text: "Save project"; onClicked: win.saveProject() }
                Action { text: "Export video ↗"; enabled: win.s.duration>0&&!win.s.busy; onClicked: exportSettings.open(); palette.buttonText: win.mint }
            }
        }
        Rectangle {
            Layout.fillWidth: true; visible: win.s.error.length>0; implicitHeight: errorText.implicitHeight+22; color: "#482c2c"
            RowLayout { anchors.fill: parent; anchors.margins: 10
                Label { id: errorText; text: win.s.error; Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#ffc2b7" }
                Action { text: "Dismiss"; onClicked: editor.clearError() }
            }
        }
        SplitView {
            Layout.fillWidth: true; Layout.fillHeight: true; orientation: Qt.Horizontal
            handle: Rectangle { implicitWidth: 1; color: "#303945" }
            Rectangle {
                SplitView.preferredWidth: 240; SplitView.minimumWidth: 190; color: "#171d24"
                ColumnLayout { anchors.fill: parent; anchors.margins: 16; spacing: 14
                    RowLayout { Layout.fillWidth: true
                        Caption { text: "MEDIA LIBRARY" }
                        Item { Layout.fillWidth: true }
                        Label { text: editor.assets.length; color: win.muted }
                    }
                    Action { text: win.s.importing ? "Reading media…" : "+ Import media"; Layout.fillWidth: true; enabled: !win.s.importing; onClicked: importDialog.open() }
                    Label { text: "Double-click media to append it.\nDrag clips in the timeline to arrange."; color: win.muted; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    RowLayout { Label { text: "Append to"; color: win.muted }
                        ComboBox { model: ["Track 1", "Track 2", "Track 3"]; currentIndex: win.targetTrack; onActivated: win.targetTrack=currentIndex; Layout.fillWidth: true }
                    }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8; model: editor.assets
                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width; height: 76; radius: 7; color: mediaMouse.containsMouse ? "#2a3640" : "#222b34"; border.color: modelData.missing ? "#da886e" : "#34404c"
                            RowLayout { anchors.fill: parent; anchors.margins: 10; spacing: 10
                                Rectangle { width: 38; height: 42; radius: 5; color: modelData.kind==="audio" ? "#344c4e" : "#354255"
                                    Label { anchors.centerIn: parent; text: modelData.kind==="audio" ? "♫" : modelData.kind==="image" ? "▧" : "▶"; color: win.mint; font.pixelSize: 20 }
                                }
                                ColumnLayout { Layout.fillWidth: true; spacing: 5
                                    Label { text: modelData.name; elide: Text.ElideMiddle; Layout.fillWidth: true; font.bold: true }
                                    Label { text: modelData.missing ? "Missing • relink in inspector" : modelData.kind.toUpperCase()+"  ·  "+modelData.seconds.toFixed(1)+"s"; font.pixelSize: 10; color: win.muted }
                                }
                            }
                            MouseArea { id: mediaMouse; anchors.fill: parent; hoverEnabled: true; onDoubleClicked: editor.addAsset(modelData.id,win.targetTrack) }
                        }
                        Label { anchors.centerIn: parent; visible: editor.assets.length===0; text: "A blank canvas.\nBring your footage."; horizontalAlignment: Text.AlignHCenter; color: win.muted; lineHeight: 1.5 }
                    }
                    Rule {}
                    Action { text: "+ Add title"; Layout.fillWidth: true; onClicked: editor.addTitle() }
                    Caption { text: "LOCAL FILES. YOUR STORY."; font.pixelSize: 9 }
                }
            }
            Rectangle {
                SplitView.fillWidth: true; SplitView.minimumWidth: 420; color: "#10151b"
                ColumnLayout { anchors.fill: parent; anchors.margins: 18; spacing: 12
                    RowLayout { Layout.fillWidth: true
                        Caption { text: "PROJECT MONITOR" }
                        Item { Layout.fillWidth: true }
                        Label { text: win.showPlayback ? "CACHED PLAYBACK" : "RENDERED FRAME"; color: win.muted; font.pixelSize: 9; font.letterSpacing: 1 }
                    }
                    Item {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        Rectangle { anchors.centerIn: parent; width: Math.min(parent.width,parent.height*win.s.width/win.s.height); height: width*win.s.height/win.s.width; color: "#07090c"; border.color: "#2e3741"
                            Image { anchors.fill: parent; source: win.s.previewUrl; cache: false; fillMode: Image.PreserveAspectFit; visible: !win.showPlayback && source.toString().length>0 }
                            VideoOutput { id: videoOutput; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectFit; visible: win.showPlayback }
                            ColumnLayout { anchors.centerIn: parent; visible: win.s.duration===0; spacing: 14
                                Label { text: "Every story starts with a cut."; font.pixelSize: 20; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                                Label { text: "Import footage or add a title to begin."; color: win.muted; Layout.alignment: Qt.AlignHCenter }
                                Action { text: "Import media"; Layout.alignment: Qt.AlignHCenter; onClicked: importDialog.open() }
                            }
                        }
                    }
                    RowLayout { Layout.alignment: Qt.AlignHCenter; spacing: 10
                        Action { text: "−1"; onClicked: { player.pause(); win.showPlayback=false; editor.seek(win.s.playhead-1) } }
                        Action { text: player.playbackState===MediaPlayer.PlayingState ? "Pause" : "Play"; enabled: win.s.playbackUrl.length>0; onClicked: win.play() }
                        Action { text: "+1"; onClicked: { player.pause(); win.showPlayback=false; editor.seek(win.s.playhead+1) } }
                        Label { text: win.clock(win.showPlayback ? Math.floor(player.position*win.s.fps/1000) : win.s.playhead)+" / "+win.clock(win.s.duration); font.family: "Consolas"; color: win.mint }
                    }
                    RowLayout { Layout.alignment: Qt.AlignHCenter
                        Action { text: "Render playback"; enabled: win.s.duration>0&&!win.s.busy; onClicked: editor.renderPlayback() }
                        Label { text: "Re-render after edits"; color: win.muted; font.pixelSize: 10 }
                    }
                }
            }
            Rectangle {
                SplitView.preferredWidth: 272; SplitView.minimumWidth: 235; color: "#171d24"
                ScrollView { anchors.fill: parent; anchors.margins: 16; clip: true; contentWidth: availableWidth
                    ColumnLayout { width: parent.width; spacing: 12
                        Caption { text: "CLIP INSPECTOR" }
                        Label { text: win.selection.name || "Select a clip"; font.pixelSize: 17; font.bold: true; Layout.fillWidth: true; wrapMode: Text.Wrap }
                        Label { visible: !win.s.selectedId.length; text: "Select a timeline clip to trim,\ntransform or adjust its sound."; color: win.muted; lineHeight: 1.5 }
                        ColumnLayout {
                            visible: win.s.selectedId.length>0; Layout.fillWidth: true; spacing: 12
                            Label { text: "Timing • values in frames"; color: win.muted; font.pixelSize: 10 }
                            Repeater {
                                model: [{key:"start",name:"Start frame"},{key:"duration",name:"Length (frames)"},{key:"sourceIn",name:"Source in (sec)"},{key:"speed",name:"Speed (0.25–4×)"}]
                                RowLayout { required property var modelData; Layout.fillWidth: true
                                    Label { text: modelData.name; Layout.fillWidth: true; color: win.muted }
                                    TextField { Layout.preferredWidth: 85; text: String(win.selection[modelData.key] ?? 0); selectByMouse: true; onEditingFinished: { if(isFinite(Number(text)))editor.setClip(modelData.key,Number(text)) } }
                                }
                            }
                            ComboBox { Layout.fillWidth: true; model: ["Track 1 · bottom", "Track 2", "Track 3 · top"]; currentIndex: win.selection.track ?? 0; onActivated: editor.setClip("track",currentIndex) }
                            Rule {}
                            ColumnLayout { Layout.fillWidth: true; visible: win.selection.assetId===""
                                Caption { text: "TITLE / CAPTION" }
                                TextArea { id: titleText; text: win.selection.text || ""; wrapMode: TextEdit.Wrap; Layout.fillWidth: true; Layout.preferredHeight: 85; selectByMouse: true; background: Rectangle { color: "#10161c"; radius: 5; border.color: "#35404b" } }
                                Action { text: "Apply text"; Layout.fillWidth: true; onClicked: editor.setClip("text",titleText.text) }
                                RowLayout { Label { text: "Font size"; Layout.fillWidth: true; color: win.muted } SpinBox { from: 8; to: 500; value: win.selection.fontSize || 72; editable: true; onValueModified: editor.setClip("fontSize",value) } }
                                TextField { Layout.fillWidth: true; text: win.selection.textColor || "#ffffff"; placeholderText: "Text colour (#rrggbb)"; onEditingFinished: editor.setClip("textColor",text) }
                            }
                            Caption { text: "PICTURE & SOUND" }
                            Repeater {
                                model: [{key:"scale",name:"Scale",lo:.1,hi:3,step:.01},{key:"x",name:"Horizontal position",lo:-1,hi:1,step:.01},{key:"y",name:"Vertical position",lo:-1,hi:1,step:.01},{key:"rotation",name:"Rotation",lo:-180,hi:180,step:1},{key:"crop",name:"Crop all edges",lo:0,hi:.45,step:.01},{key:"opacity",name:"Opacity",lo:0,hi:1,step:.01},{key:"brightness",name:"Brightness",lo:-.5,hi:.5,step:.01},{key:"contrast",name:"Contrast",lo:.1,hi:3,step:.01},{key:"saturation",name:"Saturation",lo:0,hi:3,step:.01},{key:"volume",name:"Volume",lo:0,hi:4,step:.01},{key:"fadeIn",name:"Fade in (sec)",lo:0,hi:5,step:.1},{key:"fadeOut",name:"Fade out (sec)",lo:0,hi:5,step:.1}]
                                ColumnLayout { required property var modelData; Layout.fillWidth: true; spacing: 0
                                    RowLayout { Layout.fillWidth: true
                                        Label { text: modelData.name; color: win.muted; Layout.fillWidth: true }
                                        Label { text: Number(win.selection[modelData.key] ?? 0).toFixed(2); font.pixelSize: 10 }
                                    }
                                    Slider { Layout.fillWidth: true; from: modelData.lo; to: modelData.hi; stepSize: modelData.step; value: win.selection[modelData.key] ?? 0; onPressedChanged: if(!pressed)editor.setClip(modelData.key,value); onMoved: if(!pressed)editor.setClip(modelData.key,value) }
                                }
                            }
                            RowLayout { CheckBox { text: "Reverse"; checked: win.selection.reverse || false; onToggled: editor.setClip("reverse",checked) } CheckBox { text: "Flip"; checked: win.selection.flip || false; onToggled: editor.setClip("flip",checked) } }
                            RowLayout { CheckBox { text: "Mute"; checked: win.selection.muted || false; onToggled: editor.setClip("muted",checked) } CheckBox { text: "Hide"; checked: win.selection.hidden || false; onToggled: editor.setClip("hidden",checked) } }
                            Action { text: "Relink source media…"; visible: (win.selection.assetId || "").length>0; Layout.fillWidth: true; onClicked: relinkDialog.open() }
                        }
                    }
                }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#34404b" }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 265; color: "#151b22"
            ColumnLayout { anchors.fill: parent; spacing: 0
                RowLayout { Layout.fillWidth: true; Layout.margins: 12; spacing: 8
                    Caption { text: "TIMELINE" }
                    Label { text: win.clock(win.s.playhead); color: win.mint; font.family: "Consolas"; Layout.leftMargin: 12 }
                    Item { Layout.fillWidth: true }
                    Action { text: "Split"; enabled: win.s.selectedId.length>0; onClicked: editor.split() }
                    Action { text: "Duplicate"; enabled: win.s.selectedId.length>0; onClicked: editor.duplicate() }
                    Action { text: "Delete"; enabled: win.s.selectedId.length>0; onClicked: editor.remove(false) }
                    Label { text: "Zoom"; color: win.muted }
                    Slider { from: 12; to: 180; value: win.pixelsPerSecond; Layout.preferredWidth: 110; onMoved: win.pixelsPerSecond=value }
                }
                RowLayout { Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
                    Column { Layout.preferredWidth: 80; Layout.fillHeight: true
                        Item { height: 28; width: 80 }
                        Repeater { model: win.s.tracks
                            Rectangle { required property int index; width: 80; height: 54; color: "#1c242d"; border.color: "#2d3742"
                                Label { anchors.centerIn: parent; text: "TRACK "+(win.s.tracks-index); font.pixelSize: 10; color: win.muted }
                            }
                        }
                    }
                    Flickable { id: timeline; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; boundsBehavior: Flickable.StopAtBounds
                        contentWidth: Math.max(width,(win.s.duration/win.s.fps+5)*win.pixelsPerSecond); contentHeight: 28+win.s.tracks*54
                        ScrollBar.horizontal: ScrollBar {}
                        Item { id: timelineContent; width: timeline.contentWidth; height: timeline.contentHeight
                            Repeater { model: Math.ceil(timeline.contentWidth/win.pixelsPerSecond)+1
                                Item { required property int index; x: index*win.pixelsPerSecond; height: timelineContent.height
                                    Rectangle { width: 1; height: parent.height; color: "#222c36" }
                                    Label { y: 4; x: 5; text: index+"s"; font.pixelSize: 9; color: win.muted }
                                }
                            }
                            MouseArea { anchors.fill: parent; onClicked: function(mouse) { player.pause(); win.showPlayback=false; editor.seek(Math.round(mouse.x/win.pixelsPerSecond*win.s.fps)) } }
                            Repeater { model: editor.clips
                                Rectangle {
                                    id: clipRect; required property var modelData
                                    property bool moving: false
                                    property real grabOffset: 0
                                    property real dragFrame: modelData.start
                                    property int dragTrack: modelData.track
                                    x: (moving ? dragFrame : modelData.start)/win.s.fps*win.pixelsPerSecond
                                    y: 31+(win.s.tracks-1-(moving ? dragTrack : modelData.track))*54
                                    width: Math.max(8,modelData.duration/win.s.fps*win.pixelsPerSecond); height: 47; radius: 5
                                    color: modelData.title ? "#59453e" : modelData.audio ? "#28564c" : "#334a65"
                                    border.width: win.s.selectedId===modelData.id ? 2 : 1
                                    border.color: win.s.selectedId===modelData.id ? win.mint : modelData.title ? "#ae8168" : "#6481a0"
                                    clip: true
                                    Column { anchors.left: parent.left; anchors.right: parent.right; anchors.margins: 9; anchors.verticalCenter: parent.verticalCenter; spacing: 5
                                        Label { text: clipRect.modelData.name; width: parent.width; elide: Text.ElideRight; font.pixelSize: 11; font.bold: true }
                                        Label { text: (clipRect.modelData.duration/win.s.fps).toFixed(2)+" s"; color: "#bcc9d5"; font.pixelSize: 9 }
                                    }
                                    MouseArea { anchors.fill: parent; cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                        onPressed: function(mouse) { editor.select(clipRect.modelData.id); clipRect.grabOffset=mouse.x; clipRect.dragFrame=clipRect.modelData.start; clipRect.dragTrack=clipRect.modelData.track; clipRect.moving=true }
                                        onPositionChanged: function(mouse) { if(pressed) { const p=mapToItem(timelineContent,mouse.x,mouse.y); let f=Math.max(0,Math.round((p.x-clipRect.grabOffset)/win.pixelsPerSecond*win.s.fps)); if(Math.abs(f-win.s.playhead)<6/win.pixelsPerSecond*win.s.fps)f=win.s.playhead; clipRect.dragFrame=f; clipRect.dragTrack=Math.max(0,Math.min(win.s.tracks-1,win.s.tracks-1-Math.floor((p.y-28)/54))) } }
                                        onReleased: { const id=clipRect.modelData.id; const frame=clipRect.dragFrame; const track=clipRect.dragTrack; clipRect.moving=false; editor.moveClip(id,frame,track) }
                                        onCanceled: clipRect.moving=false
                                        onDoubleClicked: { player.pause(); win.showPlayback=false; editor.seek(clipRect.modelData.start) }
                                    }
                                }
                            }
                            Rectangle { x: (win.showPlayback ? player.position/1000 : win.s.playhead/win.s.fps)*win.pixelsPerSecond; width: 2; height: parent.height; color: win.mint; z: 10
                                Rectangle { width: 8; height: 8; anchors.horizontalCenter: parent.horizontalCenter; color: win.mint; rotation: 45 }
                            }
                        }
                    }
                }
            }
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 32; color: "#202831"
            RowLayout { anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 12
                Label { text: win.s.status; elide: Text.ElideMiddle; Layout.fillWidth: true; font.pixelSize: 10; color: win.muted }
                ProgressBar { visible: win.s.busy; value: win.s.progress; Layout.preferredWidth: 160 }
                Button { text: "Cancel"; visible: win.s.busy; onClicked: editor.cancelJob(); implicitHeight: 25 }
                Label { text: "0.1.0 ALPHA"; font.pixelSize: 9; font.letterSpacing: 1; color: win.mint }
            }
        }
    }
    MediaPlayer { id: player; source: win.s.playbackUrl; audioOutput: AudioOutput {}
        videoOutput: videoOutput
        onErrorOccurred: function(error,errorString) { playbackError.text=errorString; playbackError.open() }
    }
    FileDialog { id: importDialog; title: "Import local media"; fileMode: FileDialog.OpenFiles; nameFilters: ["Media files (*.mp4 *.mov *.mkv *.webm *.avi *.mp3 *.wav *.m4a *.aac *.flac *.ogg *.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff)","All files (*)"]; onAccepted: editor.importMedia(selectedFiles) }
    FileDialog { id: openDialog; title: "Open project"; nameFilters: ["Cutlery project (*.cutlery)"]; onAccepted: editor.openProject(selectedFile) }
    FileDialog { id: saveDialog; title: "Save project"; fileMode: FileDialog.SaveFile; defaultSuffix: "cutlery"; nameFilters: ["Cutlery project (*.cutlery)"]; onAccepted: editor.save(selectedFile) }
    FileDialog { id: exportDialog; title: "Export — choose a new filename"; fileMode: FileDialog.SaveFile; defaultSuffix: win.exportProfile==="webm" ? "webm" : "mp4"; nameFilters: win.exportProfile==="webm" ? ["WebM (*.webm)"] : ["MP4 (*.mp4)"]; onAccepted: editor.exportVideo(selectedFile,win.exportProfile) }
    FileDialog { id: relinkDialog; title: "Choose replacement media"; onAccepted: editor.relink(win.selection.assetId,selectedFile) }
    FileDialog { id: srtOpen; title: "Import captions"; nameFilters: ["SubRip captions (*.srt)"]; onAccepted: editor.importSrt(selectedFile) }
    FileDialog { id: srtSave; title: "Export captions"; fileMode: FileDialog.SaveFile; defaultSuffix: "srt"; nameFilters: ["SubRip captions (*.srt)"]; onAccepted: editor.exportSrt(selectedFile) }
    Dialog { id: discardDialog; anchors.centerIn: parent; title: "Unsaved work / active render"; modal: true; width: 420; standardButtons: Dialog.Discard | Dialog.Cancel
        Label { width: parent.width; text: "Discard unsaved changes and continue? An active render will be cancelled. Cancel to save your project first."; wrapMode: Text.Wrap }
        onDiscarded: win.runAction(win.pendingAction)
    }
    Dialog { id: settings; anchors.centerIn: parent; title: "Project settings"; modal: true; standardButtons: Dialog.Ok | Dialog.Cancel; width: 360
        ColumnLayout { anchors.fill: parent
            Label { text: "Canvas" }
            ComboBox { id: canvas; Layout.fillWidth: true; model: ["1920 × 1080 · landscape","1080 × 1920 · portrait","1080 × 1080 · square","1280 × 720 · landscape"] }
            Label { text: "Frame rate (set before adding clips)"; color: win.muted }
            ComboBox { id: frameRate; enabled: win.s.duration===0; Layout.fillWidth: true; model: ["24","25","30","50","60","29.97 (30000/1001)"]; currentIndex: 2 }
        }
        onAccepted: { const dims=[[1920,1080],[1080,1920],[1080,1080],[1280,720]][canvas.currentIndex]; const rate=[24,25,30,50,60,30000][frameRate.currentIndex]; editor.configure(dims[0],dims[1],win.s.duration>0 ? win.s.fpsN : rate,win.s.duration>0 ? win.s.fpsD : (frameRate.currentIndex===5?1001:1)) }
    }
    Dialog { id: exportSettings; anchors.centerIn: parent; title: "Export video"; modal: true; width: 460; standardButtons: Dialog.Ok | Dialog.Cancel
        ColumnLayout { anchors.fill: parent; spacing: 12
            Label { text: win.s.width+" × "+win.s.height+" · "+win.s.fps.toFixed(2)+" fps · "+(win.s.duration/win.s.fps).toFixed(2)+" seconds" }
            ComboBox { id: codec; Layout.fillWidth: true; model: ["MP4 · MPEG-4 + AAC (portable default)","WebM · VP9 + Opus", "MP4 · H.264 via Windows Media Foundation"]; currentIndex: 0 }
            Label { text: "H.264 requires an available Windows encoder. This alpha exports SDR video and stereo audio. It renders an immutable snapshot of your current timeline."; wrapMode: Text.Wrap; Layout.fillWidth: true; color: win.muted }
        }
        onAccepted: { win.exportProfile=["mpeg4","webm","h264"][codec.currentIndex]; exportDialog.open() }
    }
    Dialog { id: about; anchors.centerIn: parent; title: "Cutlery · 0.1.0 alpha"; modal: true; width: 490; standardButtons: Dialog.Ok
        Label { width: parent.width; text: "A local desktop editor built around a shared FFmpeg render graph.\n\nImport media, arrange three visible tracks, trim in the inspector, add titles/captions, adjust picture and sound, render playback, and export. Higher tracks appear above lower tracks.\n\nPlayback is cached; changes require another render. This alpha does not yet include a real-time D3D11 engine, automatic captions, keyframes, masks, tracking, or the full design roadmap.\n\nProject files reference your original media. Keep those files alongside the project. Recovery data: "+win.s.dataPath; wrapMode: Text.Wrap; lineHeight: 1.3 }
    }
    MessageDialog { id: playbackError; title: "Playback error" }
}
