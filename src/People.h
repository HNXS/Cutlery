#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

namespace cutlery {
// Person search: faces found in the media (by the AI worker's "people" task, see
// worker/people.cpp) grouped into people, some of them named by the user.
//
// A face's descriptor is 128 numbers of length 1; two faces of the same person point the same
// way (cosine similarity, their dot product, above about 0.36 for SFace).
struct Face {
    QString assetId;
    double time = 0;                   // source seconds
    double x = 0, y = 0, w = 0, h = 0; // box in fractions of the picture, x, y its centre
    double score = 0;
    QVector<float> descriptor;
};
// A person named by the user, with descriptors of some of their faces, kept between projects.
struct Label {
    QString name;
    QVector<QVector<float>> descriptors;
};
// A person found: the faces (indexes into the face list) and the media they are in.
struct Person {
    QString id;   // "name:<name>" for a named person, "p<n>" for an unnamed one
    QString name; // empty when unnamed
    QVector<int> faces;
    QStringList media;
    int best = -1; // the face that shows them best (largest, then most certain)
};
// Faces from a "people" result's text, with times shifted by `offset` (the result's start).
QVector<Face> parsePeople(const QString &text, const QString &assetId, double offset);
double similarity(const QVector<float> &a, const QVector<float> &b);
// Groups faces: each goes to the named person it resembles most when at least `threshold`
// alike; the rest are grouped among themselves (each joins the first group whose average it is
// that alike to, else starts one). Named people come first in the order of `labels`, then the
// others by how often they appear; named people with no face here are left out.
QVector<Person> groupPeople(const QVector<Face> &faces, const QVector<Label> &labels,
                            double threshold = 0.4);
// Named people in a JSON file ([{"name", "descriptors": [[...], ...]}]); none when missing or
// unreadable.
QVector<Label> readLabels(const QString &path);
bool writeLabels(const QString &path, const QVector<Label> &labels);
} // namespace cutlery
