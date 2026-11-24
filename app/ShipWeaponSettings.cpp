#include "ShipWeaponSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

ShipWeaponSettings loadShipWeaponSettings(const std::string& modelPath) {
    ShipWeaponSettings settings;
    const auto path = qEnvironmentVariable("OCEAN_WEAPON_CONFIG",
        QDir(QCoreApplication::applicationDirPath()).filePath("ShipWeapons.json"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "Cannot open ship weapon catalog: " << path.toStdString()
                  << " (using low arc defaults)\n";
        return settings;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        std::cerr << "Invalid ship weapon catalog: " << path.toStdString() << '\n';
        return settings;
    }
    const auto model = QFileInfo(QString::fromStdString(modelPath)).fileName();
    for (const auto& item : document.object().value("ships").toArray()) {
        const auto ship = item.toObject();
        if (ship.value("model_file").toString().compare(model, Qt::CaseInsensitive) != 0)
            continue;
        const auto gun = ship.value("gun").toObject();
        const auto requested = gun.value("arc").toString("low").toLower();
        if (requested == "high") settings.arc = FireArc::High;
        else if (requested != "low")
            std::cerr << "Unknown gun arc for " << model.toStdString() << ": "
                      << requested.toStdString() << "; using low arc\n";
        const auto min = gun.value("min_elevation_deg").toDouble(settings.minElevationDeg);
        const auto max = gun.value("max_elevation_deg").toDouble(settings.maxElevationDeg);
        if (min > -89.0 && max < 89.0 && min < max) {
            settings.minElevationDeg = min;
            settings.maxElevationDeg = max;
        } else {
            std::cerr << "Invalid elevation limits for " << model.toStdString()
                      << "; using defaults\n";
        }
        break;
    }
    return settings;
}
