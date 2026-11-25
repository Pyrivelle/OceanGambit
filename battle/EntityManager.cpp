#include "EntityManager.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace {
template<typename Predicate>
std::vector<EntityID> matching(const std::map<EntityID, std::unique_ptr<Entity>>& entities,
                                Predicate predicate) {
    std::vector<EntityID> result;
    for (const auto& [id, entity] : entities)
        if (predicate(*entity)) result.push_back(id);
    return result;
}
}

Entity::Entity(EntityID id, EntityType type, std::string code, OwnerID owner,
               SelectionMode selection, EntityBuild built)
    : id_(id), type_(type), code_(std::move(code)), owner_(owner),
      selection_(selection), visual_(std::move(built.visual)), movement_(std::move(built.movement)) {
    setTransform({});
}

void Entity::setTransform(TransformComponent transform) {
    visual_->node().setMatrix(osg::Matrixd::rotate(transform.heading, 0, 0, 1) *
                             osg::Matrixd::translate(transform.position));
    transform_ = transform;
}

EntityManager::~EntityManager() {
    while (!entities_.empty()) destroyEntity(entities_.begin()->first);
}

void EntityManager::registerFactory(EntityType type, SelectionMode selection, Factory factory) {
    if (!factory || !factories_.emplace(type, Registration{selection, std::move(factory)}).second)
        throw std::invalid_argument("Invalid or duplicate entity factory");
}

EntityID EntityManager::createEntity(EntityType type, const std::string& code, OwnerID owner) {
    const auto registration = factories_.find(type);
    if (registration == factories_.end()) throw std::invalid_argument("No factory for entity type");
    if (code.empty()) throw std::invalid_argument("Entity definition code is empty");
    if (next_ == std::numeric_limits<EntityID>::max())
        throw std::overflow_error("Entity ID space exhausted");

    auto built = registration->second.create(code);
    if (!built.visual || built.visual->node().getNumParents() != 0)
        throw std::runtime_error("Entity factory must return a detached visual for " + code);
    const EntityID id = next_++;
    auto entity = std::unique_ptr<Entity>(
        new Entity(id, type, code, owner, registration->second.selection, std::move(built)));
    auto& node = entity->visual().node();
    entities_.emplace(id, std::move(entity));
    try {
        if (!root_.addChild(&node)) throw std::runtime_error("Could not attach entity " + code);
    } catch (...) {
        root_.removeChild(&node);
        entities_.erase(id);
        throw;
    }
    return id;
}

Entity* EntityManager::find(EntityID id) {
    const auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : it->second.get();
}

const Entity* EntityManager::find(EntityID id) const {
    const auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : it->second.get();
}

bool EntityManager::destroyEntity(EntityID id) {
    const auto it = entities_.find(id);
    if (it == entities_.end()) return false;
    it->second->lifecycle_ = EntityLifecycle::Destroying;
    root_.removeChild(&it->second->visual().node());
    entities_.erase(it);
    return true;
}

std::vector<EntityID> EntityManager::ids() const {
    return matching(entities_, [](const Entity&) { return true; });
}

std::vector<EntityID> EntityManager::findByType(EntityType type) const {
    return matching(entities_, [type](const Entity& entity) { return entity.type() == type; });
}

std::vector<EntityID> EntityManager::findByOwner(OwnerID owner) const {
    return matching(entities_, [owner](const Entity& entity) { return entity.owner() == owner; });
}

std::vector<EntityID> EntityManager::findSelectable(SelectionMode selection) const {
    return matching(entities_, [selection](const Entity& entity) {
        return entity.selectionMode() == selection;
    });
}
