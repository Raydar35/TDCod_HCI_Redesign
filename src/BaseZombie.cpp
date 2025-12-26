#include "BaseZombie.h"
#include <iostream>
#include <cmath>

BaseZombie::BaseZombie(float x, float y, float health, float attackDamage, float speed, float attackRange, float attackCooldown)
    : Entity(EntityType::Enemy, Vec2(x, y), Vec2(60.f, 60.f), false, 1.0f, true),
    currentState(ZombieState::WALK),
    speed(speed),
    currentFrame(0),
    animationTimer(0.0f),
    animationFrameTime(0.1f),
    attacking(false),
    currentAttackFrame(0),
    attackTimer(0.0f),
    attackFrameTime(0.1f),
    attackCooldown(attackCooldown),
    timeSinceLastAttack(0.0f),
    attackDamage(attackDamage),
    attackRange(attackRange),
    dead(false),
    currentDeathFrame(0),
    deathTimer(0.0f),
    deathFrameTime(0.15f),
    health(health),
    maxHealth(health),
    m_hasDealtDamageInAttack(false) {
}

void BaseZombie::update(float deltaTime, sf::Vector2f playerPosition) {
    if (dead) {
        updateAnimation(deltaTime);
        return;
    }

    Vec2 playerPos(playerPosition.x, playerPosition.y);
    Vec2 direction = playerPos - body.position;
    float distance = direction.length();

    timeSinceLastAttack += deltaTime;

    if (distance <= attackRange && timeSinceLastAttack >= attackCooldown) {
        attack();
        timeSinceLastAttack = 0.0f;
        body.velocity = Vec2(0, 0); // Stop moving while attacking
    }
    else if (!attacking) {
        if (distance > 5.0f) {
            setState(ZombieState::WALK);

            if (distance > 0) direction.normalize();
            body.velocity = direction * speed;
        }
        else {
            body.velocity = Vec2(0, 0);
        }
    }
    else {
        body.velocity = Vec2(0, 0);
    }

    // Sync sprite position and rotation with physics body
    sprite.setPosition(body.position.x, body.position.y);
    if (distance > 1.0f) {
        float angle = std::atan2(direction.y, direction.x) * 180 / 3.14159265f;
        sprite.setRotation(angle - 90.0f);
    }

    updateAnimation(deltaTime);
}

void BaseZombie::draw(sf::RenderWindow& window) const {
    window.draw(sprite);

    if (!dead) {
        float barWidth = 50.0f;
        float barHeight = 5.0f;
        sf::RectangleShape healthBarBackground;
        healthBarBackground.setSize(sf::Vector2f(barWidth, barHeight));
        healthBarBackground.setFillColor(sf::Color(100, 100, 100, 150));
        healthBarBackground.setPosition(body.position.x - barWidth / 2, body.position.y - 50);

        sf::RectangleShape healthBar;
        float healthPercent = health / maxHealth;
        if (healthPercent < 0) healthPercent = 0;
        healthBar.setSize(sf::Vector2f(barWidth * healthPercent, barHeight));
        healthBar.setFillColor(sf::Color::Red);
        healthBar.setPosition(body.position.x - barWidth / 2, body.position.y - 50);

        window.draw(healthBarBackground);
        window.draw(healthBar);
    }
}

sf::FloatRect BaseZombie::getBounds() const {
    return sprite.getGlobalBounds();
}

void BaseZombie::attack() {
    if (!attacking && !dead) {
        std::cout << "Zombie is attacking!" << std::endl; // TEMP LOG

        attacking = true;
        currentAttackFrame = 0;
        attackTimer = 0.0f;
        m_hasDealtDamageInAttack = false;
        setState(ZombieState::ATTACK);

        if (!attackTextures.empty()) {
            sprite.setTexture(attackTextures[currentAttackFrame]);
        }
    }
}

void BaseZombie::takeDamage(float amount) {
    if (!dead) {
        health -= amount;
        if (health <= 0) {
            health = 0;
            kill();
        }
    }
}

