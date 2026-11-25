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

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {
constexpr OwnerID localPlayer = 1;

void logProjectile(double time, unsigned id, unsigned source, const char* event, const osg::Vec3d& position) {
    std::cout << std::fixed << std::setprecision(3) << "t=" << time
              << " from=" << source + 1 << " projectile=" << id << " event=" << event << " xyz=("
              << position.x() << ',' << position.y() << ',' << position.z() << ')' << std::endl;
}
}

struct BattleScene::Impl {
    Impl(osgViewer::Viewer& viewer, bool moving, bool quadratic, bool wows, ocean_gambit::ballistics::Integrator integrator, bool playerControlled, bool extraShips)
        : viewer_(viewer), entities_(*root_), scenario_(moving),
          effects_(viewer, *root_, [this](unsigned slot) { return findBattleShip(slot); }),
          projectiles_([&] {
              auto ammo = wows ? AmmoConfig::wowsDemo() :
                          quadratic ? AmmoConfig::quadraticDemo() : AmmoConfig::exponentialDemo();
              if (wows) ammo.integrator = integrator;
              return ammo;
          }()), playerControlled_(playerControlled) {
        loadSky(*viewer_.getCamera());
        ocean_ = loadOcean(*root_);
        entities_.registerFactory(EntityType::Ship, SelectionMode::Multiple,
            [this](const std::string& code) { return shipAssets_.build(code); });
        loadShips(extraShips);
        viewer_.setSceneData(root_);
    }

    void advance(double now) {
        while ((tick_ + 1) * ProjectileSystem::interval <= now + 1e-9) {
            const double time = ++tick_ * ProjectileSystem::interval;
            if (rtsMovementEnabled_) {
                updateMovement(time);
                updateWeapons(time);
            } else if (!rtsWorld_) updateShipPoses(time);
            projectiles_.step(
                [this](unsigned source, const auto& from, const auto& to) { return trace(source, from, to); },
                [this, time](unsigned id, const auto& position) { effects_.sample(id, position, time); },
                [this, time](unsigned id, unsigned source, const auto& hit) {
                    const double contactTime = hit ? time - (1 - hit->fraction) * ProjectileSystem::interval : time;
                    if (hit) logProjectile(contactTime, id, source, hit->ship ? "hull" : "water", hit->position);
                    effects_.contact(id, hit, contactTime);
                });
            if (!rtsWorld_) {
                if (const auto cue = scenario_.nextShot(time)) {
                    if ((!playerControlled_ || cue->from != controlledShip_) &&
                        findBattleShip(cue->from) && findBattleShip(cue->to)) launch(*cue, time);
                }
            }
            for (const auto& request : playerRequests_) {
                if (request.free) launchFree(request.ship, request.target, time);
                else launchPlayer(request.ship, request.target, time);
            }
            playerRequests_.clear();
        }
        if (moveMarker_ && now >= markerExpires_) moveMarker_->setNodeMask(0);
        effects_.advance(now);
    }

    void updateMovement(double now) {
        for (const auto& order : moveOrders_) {
            bool accepted = false;
            for (const auto id : order.ships) {
                auto* entity = entities_.find(id);
                if (!entity || entity->owner() != localPlayer || !entity->movement()) continue;
                entity->movement()->moveTo(order.destination);
                accepted = true;
            }
            if (accepted) showMoveMarker(order.destination, now);
        }
        moveOrders_.clear();

        for (const auto id : entities_.ids()) {
            auto& entity = *entities_.find(id);
            auto* movement = entity.movement();
            if (!movement || movement->state() != MovementState::Moving) continue;
            auto pose = entity.transform();
            movement->step(pose, ProjectileSystem::interval);
            entity.setTransform(pose);
        }
    }

