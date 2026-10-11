#include "People.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>
#include <cmath>

namespace cutlery {
QVector<Face> parsePeople(const QString &text, const QString &assetId, double offset) {
    QVector<Face> faces;
    for (const auto &line : text.split('\n', Qt::SkipEmptyParts)) {
        const auto parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 7)
            continue;
        Face f;
        f.assetId = assetId;
        f.time = parts[0].toDouble() + offset;
        f.x = parts[1].toDouble();
        f.y = parts[2].toDouble();
        f.w = parts[3].toDouble();
        f.h = parts[4].toDouble();
        f.score = parts[5].toDouble();
        for (qsizetype i = 6; i < parts.size(); ++i)
            f.descriptor << parts[i].toFloat();
        faces << f;
    }
    return faces;
}
double similarity(const QVector<float> &a, const QVector<float> &b) {
    if (a.size() != b.size() || a.isEmpty())
        return -1;
    double dot = 0;
    for (qsizetype i = 0; i < a.size(); ++i)
        dot += double(a[i]) * b[i];
    return dot;
}
static QVector<float> normalised(const QVector<double> &sum) {
    double length = 0;
    for (const double v : sum)
        length += v * v;
    length = std::sqrt(length);
    QVector<float> out;
    for (const double v : sum)
        out << float(length > 0 ? v / length : 0);
    return out;
}
QVector<Person> groupPeople(const QVector<Face> &faces, const QVector<Label> &labels,
                            double threshold) {
    QVector<Person> named(labels.size());
    for (qsizetype i = 0; i < labels.size(); ++i) {
        named[i].id = "name:" + labels[i].name;
        named[i].name = labels[i].name;
    }
    struct Group {
        QVector<double> sum;
        QVector<float> centre;
        Person person;
    };
    QVector<Group> groups;
    for (int index = 0; index < faces.size(); ++index) {
        const auto &f = faces[index];
        // The named person it resembles most.
        int label = -1;
        double best = threshold;
        for (qsizetype i = 0; i < labels.size(); ++i)
            for (const auto &d : labels[i].descriptors)
                if (const double s = similarity(f.descriptor, d); s >= best) {
                    best = s;
                    label = int(i);
                }
        if (label >= 0) {
            named[label].faces << index;
            continue;
        }
        Group *group = nullptr;
        for (auto &g : groups)
            if (similarity(f.descriptor, g.centre) >= threshold) {
                group = &g;
                break;
            }
        if (!group) {
            groups.push_back({QVector<double>(f.descriptor.size(), 0.), {}, {}});
            group = &groups.last();
        }
        for (qsizetype i = 0; i < f.descriptor.size() && i < group->sum.size(); ++i)
            group->sum[i] += f.descriptor[i];
        group->centre = normalised(group->sum);
        group->person.faces << index;
    }
    std::stable_sort(groups.begin(), groups.end(), [](const Group &a, const Group &b) {
        return a.person.faces.size() > b.person.faces.size();
    });
    QVector<Person> people;
    for (auto &p : named)
        if (!p.faces.isEmpty())
            people << p;
    for (qsizetype i = 0; i < groups.size(); ++i) {
        auto p = groups[i].person;
        p.id = QString("p%1").arg(i + 1);
        people << p;
    }
    for (auto &p : people) {
        for (const int i : p.faces)
            if (!p.media.contains(faces[i].assetId))
                p.media << faces[i].assetId;
        for (const int i : p.faces)
            if (p.best < 0 || faces[i].w * faces[i].h > faces[p.best].w * faces[p.best].h * 1.05 ||
                (faces[i].w * faces[i].h > faces[p.best].w * faces[p.best].h * 0.95 &&
                 faces[i].score > faces[p.best].score))
                p.best = i;
    }
    return people;
}
QVector<Label> readLabels(const QString &path) {
    QFile file(path);
    QVector<Label> labels;
    if (!file.open(QIODevice::ReadOnly))
        return labels;
    for (const auto &value : QJsonDocument::fromJson(file.readAll()).array()) {
        const auto o = value.toObject();
        Label l;
        l.name = o["name"].toString().trimmed();
        for (const auto &d : o["descriptors"].toArray()) {
            QVector<float> v;
            for (const auto &x : d.toArray())
                v << float(x.toDouble());
            if (v.size() == 128)
                l.descriptors << v;
        }
        if (!l.name.isEmpty() && !l.descriptors.isEmpty())
            labels << l;
    }
    return labels;
}
bool writeLabels(const QString &path, const QVector<Label> &labels) {
    QJsonArray list;
    for (const auto &l : labels) {
        QJsonArray descriptors;
        for (const auto &d : l.descriptors) {
            QJsonArray v;
            for (const float x : d)
                v << std::round(double(x) * 1e5) / 1e5;
            descriptors << v;
        }
        list << QJsonObject{{"name", l.name}, {"descriptors", descriptors}};
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(list).toJson(QJsonDocument::Compact));
    return file.commit();
}
} // namespace cutlery
