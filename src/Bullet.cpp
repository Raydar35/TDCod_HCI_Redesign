#include "Bullet.h"
#include "BaseZombie.h"
#include <iostream>

sf::Texture Bullet::bulletTexture;

Bullet::Bullet(Vec2 position, Vec2 velocity, float mass, int maxPenetrations, float damage)
    : Entity(EntityType::Bullet, position, Vec2(8.f, 8.f), false, mass, true),
    remainingPenetrations(maxPenetrations),
    damage(damage)
{
    getBody().isCircle = true;
    body.isTrigger = true;
    body.velocity = velocity;
    body.getCircleShape().setRadius(4.f);
    body.getCircleShape().setOrigin(4.f, 4.f);
    body.getCircleShape().setFillColor(sf::Color::Yellow);
    
    // Load texture for bullet
    sprite.setTexture(Bullet::bulletTexture);
    sprite.setOrigin(Bullet::bulletTexture.getSize().x / 2.f + 200, Bullet::bulletTexture.getSize().y / 2.f - 10);
    sprite.setPosition(body.position.x, body.position.y);
    float angle = std::atan2(body.velocity.y, body.velocity.x) * 180.f / 3.14159f; // Get direction from velocity
    sprite.setRotation(angle);

}

void Bullet::update(float dt) {
    body.update(dt); // Add physics updates if needed
    sprite.setPosition(body.position.x, body.position.y);
    float angle = std::atan2(body.velocity.y, body.velocity.x) * 180.f / 3.14159f;
    sprite.setRotation(angle);
}

void Bullet::render(sf::RenderWindow& window) {
    window.draw(sprite);
    window.draw(body.getCircleShape()); // Uncomment for debug
}

void Bullet::onCollision(Entity* other) {
    if (!other || other == this || !other->isAlive()) return;

    EntityType targetType = other->getType();

    if (targetType == EntityType::Enemy) {
        // Avoid multiple hits on the same entity
        if (hitEntities.count(other)) return;
        hitEntities.insert(other);

        // Deal damage
        BaseZombie* zombie = dynamic_cast<BaseZombie*>(other);
        if (zombie) {
            zombie->takeDamage(damage); // Use bullet's damage
        }

        // Calculate the mass ratio between the bullet and the enemy
        float bulletMass = body.mass;
        float enemyMass = other->getBody().mass;

        // Apply pushback scaled by mass ratio
        float pushbackFactor = bulletMass / (bulletMass + enemyMass);  // Small pushback for heavy enemies, bigger for light enemies
        other->getBody().externalImpulse += body.velocity * 0.5f * pushbackFactor;

        remainingPenetrations--;
    }
    else if (targetType == EntityType::Wall) {
        remainingPenetrations = 0; // Wall stops bullet
    }

    if (remainingPenetrations <= 0) {
        destroy();
    }
}