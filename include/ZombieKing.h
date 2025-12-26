#ifndef ZOMBIE_KING_H
#define ZOMBIE_KING_H

#include "BaseZombie.h"

class ZombieKing : public BaseZombie {
public:
    ZombieKing(float x, float y);
    
    void update(float deltaTime, sf::Vector2f playerPosition) override;
    void attack() override;
    
    ZombieType getType() const override { return ZombieType::KING; }
    
protected:
    void loadTextures() override;
    
private:
    std::vector<sf::Texture> attack2Textures;
    bool useSecondAttack;
    int currentAttack2Frame;
    float attack2Timer;
    float specialAttackCooldown;
    float timeSinceLastSpecialAttack;
};

#endif