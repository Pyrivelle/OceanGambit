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

class FixtureVisual final : public EntityVisual {
public:
    osg::MatrixTransform& node() const override { return *node_; }

private:
    osg::ref_ptr<osg::MatrixTransform> node_ = new osg::MatrixTransform;
};

bool checkMovement() {
    const MovementTuning tuning{12, 3, 4, .5, 4};
    MovementTrait mover(tuning), independent(tuning);
    TransformComponent pose{};
    mover.moveTo({80, 40, 0});
    mover.step(pose, .01);
    const bool starts = mover.state() == MovementState::Moving && mover.speed() > 0 &&
        mover.speed() <= tuning.acceleration * .01 + 1e-8 &&
        std::abs(pose.heading) <= tuning.turnRate * .01 + 1e-8;

    for (int step = 0; step < 250; ++step) mover.step(pose, .01);
    const auto before = pose.position;
    const osg::Vec3d replacement(-80, 50, 0);
    mover.moveTo(replacement);
    const bool replaces = mover.destination() == replacement && pose.position == before;
    bool bounded = true;
    bool braked = false;
    double peak = mover.speed();
    for (int step = 0; step < 16000 && mover.state() == MovementState::Moving; ++step) {
        const double speed = mover.speed();
        const double heading = pose.heading;
        mover.step(pose, .01);
        peak = std::max(peak, mover.speed());
        braked |= mover.speed() + 1e-6 < speed;
        bounded &= mover.speed() <= tuning.maxSpeed + 1e-8 &&
                   mover.speed() - speed <= tuning.acceleration * .01 + 1e-8 &&
                   speed - mover.speed() <= tuning.deceleration * .01 + 1e-8 &&
                   std::abs(std::remainder(pose.heading - heading, 2 * osg::PI)) <=
                       tuning.turnRate * .01 + 1e-8;
    }
    const bool arrived = mover.state() == MovementState::Arrived && mover.speed() == 0 &&
        (pose.position - replacement).length() <= tuning.arrivalRadius + 1e-6;
    const bool separate = independent.state() == MovementState::Idle && !independent.destination() &&
                          independent.speed() == 0;
    const bool ok = starts && replaces && bounded && braked && arrived && separate && peak > 2;
    std::cout << "movement_start=" << starts << " move_replace=" << replaces << " motion_limits=" << bounded
              << " braking=" << braked << " arrived=" << arrived << " independent_movement=" << separate << '\n';
    return ok;
}

