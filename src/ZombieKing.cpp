#include "ZombieKing.h"
#include <iostream>
#include <cmath>

ZombieKing::ZombieKing(float x, float y)
    : BaseZombie(x, y, 200.0f, 20.0f, 20.0f, 150.0f, 2.5f),
    useSecondAttack(false),
    currentAttack2Frame(0),
    attack2Timer(0.0f),
    specialAttackCooldown(5.0f),
    timeSinceLastSpecialAttack(0.0f) {
    loadTextures();

    if (!walkTextures.empty()) {
        sprite.setTexture(walkTextures[0]);
        sf::Vector2u textureSize = walkTextures[0].getSize();
        sprite.setOrigin(textureSize.x / 2.0f, textureSize.y / 2.0f);
        sprite.setScale(0.5f, 0.5f);
        sprite.setPosition(body.position.x, body.position.y);
    }
}

void ZombieKing::update(float deltaTime, sf::Vector2f playerPosition) {
    if (dead) {
        updateAnimation(deltaTime);
        return;
    }

    timeSinceLastAttack += deltaTime;
    timeSinceLastSpecialAttack += deltaTime;

    Vec2 playerPos(playerPosition.x, playerPosition.y);
    Vec2 direction = playerPos - body.position;
    float distance = direction.length();

    if (distance <= attackRange && !attacking) {
        if (timeSinceLastSpecialAttack >= specialAttackCooldown && useSecondAttack) {
            attack();
            timeSinceLastSpecialAttack = 0.0f;
            useSecondAttack = false;
        }
        else if (timeSinceLastAttack >= attackCooldown) {
            attack();
            timeSinceLastAttack = 0.0f;
            useSecondAttack = true;
        }
        body.velocity = Vec2(0, 0);
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

void ZombieKing::attack() {
    if (!attacking && !dead) {
        attacking = true;
        currentAttackFrame = 0;
        currentAttack2Frame = 0;
        attackTimer = 0.0f;
        m_hasDealtDamageInAttack = false;
        setState(ZombieState::ATTACK);

        if (useSecondAttack && !attack2Textures.empty()) {
            sprite.setTexture(attack2Textures[currentAttack2Frame]);
            attackDamage = 30.0f;
        } else if (!attackTextures.empty()) {
            sprite.setTexture(attackTextures[currentAttackFrame]);
            attackDamage = 20.0f;
        }
    }
}

void ZombieKing::loadTextures() {
    for (int i = 0; i < 8; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieKing/Walk/Walk_00";
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie king walk texture: " << filePath << std::endl;
        }
        walkTextures.push_back(texture);
    }

    for (int i = 0; i < 10; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieKing/Attack1/attack1_";
        if (i < 10) filePath += "00";
        else filePath += "0";
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie king attack texture: " << filePath << std::endl;
        }
        attackTextures.push_back(texture);
    }

    for (int i = 0; i < 9; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieKing/Attack2/Attack4_00";
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie king special attack texture: " << filePath << std::endl;
        }
        attack2Textures.push_back(texture);
    }

    for (int i = 0; i < 15; ++i) {
        if (i == 1 || i == 5) continue;
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieKing/Death/Death_";
        if (i < 10) filePath += "00";
        else filePath += "0";
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie king death texture: " << filePath << std::endl;
        }
        deathTextures.push_back(texture);
    }
}