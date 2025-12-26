#include "Zombie.h"
#include <iostream>
#include <cmath>

Zombie::Zombie(float x, float y)
    : position(x, y),
      currentState(ZombieState::WALK),
      speed(50.0f),
      currentFrame(0),
      animationTimer(0.0f),
      animationFrameTime(0.1f),
      attacking(false),
      currentAttackFrame(0),
      attackTimer(0.0f),
      attackFrameTime(0.1f),
      attackCooldown(2.0f),
      timeSinceLastAttack(0.0f),
      attackDamage(5.0f), // Reduced from 10.0f
      attackRange(40.0f), // Reduced from 60.0f
      dead(false),
      currentDeathFrame(0),
      deathTimer(0.0f),
      deathFrameTime(0.15f),
      health(30.0f),
      maxHealth(30.0f),
      m_hasDealtDamageInAttack(false) { // Initialize the new flag

    loadTextures();

    // Set initial texture to walk
    if (!walkTextures.empty()) {
        sprite.setTexture(walkTextures[0]);
        sf::Vector2u textureSize = walkTextures[0].getSize();
        sprite.setOrigin(textureSize.x / 2.0f, textureSize.y / 2.0f);
        sprite.setScale(0.4f, 0.4f); // Scale to appropriate size
        sprite.setPosition(position);
    }
}

void Zombie::loadTextures() {
    // Load walk animation textures
    for (int i = 0; i < 9; ++i) { // Assuming 32 frames for run animation
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieWalker/Walk/walk_00";
        // Removed conditional extra '0' to match 3-digit filenames like walk_000.png
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie walk texture: " << filePath << std::endl;
        }
        walkTextures.push_back(texture);
    }

    // Load attack animation textures
    for (int i = 0; i < 9; ++i) { // Assuming 32 frames for attack animation
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieWalker/Attack/Attack_00";
        // Removed conditional extra '0' to match 3-digit filenames like Attack_000.png
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie attack texture: " << filePath << std::endl;
        }
        attackTextures.push_back(texture);
    }

    // Load death animation textures
    for (int i = 0; i < 6; ++i) { // Assuming 30 frames for death animation
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieWalker/Death/Death_00";
        // Removed conditional extra '0' to match 3-digit filenames like Death_000.png
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie death texture: " << filePath << std::endl;
        }
        deathTextures.push_back(texture);
    }
}

void Zombie::update(float deltaTime, sf::Vector2f playerPosition) {
    // Don't update if dead (except for death animation)
    if (dead) {
        updateAnimation(deltaTime);
        return;
    }

    // Calculate direction and distance to player
    sf::Vector2f direction = playerPosition - position;
    float distance = std::sqrt(direction.x * direction.x + direction.y * direction.y);

    // Update attack cooldown timer
    timeSinceLastAttack += deltaTime;

    // Check if within attack range
    if (distance <= attackRange && timeSinceLastAttack >= attackCooldown) {
        attack();
        timeSinceLastAttack = 0.0f;
    } else if (!attacking) {
        // Move towards player if not attacking and not already on player
        if (distance > 5.0f) {
            setState(ZombieState::WALK); // Assuming setState handles the state change and animation reset

            // Normalize direction
            if (distance > 0) {
                direction /= distance;
            }

            // Move towards player
            position.x += direction.x * speed * deltaTime;
            position.y += direction.y * speed * deltaTime;
            sprite.setPosition(position);

            // Rotate to face player
            float angle = std::atan2(direction.y, direction.x) * 180 / 3.14159265f;
            sprite.setRotation(angle - 90.0f); // Consistent with Player's downward-facing sprite logic
        }
    }

    // Update animation
    updateAnimation(deltaTime);
}

void Zombie::draw(sf::RenderWindow& window) const {
    window.draw(sprite);

    // Debug: Draw a circle at sprite's reported position (Now removed)
    // sf::CircleShape posCircle(5.f);
    // posCircle.setPosition(sprite.getPosition());
    // posCircle.setOrigin(5.f, 5.f);
    // posCircle.setFillColor(sf::Color::Blue);
    // window.draw(posCircle);

    // Debug: Draw hitbox
    sf::RectangleShape hitboxShape;
    sf::FloatRect hitbox = getHitbox(); // This is an AABB
    // To center the AABB shape on the sprite's origin (blue circle)
    // Correctly position the debug shape using the global coordinates from getHitbox()
    hitboxShape.setPosition(hitbox.left, hitbox.top);
    hitboxShape.setSize(sf::Vector2f(hitbox.width, hitbox.height));
    hitboxShape.setFillColor(sf::Color(255, 0, 255, 70)); // Magenta, semi-transparent
    hitboxShape.setOutlineColor(sf::Color::Magenta);
    hitboxShape.setOutlineThickness(1);
    window.draw(hitboxShape);
}

sf::FloatRect Zombie::getBounds() const {
    return sprite.getGlobalBounds();
}

void Zombie::attack() {
    if (!attacking && !dead) {
        attacking = true;
        currentAttackFrame = 0;
        attackTimer = 0.0f;
        m_hasDealtDamageInAttack = false; // Reset damage flag at the start of attack
        setState(ZombieState::ATTACK); // Assuming setState handles the state change and animation reset

        if (!attackTextures.empty()) {
            sprite.setTexture(attackTextures[currentAttackFrame]);
        }
    }
}

