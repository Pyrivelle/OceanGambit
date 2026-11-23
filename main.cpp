#include "app/BattleView.h"
#include "app/LocalSettings.h"

#include <QApplication>
#include <QSurfaceFormat>
#include <iostream>

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
        return runBattleView(args.contains("--moving"),
                             args.contains("--check") || args.contains("--window-check"));
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
