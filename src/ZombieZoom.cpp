#include "ZombieZoom.h"
#include <iostream>

ZombieZoom::ZombieZoom(float x, float y)
    : BaseZombie(x, y, 50.0f, 6.0f, 90.0f, 100.0f, 1.5f) {
    loadTextures();

    if (!walkTextures.empty()) {
        sprite.setTexture(walkTextures[0]);
        sf::Vector2u textureSize = walkTextures[0].getSize();
        sprite.setOrigin(textureSize.x / 2.0f, textureSize.y / 2.0f);
        sprite.setScale(0.38f, 0.38f);
        sprite.setPosition(body.position.x, body.position.y);
    }
}

void ZombieZoom::loadTextures() {
    for (int i = 0; i < 9; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieZoom/Walk/Walk_00";
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie zoom walk texture: " << filePath << std::endl;
        }
        walkTextures.push_back(texture);
    }

    for (int i = 0; i < 9; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieZoom/Attack/Attack_00";
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie zoom attack texture: " << filePath << std::endl;
        }
        attackTextures.push_back(texture);
    }

    for (int i = 0; i < 6; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieZoom/Death/Death_00";
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie zoom death texture: " << filePath << std::endl;
        }
        deathTextures.push_back(texture);
    }
}