void Zombie::takeDamage(float amount) {
    if (!dead) {
        health -= amount;
        if (health <= 0) {
            health = 0;
            kill();
        }
    }
}

void Zombie::kill() {
    if (!dead) {
        dead = true;
        attacking = false;
        currentDeathFrame = 0;
        deathTimer = 0.0f;
        setState(ZombieState::DEATH); // Assuming setState handles the state change and animation reset

        if (!deathTextures.empty()) {
            sprite.setTexture(deathTextures[currentDeathFrame]);
        }
    }
}

bool Zombie::isAttacking() const {
    return attacking;
}

bool Zombie::isDead() const {
    return dead;
}

float Zombie::getAttackDamage() const {
    return attackDamage;
}

sf::Vector2f Zombie::getPosition() const {
    return position;
}

bool Zombie::isAlive() const {
    return !dead;
}

float Zombie::getDamage() const {
    // Assuming getDamage is intended to return the same as getAttackDamage
    // If it's meant to be different, this logic should be updated.
    return attackDamage;
}

sf::FloatRect Zombie::getHitbox() const {
    // Define desired hitbox size in *scaled* pixels (these dimensions define a local box)
    const float desiredScaledWidth = 60.0f;
    const float desiredScaledHeight = 60.0f;
    const float zombieScaleFactor = 0.4f; // As defined in the constructor

    // Calculate the equivalent local (unscaled) dimensions for the local box
    float localBoxWidth = desiredScaledWidth / zombieScaleFactor;
    float localBoxHeight = desiredScaledHeight / zombieScaleFactor;

    // Create a local hitbox centered around the sprite's origin (0,0 in local origin-adjusted space)
    sf::FloatRect localBoxDefinition(
        -localBoxWidth / 2.0f,
        -localBoxHeight / 2.0f,
        localBoxWidth,
        localBoxHeight
    );

    // Get the AABB of this transformed local box. Its dimensions will be used.
    sf::FloatRect aabbOfTransformedLocalBox = sprite.getTransform().transformRect(localBoxDefinition);

    // The logical hitbox will be centered at the sprite's current position,
    // using the width and height of the AABB of the transformed local box.
    // This creates an AABB hitbox that doesn't rotate with the sprite visually.
    sf::Vector2f zombieCurrentPosition = sprite.getPosition();
    sf::FloatRect finalHitbox(
        zombieCurrentPosition.x - aabbOfTransformedLocalBox.width / 2.0f,
        zombieCurrentPosition.y - aabbOfTransformedLocalBox.height / 2.0f,
        aabbOfTransformedLocalBox.width,
        aabbOfTransformedLocalBox.height
    );

    // Optional: Log the final hitbox details
    return finalHitbox;
}
void Zombie::setState(ZombieState newState) {
    if (currentState != newState && !dead) {
        currentState = newState;

        // Reset animation for the new state
        if (currentState != ZombieState::ATTACK) {
            currentFrame = 0;
            animationTimer = 0.0f;
        }
    }
}

void Zombie::updateAnimation(float deltaTime) {
    if (dead) {
        // Death animation
        deathTimer += deltaTime;
        if (deathTimer >= deathFrameTime) {
            deathTimer -= deathFrameTime;
            if (currentDeathFrame < deathTextures.size() - 1) {
                currentDeathFrame++;
                if (!deathTextures.empty()) {
                    sprite.setTexture(deathTextures[currentDeathFrame]);
                }
            }
        }
        return;
    }

    if (attacking) {
        // Attack animation
        attackTimer += deltaTime;
        if (attackTimer >= attackFrameTime) {
            attackTimer -= attackFrameTime;
            currentAttackFrame++;

            if (currentAttackFrame < attackTextures.size()) {
                sprite.setTexture(attackTextures[currentAttackFrame]);
            } else {
                // Attack animation finished
                attacking = false;
                m_hasDealtDamageInAttack = false; // Reset damage flag when attack finishes

                // Revert to walk state
                setState(ZombieState::WALK); // Assuming setState handles the state change and animation reset
                if (!walkTextures.empty()) {
                    sprite.setTexture(walkTextures[currentFrame]);
                }
            }
        }
    } else {
        // Walk animation
        animationTimer += deltaTime;
        if (animationTimer >= animationFrameTime) {
            animationTimer -= animationFrameTime;
            currentFrame = (currentFrame + 1) % walkTextures.size();
            if (!walkTextures.empty()) {
                sprite.setTexture(walkTextures[currentFrame]);
            }

        }
    }
}

// Definition for the new getter method
bool Zombie::hasDealtDamageInAttack() const {
    return m_hasDealtDamageInAttack;
}

// Definition for the new tryDealDamage method
float Zombie::tryDealDamage() {
    if (!m_hasDealtDamageInAttack) {
        m_hasDealtDamageInAttack = true; // Mark that damage has been dealt in this attack
        return attackDamage; // Return the damage amount
    }
    return 0.0f; // Damage already dealt in this attack cycle
}
