#include "BattleScene.h"
#include "OceanLoader.h"
#include "ShipManager.h"
#include "SkyLoader.h"
#include "../battle/DemoScenario.h"
#include "NavalEffects.h"
#include "../app/ShipWeaponSettings.h"

#include <QtGlobal>
#include <osgUtil/IntersectionVisitor>
#include <osgUtil/LineSegmentIntersector>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {
void logProjectile(double time, unsigned id, unsigned source, const char* event, const osg::Vec3d& position) {
    std::cout << std::fixed << std::setprecision(3) << "t=" << time
              << " from=" << source + 1 << " projectile=" << id << " event=" << event << " xyz=("
              << position.x() << ',' << position.y() << ',' << position.z() << ')' << std::endl;
}
}

struct BattleScene::Impl {
    Impl(osgViewer::Viewer& viewer, bool moving, bool quadratic, bool wows, ocean_gambit::ballistics::Integrator integrator, bool playerControlled)
        : viewer_(viewer), ships_(*root_), scenario_(moving), effects_(viewer, *root_, ships_),
          projectiles_([&] {
              auto ammo = wows ? AmmoConfig::wowsDemo() :
                          quadratic ? AmmoConfig::quadraticDemo() : AmmoConfig::exponentialDemo();
              if (wows) ammo.integrator = integrator;
              return ammo;
          }()), playerControlled_(playerControlled) {
        loadSky(*viewer_.getCamera());
        ocean_ = loadOcean(*root_);
        loadShipManager();
        viewer_.setSceneData(root_);
    }

    void advance(double now) {
        while ((tick_ + 1) * ProjectileSystem::interval <= now + 1e-9) {
            const double time = ++tick_ * ProjectileSystem::interval;
            updateShipPoses(time);
            projectiles_.step(
                [this](unsigned source, const auto& from, const auto& to) { return trace(source, from, to); },
                [this, time](unsigned id, const auto& position) { effects_.sample(id, position, time); },
                [this, time](unsigned id, unsigned source, const auto& hit) {
                    const double contactTime = hit ? time - (1 - hit->fraction) * ProjectileSystem::interval : time;
                    if (hit) logProjectile(contactTime, id, source, hit->ship ? "hull" : "water", hit->position);
                    effects_.contact(id, hit, contactTime);
                });
            if (const auto cue = scenario_.nextShot(time)) {
                if (!playerControlled_ || cue->from != controlledShip_) launch(*cue, time);
            }
            for (const auto& request : playerRequests_)
                launchPlayer(request.ship, request.target, time);
            playerRequests_.clear();
        }
        effects_.advance(now);
    }

    void launch(const ShotCue& cue, double time) {
        const auto poses = scenario_.poses(time);
        const auto& target = poses.at(cue.to);
        const osg::Vec3d targetPoint(target.x, target.y, 3.8);
        // First pose provides the current muzzle origin; use it to estimate elevation.
        const auto initial = ships_.ship(cue.from).poseTurret(targetPoint);
        osg::Vec3d horizontal = targetPoint - initial.position;
        horizontal.z() = 0;
        const double range = horizontal.length();
        if (range > 0) horizontal /= range;
        horizontal = osg::Matrix::transform3x3(horizontal,
            osg::Matrix::rotate(cue.yawOffset, 0, 0, 1));
        const auto gun = gunSettings_.at(cue.from);
        const auto solution = projectiles_.solveElevation(
            range, targetPoint.z() - initial.position.z(),
            gun.arc, gun.minElevationRad(), gun.maxElevationRad());
        if (!solution) {
            std::cerr << "No firing solution at range=" << range << '\n';
            return;
        }
        const double elevation = *solution;
        const osg::Vec3d aimPoint = initial.position + horizontal * range +
            osg::Vec3d(0, 0, std::tan(elevation) * range);
        const auto muzzle = ships_.ship(cue.from).poseTurret(aimPoint);
        spawn(cue.from, muzzle, time);
    }

