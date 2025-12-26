#ifndef BASE_ZOMBIE_H
#define BASE_ZOMBIE_H

#include "Entity.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>

enum class ZombieState {
    WALK,
    ATTACK,
    DEATH
};

enum class ZombieType {
    WALKER,
    TANK,
    CRAWLER,
    ZOOM,
    KING
};

class BaseZombie : public Entity {
public:
    BaseZombie(float x, float y, float health, float attackDamage, float speed, float attackRange, float attackCooldown);
    virtual ~BaseZombie() = default;

    virtual void update(float deltaTime, sf::Vector2f playerPosition);
    virtual void draw(sf::RenderWindow& window) const;

    sf::FloatRect getBounds() const;
    sf::FloatRect getHitbox() const;

    virtual void attack();
    virtual void takeDamage(float amount);
    virtual void kill();

    bool isAttacking() const;
    bool isDead() const;
    bool isAlive() const;

    float getAttackDamage() const;

    sf::Vector2f getPosition() const; // Returns SFML vector for compatibility
    float tryDealDamage();
    bool hasDealtDamageInAttack() const;

    virtual ZombieType getType() const = 0;

protected:
    sf::Sprite sprite;
    std::vector<sf::Texture> walkTextures;
    std::vector<sf::Texture> attackTextures;
    std::vector<sf::Texture> deathTextures;

    int currentFrame;
    float animationTimer;
    float animationFrameTime;

    ZombieState currentState;
    float speed;

    bool attacking;
    int currentAttackFrame;
    float attackTimer;
    float attackFrameTime;
    float attackCooldown;
    float timeSinceLastAttack;
    float attackDamage;
    float attackRange;

    bool dead;
    int currentDeathFrame;
    float deathTimer;
    float deathFrameTime;

    float health;
    float maxHealth;

    virtual void setState(ZombieState newState);
    virtual void updateAnimation(float deltaTime);
    virtual void loadTextures() = 0;

    bool m_hasDealtDamageInAttack;

    void loadTextureSet(std::vector<sf::Texture>& textures, const std::string& basePath,
        const std::string& prefix, int count, bool isBoss = false);
};

#endif
