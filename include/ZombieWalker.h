#ifndef ZOMBIE_WALKER_H
#define ZOMBIE_WALKER_H

#include "BaseZombie.h"

class ZombieWalker : public BaseZombie {
public:
    ZombieWalker(float x, float y);
    
    ZombieType getType() const override { return ZombieType::WALKER; }
    
protected:
    void loadTextures() override;
};

#endif