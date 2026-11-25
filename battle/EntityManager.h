#pragma once

#include <osg/MatrixTransform>
#include "MovementTrait.h"
#include "WeaponTrait.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

using EntityID = std::uint64_t;
using OwnerID = std::uint32_t;

enum class EntityType { Ship, GroundVehicle, Aircraft, Building };
enum class EntityLifecycle { Active, Destroying };
enum class SelectionMode { None, Single, Multiple };

struct TransformComponent {
    osg::Vec3d position;
    double heading = 0; // Radians about world +Z.
};

class EntityVisual {
public:
    virtual ~EntityVisual() = default;
    virtual osg::MatrixTransform& node() const = 0;
};

struct EntityBuild {
    std::unique_ptr<EntityVisual> visual;
    std::optional<MovementTrait> movement;
    std::optional<WeaponTrait> weapon;
};

class Entity {
public:
    EntityID id() const { return id_; }
    EntityType type() const { return type_; }
    OwnerID owner() const { return owner_; }
    const std::string& code() const { return code_; }
    EntityLifecycle lifecycle() const { return lifecycle_; }
    SelectionMode selectionMode() const { return selection_; }
    const TransformComponent& transform() const { return transform_; }
    EntityVisual& visual() { return *visual_; }
    const EntityVisual& visual() const { return *visual_; }
    MovementTrait* movement() { return movement_ ? &*movement_ : nullptr; }
    const MovementTrait* movement() const { return movement_ ? &*movement_ : nullptr; }
    WeaponTrait* weapon() { return weapon_ ? &*weapon_ : nullptr; }
    const WeaponTrait* weapon() const { return weapon_ ? &*weapon_ : nullptr; }
    void setTransform(TransformComponent transform);

private:
    friend class EntityManager;
    Entity(EntityID id, EntityType type, std::string code, OwnerID owner,
           SelectionMode selection, EntityBuild built);

    EntityID id_;
    EntityType type_;
    std::string code_;
    OwnerID owner_;
    EntityLifecycle lifecycle_ = EntityLifecycle::Active;
    SelectionMode selection_;
    TransformComponent transform_;
    std::unique_ptr<EntityVisual> visual_;
    std::optional<MovementTrait> movement_;
    std::optional<WeaponTrait> weapon_;
};

class EntityManager {
public:
    using Factory = std::function<EntityBuild(const std::string&)>;
    explicit EntityManager(osg::Group& root) : root_(root) {}
    ~EntityManager();
    EntityManager(const EntityManager&) = delete;
    EntityManager& operator=(const EntityManager&) = delete;

    void registerFactory(EntityType type, SelectionMode selection, Factory factory);
    EntityID createEntity(EntityType type, const std::string& code, OwnerID owner);
    Entity* find(EntityID id);
    const Entity* find(EntityID id) const;
    bool destroyEntity(EntityID id);

    // Stable snapshots in ascending ID order. Resolve each ID before use after deletion.
    std::vector<EntityID> ids() const;
    std::vector<EntityID> findByType(EntityType type) const;
    std::vector<EntityID> findByOwner(OwnerID owner) const;
    std::vector<EntityID> findSelectable(SelectionMode selection) const;

private:
    struct Registration { SelectionMode selection; Factory create; };
    osg::Group& root_;
    EntityID next_ = 1;
    std::map<EntityType, Registration> factories_;
    std::map<EntityID, std::unique_ptr<Entity>> entities_;
};
