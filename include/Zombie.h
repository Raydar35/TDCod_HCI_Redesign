#ifndef ZOMBIE_H
#define ZOMBIE_H

#include <SFML/Graphics.hpp>
#include <vector>

#include "BaseZombie.h"


class Zombie {
public:
    Zombie(float x, float y);
    
    void update(float deltaTime, sf::Vector2f playerPosition);
    void draw(sf::RenderWindow& window) const;
    
    sf::FloatRect getBounds() const;
    sf::FloatRect getHitbox() const; // Added getHitbox method
    
    // Zombie actions
    void attack();
    void takeDamage(float amount);
    void kill();
    
    // Zombie state checks
    bool isAttacking() const;
    bool isDead() const;
    bool isAlive() const; // Added isAlive method
    
    float getAttackDamage() const;
    float getDamage() const; // Added getDamage method
    
    sf::Vector2f getPosition() const; // Added getPosition method

    // Method to attempt to deal damage (returns damage amount and sets flag)
    float tryDealDamage();

    // Added method to check if the zombie has dealt damage in the current attack
    bool hasDealtDamageInAttack() const;
    
 private:
    sf::Sprite sprite;
    std::vector<sf::Texture> walkTextures;
    std::vector<sf::Texture> attackTextures;
    std::vector<sf::Texture> deathTextures;
    
    int currentFrame;
    float animationTimer;
    float animationFrameTime;
    
    ZombieState currentState;
    sf::Vector2f position;
    float speed;
    
    // Attack properties
    bool attacking;
    int currentAttackFrame;
    float attackTimer;
    float attackFrameTime;
    float attackCooldown;
    float timeSinceLastAttack;
    float attackDamage;
    float attackRange;
    
    // Death properties
    bool dead;
    int currentDeathFrame;
    float deathTimer;
    float deathFrameTime;
    
    // Health
    float health;
    float maxHealth;
    
    // Helper methods
    void setState(ZombieState newState);
    void updateAnimation(float deltaTime);
    void loadTextures();

    // Flag to track if damage has been dealt in the current attack animation
    bool m_hasDealtDamageInAttack;
};

#endif // ZOMBIE_H