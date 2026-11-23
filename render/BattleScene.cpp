#include "BattleScene.h"
#include "OceanLoader.h"
#include "ShipManager.h"
#include "SkyLoader.h"
#include "../battle/DemoScenario.h"
#include "NavalEffects.h"

#include <QtGlobal>

struct BattleScene::Impl {
    Impl(osgViewer::Viewer& viewer, bool moving)
        : viewer_(viewer), ships_(*root_), scenario_(moving), effects_(viewer, *root_, ships_) {
        loadSky(*viewer_.getCamera());
        ocean_ = loadOcean(*root_);
        loadShipManager();
        viewer_.setSceneData(root_);
    }

    void advance(double now) {
        updateShipPoses(now);
        if (const auto cue = scenario_.nextShot(now)) effects_.launch(*cue, now);
        effects_.advance(now);
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
};

BattleScene::BattleScene(osgViewer::Viewer& viewer, bool moving)
    : impl_(std::make_unique<Impl>(viewer, moving)) {}
BattleScene::~BattleScene() = default;
osg::Group& BattleScene::root() const { return *impl_->root_; }
void BattleScene::advance(double now) { impl_->advance(now); }
bool BattleScene::oceanReady() const { return impl_->ocean_->isInitialized(); }
SceneStatus BattleScene::status() const { return impl_->status(); }
