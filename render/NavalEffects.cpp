#include "NavalEffects.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

using Point = osg::Vec3d;

void NavalEffects::launch(const ShotCue& event, double now) {
    const auto [id, from, to, splash] = event;
    auto& source = ships_.ship(from);
    auto& receiver = ships_.ship(to);
    const Point cue = splash ? Point(95, 8, -.15)
                             : Point(0, 0, 3.8) * receiver.node().getMatrix();
    const Point muzzle = source.poseTurret(cue);
    Point direction = cue - muzzle;
    const double cueDistance = direction.normalize();

    Shot shot{};
    shot.id = id;
    shot.from = from;
    shot.cue = cue;
    shot.start = now;
    // The prototype projects in scene coordinates. Store its output under the
    // target's launch pose inverse so the marks travel with the rendered ship.
    shot.target = new osg::MatrixTransform(receiver.node().getMatrix());
    shot.target->addChild(&receiver.model());
    shot.decalLayer = new osg::MatrixTransform(osg::Matrix::inverse(shot.target->getMatrix()));
    receiver.node().addChild(shot.decalLayer);
    shot.decals = new ShipDecals(viewer_, *shot.target, *shot.decalLayer, false);
    shot.effects = new NavalEffectsPreview(viewer_, muzzle, cue);
    // Preserve the prototype effects while setting the timing of this scene's visual cue.
    shot.effects->setPresentationFlight(cueDistance + 10, 1.5);
    shot.effects->enableGunnery(*shot.target, *shot.decals, [muzzle, direction](double, double) {
        return NavalEffectsPreview::Aim{muzzle, direction};
    });
    root_.addChild(shot.effects->node());
    osg::ref_ptr<osgGA::GUIEventAdapter> input = new osgGA::GUIEventAdapter;
    input->setEventType(osgGA::GUIEventAdapter::KEYDOWN);
    input->setKey(' ');
    shot.effects->handle(*input, viewer_);
    std::cout << std::fixed << std::setprecision(3) << "t=" << now
              << " from=" << from + 1 << " projectile=" << id
              << " event=launch xyz=(" << muzzle.x() << ',' << muzzle.y()
              << ',' << muzzle.z() << ')' << std::endl;
    shots_.push_back(std::move(shot));
}

void NavalEffects::updateShot(Shot& shot, double now) {
    osg::ref_ptr<osgGA::GUIEventAdapter> input = new osgGA::GUIEventAdapter;
    input->setEventType(osgGA::GUIEventAdapter::FRAME);
    shot.effects->handle(*input, viewer_);
    const auto stages = shot.effects->visibleStages();
    visibleFlight_ |= (stages & NavalEffectsPreview::Flight) != 0;
    visibleSplash_ |= (stages & NavalEffectsPreview::Splash) != 0;
    if (!shot.recorded && (shot.decals->counts()[2] || (stages & NavalEffectsPreview::Splash))) {
        shot.recorded = true;
        const bool hit = shot.decals->counts()[2] != 0;
        if (hit) {
            ++decalCounts_[1 - shot.from];
            if (shot.from == 0 && !markedMovingHull_.valid()) {
                markedMovingHull_ = shot.decalLayer;
                markWorld_ = markedMovingHull_->getWorldMatrices().front().getTrans();
                markShipPosition_ = ships_.ship(1).node().getMatrix().getTrans();
            }
        }
        const Point position = hit
            ? Point(shot.decalLayer->getChild(0)->getBound().center()) *
                shot.decalLayer->getWorldMatrices().front()
            : shot.cue;
        if (hit) {
            auto& ship = ships_.ship(1 - shot.from).node();
            const Point localImpact = position * osg::Matrix::inverse(ship.getMatrix());
            explosions_.emplace_back(ship, localImpact, now);
            ++explosionsStarted_;
        }
        std::cout << "t=" << now << " from=" << shot.from + 1 << " projectile=" << shot.id
                  << " event=" << (hit ? "hull" : "water")
                  << " xyz=(" << position.x() << ',' << position.y()
                  << ',' << position.z() << ')' << std::endl;
    }
}

void NavalEffects::advance(double now) {
    for (auto& shot : shots_) updateShot(shot, now);
    explosions_.erase(std::remove_if(explosions_.begin(), explosions_.end(),
        [now](ImpactExplosion& explosion) { return !explosion.advance(now); }), explosions_.end());
    shots_.erase(std::remove_if(shots_.begin(), shots_.end(), [this, now](const Shot& shot) {
        if (now - shot.start < 6) return false;
        root_.removeChild(shot.effects->node());
        if (shot.decalLayer->getNumChildren() == 0)
            ships_.ship(1 - shot.from).node().removeChild(shot.decalLayer);
        return true;
    }), shots_.end());
}

EffectsStatus NavalEffects::status() const {
    const auto matrix = markedMovingHull_.valid() ? markedMovingHull_->getWorldMatrices()
                                          : osg::MatrixList{};
    const bool decalAttached = !matrix.empty() &&
        (matrix.front().getTrans() - markWorld_ -
         (ships_.ship(1).node().getMatrix().getTrans() - markShipPosition_)).length() < 1e-6;
    const bool moved = !matrix.empty() && (matrix.front().getTrans() - markWorld_).length() > .1;
    return {decalCounts_, visibleFlight_, visibleSplash_, decalAttached, moved, explosionsStarted_};
}
