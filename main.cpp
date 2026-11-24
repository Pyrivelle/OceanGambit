#include "app/BattleView.h"
#include "app/LocalSettings.h"

#include <QApplication>
#include <QSurfaceFormat>
#include <iostream>
#include "battle/WowsMotion.h"

int main(int argc, char** argv) {
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CompatibilityProfile); // DMU meshes contain GL_QUADS.
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication application(argc, argv);
    const auto args = application.arguments();
    try {
        loadLocalSettings();
        using ocean_gambit::ballistics::Integrator;
        const bool wows = args.contains("--ballistics-wows");
        auto method = Integrator::RungeKutta4;
        if (args.contains("--integrator-euler")) method = Integrator::ForwardEuler;
        if (args.contains("--integrator-rk2")) method = Integrator::RungeKutta2;
        if (args.contains("--integrator-ab5")) method = Integrator::AdamsBashforth5;
        return runBattleView(args.contains("--moving"),
                             args.contains("--check") || args.contains("--window-check"),
                             args.contains("--drag-quadratic") && !wows, wows, method,
                             args.contains("--overview"), args.contains("--check-camera"));
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
