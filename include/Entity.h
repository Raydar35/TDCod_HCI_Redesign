#pragma once
#include "PhysicsBody.h"

enum class EntityType {
    Player,
    Enemy,
    Bullet,
    Wall
};

class Entity {
public:
    Entity(EntityType type, Vec2 position, Vec2 size, bool isStatic, float mass, bool isCircle = false);

    virtual void update(float dt);
    virtual void render(sf::RenderWindow& window);
    virtual void onCollision(Entity* other);

    // Accessors
    PhysicsBody& getBody();
    const PhysicsBody& getBody() const;
    EntityType getType() const;
    bool isAlive() const;
    void destroy();

protected:
    EntityType type;
    PhysicsBody body;
    bool alive = true;
};