    // RTS target acquisition is independent of movement orders. No fog of war in V1.
    void updateWeapons(double now) {
        const auto ids = entities_.findByType(EntityType::Ship);
        for (const auto sourceId : ids) {
            auto* source = entities_.find(sourceId);
            if (!source || !source->weapon() || source->lifecycle() != EntityLifecycle::Active) continue;
            auto& weapon = *source->weapon();

            const auto valid = [&](EntityID targetId) {
                const auto* target = entities_.find(targetId);
                if (!target || target->type() != EntityType::Ship ||
                    target->lifecycle() != EntityLifecycle::Active ||
                    target->owner() == source->owner()) return false;
                const auto delta = target->transform().position - source->transform().position;
                return std::hypot(delta.x(), delta.y()) <= weapon.range();
            };
            // Retain an existing valid lock; otherwise choose the nearest enemy in range.
            if (!weapon.target() || !valid(*weapon.target())) {
                std::optional<EntityID> nearest;
                double bestRange2 = weapon.range() * weapon.range();
                for (const auto targetId : ids) {
                    if (!valid(targetId)) continue;
                    const auto delta = entities_.find(targetId)->transform().position -
                                       source->transform().position;
                    const double range2 = delta.x() * delta.x() + delta.y() * delta.y();
                    if (range2 < bestRange2) {
                        bestRange2 = range2;
                        nearest = targetId;
                    }
                }
                weapon.track(nearest);
            }
            if (!weapon.target() || !weapon.ready(now)) continue;
            const auto slot = std::find(battleShips_.begin(), battleShips_.end(), sourceId);
            if (slot == battleShips_.end()) continue;
            launchAuto(static_cast<unsigned>(slot - battleShips_.begin()), *weapon.target(), now);
            weapon.fired(now);
        }
    }

    void launchAuto(unsigned slot, EntityID targetId, double now) {
        auto* target = entities_.find(targetId);
        auto* source = battleEntity(slot);
        if (!source || !target || !target->weapon()) return;
        osg::Vec3d aim = target->transform().position + osg::Vec3d(0, 0, 3.8);
        // First-order lead from target's current velocity and the existing ballistics
        // flight-time solver. The live fire-control solver handles shooter motion.
        osg::Vec3d velocity;
        if (const auto* movement = target->movement()) {
            const double speed = movement->speed();
            const auto heading = target->transform().heading;
            velocity.set(-std::sin(heading) * speed, std::cos(heading) * speed, 0);
        }
        if (velocity.length2() > 0) {
            const double range = std::hypot(aim.x() - source->transform().position.x(),
                                            aim.y() - source->transform().position.y());
            const auto& gun = gunSettings_.at(slot);
            const auto estimate = projectiles_.fireControlSolution(
                range, aim.z() - source->transform().position.z(),
                gun.arc, gun.minElevationRad(), gun.maxElevationRad());
            const double flight = estimate.flightSeconds.value_or(
                range / std::max(1.0, projectiles_.ammo().launchPace));
            aim += velocity * flight;
        }
        launchPlayer(slot, aim, now);
    }

    void showMoveMarker(const osg::Vec3d& destination, double now) {
        if (!moveMarker_) {
            moveMarker_ = new osg::MatrixTransform;
            moveMarker_->addChild(createMoveMarker());
            root_->addChild(moveMarker_);
        }
        moveMarker_->setMatrix(osg::Matrixd::translate(destination.x(), destination.y(), 0));
        moveMarker_->setNodeMask(~0u);
        markerExpires_ = now + 1.4;
    }

    osg::Vec3d shipMotion(ShipID source, double time) const {
        if (rtsWorld_) {
            const auto* entity = battleEntity(source);
            if (!entity || !entity->movement()) return {};
            const auto speed = entity->movement()->speed();
            const auto heading = entity->transform().heading;
            return {-std::sin(heading) * speed, std::cos(heading) * speed, 0};
        }
        const auto prev = scenario_.poses(time - ProjectileSystem::interval).at(source);
        const auto pose = scenario_.poses(time).at(source);
        return {(pose.x - prev.x) / ProjectileSystem::interval,
                (pose.y - prev.y) / ProjectileSystem::interval, 0};
    }

