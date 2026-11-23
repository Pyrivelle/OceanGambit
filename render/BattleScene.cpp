#include "BattleScene.h"
#include "OceanLoader.h"
#include "ShipManager.h"
#include "SkyLoader.h"
#include "../battle/DemoScenario.h"
#include "NavalEffects.h"

#include <QtGlobal>
#include <osgUtil/IntersectionVisitor>
#include <osgUtil/LineSegmentIntersector>

#include <iomanip>
#include <iostream>

namespace {
void logProjectile(double time, unsigned id, unsigned source, const char* event, const osg::Vec3d& position) {
    std::cout << std::fixed << std::setprecision(3) << "t=" << time
              << " from=" << source + 1 << " projectile=" << id << " event=" << event << " xyz=("
              << position.x() << ',' << position.y() << ',' << position.z() << ')' << std::endl;
}
}

struct BattleScene::Impl {
    Impl(osgViewer::Viewer& viewer, bool moving)
        : viewer_(viewer), ships_(*root_), scenario_(moving), effects_(viewer, *root_, ships_) {
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
            if (const auto cue = scenario_.nextShot(time)) launch(*cue, time);
        }
        effects_.advance(now);
    }

    void launch(const ShotCue& cue, double time) {
        const auto poses = scenario_.poses(0);
        const auto& source = poses.at(cue.from);
        const auto& target = poses.at(cue.to);
        const osg::Vec3d start(source.x, source.y, 0);
        const auto heading = osg::Matrix::rotate(cue.yawOffset, 0, 0, 1);
        const auto offset = osg::Matrix::transform3x3(
            osg::Vec3d(target.x - source.x, target.y - source.y, 22), heading);
        const auto muzzle = ships_.ship(cue.from).poseTurret(start + offset);
        const auto previous = scenario_.poses(time - ProjectileSystem::interval).at(cue.from);
        const auto current = scenario_.poses(time).at(cue.from);
        const osg::Vec3d motion((current.x - previous.x) / ProjectileSystem::interval,
                               (current.y - previous.y) / ProjectileSystem::interval, 0);
        projectiles_.launch(cue.id, cue.from, muzzle.position, muzzle.forward, motion);
        effects_.launch(cue.id, muzzle, time);
        logProjectile(time, cue.id, cue.from, "launch", muzzle.position);
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
                state.decalAttached, state.decalMoved, state.explosions};
    }

    void loadShipManager() {
        const auto model = qEnvironmentVariable("OCEAN_MODEL").toStdString();
        for (const auto& pose : scenario_.poses(0))
            ships_.ship(ships_.addShip(model)).place(pose.x, pose.y, pose.heading);
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
    unsigned tick_ = 0;
};

BattleScene::BattleScene(osgViewer::Viewer& viewer, bool moving)
    : impl_(std::make_unique<Impl>(viewer, moving)) {}
BattleScene::~BattleScene() = default;
osg::Group& BattleScene::root() const { return *impl_->root_; }
void BattleScene::advance(double now) { impl_->advance(now); }
bool BattleScene::oceanReady() const { return impl_->ocean_->isInitialized(); }
SceneStatus BattleScene::status() const { return impl_->status(); }
