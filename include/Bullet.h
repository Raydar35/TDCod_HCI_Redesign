#pragma once

#include "Entity.h"
#include <unordered_set>
#include <SFML/Graphics.hpp>

class Bullet : public Entity {
public:
    Bullet(Vec2 position, Vec2 velocity, float mass = 1.0f, int maxPenetrations = 1, float damage = 25);

    void update(float dt) override;
    void onCollision(Entity* other) override;
    void render(sf::RenderWindow& window) override;

    static sf::Texture bulletTexture;
private:
    float damage;
    int remainingPenetrations;
    std::unordered_set<Entity*> hitEntities;

    sf::Sprite sprite;
};