    // Aim from the physical muzzle, not from the camera's bridge/eye point.
    // Barrel rotation can move the muzzle, so iteratively re-evaluate its
    // world origin before solving the low-arc launch elevation.
    void launchPlayer(ShipID source, const osg::Vec3d& target, double time) {
        if (source >= ships_.count()) return;
        auto& ship = ships_.ship(source);
        const auto gun = gunSettings_.at(source);
        MuzzlePose muzzle = ship.poseTurret(target);
        double elevation = 0;
        for (int i = 0; i < 5; ++i) {
            osg::Vec3d horizontal = target - muzzle.position;
            horizontal.z() = 0;
            const double range = horizontal.length();
            if (range < 1e-3) {
                std::cerr << "Fire control: target is too close to muzzle\n";
                return;
            }
            const auto solution = projectiles_.solveElevation(
                range, target.z() - muzzle.position.z(),
                gun.arc, gun.minElevationRad(), gun.maxElevationRad());
            if (!solution) {
                std::cerr << "Fire control: no ballistic solution, range="
                          << range << " height=" << target.z() - muzzle.position.z() << '\n';
                return;
            }
            elevation = *solution;
            horizontal /= range;
            const osg::Vec3d direction =
                horizontal * std::cos(elevation) + osg::Vec3d(0, 0, std::sin(elevation));
            const auto next = ship.poseTurretDirection(direction);
            const double delta = (next.position - muzzle.position).length();
            muzzle = next;
            if (delta < 1e-5) break;
        }
        std::cout << "fire_control ship=" << source + 1
                  << " arc=" << (gun.arc == FireArc::High ? "high" : "low")
                  << " elevation_deg=" << elevation * (180.0 / 3.14159265358979323846)
                  << " target=(" << target.x() << ',' << target.y() << ',' << target.z()
                  << ") muzzle=(" << muzzle.position.x() << ','
                  << muzzle.position.y() << ',' << muzzle.position.z() << ")\n";
        spawn(source, muzzle, time);
    }

    void spawn(ShipID source, const MuzzlePose& muzzle, double time) {
        const unsigned id = ++projectileId_;
        ++launchCounts_.at(source);
        const auto previous = scenario_.poses(time - ProjectileSystem::interval).at(source);
        const auto current = scenario_.poses(time).at(source);
        const osg::Vec3d motion((current.x - previous.x) / ProjectileSystem::interval,
                               (current.y - previous.y) / ProjectileSystem::interval, 0);
        projectiles_.launch(id, source, muzzle.position, muzzle.forward, motion);
        effects_.launch(id, muzzle, time);
        logProjectile(time, id, source, "launch", muzzle.position);
    }

    std::optional<ProjectileContact> trace(unsigned source, const osg::Vec3d& from,
                                           const osg::Vec3d& to) {
        std::optional<ProjectileContact> nearest;
        if (from.z() >= 0 && to.z() < 0) {
            const double portion = from.z() / (from.z() - to.z());
            nearest = ProjectileContact{portion, from + (to - from) * portion, {0, 0, 1}, std::nullopt};
        }
        for (unsigned target = 0; target < scenario_.poses(0).size(); ++target) {
            if (target == source) continue;
            auto& ship = ships_.ship(target);
            const auto matrix = ship.node().getMatrix();
            const auto inverse = osg::Matrix::inverse(matrix);
            osg::ref_ptr<osgUtil::LineSegmentIntersector> segment =
                new osgUtil::LineSegmentIntersector(from * inverse, to * inverse);
            osgUtil::IntersectionVisitor query(segment);
            ship.model().accept(query);
            if (!segment->containsIntersections()) continue;
            const auto hit = segment->getFirstIntersection();
            if (!nearest || hit.ratio < nearest->fraction)
                nearest = ProjectileContact{hit.ratio, hit.getWorldIntersectPoint() * matrix,
                    osg::Matrix::transform3x3(hit.getWorldIntersectNormal(), matrix), target};
        }
        return nearest;
    }

    SceneStatus status() const {
        const auto state = effects_.status();
        return {ocean_->isInitialized(), state.decalCounts, state.flight, state.splash,
                state.decalAttached, state.decalMoved, state.explosions, launchCounts_};
    }

    void loadShipManager() {
        const auto model = qEnvironmentVariable("OCEAN_MODEL").toStdString();
        for (const auto& pose : scenario_.poses(0)) {
            const auto id = ships_.addShip(model);
            ships_.ship(id).place(pose.x, pose.y, pose.heading);
            gunSettings_.push_back(loadShipWeaponSettings(model));
        }
    }