void BaseZombie::kill() {
    if (!dead) {
        dead = true;
        attacking = false;
        currentDeathFrame = 0;
        deathTimer = 0.0f;
        setState(ZombieState::DEATH);

        if (!deathTextures.empty()) {
            sprite.setTexture(deathTextures[currentDeathFrame]);
        }
    }
}

bool BaseZombie::isAttacking() const {
    return attacking;
}

bool BaseZombie::isDead() const {
    return dead;
}

bool BaseZombie::isAlive() const {
    return !dead;
}

float BaseZombie::getAttackDamage() const {
    return attackDamage;
}

sf::Vector2f BaseZombie::getPosition() const {
    return sf::Vector2f(body.position.x, body.position.y);
}

sf::FloatRect BaseZombie::getHitbox() const {
    const float desiredScaledWidth = 60.0f;
    const float desiredScaledHeight = 60.0f;
    const float zombieScaleFactor = 0.4f;

    float localBoxWidth = desiredScaledWidth / zombieScaleFactor;
    float localBoxHeight = desiredScaledHeight / zombieScaleFactor;

    sf::FloatRect localBoxDefinition(
        -localBoxWidth / 2.0f,
        -localBoxHeight / 2.0f,
        localBoxWidth,
        localBoxHeight
    );

    sf::FloatRect aabbOfTransformedLocalBox = sprite.getTransform().transformRect(localBoxDefinition);

    sf::Vector2f zombieCurrentPosition = sprite.getPosition();
    sf::FloatRect finalHitbox(
        zombieCurrentPosition.x - aabbOfTransformedLocalBox.width / 2.0f,
        zombieCurrentPosition.y - aabbOfTransformedLocalBox.height / 2.0f,
        aabbOfTransformedLocalBox.width,
        aabbOfTransformedLocalBox.height
    );

    return finalHitbox;
}

void BaseZombie::setState(ZombieState newState) {
    if (currentState != newState && !dead) {
        currentState = newState;
        currentFrame = 0;
        animationTimer = 0.0f;

        if (newState == ZombieState::ATTACK) {
            currentAttackFrame = 0;
            attackTimer = 0.0f;
        }
        else if (newState == ZombieState::WALK) {
            currentFrame = 0;
            animationTimer = 0.0f;
        }
    }
}

void BaseZombie::updateAnimation(float deltaTime) {
    if (dead) {
        deathTimer += deltaTime;
        if (deathTimer >= deathFrameTime) {
            deathTimer -= deathFrameTime;
            if (!deathTextures.empty() && currentDeathFrame < deathTextures.size()) {
                if(currentDeathFrame < deathTextures.size() - 1){
                    currentDeathFrame++;
                }
                sprite.setTexture(deathTextures[currentDeathFrame]);
            }
        }
        return;
    }

    if (attacking) {
        attackTimer += deltaTime;
        if (attackTimer >= attackFrameTime) {
            attackTimer -= attackFrameTime;
            currentAttackFrame++;

            if (currentAttackFrame < attackTextures.size()) {
                sprite.setTexture(attackTextures[currentAttackFrame]);
            }
            else {
                attacking = false;
                m_hasDealtDamageInAttack = false;
                setState(ZombieState::WALK);
                if (!walkTextures.empty()) {
                    sprite.setTexture(walkTextures[currentFrame]);
                }
            }
        }
    }
    else {
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

bool BaseZombie::hasDealtDamageInAttack() const {
    return m_hasDealtDamageInAttack;
}

float BaseZombie::tryDealDamage() {
    if (!m_hasDealtDamageInAttack) {
        m_hasDealtDamageInAttack = true;
        return attackDamage;
    }
    return 0.0f;
}

void BaseZombie::loadTextureSet(std::vector<sf::Texture>& textures, const std::string& basePath,
    const std::string& prefix, int count, bool isBoss) {
    textures.clear();

    for (int i = 0; i < count; ++i) {
        sf::Texture texture;
        std::string filePath = basePath + "/" + prefix + "_";

        if (i < 10) {
            filePath += "0" + std::to_string(i) + ".png";
        }
        else {
            filePath += std::to_string(i) + ".png";
        }

        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie texture: " << filePath << std::endl;
        }
        textures.push_back(texture);
    }
}
