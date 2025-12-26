#ifndef ZOMBIE_CRAWLER_H
#define ZOMBIE_CRAWLER_H

#include "BaseZombie.h"

class ZombieCrawler : public BaseZombie {
public:
    ZombieCrawler(float x, float y);
    
    ZombieType getType() const override { return ZombieType::CRAWLER; }
    
protected:
    void loadTextures() override;
};

#endif