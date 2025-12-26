#ifndef ZOMBIE_ZOOM_H
#define ZOMBIE_ZOOM_H

#include "BaseZombie.h"

class ZombieZoom : public BaseZombie {
public:
    ZombieZoom(float x, float y);
    
    ZombieType getType() const override { return ZombieType::ZOOM; }
    
protected:
    void loadTextures() override;
};

#endif