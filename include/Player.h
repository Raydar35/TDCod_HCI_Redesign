#ifndef PLAYER_H
#define PLAYER_H

#include "PhysicsWorld.h"
#include "Entity.h"
#include "Bullet.h"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <vector>

enum class PlayerState {
    IDLE,
    WALK,
    ATTACK,
    DEATH
};

enum class WeaponType {
    KNIFE,
    BAT,
    PISTOL,
    RIFLE,
    FLAMETHROWER
};

class Player : public Entity {
public:
    Player(Vec2 position);
    
    void update(float deltaTime, sf::RenderWindow& window, sf::Vector2u mapSize, sf::Vector2f worldMousePosition);
    void draw(sf::RenderWindow& window);
    void render(sf::RenderWindow& window);
    void attack();
    void takeDamage(float amount);
    void kill();
    
    void setPosition(float x, float y);
    sf::Vector2f getPosition() const;
    
    void sprint(bool isSprinting);
    bool isSprinting() const;
    
    bool isAttacking() const;
    bool isDead() const;

    float getCurrentHealth() const;
    float getMaxHealth() const;
    
    float getCurrentStamina() const;
    float getMaxStamina() const;
    
    sf::FloatRect getAttackBounds() const;
    sf::Sprite& getSprite();
    
    void applyKnockback(const sf::Vector2f& direction, float force);
    
    float getAttackDamage() const;
    
    sf::FloatRect getHitbox() const;
    sf::FloatRect getAttackHitbox() const;
    
    void reset();
    
    WeaponType getCurrentWeapon() const { return currentWeapon; }
    void setWeapon(WeaponType weapon);
    void shoot(const sf::Vector2f& target, PhysicsWorld& physicsWorld, std::vector<std::unique_ptr<Bullet>>& bullets);

    float timeSinceLastShot = 0.0f;
    float fireCooldown = 0.0f; // Time (in seconds) between shots

    void syncSpriteWithBody();
private:
    void loadTextures();
    void updateAnimation(float deltaTime);
    void setState(PlayerState newState);
    void updateStamina(float deltaTime);
    
    sf::Sprite sprite;
    PlayerState currentState;
    
    float speed;
    float sprintSpeed;
    float rotationSpeed;
    float scaleFactor;
    
    float health;
    float maxHealth;
    
    float healthRegenRate = 5.0f;
    float timeSinceLastDamage = 0.0f;
    const float HEALTH_REGEN_DELAY = 5.0f;

    float stamina;
    float maxStamina;
    float staminaRegenRate;
    float staminaDrainRate;
    float staminaRegenDelay;
    float timeSinceStaminaUse;
    bool isStaminaRegenerating;
    
    int currentFrame;
    float animationTimer;
    float animationFrameTime;
    
    bool attacking;
    int currentAttackFrame;
    float attackTimer;
    float attackFrameTime;
    sf::FloatRect attackBounds;
    
    float timeSinceLastMeleeAttack = 0.0f;
    float meleeAttackCooldown = 0.4f;

    bool dead;
    int currentDeathFrame;
    float deathTimer;
    float deathFrameTime;
    
    // Current weapon
    WeaponType currentWeapon;

    // Texture collections for each weapon
    std::vector<sf::Texture> knifeIdleTextures;
    std::vector<sf::Texture> knifeWalkingTextures;
    std::vector<sf::Texture> knifeAttackTextures;
    
    std::vector<sf::Texture> batIdleTextures;
    std::vector<sf::Texture> batWalkingTextures;
    std::vector<sf::Texture> batAttackTextures;
    
    std::vector<sf::Texture> pistolIdleTextures;
    std::vector<sf::Texture> pistolWalkingTextures;
    std::vector<sf::Texture> pistolAttackTextures;
    
    std::vector<sf::Texture> rifleIdleTextures;
    std::vector<sf::Texture> rifleWalkingTextures;
    std::vector<sf::Texture> rifleAttackTextures;
    
    std::vector<sf::Texture> flamethrowerIdleTextures;
    std::vector<sf::Texture> flamethrowerWalkingTextures;
    std::vector<sf::Texture> flamethrowerAttackTextures;
    
    std::vector<sf::Texture> deathTextures;

    // Sound buffers and sounds for each weapon
    sf::SoundBuffer knifeAttackBuffer;
    sf::Sound knifeAttackSound;
    sf::SoundBuffer batAttackBuffer;
    sf::Sound batAttackSound;
    sf::SoundBuffer pistolAttackBuffer;
    sf::Sound pistolAttackSound;
    sf::SoundBuffer rifleAttackBuffer;
    sf::Sound rifleAttackSound;
    sf::SoundBuffer flamethrowerAttackBuffer;
    sf::Sound flamethrowerAttackSound;
    sf::SoundBuffer walkingBuffer;
    sf::Sound walkingSound;
};

#endif // PLAYER_H