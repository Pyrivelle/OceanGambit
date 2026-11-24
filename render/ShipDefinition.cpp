#include "ShipDefinition.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <set>
#include <stdexcept>

namespace {
class DefinitionReader {
public:
    explicit DefinitionReader(const std::string& code) : code_(code) {}

    [[noreturn]] void fail(const std::string& field) const {
        throw std::runtime_error("Ship " + code_ + ": invalid " + field);
    }

    std::string str(const QJsonObject& object, const char* field) const {
        const auto value = object.value(field).toString();
        if (value.trimmed().isEmpty()) fail(field);
        return value.toStdString();
    }

    double number(const QJsonValue& value, const char* field) const {
        if (!value.isDouble() || !std::isfinite(value.toDouble())) fail(field);
        return value.toDouble();
    }

    osg::Vec3d vector(const QJsonObject& object, const char* field) const {
        const auto values = object.value(field).toArray();
        if (values.size() != 3) fail(field);
        return {number(values[0], field), number(values[1], field), number(values[2], field)};
    }

    RotationBinding rotation(const QJsonObject& object) const {
        RotationBinding result{{str(object, "node"), str(object, "path")}};
        if (object.contains("pivot")) result.pivot = vector(object, "pivot");
        if (object.contains("axis")) {
            auto axis = vector(object, "axis");
            if (axis.normalize() == 0) fail(result.node.path + " axis");
            result.axis = axis;
        }
        if (object.contains("sign")) {
            result.sign = number(object.value("sign"), "sign");
            if (*result.sign != -1 && *result.sign != 1) fail(result.node.path + " sign");
        }
        return result;
    }

private:
    const std::string& code_;
};
}

ShipDefinition loadShipDefinition(const std::string& code) {
    const DefinitionReader read(code);
    if (code.empty() || code.find_first_not_of("0123456789") != std::string::npos) read.fail("code");
    const auto directory = qEnvironmentVariable("OCEAN_SHIP_CONFIG_DIR",
        QDir(QCoreApplication::applicationDirPath()).filePath("ships"));
    QFile file(QDir(directory).filePath(QString::fromStdString(code) + ".json"));
    if (!file.open(QIODevice::ReadOnly)) read.fail(file.fileName().toStdString());
    QJsonParseError error;
    const auto object = QJsonDocument::fromJson(file.readAll(), &error).object();
    if (error.error != QJsonParseError::NoError || object.value("schema_version").toInt() != 1)
        read.fail("JSON or schema_version");
    if (read.str(object, "code") != code) read.fail("code mismatch");

    const auto model = object.value("model").toObject();
    const auto fileName = read.str(model, "file");
    if (QFileInfo(QString::fromStdString(fileName)).fileName().toStdString() != fileName)
        read.fail("model.file");
    const auto dimensions = model.value("dimensions_m").toObject();
    for (const auto* field : {"length", "beam", "height", "draft"})
        if (read.number(dimensions.value(field), field) <= 0) read.fail(field);
    const auto nodes = object.value("nodes").toObject();
    ShipDefinition result{code, fileName,
        {read.str(nodes, "hull"), read.str(nodes, "hull_path")},
        {read.str(nodes, "bridge"), read.str(nodes, "bridge_path")}};

    std::set<std::string> turretIds;
    for (const auto& entry : object.value("turrets").toArray()) {
        const auto source = entry.toObject();
        TurretDefinition turret{read.str(source, "id"), read.rotation(source.value("yaw").toObject())};
        if (!turretIds.insert(turret.id).second) read.fail("duplicate turret " + turret.id);
        std::set<std::string> barrelIds;
        for (const auto& item : source.value("barrels").toArray()) {
            const auto barrel = item.toObject();
            BarrelDefinition definition{read.str(barrel, "id"),
                read.rotation(barrel.value("pitch").toObject()), {}, read.vector(barrel, "forward")};
            if (!barrelIds.insert(definition.id).second) read.fail("duplicate barrel " + definition.id);
            if (!barrel.value("muzzle").isNull()) definition.muzzle = read.vector(barrel, "muzzle");
            if (definition.forward.normalize() == 0) read.fail("barrel forward");
            turret.barrels.push_back(std::move(definition));
        }
        if (turret.barrels.empty()) read.fail("barrels");
        result.turrets.push_back(std::move(turret));
    }
    if (result.turrets.empty()) read.fail("turrets");

    std::set<std::string> viewIds;
    for (const auto& item : object.value("views").toArray()) {
        const auto view = item.toObject();
        ViewDefinition definition{read.str(view, "id"), read.vector(view, "eye")};
        if (!viewIds.insert(definition.id).second) read.fail("duplicate view " + definition.id);
        if (view.contains("node") || view.contains("path"))
            definition.node = NodeBinding{read.str(view, "node"), read.str(view, "path")};
        result.views.push_back(std::move(definition));
    }
    return result;
}