    static osg::Vec3d boundedDirection(osg::Vec3d direction,
                                      const ShipWeaponSettings& gun) {
        if (direction.normalize() == 0) return {0, 1, 0};
        const double yaw = std::atan2(direction.y(), direction.x());
        const double elevation = std::clamp(
            std::atan2(direction.z(), std::hypot(direction.x(), direction.y())),
            gun.minElevationRad(), gun.maxElevationRad());
        return {std::cos(yaw) * std::cos(elevation),
                std::sin(yaw) * std::cos(elevation), std::sin(elevation)};
    }

    void launch(const ShotCue& cue, double time) {
        const auto poses = scenario_.poses(time);
        const auto& target = poses.at(cue.to);
        const osg::Vec3d targetPoint(target.x, target.y, 3.8);
        auto& ship = *findBattleShip(cue.from);
        const auto gun = gunSettings_.at(cue.from);
        const auto motion = gun.compensateShipMotion ? shipMotion(cue.from, time)
                                                     : osg::Vec3d{};
        auto muzzle = ship.poseTurret(targetPoint);
        double flight = (targetPoint - muzzle.position).length() /
                        projectiles_.ammo().launchPace;
        bool fallback = false;
        for (int iteration = 0; iteration < 6; ++iteration) {
            const osg::Vec3d adjusted = targetPoint - motion * flight;
            osg::Vec3d horizontal = adjusted - muzzle.position;
            horizontal.z() = 0;
            const double range = horizontal.length();
            if (range < 1e-3) return;
            horizontal /= range;
            horizontal = osg::Matrix::transform3x3(horizontal,
                osg::Matrix::rotate(cue.yawOffset, 0, 0, 1));
            const auto solution = projectiles_.fireControlSolution(
                range, adjusted.z() - muzzle.position.z(),
                gun.arc, gun.minElevationRad(), gun.maxElevationRad());
            if (!solution.elevation) {
                std::cerr << "Fire control AI: " << fireControlStatusName(solution.status)
                          << " ship=" << cue.from + 1 << " range=" << range << '\n';
                return;
            }
            fallback = solution.status == FireControlStatus::FallbackLow;
            const auto direction = horizontal * std::cos(*solution.elevation) +
                osg::Vec3d(0, 0, std::sin(*solution.elevation));
            const auto next = ship.poseTurretDirection(boundedDirection(direction, gun));
            const double nextFlight = solution.flightSeconds.value_or(flight);
            const double delta = (next.position - muzzle.position).length();
            const double flightDelta = std::abs(nextFlight - flight);
            muzzle = next;
            flight = nextFlight;
            if (delta < 1e-4 && flightDelta < .001) break;
        }
        if (fallback)
            std::cout << "Fire control AI: high arc unavailable, using low arc ship="
                      << cue.from + 1 << '\n';
        spawn(cue.from, muzzle, time);
    }

