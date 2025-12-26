#include "Entity.h"

Entity::Entity(EntityType type, Vec2 position, Vec2 size, bool isStatic, float mass, bool isCircle)
    : type(type), body(position, size, isStatic, mass, isCircle)
{
    body.owner = this;
}

void Entity::update(float dt) {
}

void Entity::render(sf::RenderWindow& window) {
    window.draw(body.getShape());
}

void Entity::onCollision(Entity* other) {
}

PhysicsBody& Entity::getBody() {
    return body;
}

const PhysicsBody& Entity::getBody() const {
    return body;
}

EntityType Entity::getType() const {
    return type;
}

bool Entity::isAlive() const {
    return alive;
}

void Entity::destroy() {
    alive = false;
}