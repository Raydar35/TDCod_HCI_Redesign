#pragma once

#include "Entity.h"
#include <unordered_set>
#include <SFML/Graphics.hpp>

class Bullet : public Entity {
public:
    Bullet(Vec2 position, Vec2 velocity, float mass = 1.0f, int maxPenetrations = 1, float damage = 25,
           sf::Vector2f spriteOffset = sf::Vector2f(0.f, 0.f), float spriteScale = 0.07f, float rotationOffset = 90.0f);

    void update(float dt) override;
    void onCollision(Entity* other) override;
    void render(sf::RenderWindow& window) override; // matches base Entity

    // set interpolation alpha (0..1) before rendering
    void setRenderAlpha(float a) { renderAlpha = a; }

    static sf::Texture bulletTexture;
    // Runtime visual adjustments
    void setSpriteScale(float s) { spriteScale = s; sprite.setScale(s, s); }
    void setRotationOffset(float off) { rotationOffset = off; }
    void setSpriteOffset(const sf::Vector2f& off) { spriteOffset = off; }
    void setTexture(const sf::Texture& tex) { Bullet::bulletTexture = tex; sprite.setTexture(Bullet::bulletTexture); sprite.setOrigin(Bullet::bulletTexture.getSize().x / 2.f, Bullet::bulletTexture.getSize().y / 2.f); }
private:
    float damage;
    int remainingPenetrations;
    // When a bullet penetrates an enemy, subsequent hits will deal this
    // fraction of the previous damage (e.g. 0.6 -> 60% damage after each penetration)
    float penetrationDamageMultiplier = 0.5f;
    std::unordered_set<Entity*> hitEntities;

    sf::Sprite sprite;

    // Visual offset relative to physics body (in pixels). Rotated with velocity so sprite matches body visually.
    sf::Vector2f spriteOffset = sf::Vector2f(-30.f, 0.f);
    float spriteScale = 0.01f;
    float rotationOffset = 0.0f;

    // Interpolation positions
    sf::Vector2f prevPos;
    sf::Vector2f currPos;

    // interpolation alpha used during render
    float renderAlpha = 1.0f;

    // Lifetime to auto-destroy bullets that miss forever
    float lifeTimer = 0.0f;
    float maxLife = 3.0f; // seconds
};