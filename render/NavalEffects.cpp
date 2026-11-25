#include "NavalEffects.h"

#include <algorithm>

void NavalEffects::launch(unsigned id, const MuzzlePose& pose, double now) {
    osg::ref_ptr<NavalEffectsPreview> effects = new NavalEffectsPreview(viewer_, pose.position, pose.position);
    effects->beginExternalFlight(now, {pose.position, pose.forward});
    root_.addChild(effects->node());
    flights_.emplace(id, Flight{pose.position, effects});
}

void NavalEffects::sample(unsigned id, const osg::Vec3d& position, double now) {
    auto& flight = flights_.at(id);
    flight.position = position;
    flight.effects->sampleExternalFlight(now, position);
    visibleFlight_ |= (flight.effects->visibleStages() & NavalEffectsPreview::Flight) != 0;
}

void NavalEffects::contact(unsigned id, const std::optional<ProjectileContact>& hit, double now) {
    auto& flight = flights_.at(id);
    const bool water = hit && !hit->ship;
    flight.effects->finishExternalFlight(now, hit ? hit->position : flight.position, water);
    visibleSplash_ |= water;
    if (hit && hit->ship) markShip(*hit->ship, *hit, now);
}

void NavalEffects::markShip(unsigned target, const ProjectileContact& hit, double now) {
    auto* targetShip = lookup_(target);
    if (!targetShip || target >= decals_.size()) return;
    auto& ship = *targetShip;
    const auto inverse = osg::Matrix::inverse(ship.node().getMatrix());
    const auto local = hit.position * inverse;
    const auto normal = osg::Matrix::transform3x3(hit.normal, inverse);
    if (!decals_[target]) {
        auto layer = new osg::MatrixTransform;
        ship.node().addChild(layer);
        decals_[target] = new ShipDecals(viewer_, ship.model(), *layer, false);
        if (target == 1) {
            markedMovingHull_ = layer;
            markWorld_ = layer->getWorldMatrices().front().getTrans();
            markShipPosition_ = ship.node().getMatrix().getTrans();
        }
    }
    if (decals_[target]->stampAt(local, normal, 2)) ++decalCounts_[target];
    explosions_.emplace_back(ship.node(), local, now);
    ++explosionsStarted_;
}

void NavalEffects::advance(double now) {
    for (unsigned slot = 0; slot < decals_.size(); ++slot)
        if (decals_[slot] && !lookup_(slot)) decals_[slot] = nullptr;
    for (auto it = flights_.begin(); it != flights_.end();) {
        auto& flight = it->second;
        flight.effects->sampleExternalFlight(now, flight.position);
        if (flight.effects->externalFlightVisible()) {
            ++it;
        } else {
            root_.removeChild(flight.effects->node());
            it = flights_.erase(it);
        }
    }
    explosions_.erase(std::remove_if(explosions_.begin(), explosions_.end(),
        [now](ImpactExplosion& explosion) { return !explosion.advance(now); }), explosions_.end());
}

EffectsStatus NavalEffects::status() const {
    const auto matrix = markedMovingHull_.valid() ? markedMovingHull_->getWorldMatrices()
                                          : osg::MatrixList{};
    const auto* ship = lookup_(1);
    const bool decalAttached = ship && !matrix.empty() &&
        (matrix.front().getTrans() - markWorld_ -
         (ship->node().getMatrix().getTrans() - markShipPosition_)).length() < 1e-6;
    const bool moved = !matrix.empty() && (matrix.front().getTrans() - markWorld_).length() > .1;
    return {decalCounts_, visibleFlight_, visibleSplash_, decalAttached, moved, explosionsStarted_};
}
