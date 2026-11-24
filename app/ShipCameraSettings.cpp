#include "ShipCameraSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

ShipCameraSettings loadShipCameraSettings(const std::string& modelPath) {
    ShipCameraSettings settings;
    const auto path = qEnvironmentVariable("OCEAN_CAMERA_CONFIG",
        QDir(QCoreApplication::applicationDirPath()).filePath("ShipCameras.json"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "Cannot open ship camera catalog: " << path.toStdString() << std::endl;
        return settings;
    }
    QJsonParseError error;
    const auto catalog = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        std::cerr << "Invalid ship camera catalog: " << error.errorString().toStdString() << std::endl;
        return settings;
    }
    const auto model = QFileInfo(QString::fromStdString(modelPath)).fileName();
    for (const auto& item : catalog.object().value("ships").toArray()) {
        const auto ship = item.toObject();
        if (ship.value("model_file").toString().compare(model, Qt::CaseInsensitive) != 0) continue;
        const auto follow = ship.value("follow").toObject();
        const auto read = [&](const char* name, double fallback, double min, double max) {
            const double value = follow.value(name).toDouble(fallback);
            return value >= min && value <= max ? value : fallback;
        };
        settings.distance = read("distance", settings.distance, 1, 5000);
        settings.height = read("height", settings.height, 0, 5000);
        settings.pitch = read("pitch_deg", settings.pitch, -80, 80);
        settings.followFov = read("fov_deg", settings.followFov, 1, 150);
        settings.smoothingSeconds = read("smoothing_seconds", settings.smoothingSeconds, 0, 60);
        const auto views = ship.value("view_pos").toArray();
        const auto first = views.isEmpty() ? QJsonArray{} : views.first().toArray();
        const double fov = ship.value("binocular").toObject().value("fov_deg").toDouble();
        if (first.size() == 3 && first[0].isDouble() && first[1].isDouble() &&
            first[2].isDouble() && fov > 0 && fov < settings.followFov) {
            settings.bridgeEye = osg::Vec3d(first[0].toDouble(), first[1].toDouble(), first[2].toDouble());
            settings.binocularFov = fov;
        }
        break;
    }
    if (!settings.bridgeEye)
        std::cerr << "No bridge eye and binocular FOV for ship model " << model.toStdString() << std::endl;
    return settings;
}