    // Aim from the physical muzzle; account for turret-induced displacement.
    void launchPlayer(ShipID source, const osg::Vec3d& target, double time) {
        auto* active = findBattleShip(source);
        if (!active) return;
        auto& ship = *active;
        const auto gun = gunSettings_.at(source);
        MuzzlePose muzzle = ship.poseTurret(target);
        double elevation = 0;
        double flight = (target - muzzle.position).length() /
                        projectiles_.ammo().launchPace;
        const auto motion = gun.compensateShipMotion ? shipMotion(source, time)
                                                     : osg::Vec3d{};
        FireControlStatus status = FireControlStatus::Ready;
        FireArc chosenArc = gun.arc;
        for (int i = 0; i < 6; ++i) {
            // Use solved flight time to correct for inherited platform motion,
            // while the live projectile physics remains unchanged.
            const osg::Vec3d predictedPoint = target - motion * flight;
            osg::Vec3d horizontal = predictedPoint - muzzle.position;
            horizontal.z() = 0;
            const double range = horizontal.length();
            if (range < 1e-3) {
                std::cerr << "Fire control: target is too close to muzzle\n";
                return;
            }
            const auto solution = projectiles_.fireControlSolution(
                range, predictedPoint.z() - muzzle.position.z(),
                gun.arc, gun.minElevationRad(), gun.maxElevationRad());
            if (!solution.elevation) {
                std::cerr << "Fire control: " << fireControlStatusName(solution.status)
                          << " ship=" << source + 1
                          << " range=" << range
                          << " height=" << target.z() - muzzle.position.z() << '\n';
                return;
            }
            elevation = *solution.elevation;
            status = solution.status;
            chosenArc = solution.selectedArc;
            horizontal /= range;
            const osg::Vec3d direction =
                horizontal * std::cos(elevation) + osg::Vec3d(0, 0, std::sin(elevation));
            const auto next = ship.poseTurretDirection(boundedDirection(direction, gun));
            const double nextFlight = solution.flightSeconds.value_or(flight);
            const double delta = (next.position - muzzle.position).length();
            const double flightDelta = std::abs(nextFlight - flight);
            muzzle = next;
            flight = nextFlight;
            if (delta < 1e-4 && flightDelta < .001) break;
        }
        std::cout << "fire_control ship=" << source + 1
                  << " arc=" << (chosenArc == FireArc::High ? "high" : "low")
                  << " status=" << fireControlStatusName(status)
                  << " motion_compensation=" << (gun.compensateShipMotion ? "on" : "off")
                  << " flight_seconds=" << flight
                  << " elevation_deg=" << elevation * (180.0 / 3.14159265358979323846)
                  << " target=(" << target.x() << ',' << target.y() << ',' << target.z()
                  << ") muzzle=(" << muzzle.position.x() << ','
                  << muzzle.position.y() << ',' << muzzle.position.z() << ")\n";
        spawn(source, muzzle, time);
    }

    void launchFree(ShipID source, const osg::Vec3d& viewDirection, double time) {
        auto* active = findBattleShip(source);
        if (!active) return;
        const auto& gun = gunSettings_.at(source);
        auto constrained = boundedDirection(viewDirection, gun);
        const double actual = std::atan2(constrained.z(),
                                         std::hypot(constrained.x(), constrained.y()));
        auto muzzle = active->poseTurretDirection(constrained);
        std::cout << "fire_control ship=" << source + 1
                  << " mode=free elevation_deg=" << actual * (180.0 / 3.14159265358979323846)
                  << " motion_compensation=not_applicable" << '\n';
        spawn(source, muzzle, time);
    }

