#include "Bullet.h"
#include "BaseZombie.h"
#include <iostream>

sf::Texture Bullet::bulletTexture;

Bullet::Bullet(Vec2 position, Vec2 velocity, float mass, int maxPenetrations, float damage,
               sf::Vector2f spriteOffsetIn, float spriteScaleIn, float rotationOffsetIn)
    : Entity(EntityType::Bullet, position, Vec2(8.f, 8.f), false, mass, true),
    remainingPenetrations(maxPenetrations),
    damage(damage),
    spriteOffset(spriteOffsetIn),
    spriteScale(spriteScaleIn),
    rotationOffset(rotationOffsetIn)
{
    getBody().isCircle = true;
    body.isTrigger = true;
    body.velocity = velocity;
    body.getCircleShape().setRadius(4.f);
    body.getCircleShape().setOrigin(4.f, 4.f);
    body.getCircleShape().setFillColor(sf::Color::Yellow);
    
    // Load texture for bullet
    sprite.setTexture(Bullet::bulletTexture);
    sprite.setOrigin(Bullet::bulletTexture.getSize().x / 2.f, Bullet::bulletTexture.getSize().y / 2.f);
    sprite.setScale(spriteScale, spriteScale);
    sprite.setPosition(body.position.x, body.position.y);
    float angle = std::atan2(body.velocity.y, body.velocity.x) * 180.f / 3.14159f; // Get direction from velocity
    sprite.setRotation(angle + rotationOffset);

    prevPos = currPos = sf::Vector2f(body.position.x, body.position.y);

    // shorter default life to avoid accumulating many stray bullets
    lifeTimer = 0.0f;
    maxLife = 2.0f;
}

void Bullet::update(float dt) {
    // store previous position for interpolation
    prevPos = currPos;

    body.update(dt); // Add physics updates if needed

    currPos = sf::Vector2f(body.position.x, body.position.y);

    // lifetime decay
    lifeTimer += dt;
    if (lifeTimer >= maxLife) {
        destroy();
    }
}

void Bullet::render(sf::RenderWindow& window) {
    float alpha = renderAlpha;
    // Interpolate position between prevPos and currPos
    sf::Vector2f interp = prevPos + (currPos - prevPos) * alpha;

    // Compute rotation from velocity
    float rad = std::atan2(body.velocity.y, body.velocity.x);
    float deg = rad * 180.f / 3.14159f;
    float cs = std::cos(rad), sn = std::sin(rad);

    // Rotate offset vector by the bullet rotation so offset follows facing
    sf::Vector2f rotatedOffset(spriteOffset.x * cs - spriteOffset.y * sn,
                               spriteOffset.x * sn + spriteOffset.y * cs);

    sprite.setPosition(interp.x + rotatedOffset.x, interp.y + rotatedOffset.y);
    sprite.setRotation(deg + rotationOffset);

    window.draw(sprite);
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
            zombie->takeDamage(damage); // Use bullet's current damage
        }

        // Calculate the mass ratio between the bullet and the enemy
        float bulletMass = body.mass;
        float enemyMass = other->getBody().mass;

        // Apply pushback scaled by mass ratio
        float pushbackFactor = bulletMass / (bulletMass + enemyMass);  // Small pushback for heavy enemies, bigger for light enemies
        other->getBody().externalImpulse += body.velocity * 0.5f * pushbackFactor;

        remainingPenetrations--;
        // After penetrating, reduce damage for next potential hit
        damage *= penetrationDamageMultiplier;
    }
    else if (targetType == EntityType::Wall) {
        remainingPenetrations = 0; // Wall stops bullet
    }

    if (remainingPenetrations <= 0) {
        destroy();
    }
}