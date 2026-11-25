#pragma once

#include "ImpactExplosion.h"
#include "ShipController.h"
#include "NavalEffectsPreview.h"
#include "../battle/ProjectileSystem.h"

#include <array>
#include <functional>
#include <utility>
#include <unordered_map>
#include <vector>

struct EffectsStatus {
    std::array<unsigned, 2> decalCounts;
    bool flight, splash, decalAttached, decalMoved;
    unsigned explosions;
};

class NavalEffects {
public:
    using ShipLookup = std::function<ShipController*(unsigned)>;
    NavalEffects(osgViewer::Viewer& viewer, osg::Group& root, ShipLookup lookup)
        : viewer_(viewer), root_(root), lookup_(std::move(lookup)) {}
    void launch(unsigned id, const MuzzlePose& pose, double now);
    void sample(unsigned id, const osg::Vec3d& position, double now);
    void contact(unsigned id, const std::optional<ProjectileContact>& hit, double now);
    void advance(double now);
    EffectsStatus status() const;

private:
    struct Flight {
        osg::Vec3d position;
        osg::ref_ptr<NavalEffectsPreview> effects;
    };

    void markShip(unsigned target, const ProjectileContact& hit, double now);

    osgViewer::Viewer& viewer_;
    osg::Group& root_;
    ShipLookup lookup_;
    std::unordered_map<unsigned, Flight> flights_;
    std::array<osg::ref_ptr<ShipDecals>, 2> decals_;
    std::vector<ImpactExplosion> explosions_;
    unsigned explosionsStarted_ = 0;
    std::array<unsigned, 2> decalCounts_{};
    bool visibleFlight_ = false;
    bool visibleSplash_ = false;
    osg::observer_ptr<osg::MatrixTransform> markedMovingHull_;
    osg::Vec3d markWorld_, markShipPosition_;
};
