#include "ZombieWalker.h"
#include <iostream>

ZombieWalker::ZombieWalker(float x, float y)
    : BaseZombie(x, y, 50.0f, 5.0f, 50.0f, 100.0f, 2.0f) // health, attackDamage, speed, attackRange, attackCooldown
{
    loadTextures();

    if (!walkTextures.empty()) {
        sprite.setTexture(walkTextures[0]);
        sf::Vector2u textureSize = walkTextures[0].getSize();
        sprite.setOrigin(textureSize.x / 2.0f, textureSize.y / 2.0f);
        sprite.setScale(0.4f, 0.4f);
        // Set sprite position to match the physics body
        sprite.setPosition(body.position.x, body.position.y);
    }
}

void ZombieWalker::loadTextures() {
    for (int i = 0; i < 9; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieWalker/Walk/walk_00";
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie walk texture: " << filePath << std::endl;
        }
        walkTextures.push_back(texture);
    }

    for (int i = 0; i < 9; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieWalker/Attack/Attack_00";
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie attack texture: " << filePath << std::endl;
        }
        attackTextures.push_back(texture);
    }

    for (int i = 0; i < 6; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieWalker/Death/Death_00";
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie death texture: " << filePath << std::endl;
        }
        deathTextures.push_back(texture);
    }
}
