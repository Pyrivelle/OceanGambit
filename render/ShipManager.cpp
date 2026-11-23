#include "ShipManager.h"
#include "InfraredPreview.h"

#include <QCoreApplication>
#include <QDir>
#include <osg/ComputeBoundsVisitor>
#include <osgDB/ReadFile>
#include <osgDB/Registry>
#include <cmath>
#include <stdexcept>

namespace {
using Point = osg::Vec3d;
using Preview = Pyrive::OsgKit::InfraredPreview;
// The same DMU reader and visible-light material pass used by InfraredPreview.
osg::ref_ptr<osg::Node> loadShip(const std::string& modelPath) {
    auto* registry = osgDB::Registry::instance();
    registry->addFileExtensionAlias("dmuship", "dmu");
    registry->addFileExtensionAlias("dmuShip", "dmu");
    const auto plugin = QDir(qEnvironmentVariable("OSG_RUNTIME", QCoreApplication::applicationDirPath()))
        .filePath("osgPlugins-3.6.5/osgdb_dmu.dll");
    if (registry->loadLibrary(plugin.toStdString()) == osgDB::Registry::NOT_LOADED)
        throw std::runtime_error("DMU preview reader unavailable");
    osg::ref_ptr<osgDB::Options> options = new osgDB::Options(qgetenv("DMU_READER_OPTIONS").toStdString());
    auto resource = osgDB::readRefNodeFile(modelPath, options);
    auto* root = resource ? resource->asGroup() : nullptr;
    if (!root || root->getNumChildren() == 0)
        throw std::runtime_error("Could not load 020528 DMU model");
    Preview visible(root->getChild(0));
    return visible.node();
}

osg::Node& namedChild(osg::Group& group, const char* name) {
    for (unsigned i = 0; i < group.getNumChildren(); ++i)
        if (group.getChild(i)->getName() == name) return *group.getChild(i);
    throw std::runtime_error(std::string("Missing 020528 node: ") + name);
}
}

Ship::Ship(const std::string& modelPath) {
    model_ = loadShip(modelPath);
    auto& body = *model_->asGroup();
    auto& hull = *namedChild(body, "ChuanTi").asGroup();
    auto& barrel = namedChild(hull, "l203");
    auto& housing = namedChild(hull, "l202");
    osg::ComputeBoundsVisitor bounds;
    barrel.accept(bounds);
    const auto box = bounds.getBoundingBox();
    if (!box.valid()) throw std::runtime_error("Missing 020528 barrel geometry");
    muzzle_.set(box.center().x(), box.yMax(), box.center().z());
    turretYaw_->addChild(&housing);
    barrelPitch_->addChild(&barrel);
    turretYaw_->addChild(barrelPitch_);
    hull.replaceChild(&housing, turretYaw_);
    hull.removeChild(&barrel);
    placement_->addChild(model_);
}

Point Ship::poseTurret(const Point& cue) {
    const auto inverse = osg::Matrix::inverse(placement_->getMatrix());
    const Point direction = cue * inverse - muzzle_;
    const Point yawPivot(0, 26.6, 9.4), pitchPivot(0, 26.7, 10.7);
    const double yaw = -std::atan2(direction.x(), direction.y());
    const double pitch = std::atan2(direction.z(), std::hypot(direction.x(), direction.y()));
    turretYaw_->setMatrix(osg::Matrix::translate(-yawPivot) *
                        osg::Matrix::rotate(yaw, 0, 0, 1) * osg::Matrix::translate(yawPivot));
    barrelPitch_->setMatrix(osg::Matrix::translate(-pitchPivot) *
                          osg::Matrix::rotate(pitch, 1, 0, 0) * osg::Matrix::translate(pitchPivot));
    return muzzle_ * barrelPitch_->getMatrix() * turretYaw_->getMatrix() * placement_->getMatrix();
}

void Ship::place(double x, double y, double heading) {
    placement_->setMatrix(osg::Matrix::rotate(heading, 0, 0, 1) *
                         osg::Matrix::translate(x, y, 0));
}

unsigned ShipManager::addShip(const std::string& modelPath) {
    auto& ship = ships_.emplace_back(modelPath);
    root_.addChild(&ship.node());
    return static_cast<unsigned>(ships_.size() - 1);
}
