#include "../render/NodeController.h"
#include "../render/ShipManager.h"
#include "../app/LocalSettings.h"

#include <QCoreApplication>
#include <QDir>

#include <string_view>

#include <osg/Group>
#include <osg/Math>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
bool close(const osg::Vec3d& a, const osg::Vec3d& b) {
    return (a - b).length() < 1e-6;
}

int checkAsset(const QString& bin) {
    qputenv("OSG_RUNTIME", bin.toUtf8());
    qputenv("OCEAN_SHIP_CONFIG_DIR", QDir(bin).filePath("ships").toUtf8());
    loadLocalSettings();

    osg::ref_ptr<osg::Group> root = new osg::Group;
    ShipManager manager(*root);
    auto& first = manager.ship(manager.addShip("020528"));
    auto& second = manager.ship(manager.addShip("020528"));
    const auto definition = loadShipDefinition("020528");
    const auto& mount = definition.turrets.at(0);
    const auto& barrel = mount.barrels.at(0);
    if (!barrel.muzzle) throw std::runtime_error("Expected configured test asset muzzle");

    const auto rest = first.muzzlePose(0, 0);
    const bool restMatches = close(rest.position, *barrel.muzzle) &&
                             close(rest.forward, barrel.forward);
    first.setTurretAngles(0, .25, 0);
    const auto yawed = first.muzzlePose(0, 0);
    first.setTurretAngles(0, .25, .13);
    const auto tilted = first.muzzlePose(0, 0);
    const bool yaw = !close(yawed.position, rest.position) &&
                     !close(yawed.forward, rest.forward) &&
                     std::abs(yawed.forward.z() - rest.forward.z()) < 1e-6;
    const bool pitch = !close(tilted.position, yawed.position) &&
                       tilted.forward.z() > yawed.forward.z();
    const bool independent = close(second.muzzlePose(0, 0).position, rest.position) &&
                             close(second.muzzlePose(0, 0).forward, rest.forward);
    first.setTurretAngles(0, 0, 0);
    const bool reset = close(first.muzzlePose(0, 0).position, rest.position);

    auto sharedDefinition = definition;
    auto& barrels = sharedDefinition.turrets.at(0).barrels;
    auto secondBarrel = barrels.at(0);
    secondBarrel.id = "shared_pitch_fixture";
    *secondBarrel.muzzle += osg::Vec3d(1, 0, 0);
    barrels.push_back(secondBarrel);
    ShipController shared(sharedDefinition, first.modelPath());
    shared.setTurretAngles(0, .25, .13);
    const auto firstOutput = shared.muzzlePose(0, 0);
    const auto secondOutput = shared.muzzlePose(0, 1);
    const bool sharedPitch = close(firstOutput.position, tilted.position) &&
        close(firstOutput.forward, secondOutput.forward) &&
        std::abs((secondOutput.position - firstOutput.position).length() - 1.) < 1e-6;

    std::cout << "asset_rest=" << restMatches << " yaw=" << yaw << " pitch=" << pitch
              << " independent_instances=" << independent << " reset=" << reset
              << " shared_pitch_distinct_muzzles=" << sharedPitch << '\n';
    return restMatches && yaw && pitch && independent && reset && sharedPitch ? 0 : 1;
}
}

int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--020528") {
        QCoreApplication application(argc, argv);
        try {
            return checkAsset(QString::fromLocal8Bit(argv[2]));
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 1;
        }
    }
    osg::ref_ptr<osg::Group> root = new osg::Group;
    osg::ref_ptr<osg::Group> mesh = new osg::Group;
    mesh->setName("duplicate");
    root->addChild(mesh);
    osg::ref_ptr<osgSim::DOFTransform> dof = new osgSim::DOFTransform;
    dof->setName("duplicate");
    dof->setAnimationOn(true);
    dof->setCurrentHPR({.1f, .2f, .3f});
    root->addChild(dof);

    const RotationBinding ordinary{{"duplicate", "/n0/n0"}, osg::Vec3d(1, 0, 0),
                                                               osg::Vec3d(0, 0, 1), -1};
    const RotationBinding native{{"duplicate", "/n0/n1"}, osg::Vec3d(200, 0, 0),
                                                             osg::Vec3d(0, 0, 1), -1};
    const auto meshPath = NodeController::resolve(*root, ordinary.node, "fixture");
    const auto dofPath = NodeController::resolve(*root, native.node, "fixture");
    NodeController wrapper(meshPath, ordinary, NodeController::Motion::Yaw, "fixture");
    NodeController builtIn(dofPath, native, NodeController::Motion::Pitch, "fixture");

    wrapper.setAngle(osg::PI_2);
    const auto matrix = osg::computeLocalToWorld({root.get(), &wrapper.motionNode(), mesh.get()});
    const bool pivotPreserved = close(osg::Vec3d(1, 0, 0) * matrix, {1, 0, 0}) &&
                                close(osg::Vec3d(2, 0, 0) * matrix, {1, -1, 0});
    wrapper.setAngle(0);
    const bool ordinaryReset = close(osg::Vec3d(2, 0, 0) *
        osg::computeLocalToWorld({root.get(), &wrapper.motionNode(), mesh.get()}), {2, 0, 0});

    builtIn.setAngle(.35);
    const bool nativePitch = &builtIn.motionNode() == dof && !dof->getAnimationOn() &&
                             close(osg::Vec3d(dof->getCurrentHPR()), {.1, .55, .3});
    builtIn.setAngle(0);
    const bool nativeReset = close(osg::Vec3d(dof->getCurrentHPR()), {.1, .2, .3});
    const auto rejects = [&](const NodeBinding& invalid) {
        try {
            NodeController::resolve(*root, invalid, "fixture");
        } catch (const std::runtime_error&) {
            return true;
        }
        return false;
    };
    const bool strict = dofPath.back() == dof && meshPath.back() == mesh &&
        rejects({"wrong", "/n0/n1"}) && rejects({"duplicate", "/n0/n9"}) &&
        rejects({"duplicate", "/n0/n01"});
    const bool ok = pivotPreserved && ordinaryReset && nativePitch && nativeReset && strict;
    std::cout << "ordinary=" << (pivotPreserved && ordinaryReset)
              << " dof=" << (nativePitch && nativeReset) << " strict_paths=" << strict << '\n';
    return ok ? 0 : 1;
}
