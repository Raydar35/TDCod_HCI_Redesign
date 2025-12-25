#pragma once

#include "Entity.h"
#include "Vec2.h"
#include <SFML/Graphics.hpp>
#include <vector>

class Guts : public Entity {
public:
    Guts();
    Guts(const Vec2& pos, const Vec2& v);

    bool isDone() const { return _isDone; }

    // Project-style API: update and render accept simple parameters
    void update(float dt);
    void render(sf::RenderWindow& window);
    void kill();

    static void init();

    // Static management
    static void add(const Vec2& pos, const Vec2& vel);
    static void updateAll(float dt);
    static void renderAll(sf::RenderWindow& window);

    // Allow external code to provide a texture loaded by Game
    static void setTexture(const sf::Texture& tex);

private:
    Vec2 _initialVelocity;
    bool _isDone;
    float _duration;

    // simple particles for visual effect
    std::vector<Vec2> _particlesPos;
    std::vector<Vec2> _particlesVel;

    static sf::Texture _texture; // optional; if not loaded we render simple shapes
    sf::Sprite _sprite;

    static std::vector<Guts*> _active;
};