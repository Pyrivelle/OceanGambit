#pragma once

#include <array>
#include <memory>
#include "../battle/WowsMotion.h"

namespace osg { class Group; }
namespace osgViewer { class Viewer; }

struct SceneStatus {
    bool oceanReady;
    std::array<unsigned, 2> decalCounts;
    bool flight, splash, decalAttached, decalMoved;
    unsigned explosions;
};

class BattleScene {
public:
    BattleScene(osgViewer::Viewer& viewer, bool moving, bool quadratic = false, bool wows = false,
                ocean_gambit::ballistics::Integrator integrator = ocean_gambit::ballistics::Integrator::RungeKutta4);
    ~BattleScene();
    osg::Group& root() const;
    void advance(double now);
    bool oceanReady() const;
    SceneStatus status() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
