#include "Guts.hpp" // adjust path if you placed header elsewhere
#include <random>
#include <cmath>

sf::Texture Guts::_texture;
std::vector<Guts*> Guts::_active;

Guts::Guts()
    : Entity(EntityType::Bullet, Vec2(0,0), Vec2(8.f,8.f), false, 0.01f, true),
      _initialVelocity(0,0), _isDone(true), _duration(0.0f)
{
}

Guts::Guts(const Vec2& pos, const Vec2& v)
    : Entity(EntityType::Bullet, pos, Vec2(6.f,6.f), false, 0.01f, true),
      _initialVelocity(v), _isDone(false), _duration(20.0f)
{
    body.isCircle = true;
    body.getCircleShape().setRadius(3.f);
    body.getCircleShape().setOrigin(3.f, 3.f);
    body.position = pos;

    // create simple particle cloud
    std::mt19937 rng(static_cast<unsigned int>(std::rand()));
    std::uniform_real_distribution<float> angDist(0.0f, 2.0f * 3.14159265f);
    std::uniform_real_distribution<float> speedDist(10.0f, 80.0f);
    int count = 12;
    _particlesPos.reserve(count);
    _particlesVel.reserve(count);
    for (int i = 0; i < count; ++i) {
        float a = angDist(rng);
        float s = speedDist(rng);
        Vec2 vel(std::cos(a) * s + _initialVelocity.x, std::sin(a) * s + _initialVelocity.y);
        _particlesPos.emplace_back(pos.x + std::cos(a) * 2.0f, pos.y + std::sin(a) * 2.0f);
        _particlesVel.emplace_back(vel.x * 0.01f, vel.y * 0.01f);
    }

    if (_texture.getSize().x > 0) {
        _sprite.setTexture(_texture);
        _sprite.setOrigin(_texture.getSize().x/2.f, _texture.getSize().y/2.f);
        _sprite.setScale(0.05f, 0.05f);
    }
}

void Guts::init() {
    if (Guts::_texture.getSize().x == 0) {
        // try load optional texture - support both proposed locations
        if (!Guts::_texture.loadFromFile("assets/Props/guts.png")) {
            Guts::_texture.loadFromFile("assets/guts.png");
        }
    }
}

void Guts::setTexture(const sf::Texture& tex) {
    Guts::_texture = tex;
    if (Guts::_texture.getSize().x > 0) {
        for (auto g : Guts::_active) {
            g->_sprite.setTexture(Guts::_texture);
            g->_sprite.setOrigin(Guts::_texture.getSize().x / 2.f,
                Guts::_texture.getSize().y / 2.f);
            g->_sprite.setScale(0.05f, 0.05f);
        }
    }
}

void Guts::update(float dt) {
    if (_isDone) return;
    _duration -= dt;
    if (_duration <= 0.0f) {
        _isDone = true;
    }

    // simple particle motion with tiny gravity
    for (size_t i = 0; i < _particlesPos.size(); ++i) {
        _particlesVel[i].y += 9.8f * dt * 0.05f;
        _particlesPos[i] += _particlesVel[i] * dt * 60.0f; // scale to match world units
    }

    if (!_particlesPos.empty()) {
        body.position = _particlesPos[0];
        body.update(dt);
    }
}

void Guts::render(sf::RenderWindow& window) {
    for (size_t i = 0; i < _particlesPos.size(); ++i) {
        if (_texture.getSize().x > 0) {
            _sprite.setPosition(_particlesPos[i].x, _particlesPos[i].y);
            _sprite.setRotation((float)(std::fmod(i * 37.0, 360.0)));
            window.draw(_sprite);
        } else {
            sf::CircleShape c(3.0f);
            c.setOrigin(3.0f, 3.0f);
            c.setPosition(_particlesPos[i].x, _particlesPos[i].y);
            c.setFillColor(!_isDone ? sf::Color::Red : sf::Color(120, 40, 40));
            window.draw(c);
        }
    }
}

void Guts::kill() {
    _isDone = true;
    _particlesPos.clear();
    _particlesVel.clear();
    destroy(); // Entity::destroy() to mark it dead
}

// Static management
void Guts::add(const Vec2& pos, const Vec2& vel) {
    Guts* g = new Guts(pos, vel);
    _active.push_back(g);
}

void Guts::updateAll(float dt) {
    for (int i = (int)_active.size() - 1; i >= 0; --i) {
        _active[i]->update(dt);
        if (_active[i]->_isDone) {
            delete _active[i];
            _active.erase(_active.begin() + i);
        }
    }
}

void Guts::renderAll(sf::RenderWindow& window) {
    for (auto g : _active) g->render(window);
}