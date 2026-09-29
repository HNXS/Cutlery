#pragma once
#include "Project.h"
#include <QStringList>

namespace cutlery {
struct RenderPlan {
    QStringList inputs;
    QString graph;
    double duration = 0;
    qint64 frames = 0;
    int width = 0, height = 0;
};
// The same compiler handles preview stills, cached playback and final export.
// FFmpeg is the first CPU reference backend; a D3D11 backend is not implemented.
RenderPlan compileRender(const Project &, const QString &workDir, int width, int height,
                         bool audio = true);
QStringList renderArguments(const RenderPlan &, const QString &graphFile, const QString &output,
                            const QString &profile, double seek = -1);
} // namespace cutlery
