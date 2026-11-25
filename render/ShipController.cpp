#include "ShipController.h"
#include "ShipSelection.h"
#include "InfraredPreview.h"

#include <QCoreApplication>
#include <QDir>
#include <osg/ComputeBoundsVisitor>
#include <osg/Callback>
#include <osgDB/ReadFile>
#include <osgDB/Registry>
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace {
using Point = osg::Vec3d;

class HideFromCamera : public osg::NodeCallback {
public:
    void operator()(osg::Node*, osg::NodeVisitor*) override {}
};
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
        throw std::runtime_error("Could not load DMU ship model");
    Preview visible(root);
    return visible.node();
}


const osg::NodePath& pathFor(const std::map<std::string, osg::NodePath>& paths,
                             const RotationBinding& binding) {
    return paths.at(binding.node.path);
}

osg::Matrixd toShipWorld(const osg::Node& node, const osg::Node& placement) {
    osg::NodePath path;
    const auto* current = &node;
    while (current != &placement) {
        path.push_back(const_cast<osg::Node*>(current));
        if (current->getNumParents() != 1)
            throw std::runtime_error("Ambiguous model path for " + current->getName());
        current = current->getParent(0);
    }
    path.push_back(const_cast<osg::Node*>(&placement));
    std::reverse(path.begin(), path.end());
    return osg::computeLocalToWorld(path);
}
}

ShipController::ShipController(std::shared_ptr<const ShipDefinition> shipDefinition, const std::string& modelPath)
    : modelPath_(modelPath), definition_(std::move(shipDefinition)) {
    const auto& definition = *definition_;
    auto loaded = loadShip(modelPath);
    auto* raw = loaded->asGroup();
    if (!raw || !raw->getNumChildren())
        throw std::runtime_error("Ship " + definition.code + ": empty model hierarchy");
    const auto hull = NodeController::resolve(*raw, definition.hull, definition.code);
    if (hull.size() < 2)
        throw std::runtime_error("Ship " + definition.code + " hull " + definition.hull.path +
                                 ": missing physical model subtree");
    model_ = hull[1];

    const auto physicalPath = [&](const NodeBinding& binding) {
        auto path = NodeController::resolve(*raw, binding, definition.code);
        if (path.size() < 2 || path[1] != model_.get())
            throw std::runtime_error("Ship " + definition.code + " node " + binding.path +
                                     ": outside physical model");
        return path;
    };
    bridge_ = physicalPath(definition.bridge).back();
    for (const auto& view : definition.views)
        if (view.node) NodeController::resolve(*raw, *view.node, definition.code);

    // Resolve all index paths before adding any wrappers or moving sibling nodes.
    std::map<std::string, osg::NodePath> paths;
    std::map<std::string, const RotationBinding*> bindings;
    const auto registerMotion = [&](const RotationBinding& binding) {
        auto path = physicalPath(binding.node);
        if (std::find(path.begin(), path.end(), hull.back()) == path.end())
            throw std::runtime_error("Ship " + definition.code + " node " + binding.node.path +
                                     ": outside hull hierarchy");
        const auto found = bindings.find(binding.node.path);
        if (found != bindings.end()) {
            const auto& previous = *found->second;
            if (binding.pivot != previous.pivot || binding.axis != previous.axis ||
                binding.sign != previous.sign)
                throw std::runtime_error("Ship " + definition.code + " node " + binding.node.path +
                                         ": inconsistent shared rotation binding");
            return;
        }
        bindings.emplace(binding.node.path, &binding);
        paths.emplace(binding.node.path, std::move(path));
    };
    for (const auto& turret : definition.turrets) {
        registerMotion(turret.yaw);
        for (const auto& barrel : turret.barrels) registerMotion(barrel.pitch);
    }

    std::map<std::string, NodeController*> attached;
    const auto attach = [&](const RotationBinding& binding, NodeController::Motion motion) {
        controllers_.push_back(std::make_unique<NodeController>(
            pathFor(paths, binding), binding, motion, definition.code));
        auto* controller = controllers_.back().get();
        attached.emplace(binding.node.path, controller);
        return controller;
    };

    for (const auto& source : definition.turrets) {
        const auto& yawPath = pathFor(paths, source.yaw);
        auto fail = [&](const std::string& path, const char* reason) {
            throw std::runtime_error("Ship " + definition.code + " node " + path + ": " + reason);
        };
        if (attached.count(source.yaw.node.path))
            fail(source.yaw.node.path, "yaw node already controlled by another mount");
        Turret turret{attach(source.yaw, NodeController::Motion::Yaw)};
        std::map<std::string, NodeController*> pitches;
        for (const auto& barrel : source.barrels) {
            const auto& pitchPath = pathFor(paths, barrel.pitch);
            auto* pitchNode = pitchPath.back();
            const auto rest = osg::computeLocalToWorld(pitchPath);
            osg::Matrixd inverse;
            if (!inverse.invert(rest)) fail(barrel.pitch.node.path, "singular rest pose");

            osg::Vec3d muzzle;
            if (barrel.muzzle) {
                muzzle = *barrel.muzzle;
            } else {
                osg::ComputeBoundsVisitor bounds;
                pitchNode->accept(bounds);
                const auto box = bounds.getBoundingBox();
                if (!box.valid()) fail(barrel.pitch.node.path, "cannot estimate muzzle from bounds");
                const osg::NodePath parent(pitchPath.begin(), pitchPath.end() - 1);
                muzzle = osg::Vec3d(box.center().x(), box.yMax(), box.center().z()) *
                         osg::computeLocalToWorld(parent);
            }
            if (!pitches.count(barrel.pitch.node.path)) {
                if (attached.count(barrel.pitch.node.path))
                    fail(barrel.pitch.node.path, "pitch node belongs to another mount or yaw axis");
                auto* pitch = attach(barrel.pitch, NodeController::Motion::Pitch);
                pitches.emplace(barrel.pitch.node.path, pitch);
                turret.pitches.push_back(pitch);

                const bool nested = std::find(pitchPath.begin(), pitchPath.end(), yawPath.back()) != pitchPath.end();
                if (!nested) {
                    auto* parent = pitchPath[pitchPath.size() - 2]->asGroup();
                    if (parent != yawPath[yawPath.size() - 2]->asGroup())
                        fail(barrel.pitch.node.path, "pitch and yaw must be nested or siblings");
                    osg::NodePath yawMotionPath(yawPath.begin(), yawPath.end() - 1);
                    yawMotionPath.push_back(&turret.yaw->motionNode());
                    const auto yawRest = osg::computeLocalToWorld(yawMotionPath);
                    osg::Matrixd invYaw;
                    if (!invYaw.invert(yawRest)) fail(source.yaw.node.path, "singular yaw rest pose");
                    const osg::NodePath parentPath(pitchPath.begin(), pitchPath.end() - 1);
                    auto offset = new osg::MatrixTransform(osg::computeLocalToWorld(parentPath) * invYaw);
                    offset->addChild(&pitch->motionNode());
                    turret.yaw->motionNode().addChild(offset);
                    parent->removeChild(&pitch->motionNode());
                }
            }
            const auto localForward = osg::Matrixd::transform3x3(barrel.forward, inverse);
            turret.barrels.push_back({pitchNode, muzzle * inverse, localForward});
            if (turrets_.empty() && turret.barrels.size() == 1) {
                restMuzzle_ = muzzle;
                modelForward_ = barrel.forward;
            }
        }
        turrets_.push_back(std::move(turret));
    }

    // Metadata has been validated against the full tree; render only the physical model.
    raw->removeChild(model_);
    placement_->addChild(model_);
    for (size_t turret = 0; turret < definition.turrets.size(); ++turret) {
        const auto& barrels = definition.turrets[turret].barrels;
        for (size_t barrel = 0; barrel < barrels.size(); ++barrel) {
            const auto& entry = barrels[barrel];
            const auto rest = muzzlePose(turret, barrel);
            if ((entry.muzzle && (rest.position - *entry.muzzle).length() > 1e-5) ||
                (rest.forward - entry.forward).length() > 1e-6)
                throw std::runtime_error("Ship " + definition.code + " node " + entry.pitch.node.path +
                                         ": mounting changed configured barrel rest pose");
        }
    }
}

