#include "ShipManager.h"

#include <QDir>
#include <QFileInfo>
#include <stdexcept>

unsigned ShipManager::addShip(const std::string& code) {
    auto entry = definitions_.find(code);
    if (entry == definitions_.end()) {
        auto definition = loadShipDefinition(code);
        entry = definitions_.emplace(code, std::move(definition)).first;
    }

    const auto file = QString::fromStdString(entry->second.file);
    const auto override = qEnvironmentVariable("OCEAN_MODEL");
    const auto path = override.isEmpty()
        ? QDir(qEnvironmentVariable("OCEAN_SHIP_MODEL_ROOT", "D:/Dragon/_Data"))
              .filePath(QString::fromStdString(code.substr(0, 2) + "/" + code + "/") + file)
        : override;
    if (QFileInfo(path).fileName().compare(file, Qt::CaseInsensitive) != 0)
        throw std::runtime_error("Ship " + code + ": model file must be " + entry->second.file);
    auto ship = std::make_unique<ShipController>(entry->second, path.toStdString());
    ships_.push_back(std::move(ship));
    root_.addChild(&ships_.back()->node());
    return count() - 1;
}
