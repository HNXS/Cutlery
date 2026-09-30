#include "KeyboardShortcuts.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QSaveFile>
#include <QSet>
#include <stdexcept>
namespace cutlery {
QString KeyboardShortcuts::normalize(const QString &text) {
    if (text.trimmed().isEmpty())
        return {};
    if (text.size() > 80)
        throw std::runtime_error("Shortcut is too long");
    const auto sequence = QKeySequence::fromString(text.trimmed(), QKeySequence::PortableText);
    if (sequence.count() != 1 || sequence[0].key() == Qt::Key_unknown || sequence[0].key() == 0)
        throw std::runtime_error("Enter one key combination, for example Ctrl+B or Space");
    const auto result = sequence.toString(QKeySequence::PortableText);
    if (result == "Escape" || result == "Alt+F4" || result == "Ctrl+Alt+Del")
        throw std::runtime_error("This key is reserved for dialogs or Windows");
    return result;
}
KeyboardShortcuts::KeyboardShortcuts(QString path, QObject *parent)
    : QObject(parent), m_path(std::move(path)) {
    auto add = [&](QString id, QString label, QString category, QString sequence) {
        const auto key = normalize(sequence);
        m_bindings.push_back({id, label, category, key, key});
    };
    add("new", "New project", "Project", "Ctrl+N");
    add("open", "Open project", "Project", "Ctrl+O");
    add("import", "Import media", "Project", "Ctrl+I");
    add("save", "Save project", "Project", "Ctrl+S");
    add("saveAs", "Save project as", "Project", "Ctrl+Shift+S");
    add("export", "Export video", "Project", "Ctrl+E");
    add("undo", "Undo", "Edit", "Ctrl+Z");
    add("redo", "Redo", "Edit", "Ctrl+Y");
    add("split", "Split selected clip", "Edit", "Ctrl+B");
    add("duplicate", "Duplicate clip", "Edit", "Ctrl+D");
    add("delete", "Delete clip", "Edit", "Del");
    add("rippleDelete", "Delete and close track gap", "Edit", "Shift+Del");
    add("trimStart", "Trim start to playhead", "Edit", "Q");
    add("trimEnd", "Trim end to playhead", "Edit", "W");
    add("detach", "Detach audio", "Edit", "Ctrl+Shift+A");
    add("title", "Add title", "Edit", "Ctrl+T");
    add("play", "Play / pause (render if needed)", "Playback", "Space");
    add("pause", "Pause at current frame", "Playback", "K");
    add("render", "Render playback cache", "Playback", "Ctrl+R");
    add("previousFrame", "Previous frame", "Navigation", "Left");
    add("nextFrame", "Next frame", "Navigation", "Right");
    add("previousCut", "Previous edit", "Navigation", "Up");
    add("nextCut", "Next edit", "Navigation", "Down");
    add("start", "Start of timeline", "Navigation", "Home");
    add("end", "End of timeline", "Navigation", "End");
    add("zoomIn", "Zoom in", "Timeline", "Ctrl+=");
    add("zoomOut", "Zoom out", "Timeline", "Ctrl+-");
    add("fit", "Fit timeline", "Timeline", "Shift+Z");
    add("snap", "Toggle snapping", "Timeline", "N");
    add("addTrack", "Add track", "Timeline", "Ctrl+Shift+N");
    add("shortcuts", "Keyboard shortcuts", "Help", "Ctrl+/");
    QFile f(m_path);
    if (!f.exists())
        return;
    try {
        if (!f.open(QIODevice::ReadOnly) || f.size() > 64000)
            throw std::runtime_error("Cannot read keyboard settings");
        const auto o = QJsonDocument::fromJson(f.readAll()).object();
        if (o["version"].toInt() != 1 || !o["keys"].isObject())
            throw std::runtime_error("Unsupported keyboard settings");
        const auto keys = o["keys"].toObject();
        auto next = m_bindings;
        QSet<QString> used;
        for (auto &b : next) {
            if (keys.contains(b.id))
                b.sequence = normalize(keys[b.id].toString());
            if (!b.sequence.isEmpty()) {
                if (used.contains(b.sequence))
                    throw std::runtime_error("Conflicting keyboard settings; defaults restored");
                used.insert(b.sequence);
            }
        }
        m_bindings = std::move(next);
    } catch (const std::exception &e) {
        m_error = QString::fromUtf8(e.what());
    }
}
QVariantList KeyboardShortcuts::bindings() const {
    QVariantList result;
    for (const auto &b : m_bindings)
        result << QVariantMap{{"id", b.id},
                              {"label", b.label},
                              {"category", b.category},
                              {"standard", b.standard},
                              {"sequence", b.sequence}};
    return result;
}
bool KeyboardShortcuts::persist(const QVector<Binding> &bindings) {
    QJsonObject keys;
    for (const auto &b : bindings)
        keys[b.id] = b.sequence;
    const auto data = QJsonDocument(QJsonObject{{"version", 1}, {"keys", keys}}).toJson();
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        m_error = "Could not save keyboard settings: " + file.errorString();
        emit changed();
        return false;
    }
    return true;
}
bool KeyboardShortcuts::assign(const QString &id, const QString &value) {
    try {
        const auto key = normalize(value);
        auto next = m_bindings;
        bool found = false;
        for (auto &b : next) {
            if (b.id == id) {
                b.sequence = key;
                found = true;
            } else if (!key.isEmpty() && b.sequence == key)
                throw std::runtime_error(
                    ("Already assigned to " + b.label + ". Clear that binding first.")
                        .toStdString());
        }
        if (!found)
            throw std::runtime_error("Unknown command");
        if (!persist(next))
            return false;
        m_bindings = std::move(next);
        m_error.clear();
        emit changed();
        return true;
    } catch (const std::exception &e) {
        m_error = QString::fromUtf8(e.what());
        emit changed();
        return false;
    }
}
bool KeyboardShortcuts::reset() {
    auto next = m_bindings;
    for (auto &b : next)
        b.sequence = b.standard;
    if (!persist(next))
        return false;
    m_bindings = std::move(next);
    m_error.clear();
    emit changed();
    return true;
}
} // namespace cutlery