void ShipController::setTurretAngles(size_t turret, double yaw, double pitch) {
    auto& mount = turrets_.at(turret);
    mount.yaw->setAngle(yaw);
    for (auto* node : mount.pitches) node->setAngle(pitch);
}

MuzzlePose ShipController::muzzlePose(size_t turret, size_t barrel) const {
    const auto& tube = turrets_.at(turret).barrels.at(barrel);
    const auto matrix = toShipWorld(*tube.node, *placement_);
    auto forward = osg::Matrixd::transform3x3(tube.localForward, matrix);
    forward.normalize();
    return {tube.localMuzzle * matrix, forward};
}

std::optional<osg::Vec3d> ShipController::viewEye(const std::string& id) const {
    for (const auto& view : definition_->views)
        if (view.id == id) return view.eye;
    return std::nullopt;
}

MuzzlePose ShipController::poseTurret(const Point& cue) {
    return poseTurretLocal(cue * osg::Matrix::inverse(placement_->getMatrix()) - restMuzzle_);
}

MuzzlePose ShipController::poseTurretDirection(const Point& worldDirection) {
    return poseTurretLocal(osg::Matrix::transform3x3(worldDirection,
                           osg::Matrix::inverse(placement_->getMatrix())));
}

MuzzlePose ShipController::poseTurretLocal(const Point& direction) {
    const double yaw = std::atan2(direction.x(), direction.y()) -
                       std::atan2(modelForward_.x(), modelForward_.y());
    const double pitch = std::atan2(direction.z(), std::hypot(direction.x(), direction.y())) -
                         std::atan2(modelForward_.z(), std::hypot(modelForward_.x(), modelForward_.y()));
    setTurretAngles(0, yaw, pitch);
    return muzzlePose(0, 0);
}

void ShipController::setBridgeHidden(bool hidden) {
    if (!bridge_) return;
    if (hidden && !bridgeHider_) {
        bridgeHider_ = new HideFromCamera;
        bridge_->addCullCallback(bridgeHider_);
    } else if (!hidden && bridgeHider_) {
        bridge_->removeCullCallback(bridgeHider_);
        bridgeHider_ = nullptr;
    }
}

void ShipController::setSelectionVisible(bool visible) {
    if (visible && !selectionRing_) {
        selectionRing_ = createShipSelectionRing(model_->getBound());
        placement_->addChild(selectionRing_);
    }
    if (selectionRing_) selectionRing_->setNodeMask(visible ? ~0u : 0u);
}
