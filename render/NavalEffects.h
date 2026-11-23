#pragma once

#include "ImpactExplosion.h"
#include "ShipManager.h"
#include "NavalEffectsPreview.h"
#include "../battle/DemoScenario.h"

#include <array>
#include <vector>

struct EffectsStatus {
    std::array<unsigned, 2> decalCounts;
    bool flight, splash, decalAttached, decalMoved;
    unsigned explosions;
};

class NavalEffects {
public:
    NavalEffects(osgViewer::Viewer& viewer, osg::Group& root, ShipManager& ships)
        : viewer_(viewer), root_(root), ships_(ships) {}
    void launch(const ShotCue& event, double now);
    void advance(double now);
    EffectsStatus status() const;

private:
    struct Shot {
        unsigned id;
        unsigned from;
        osg::Vec3d cue;
        double start;
        bool recorded = false;
        osg::ref_ptr<osg::MatrixTransform> target;
        osg::ref_ptr<osg::MatrixTransform> decalLayer;
        osg::ref_ptr<ShipDecals> decals;
        osg::ref_ptr<NavalEffectsPreview> effects;
    };

    void updateShot(Shot& shot, double now);

    osgViewer::Viewer& viewer_;
    osg::Group& root_;
    ShipManager& ships_;
    std::vector<Shot> shots_;
    std::vector<ImpactExplosion> explosions_;
    unsigned explosionsStarted_ = 0;
    std::array<unsigned, 2> decalCounts_{};
    bool visibleFlight_ = false;
    bool visibleSplash_ = false;
    osg::observer_ptr<osg::MatrixTransform> markedMovingHull_;
    osg::Vec3d markWorld_, markShipPosition_;
};
