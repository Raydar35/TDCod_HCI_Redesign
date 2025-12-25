#ifndef ZOMBIE_WALKER_H
#define ZOMBIE_WALKER_H

#include "BaseZombie.h"

class ZombieWalker : public BaseZombie {
public:
    ZombieWalker(float x, float y);
    ZombieWalker(float x, float y, float health, float attackDamage, float speed);
    
    ZombieType getType() const override { return ZombieType::WALKER; }
    
    // override to implement lunge and timed attack window
    float tryDealDamage() override;
    void updateAnimation(float deltaTime) override;
    
protected:
    void loadTextures() override;
    // Lunge state
    bool lunging = false;
    Vec2 lungeDir = Vec2(0,0);
    float lungeSpeed = 120.0f; // reduced distance/speed
    float lungeDuration = 0.09f; // shorter duration
    float lungeTimer = 0.0f;
    // snapshot of player's position when lunge starts so lunge continues forward
    Vec2 lungeTargetPos = Vec2(0,0);

    // Attack timing
    int lungeStartFrame = 4; // 0-based: start lunge on 5th frame
    int preLungeFrame = 2; // frame before lunge (stop movement on this frame)
    bool preLungeHandled = false;
    int lungeStopFrame = 6; // 0-based stop frame (7th frame)
    std::vector<int> damageFrames = {5,6}; // 0-based frames where damage is allowed (6th and 7th)
    bool damageDealtThisAttack = false;
    bool canRotateOverride = true;
    bool canRotate() const override { return canRotateOverride; }
    bool isMovementLocked() const override { return lunging; }
 };

#endif