    void updateShipPoses(double now) {
        const auto poses = scenario_.poses(now);
        for (unsigned id = 0; id < poses.size(); ++id) {
            const auto& pose = poses[id];
            ships_.ship(id).place(pose.x, pose.y, pose.heading);
        }
    }

    osgViewer::Viewer& viewer_;
    osg::ref_ptr<osg::Group> root_ = new osg::Group;
    osg::ref_ptr<TritonOceanSurface> ocean_;
    ShipManager ships_;
    DemoScenario scenario_;
    NavalEffects effects_;
    ProjectileSystem projectiles_;
    std::vector<ShipWeaponSettings> gunSettings_;
    bool playerControlled_;
    ShipID controlledShip_ = 0;
    struct PlayerShot { ShipID ship; osg::Vec3d target; };
    unsigned tick_ = 0;
    unsigned projectileId_ = 0;
    std::array<unsigned, 2> launchCounts_{};
    std::vector<PlayerShot> playerRequests_;
};

BattleScene::BattleScene(osgViewer::Viewer& viewer, bool moving, bool quadratic,
                         bool wows, ocean_gambit::ballistics::Integrator integrator, bool playerControlled)
    : impl_(std::make_unique<Impl>(viewer, moving, quadratic, wows, integrator, playerControlled)) {}
BattleScene::~BattleScene() = default;
osg::Group& BattleScene::root() const { return *impl_->root_; }
void BattleScene::advance(double now) { impl_->advance(now); }
void BattleScene::setControlledShip(ShipID ship) {
    if (impl_->playerControlled_ && ship < impl_->ships_.count())
        impl_->controlledShip_ = ship;
}

void BattleScene::requestPlayerShot(ShipID ship, const osg::Vec3d& target) {
    if (impl_->playerControlled_ && ship == impl_->controlledShip_ &&
        ship < impl_->ships_.count() &&
        std::isfinite(target.x()) && std::isfinite(target.y()) && std::isfinite(target.z()))
        impl_->playerRequests_.push_back({ship, target});
}
bool BattleScene::oceanReady() const { return impl_->ocean_->isInitialized(); }
SceneStatus BattleScene::status() const { return impl_->status(); }

std::optional<std::string> BattleScene::shipModelPath(ShipID id) const {
    if (id >= impl_->ships_.count()) return std::nullopt;
    return impl_->ships_.ship(id).modelPath();
}

std::optional<osg::Matrixd> BattleScene::shipTransform(ShipID id) const {
    if (id >= impl_->ships_.count()) return std::nullopt;
    return impl_->ships_.ship(id).node().getMatrix();
}

void BattleScene::setBridgeHidden(ShipID id, bool hidden) {
    if (id < impl_->ships_.count()) impl_->ships_.ship(id).setBridgeHidden(hidden);
}

std::optional<CameraTarget> BattleScene::pickTarget(const CameraRay& ray, ShipID exclude) const {
    constexpr double maxPickDistance = 14000;
    const auto end = ray.origin + ray.direction * maxPickDistance;
    std::optional<CameraTarget> result;
    double nearest = 1.;
    for (ShipID id = 0; id < impl_->ships_.count(); ++id) {
        if (id == exclude) continue;
        const auto& ship = impl_->ships_.ship(id);
        const auto pose = ship.node().getMatrix();
        const auto inverse = osg::Matrixd::inverse(pose);
        osg::ref_ptr<osgUtil::LineSegmentIntersector> line =
            new osgUtil::LineSegmentIntersector(ray.origin * inverse, end * inverse);
        osgUtil::IntersectionVisitor query(line);
        ship.model().accept(query);
        if (!line->containsIntersections()) continue;
        const auto hit = line->getFirstIntersection();
        if (hit.ratio <= nearest) {
            nearest = hit.ratio;
            result = CameraTarget{hit.getWorldIntersectPoint() * pose, id};
        }
    }
    if (result) return result;
    if (const auto sea = ShipCameraController::intersectSea(ray))
        return CameraTarget{*sea, std::nullopt};
    return std::nullopt;
}
