#ifndef ZOMBIE_TANK_H
#define ZOMBIE_TANK_H

#include "BaseZombie.h"

class ZombieTank : public BaseZombie {
public:
    ZombieTank(float x, float y);
    
    ZombieType getType() const override { return ZombieType::TANK; }
    
protected:
    void loadTextures() override;
};

#endif