    void spawn(ShipID source, const MuzzlePose& muzzle, double time) {
        const unsigned id = ++projectileId_;
        if (source < launchCounts_.size()) ++launchCounts_[source];
        projectiles_.launch(id, source, muzzle.position, muzzle.forward,
                            shipMotion(source, time));
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
        for (unsigned target = 0; target < battleShips_.size(); ++target) {
            auto* candidate = findBattleShip(target);
            if (target == source || !candidate) continue;
            auto& ship = *candidate;
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

    // The original demo uses slots for its fixed script, projectile events and HUD.
    // Resolve those slots to IDs at the boundary. They are never entity IDs.
    Entity* battleEntity(unsigned slot) {
        return slot < battleShips_.size() ? entities_.find(battleShips_[slot]) : nullptr;
    }

    ShipController* findBattleShip(unsigned slot) {
        auto* entity = battleEntity(slot);
        return entity ? static_cast<ShipController*>(&entity->visual()) : nullptr;
    }

    void loadShips(bool extraShips) {
        const auto add = [this](const ShipPose& pose, OwnerID owner) {
            const auto id = entities_.createEntity(EntityType::Ship, "020528", owner);
            entities_.find(id)->setTransform({{pose.x, pose.y, 0}, pose.heading});
            return id;
        };
        const auto poses = scenario_.poses(0);
        for (unsigned slot = 0; slot < poses.size(); ++slot) {
            battleShips_.push_back(add(poses[slot], slot + 1));
            gunSettings_.push_back(loadShipWeaponSettings(findBattleShip(slot)->modelPath()));
        }
        if (extraShips) {
            const std::array<ShipPose, 4> formation{
                {{-255, -265, osg::PI_4}, {255, -265, osg::PI_4},
                 {-255, 265, 3 * osg::PI_4}, {255, 265, 3 * osg::PI_4}}};
            for (const auto& pose : formation) {
                battleShips_.push_back(add(pose, pose.y < 0 ? 1 : 2));
                gunSettings_.push_back(loadShipWeaponSettings(
                    findBattleShip(static_cast<unsigned>(battleShips_.size() - 1))->modelPath()));
            }
        }
    }

    void updateShipPoses(double now) {
        const auto poses = scenario_.poses(now);
        for (unsigned id = 0; id < poses.size(); ++id) {
            const auto& pose = poses[id];
            if (auto* entity = battleEntity(id))
                entity->setTransform({{pose.x, pose.y, 0}, pose.heading});
        }
    }

    osgViewer::Viewer& viewer_;
    osg::ref_ptr<osg::Group> root_ = new osg::Group;
    osg::ref_ptr<TritonOceanSurface> ocean_;
    ShipManager shipAssets_;
    EntityManager entities_;
    std::vector<EntityID> battleShips_;
    DemoScenario scenario_;
    NavalEffects effects_;
    ProjectileSystem projectiles_;
    std::vector<ShipWeaponSettings> gunSettings_;
    bool playerControlled_;
    bool rtsWorld_ = false;
    bool rtsMovementEnabled_ = false;
    struct MoveOrder { std::vector<EntityID> ships; osg::Vec3d destination; };
    std::vector<MoveOrder> moveOrders_;
    osg::ref_ptr<osg::MatrixTransform> moveMarker_;
    double markerExpires_ = 0;
    ShipID controlledShip_ = 0;
    struct PlayerShot { ShipID ship; osg::Vec3d target; bool free = false; };
    unsigned tick_ = 0;
    unsigned projectileId_ = 0;
    std::array<unsigned, 2> launchCounts_{};
    std::vector<PlayerShot> playerRequests_;
};

BattleScene::BattleScene(osgViewer::Viewer& viewer, bool moving, bool quadratic,
                         bool wows, ocean_gambit::ballistics::Integrator integrator, bool playerControlled, bool extraShips)
    : impl_(std::make_unique<Impl>(viewer, moving, quadratic, wows, integrator, playerControlled, extraShips)) {}
BattleScene::~BattleScene() = default;
osg::Group& BattleScene::root() const { return *impl_->root_; }
void BattleScene::advance(double now) { impl_->advance(now); }
void BattleScene::setRtsMovementEnabled(bool enabled) {
    impl_->rtsMovementEnabled_ = enabled;
    impl_->rtsWorld_ |= enabled;
    if (!enabled) {
        impl_->moveOrders_.clear();
        if (impl_->moveMarker_) impl_->moveMarker_->setNodeMask(0);
    }
}

void BattleScene::requestMove(const std::vector<EntityID>& selected, const osg::Vec3d& destination) {
    if (impl_->rtsMovementEnabled_ && !selected.empty() &&
        std::isfinite(destination.x()) && std::isfinite(destination.y()))
        impl_->moveOrders_.push_back({selected, {destination.x(), destination.y(), 0}});
}

std::optional<EntityID> BattleScene::pickOwnShip(const osg::Vec3d& from, const osg::Vec3d& to) const {
    std::optional<EntityID> selected;
    double nearest = 1;
    for (const auto id : impl_->entities_.findByOwner(localPlayer)) {
        const auto& entity = *impl_->entities_.find(id);
        if (entity.type() != EntityType::Ship || entity.selectionMode() == SelectionMode::None) continue;
        const auto& ship = static_cast<const ShipController&>(entity.visual());
        const auto inverse = osg::Matrixd::inverse(ship.node().getMatrix());
        osg::ref_ptr<osgUtil::LineSegmentIntersector> line =
            new osgUtil::LineSegmentIntersector(from * inverse, to * inverse);
        osgUtil::IntersectionVisitor query(line);
        ship.model().accept(query);
        if (!line->containsIntersections()) continue;
        const auto ratio = line->getFirstIntersection().ratio;
        if (ratio >= nearest) continue;
        selected = id;
        nearest = ratio;
    }
    return selected;
}

void BattleScene::setPlayerControlEnabled(bool enabled) {
    impl_->playerControlled_ = enabled;
    if (!enabled) impl_->playerRequests_.clear();
}

void BattleScene::setControlledShip(ShipID ship) {
    if (impl_->playerControlled_ && ship < impl_->gunSettings_.size())
        impl_->controlledShip_ = ship;
}

void BattleScene::requestPlayerShot(ShipID ship, const osg::Vec3d& target) {
    if (impl_->playerControlled_ && ship == impl_->controlledShip_ &&
        ship < impl_->gunSettings_.size() &&
        std::isfinite(target.x()) && std::isfinite(target.y()) && std::isfinite(target.z()))
        impl_->playerRequests_.push_back({ship, target});
}
void BattleScene::requestPlayerFreeShot(ShipID ship, const osg::Vec3d& direction) {
    if (impl_->playerControlled_ && ship == impl_->controlledShip_ &&
        ship < impl_->gunSettings_.size() &&
        std::isfinite(direction.x()) && std::isfinite(direction.y()) &&
        std::isfinite(direction.z()) && direction.length2() > 1e-12)
        impl_->playerRequests_.push_back({ship, direction, true});
}
bool BattleScene::oceanReady() const { return impl_->ocean_->isInitialized(); }
SceneStatus BattleScene::status() const { return impl_->status(); }

std::optional<std::string> BattleScene::shipModelPath(ShipID id) const {
    if (const auto* ship = impl_->findBattleShip(id)) return ship->modelPath();
    return std::nullopt;
}

std::optional<osg::Vec3d> BattleScene::shipBridgeEye(ShipID id) const {
    if (const auto* ship = impl_->findBattleShip(id)) return ship->viewEye("bridge");
    return std::nullopt;
}

std::optional<osg::Matrixd> BattleScene::shipTransform(ShipID id) const {
    if (const auto* ship = impl_->findBattleShip(id)) return ship->node().getMatrix();
    return std::nullopt;
}

std::vector<EntityID> BattleScene::shipsInScreenBox(const ShipSelectionBox& box,
                                                 const osg::Matrixd& viewProjection) const {
    std::vector<EntityID> selected;
    for (const EntityID id : impl_->entities_.findSelectable(SelectionMode::Multiple)) {
        const auto& entity = *impl_->entities_.find(id);
        if (entity.type() != EntityType::Ship || entity.owner() != localPlayer) continue;
        const auto& ship = static_cast<const ShipController&>(entity.visual());
        const auto bounds = ship.model().getBound();
        if (!bounds.valid()) continue;
        const auto center = osg::Vec3d(bounds.center()) * ship.node().getMatrix();
        if (box.contains(center, viewProjection)) selected.push_back(id);
    }
    return selected;
}

void BattleScene::showShipSelection(const std::vector<EntityID>& selected) {
    for (const auto id : impl_->entities_.findByType(EntityType::Ship)) {
        auto& entity = *impl_->entities_.find(id);
        auto& ship = static_cast<ShipController&>(entity.visual());
        ship.setSelectionVisible(entity.owner() == localPlayer && entity.selectionMode() != SelectionMode::None &&
            std::find(selected.begin(), selected.end(), id) != selected.end());
    }
}

void BattleScene::setBridgeHidden(ShipID id, bool hidden) {
    if (auto* ship = impl_->findBattleShip(id)) ship->setBridgeHidden(hidden);
}

std::optional<CameraTarget> BattleScene::pickTarget(const CameraRay& ray, ShipID exclude) const {
    constexpr double maxPickDistance = 14000;
    const auto end = ray.origin + ray.direction * maxPickDistance;
    std::optional<CameraTarget> result;
    double nearest = 1.;
    for (ShipID id = 0; id < impl_->gunSettings_.size(); ++id) {
        const auto* candidate = impl_->findBattleShip(id);
        if (id == exclude || !candidate) continue;
        const auto& ship = *candidate;
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
