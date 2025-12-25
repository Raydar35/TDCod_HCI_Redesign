#include "ZombieWalker.h"
#include <iostream>

ZombieWalker::ZombieWalker(float x, float y)
    : ZombieWalker(x, y, 50.0f, 5.0f, 50.0f) {}

ZombieWalker::ZombieWalker(float x, float y, float health, float attackDamage, float speed)
    : BaseZombie(x, y, health, attackDamage, speed, 100.0f, 2.0f) // attackRange, attackCooldown
{
    // Tweak walker rotation to match sprite art orientation
    rotationOffset = 0.0f; // adjust if sprite faces a different base direction
    loadTextures();

    // If a spritesheet was provided (walkSheetTexture + walkRects), use that with Animator; otherwise fall back to per-frame textures
    if (hasWalkSheet && walkRects.size() > 0) {
        sprite.setTexture(walkSheetTexture);
        // show the first frame immediately instead of the full sheet
        sprite.setTextureRect(walkRects[0]);
        // center origin based on first rect
        sprite.setOrigin(walkRects[0].width / 2.0f, walkRects[0].height / 2.0f);
        sprite.setScale(0.37f, 0.37f);
    } else if (!walkTextures.empty()) {
        sprite.setTexture(walkTextures[0]);
        sf::Vector2u textureSize = walkTextures[0].getSize();
        sprite.setOrigin(textureSize.x / 2.0f, textureSize.y / 2.0f);
        sprite.setScale(0.4f, 0.4f);
    }
    // Start walk animation
    setState(ZombieState::WALK);
    // Ensure animator is configured and playing immediately (some edge cases need explicit start)
    if (hasWalkSheet && !walkRects.empty()) {
        animator.setFrames(&walkSheetTexture, walkRects, walkFrameTime, true);
        animator.play(true);
    } else if (!walkTextures.empty()) {
        animator.setFrames(&walkTextures, walkFrameTime, true);
        animator.play(true);
    }
    sprite.setPosition(body.position.x, body.position.y);
}

void ZombieWalker::loadTextures() {
    // load a single sheet instead of per-frame files
    if (walkSheetTexture.loadFromFile("assets/ZombieWalker/zombie_move.png")) {
        hasWalkSheet = true;
        int frameW = 228; int frameH = 311; // set your frame size
        sf::Vector2u ts = walkSheetTexture.getSize();
        int cols = ts.x / frameW;
        int rows = ts.y / frameH;
        walkRects.clear();
        for (int r = 0; r < rows; ++r)
          for (int c = 0; c < cols; ++c)
            walkRects.emplace_back(c*frameW, r*frameH, frameW, frameH);
    }

    if (attackSheetTexture.loadFromFile("assets/ZombieWalker/zombie_attack.png")) {
        hasAttackSheet = true;
        int frameW = 318; int frameH = 294; // set your frame size
        sf::Vector2u ts = attackSheetTexture.getSize();
        int cols = ts.x / frameW;
        int rows = ts.y / frameH;
        attackRects.clear();
        for (int r = 0; r < rows; ++r)
          for (int c = 0; c < cols; ++c)
            attackRects.emplace_back(c*frameW, r*frameH, frameW, frameH);
    }

    // No death textures needed — zombies despawn immediately on death
}

void ZombieWalker::updateAnimation(float deltaTime) {
    // run base animation handling first (this will also advance animator)
    BaseZombie::updateAnimation(deltaTime);

    // If currently attacking, check animator frame to trigger lunge or allow damage
    if (currentState == ZombieState::ATTACK) {
        size_t frame = animator.getCurrentFrameIndex();

        // On pre-lunge frame, stop movement next frame (but keep rotating until lunge starts)
        if (!this->preLungeHandled && (int)frame >= this->preLungeFrame) {
            this->preLungeHandled = true;
            body.velocity = Vec2(0,0);
        }

        // Speed up first 4 frames by lowering frame time; restore after
        if ((int)frame < 4) animator.setFrameTime(attackFrameTime * 0.5f);
        else animator.setFrameTime(attackFrameTime);

        // Start lunge on configured start frame
        if (!this->lunging && (int)frame >= this->lungeStartFrame) {
            // snapshot player's position so lunge continues even if player moves
            this->lungeTargetPos = this->lastSeenPlayerPos;
            sf::Vector2f pos = sf::Vector2f(body.position.x, body.position.y);
            sf::Vector2f dir = sf::Vector2f(this->lungeTargetPos.x - pos.x, this->lungeTargetPos.y - pos.y);
            float len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
            if (len > 0.0001f) this->lungeDir = Vec2(dir.x/len, dir.y/len);
            else this->lungeDir = Vec2(std::cos((sprite.getRotation()-rotationOffset)*3.14159265f/180.f), std::sin((sprite.getRotation()-rotationOffset)*3.14159265f/180.f));

            this->lunging = true;
            this->lungeTimer = 0.0f;
            // lock facing to lunge direction and disable rotation
            float ang = std::atan2(this->lungeDir.y, this->lungeDir.x) * 180.0f / 3.14159265f;
            sprite.setRotation(ang + rotationOffset);
            this->canRotateOverride = false;
            // (debug logging removed)
        }

        // reset damage flag when attack starts (frame 0)
        if (animator.getCurrentFrameIndex() == 0) this->damageDealtThisAttack = false;
        // update BaseZombie flag indicating whether this frame is part of damage window
        bool inWindow = false;
        int cf = static_cast<int>(animator.getCurrentFrameIndex());
        for (int f : damageFrames) if (cf == f) { inWindow = true; break; }
        this->isInDamageWindow = inWindow;
    }

    if (this->lunging) {
        this->lungeTimer += deltaTime;
        body.velocity = this->lungeDir * this->lungeSpeed;
        int currentFrame = static_cast<int>(animator.getCurrentFrameIndex());
        if (currentFrame >= this->lungeStopFrame || this->lungeTimer >= this->lungeDuration) {
            this->lunging = false;
            this->lungeTimer = 0.0f;
            body.velocity = Vec2(0,0);
            this->preLungeHandled = false;
            this->canRotateOverride = true;
        }
    }
}

float ZombieWalker::tryDealDamage() {
    // Only allow damage during specific attack frames
    if (currentState == ZombieState::ATTACK) {
        int frame = static_cast<int>(animator.getCurrentFrameIndex());
        for (int f : damageFrames) {
            if (frame == f && !damageDealtThisAttack) {
                damageDealtThisAttack = true;
                return attackDamage;
            }
        }
    }
    return 0.0f;
}
