#pragma once

#include "ShipController.h"

#include <unordered_map>

// Ship asset factory. Scene attachment and instance identity belong to EntityManager.
class ShipManager {
public:
    EntityBuild build(const std::string& code);
    std::unique_ptr<ShipController> create(const std::string& code);

private:
    std::unordered_map<std::string, std::shared_ptr<const ShipDefinition>> definitions_;
};
