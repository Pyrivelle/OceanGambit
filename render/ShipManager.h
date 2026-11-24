#pragma once

#include "ShipController.h"

#include <unordered_map>

class ShipManager {
public:
    explicit ShipManager(osg::Group& root) : root_(root) {}
    unsigned addShip(const std::string& code);
    unsigned count() const { return static_cast<unsigned>(ships_.size()); }
    ShipController& ship(unsigned id) { return *ships_.at(id); }
    const ShipController& ship(unsigned id) const { return *ships_.at(id); }

private:
    osg::Group& root_;
    std::unordered_map<std::string, ShipDefinition> definitions_;
    std::vector<std::unique_ptr<ShipController>> ships_;
};
