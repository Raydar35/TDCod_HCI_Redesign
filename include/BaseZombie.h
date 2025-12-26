#ifndef BASE_ZOMBIE_H
#define BASE_ZOMBIE_H

#include "Entity.h"
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include "include/Animator.h"

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
    // Return the attack hitbox (area the zombie can damage when attacking). Default implementation provided in .cpp
    virtual sf::FloatRect getAttackHitbox() const;
    // Provide oriented attack box: center (world), half extents (half width, half height), rotation degrees
    virtual void getAttackOBB(sf::Vector2f& outCenter, sf::Vector2f& outHalfExtents, float& outRotationDeg) const;

    virtual void attack();
    virtual void takeDamage(float amount);
    virtual void kill();

    // Prepare this zombie to be reused from a pool. Sets position, health, damage and speed
    void resetForSpawn(float x, float y, float healthVal, float damageVal, float speedVal);

    bool isAttacking() const;
    bool isDead() const;
    bool isAlive() const;

    float getAttackDamage() const;

    sf::Vector2f getPosition() const; // Returns SFML vector for compatibility
    virtual float tryDealDamage();
    bool hasDealtDamageInAttack() const;

    virtual ZombieType getType() const = 0;

    // Allow runtime tuning of walk animation frame time
    void setWalkFrameTime(float t) { walkFrameTime = t; if (currentState == ZombieState::WALK) animator.setFrameTime(walkFrameTime); }
    float getWalkFrameTime() const { return walkFrameTime; }

    // Interpolation positions for smooth rendering
    sf::Vector2f prevPos = sf::Vector2f(0.f, 0.f);
    sf::Vector2f currPos = sf::Vector2f(0.f, 0.f);
    float renderAlpha = 1.0f;
    void setRenderAlpha(float a) { renderAlpha = a; }
    // Shadow support: set a shadow texture to render under the zombie
    void setShadowTexture(const sf::Texture& tex) { shadowTexture = &tex; }

protected:
    sf::Sprite sprite;
    std::vector<sf::Texture> walkTextures;
    std::vector<sf::Texture> attackTextures;
    std::vector<sf::Texture> deathTextures;
    // Optional single-sheet support for walk animation
    sf::Texture walkSheetTexture;
    std::vector<sf::IntRect> walkRects;
    bool hasWalkSheet = false;
    // Optional single-sheet support for attack animation
    sf::Texture attackSheetTexture;
    std::vector<sf::IntRect> attackRects;
    bool hasAttackSheet = false;

    // Animator to drive sprite animations (replaces some manual frame/timer logic)
    Animator animator;
    float walkFrameTime = 0.1f;
    int currentFrame;

    ZombieState currentState;
    float speed;
    // Rotation offset (degrees) to apply when orienting sprite towards movement direction.
    // Many sprite sheets face up/down by default; adjust per-zombie type in their constructor.
    float rotationOffset = 0.0f;

    // Last seen player position updated each update; derived classes can use it for attacks
    Vec2 lastSeenPlayerPos = Vec2(0,0);

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

    // Hook for derived classes to disable rotation (e.g., during lunges)
    virtual bool canRotate() const { return true; }
    // Hook for derived classes to disable base movement logic (e.g., during lunges)
    virtual bool isMovementLocked() const { return false; }

    bool m_hasDealtDamageInAttack;

    void loadTextureSet(std::vector<sf::Texture>& textures, const std::string& basePath,
        const std::string& prefix, int count, bool isBoss = false);
    const sf::Texture* shadowTexture = nullptr;
};

#endif
