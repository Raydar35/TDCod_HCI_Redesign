#include "ZombieTank.h"
#include <iostream>

ZombieTank::ZombieTank(float x, float y)
    : BaseZombie(x, y, 500.0f, 12.0f, 30.0f, 100.0f, 3.0f) {
    loadTextures();

    if (!walkTextures.empty()) {
        sprite.setTexture(walkTextures[0]);
        sf::Vector2u textureSize = walkTextures[0].getSize();
        sprite.setOrigin(textureSize.x / 2.0f, textureSize.y / 2.0f);
        sprite.setScale(0.45f, 0.45f);
        sprite.setPosition(body.position.x, body.position.y);
    }
}

void ZombieTank::loadTextures() {
    for (int i = 0; i < 9; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieTank/Walk/walk_00";
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie tank walk texture: " << filePath << std::endl;
        }
        walkTextures.push_back(texture);
    }

    for (int i = 0; i < 9; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieTank/Attack/attack_00";
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie tank attack texture: " << filePath << std::endl;
        }
        attackTextures.push_back(texture);
    }

    for (int i = 0; i < 6; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/ZombieTank/Death/death_00";
        filePath += std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie tank death texture: " << filePath << std::endl;
        }
        deathTextures.push_back(texture);
    }
}