bool checkEntityManager() {
    osg::ref_ptr<osg::Group> root = new osg::Group;
    auto unrelated = new osg::Group;
    root->addChild(unrelated);
    bool ok = false;
    {
        EntityManager entities(*root);
        auto build = [](const std::string& code) {
            auto visual = std::make_unique<FixtureVisual>();
            if (code == "broken") throw std::runtime_error("fixture creation failed");
            return EntityBuild{std::move(visual), std::nullopt};
        };
        entities.registerFactory(EntityType::Ship, SelectionMode::Multiple, build);
        entities.registerFactory(EntityType::Building, SelectionMode::Single, build);
        const auto ship = entities.createEntity(EntityType::Ship, "fixture", 1);
        const auto building = entities.createEntity(EntityType::Building, "fixture", 2);
        auto* entry = entities.find(ship);
        entry->setTransform({{10, -20, 30}, .4});
        const bool indices = entities.ids() == std::vector<EntityID>{ship, building} &&
            entities.findByType(EntityType::Ship) == std::vector<EntityID>{ship} &&
            entities.findByOwner(2) == std::vector<EntityID>{building} &&
            entities.findSelectable(SelectionMode::Single) == std::vector<EntityID>{building} &&
            entry->id() == ship && entry->code() == "fixture" && entry->owner() == 1 &&
            entry->lifecycle() == EntityLifecycle::Active &&
            entry->transform().heading == .4 &&
            close(entry->visual().node().getMatrix().getTrans(), {10, -20, 30});

        bool rejected = false;
        try {
            entities.createEntity(EntityType::Ship, "broken", 1);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        const bool rollback = rejected && entities.ids().size() == 2 && root->getNumChildren() == 3;
        osg::ref_ptr<osg::MatrixTransform> oldNode = &entry->visual().node();
        const bool removed = entities.destroyEntity(ship) && !entities.destroyEntity(ship) &&
            !entities.find(ship) && entities.find(building) && oldNode->getNumParents() == 0;
        const auto fresh = entities.createEntity(EntityType::Ship, "fixture", 1);
        const bool freshId = fresh > building && !entities.find(ship) &&
            entities.findByOwner(1) == std::vector<EntityID>{fresh};
        ok = indices && rollback && removed && freshId;
    }
    return ok && root->getNumChildren() == 1 && root->getChild(0) == unrelated;
}

int checkAsset(const QString& bin) {
    qputenv("OSG_RUNTIME", bin.toUtf8());
    qputenv("OCEAN_SHIP_CONFIG_DIR", QDir(bin).filePath("ships").toUtf8());
    loadLocalSettings();

    osg::ref_ptr<osg::Group> root = new osg::Group;
    ShipManager assets;
    EntityManager entities(*root);
    entities.registerFactory(EntityType::Ship, SelectionMode::Multiple,
        [&assets](const std::string& code) { return assets.build(code); });
    const auto firstId = entities.createEntity(EntityType::Ship, "020528", 1);
    const auto secondId = entities.createEntity(EntityType::Ship, "020528", 2);
    auto& first = static_cast<ShipController&>(entities.find(firstId)->visual());
    auto& second = static_cast<ShipController&>(entities.find(secondId)->visual());
    const auto& definition = first.definition();
    const bool sharedAsset = &definition == &second.definition() && &first.model() != &second.model() &&
        entities.findByType(EntityType::Ship) == std::vector<EntityID>{firstId, secondId} &&
        entities.findByOwner(1) == std::vector<EntityID>{firstId} &&
        entities.findByOwner(2) == std::vector<EntityID>{secondId};
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
    ShipController shared(std::make_shared<const ShipDefinition>(std::move(sharedDefinition)), first.modelPath());
    shared.setTurretAngles(0, .25, .13);
    const auto firstOutput = shared.muzzlePose(0, 0);
    const auto secondOutput = shared.muzzlePose(0, 1);
    const bool sharedPitch = close(firstOutput.position, tilted.position) &&
        close(firstOutput.forward, secondOutput.forward) &&
        std::abs((secondOutput.position - firstOutput.position).length() - 1.) < 1e-6;

    auto* secondEntity = entities.find(secondId);
    secondEntity->setTransform({{70, -120, 0}, .5});
    const auto secondPose = second.muzzlePose(0, 0);
    const bool movedAlone = close(first.muzzlePose(0, 0).position, rest.position) &&
        !close(secondPose.position, rest.position) && !close(secondPose.forward, rest.forward);
    osg::ref_ptr<osg::MatrixTransform> releasedNode = &first.node();
    const bool destroyed = entities.destroyEntity(firstId) && !entities.find(firstId) &&
        root->getNumChildren() == 1 && releasedNode->getNumParents() == 0 &&
        entities.find(secondId) == secondEntity && close(second.muzzlePose(0, 0).position, secondPose.position);
    const auto nextId = entities.createEntity(EntityType::Ship, "020528", 1);
    const bool lifecycle = destroyed && nextId > secondId && !entities.find(firstId);

    std::cout << "asset_rest=" << restMatches << " yaw=" << yaw << " pitch=" << pitch
              << " independent_instances=" << independent << " reset=" << reset
              << " shared_pitch_distinct_muzzles=" << sharedPitch << " shared_definition=" << sharedAsset
              << " independent_transform=" << movedAlone << " ship_lifecycle=" << lifecycle << '\n';
    return restMatches && yaw && pitch && independent && reset && sharedPitch &&
        sharedAsset && movedAlone && lifecycle ? 0 : 1;
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
    const bool entities = checkEntityManager();
    const bool movement = checkMovement();
    const bool ok = pivotPreserved && ordinaryReset && nativePitch && nativeReset && strict && entities && movement;
    std::cout << "ordinary=" << (pivotPreserved && ordinaryReset)
              << " dof=" << (nativePitch && nativeReset) << " strict_paths=" << strict
              << " entities=" << entities << '\n';
    return ok ? 0 : 1;
}
