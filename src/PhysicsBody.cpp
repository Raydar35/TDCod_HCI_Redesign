#include "PhysicsBody.h"

PhysicsBody::PhysicsBody(Vec2 position, Vec2 size, bool isStatic, float mass, bool isCircle)
    : position(position), size(size), isStatic(isStatic), mass(mass), isCircle(isCircle) {
    // Initialize both shapes, one will be used depending on isCircle
    rectShape.setSize(sf::Vector2f(size.x, size.y));
    rectShape.setOrigin(size.x / 2, size.y / 2); // important
    rectShape.setPosition(position.x, position.y);
    rectShape.setFillColor(isStatic ? sf::Color::White : sf::Color::Red);

    float radius = std::max(size.x, size.y) / 2.f;
    circleShape.setRadius(radius);
    circleShape.setOrigin(radius, radius);
    circleShape.setFillColor(isStatic ? sf::Color::White : sf::Color::Blue);
}

void PhysicsBody::update(float dt) {
    if (!isStatic) {
        Vec2 totalVelocity = velocity + externalImpulse;
        position += totalVelocity * dt;

        // Decay external impulse (acts like damping)
        externalImpulse = externalImpulse * 0.9f;

    }

    // Always update shape positions
    rectShape.setPosition(position.x, position.y);
    circleShape.setPosition(position.x, position.y);
}

void PhysicsBody::applyDamping(float factor) {
    if (!isStatic) {
        velocity.x *= (1.f - factor);
        velocity.y *= (1.f - factor);
    }
}

sf::Shape& PhysicsBody::getShape() {
    return isCircle ? static_cast<sf::Shape&>(circleShape)
        : static_cast<sf::Shape&>(rectShape);
}

const sf::Shape& PhysicsBody::getShape() const {
    return isCircle ? static_cast<const sf::Shape&>(circleShape)
        : static_cast<const sf::Shape&>(rectShape);
}

sf::CircleShape& PhysicsBody::getCircleShape() { return circleShape; }
const sf::CircleShape& PhysicsBody::getCircleShape() const { return circleShape; }

sf::RectangleShape& PhysicsBody::getRectShape() { return rectShape; }
const sf::RectangleShape& PhysicsBody::getRectShape() const { return rectShape; }