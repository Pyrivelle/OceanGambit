#include "ShipManager.h"

#include <QDir>
#include <QFileInfo>
#include <stdexcept>

EntityBuild ShipManager::build(const std::string& code) {
    // RTS prototype movement for 020528, independent of the model JSON.
    constexpr MovementTuning demoMovement{22., 3.5, 5., .40, 8.};
    EntityBuild result{create(code), std::nullopt};
    if (code == "020528") result.movement.emplace(demoMovement);
    return result;
}

std::unique_ptr<ShipController> ShipManager::create(const std::string& code) {
    auto entry = definitions_.find(code);
    if (entry == definitions_.end()) {
        auto definition = std::make_shared<const ShipDefinition>(loadShipDefinition(code));
        entry = definitions_.emplace(code, std::move(definition)).first;
    }

    const auto file = QString::fromStdString(entry->second->file);
    const auto override = qEnvironmentVariable("OCEAN_MODEL");
    const auto path = override.isEmpty()
        ? QDir(qEnvironmentVariable("OCEAN_SHIP_MODEL_ROOT", "D:/Dragon/_Data"))
              .filePath(QString::fromStdString(code.substr(0, 2) + "/" + code + "/") + file)
        : override;
    if (QFileInfo(path).fileName().compare(file, Qt::CaseInsensitive) != 0)
        throw std::runtime_error("Ship " + code + ": model file must be " + entry->second->file);
    return std::make_unique<ShipController>(entry->second, path.toStdString());
}
