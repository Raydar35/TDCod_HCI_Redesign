#include "Player.h"
#include "Bullet.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>

// Recent muzzle markers for debug visualization
static std::vector<std::pair<sf::Vector2f, float>> g_recentMuzzles; // position, remaining life

// Helper: apply texture and consistent origin/scale to the player sprite
static inline void applyTextureToSprite(sf::Sprite& sprite, const sf::Texture& tex, float scaleFactor) {
    sprite.setTexture(tex);
    sf::Vector2u ts = tex.getSize();
    // center origin
    sprite.setOrigin(ts.x / 2.0f, ts.y / 2.0f);
    sprite.setScale(scaleFactor, scaleFactor);
}

// Helper: angle between two vectors in degrees (-180,180]
static float signedAngleDeg(const sf::Vector2f& a, const sf::Vector2f& b) {
    float cross = a.x * b.y - a.y * b.x;
    float dot = a.x * b.x + a.y * b.y;
    float ang = std::atan2(cross, dot) * 180.0f / 3.14159265f;
    return ang;
}

Player::Player(Vec2 position)
    : Entity(EntityType::Player, position, Vec2(60.f, 60.f), false, 1.f, true),
    currentState(PlayerState::IDLE),
    speed(70.0f),
    sprintSpeed(105.0f),
    health(100.0f),
    maxHealth(100.0f),
    stamina(100.0f),
    maxStamina(100.0f),
    staminaRegenRate(50.0f),
    staminaDrainRate(20.0f),
    staminaRegenDelay(1.0f),
    timeSinceStaminaUse(0.0f),
    isStaminaRegenerating(true),
    currentFrame(0),
    animationTimer(0.0f),
    animationFrameTime(0.1f),
    attacking(false),
    currentAttackFrame(0),
    attackTimer(0.0f),
    attackFrameTime(0.05f),
    dead(false),
    currentDeathFrame(0),
    deathTimer(0.0f),
    deathFrameTime(0.1f),
    rotationSpeed(180.0f),
    scaleFactor(0.35f),
    weaponAmmoInitialized(false),
    pistolAmmoInMag(0),
    rifleAmmoInMag(0)
{
    loadTextures();
    // Initialize default weapon after textures are loaded
    setWeapon(WeaponType::PISTOL);
    // Ensure initial magazine is full on startup only
    if (!weaponAmmoInitialized) {
        pistolAmmoInMag = 12;
        rifleAmmoInMag = 30;
        currentAmmo = pistolAmmoInMag;
        weaponAmmoInitialized = true;
    }

    // Attach animators to sprites
    upperAnimator.setSprite(&sprite);
    feetAnimator.setSprite(&feetSprite);

    feetSprite.setOrigin(0,0);

    upperAnimator.setOnComplete([this]() {
        this->attacking = false;
        if (this->body.velocity.x != 0.0f || this->body.velocity.y != 0.0f) this->setState(PlayerState::WALK);
        else this->setState(PlayerState::IDLE);
    });
}

void Player::setFeetStateSheet(FeetState state, const sf::Texture& sheetTexture, const std::vector<sf::IntRect>& frames, float frameTime) {
    int idx = static_cast<int>(state);
    if (idx < 0 || idx >= (int)feetStateTextures.size()) return;
    feetStateTextures[idx] = &sheetTexture;
    if (idx >= (int)feetFramesByState.size()) feetFramesByState.resize(feetStateTextures.size());
    feetFramesByState[idx] = frames;
    feetFrameTimes[idx] = frameTime;
}

void Player::setFeetSpriteSheet(const sf::Texture& sheetTexture, const std::vector<std::vector<sf::IntRect>>& framesByState, float frameTime) {
    // Apply same frameTime to all states if provided as uniform
    for (size_t i = 0; i < framesByState.size() && i < feetStateTextures.size(); ++i) {
        feetStateTextures[i] = &sheetTexture;
        if (i >= feetFramesByState.size()) feetFramesByState.resize(feetStateTextures.size());
        feetFramesByState[i] = framesByState[i];
        feetFrameTimes[i] = frameTime;
    }

    // Start idle
    if (!feetFramesByState.empty() && !feetFramesByState[static_cast<int>(FeetState::IDLE)].empty()) {
        int idx = static_cast<int>(FeetState::IDLE);
        feetAnimator.setFrames(feetStateTextures[idx], feetFramesByState[idx], feetFrameTimes[idx], true);
        feetAnimator.play(true);
        feetSprite.setTexture(*feetStateTextures[idx]);
        feetSprite.setTextureRect(feetFramesByState[idx][0]);
        feetSprite.setOrigin(feetFramesByState[idx][0].width/2.f + feetOffsetX, feetFramesByState[idx][0].height/2.f + feetOffsetY);
        feetSprite.setScale(scaleFactor, scaleFactor);
    }
}

void Player::setFeetStateInternal(FeetState newState) {
    if (currentFeetState == newState) return;
    int idx = static_cast<int>(newState);
    if (idx < 0 || idx >= (int)feetStateTextures.size()) return;
    if (!feetStateTextures[idx] || feetFramesByState.size() <= idx || feetFramesByState[idx].empty()) {
        // nothing assigned for this state
        return;
    }

    currentFeetState = newState;
    feetAnimator.setFrames(feetStateTextures[idx], feetFramesByState[idx], feetFrameTimes[idx], true);
    feetAnimator.play(true);
    feetSprite.setTexture(*feetStateTextures[idx]);
    feetSprite.setTextureRect(feetFramesByState[idx][0]);
    feetSprite.setOrigin(feetFramesByState[idx][0].width/2.f + feetOffsetX, feetFramesByState[idx][0].height/2.f + feetOffsetY);
    feetSprite.setScale(scaleFactor + 0.05f, scaleFactor + 0.05f);
}

// New: accept array of 3 sheet pointers per weapon
void Player::setUpperWeaponSheet(WeaponType weapon, const std::array<const sf::Texture*,4>& sheets, const std::vector<std::vector<sf::IntRect>>& framesByState, const std::array<float,4>& frameTimes) {
    if (weapon == WeaponType::PISTOL) {
        pistolUpperSheets = sheets;
        pistolUpperFramesByState = framesByState;
        pistolUpperFrameTimes = frameTimes;
        // start with idle if available
        if (pistolUpperSheets[0] && !pistolUpperFramesByState.empty() && !pistolUpperFramesByState[0].empty()) {
            upperAnimator.setFrames(pistolUpperSheets[0], pistolUpperFramesByState[0], pistolUpperFrameTimes[0], true);
            upperAnimator.play(true);
            sprite.setTexture(*pistolUpperSheets[0]);
            sprite.setTextureRect(pistolUpperFramesByState[0][0]);
            sprite.setOrigin(pistolUpperFramesByState[0][0].width/2.f + upperOriginOffsetX, pistolUpperFramesByState[0][0].height/2.f + upperOriginOffsetY);
            sprite.setScale(scaleFactor, scaleFactor);
        }
    }
    else if (weapon == WeaponType::RIFLE) {
        rifleUpperSheets = sheets;
        rifleUpperFramesByState = framesByState;
        rifleUpperFrameTimes = frameTimes;
        if (rifleUpperSheets[0] && !rifleUpperFramesByState.empty() && !rifleUpperFramesByState[0].empty()) {
            upperAnimator.setFrames(rifleUpperSheets[0], rifleUpperFramesByState[0], rifleUpperFrameTimes[0], true);
            upperAnimator.play(true);
            sprite.setTexture(*rifleUpperSheets[0]);
            sprite.setTextureRect(rifleUpperFramesByState[0][0]);
            sprite.setOrigin(rifleUpperFramesByState[0][0].width/2.f + upperOriginOffsetX, rifleUpperFramesByState[0][0].height/2.f + upperOriginOffsetY);
            sprite.setScale(scaleFactor, scaleFactor);
        }
    }
}

void Player::updateAnimation(float deltaTime) {
    // Upper: decide which state frames to use: 0=idle,1=move,2=shoot
    const sf::Texture* upperSheet = nullptr;
    std::vector<std::vector<sf::IntRect>>* upperFramesByState = nullptr;

    if (currentWeapon == WeaponType::PISTOL && pistolUpperSheets[0]) {
        upperFramesByState = &pistolUpperFramesByState;
    } else if (currentWeapon == WeaponType::RIFLE && rifleUpperSheets[0]) {
        upperFramesByState = &rifleUpperFramesByState;
    }

    if (upperFramesByState && !upperFramesByState->empty()) {
        int stateIndex = 0; // idle
        bool moving = (body.velocity.x != 0.f || body.velocity.y != 0.f);
        // If reloading, force reload state (index 3) so movement doesn't override reload animation
        if (reloading) {
            stateIndex = 3;
        }
        else if (upperShooting) stateIndex = 2;
        else if (moving) stateIndex = 1;
        stateIndex = std::min<int>(stateIndex, (int)upperFramesByState->size()-1);

        // choose sheet pointer and frameTime for this state (pistol or rifle)
        float frameTime = 0.1f;
        if (currentWeapon == WeaponType::PISTOL) {
            upperSheet = pistolUpperSheets[stateIndex];
            if (stateIndex < (int)pistolUpperFrameTimes.size()) frameTime = pistolUpperFrameTimes[stateIndex];
        }
        else if (currentWeapon == WeaponType::RIFLE) {
            upperSheet = rifleUpperSheets[stateIndex];
            if (stateIndex < (int)rifleUpperFrameTimes.size()) frameTime = rifleUpperFrameTimes[stateIndex];
        }

        if (upperSheet) {
            // Only change frames if sheet/frame set or shooting flag changed
            if (currentUpperSheetPtr != upperSheet || currentUpperStateIndex != stateIndex || currentUpperShootingFlag != upperShooting) {
                upperAnimator.setFrames(upperSheet, (*upperFramesByState)[stateIndex], frameTime, !upperShooting);
                upperAnimator.play(true);
                currentUpperSheetPtr = upperSheet;
                currentUpperStateIndex = stateIndex;
                currentUpperShootingFlag = upperShooting;
            }
        }
    }

    upperAnimator.update(deltaTime);
    feetAnimator.update(deltaTime);
    // feet pos
    feetSprite.setPosition(body.position.x + feetOffsetX, body.position.y + feetOffsetY);
}

// update() uses setFeetStateInternal to switch feet animations based on movement
void Player::update(float deltaTime, sf::RenderWindow& window, sf::Vector2u mapSize, sf::Vector2f worldMousePosition) {
    // at top of update store previous pos
    prevPos = currPos;

    // Decay debug muzzle markers here rather than in render() so lifetime is frame-rate independent
    if (!g_recentMuzzles.empty()) {
        for (auto it = g_recentMuzzles.begin(); it != g_recentMuzzles.end(); ) {
            it->second -= deltaTime;
            if (it->second <= 0.0f) it = g_recentMuzzles.erase(it);
            else ++it;
        }
    }

    timeSinceLastMeleeAttack += deltaTime;
    timeSinceLastShot += deltaTime;

    if (dead) {
        updateAnimation(deltaTime);
        if (walkingSound.getStatus() == sf::Sound::Playing) walkingSound.stop();
        return;
    }

    timeSinceLastDamage += deltaTime;
    if (timeSinceLastDamage >= HEALTH_REGEN_DELAY && health < maxHealth) {
        health = std::min(maxHealth, health + healthRegenRate * deltaTime);
    }

    // Input (movement keys are still read here; sprint/aim intent comes from Game via public API)
    bool left = sf::Keyboard::isKeyPressed(sf::Keyboard::A);
    bool right = sf::Keyboard::isKeyPressed(sf::Keyboard::D);
    bool up = sf::Keyboard::isKeyPressed(sf::Keyboard::W);
    bool down = sf::Keyboard::isKeyPressed(sf::Keyboard::S);
    // 'desiredSprint' and 'aiming' are set externally by Game

    // Build movement vector relative to world
    sf::Vector2f moveDir(0.f, 0.f);
    if (left) moveDir.x -= 1.f;
    if (right) moveDir.x += 1.f;
    if (up) moveDir.y -= 1.f;
    if (down) moveDir.y += 1.f;
    if (moveDir.x != 0.f || moveDir.y != 0.f) {
        float len = std::sqrt(moveDir.x*moveDir.x + moveDir.y*moveDir.y);
        moveDir.x /= len; moveDir.y /= len;
    }

    // Compute body-facing (from physics body to mouse) used for strafing angle
    sf::Vector2f bodyPos = sf::Vector2f(body.position.x, body.position.y);
    sf::Vector2f bodyFacing = worldMousePosition - bodyPos;
    if (bodyFacing.x == 0.f && bodyFacing.y == 0.f) bodyFacing = sf::Vector2f(0.f, -1.f);
    else { float bfl = std::sqrt(bodyFacing.x*bodyFacing.x + bodyFacing.y*bodyFacing.y); bodyFacing /= bfl; }

    // Determine if movement is mostly forward/back or strafing relative to facing
    float moveAngle = 0.f;
    if (moveDir.x != 0.f || moveDir.y != 0.f) {
        moveAngle = signedAngleDeg(bodyFacing, moveDir); // positive -> moveDir is to the left of facing
    }
    // Provide legacy alias 'angle' for any remaining code expecting that name
    float angle = moveAngle;

    // Compute rotation towards mouse using physics body as origin.
    // If the mouse is within a circular dead zone around the origin with radius equal to
    // the distance from origin to muzzle, skip rotation. This prevents the player from
    // trying to rotate toward positions that lie between origin and muzzle.
    sf::Vector2f toMouse = worldMousePosition - bodyPos;
    float toMouseLen = std::sqrt(toMouse.x*toMouse.x + toMouse.y*toMouse.y);
    if (toMouseLen > 1e-4f) {
        // Determine origin->muzzle distance
        sf::Vector2f muzzlePos = getMuzzlePosition(worldMousePosition);
        sf::Vector2f M = muzzlePos - bodyPos;
        float originToMuzzleDist = std::sqrt(M.x*M.x + M.y*M.y);

        // Compute aim direction from origin to mouse (unit)
        sf::Vector2f aimDir = toMouse / toMouseLen;

        // We'll search numerically for a sprite rotation (in radians) such that a forward
        // ray emitted from the muzzle (which depends on sprite rotation) passes through the mouse.
        // This is robust to rotationOffset and arbitrary local offsets.
        // Prepare local offsets consistent with getMuzzlePosition (no extra scale applied here)
        sf::Vector2f localOffset(0.f, 0.f);
        if (currentWeapon == WeaponType::PISTOL) localOffset = muzzleOffsetPistol;
        else if (currentWeapon == WeaponType::RIFLE) localOffset = muzzleOffsetRifle;

        // Numeric search: start around direct facing angle and sample to find best rotation
        float vx = toMouse.x;
        float vy = toMouse.y;
        float baseAngle = std::atan2(vy, vx);
        float bestAngle = baseAngle; // this will represent the sprite.getRotation in radians
        float bestErr = 1e30f;

        // search window +/- 90 degrees around baseAngle
        const int samples = 121;
        const float halfRange = 3.14159265f * 0.5f; // 90 degrees
        for (int i = 0; i < samples; ++i) {
            float t = (float)i / (float)(samples - 1);
            float a = baseAngle - halfRange + t * (2.0f * halfRange);
            // compute muzzle pos for candidate rotation a
            sf::Vector2f face(std::cos(a), std::sin(a));
            sf::Vector2f rightPerp(-face.y, face.x);
            sf::Vector2f muzzle = sf::Vector2f(bodyPos.x, bodyPos.y) + face * localOffset.x + rightPerp * localOffset.y;
            // vector from muzzle to mouse
            float mx = worldMousePosition.x - muzzle.x;
            float my = worldMousePosition.y - muzzle.y;
            // perpendicular error = rightPerp · (mouse - muzzle)
            float err = std::abs(rightPerp.x * mx + rightPerp.y * my);
            if (err < bestErr) { bestErr = err; bestAngle = a; }
        }

        // refine around bestAngle
        for (int iter = 0; iter < 4; ++iter) {
            float range = (halfRange / std::pow(4.0f, iter+1));
            int subSamples = 41;
            float localBest = bestAngle;
            float localBestErr = bestErr;
            for (int i = 0; i < subSamples; ++i) {
                float t = (float)i / (float)(subSamples - 1);
                float a = bestAngle - range + t * (2.0f * range);
                sf::Vector2f face(std::cos(a), std::sin(a));
                sf::Vector2f rightPerp(-face.y, face.x);
                sf::Vector2f muzzle = sf::Vector2f(bodyPos.x, bodyPos.y) + face * localOffset.x + rightPerp * localOffset.y;
                float mx = worldMousePosition.x - muzzle.x;
                float my = worldMousePosition.y - muzzle.y;
                float err = std::abs(rightPerp.x * mx + rightPerp.y * my);
                if (err < localBestErr) { localBestErr = err; localBest = a; }
            }
            bestAngle = localBest; bestErr = localBestErr;
        }

        float angleToMouseDegrees = bestAngle * 180.0f / 3.14159265f;

        // If mouse is inside deadzone: do not change rotation, but mark state
        if (toMouseLen < originToMuzzleDist && originToMuzzleDist > 1e-6f) {
            // entering or staying inside deadzone: remember start angle for transition
            if (!wasInDeadzone) {
                wasInDeadzone = true;
                deadzoneExitTimer = 0.0f;
                deadzoneStartAngle = sprite.getRotation();
            }
            // do not update sprite rotation while inside deadzone
        }
        else {
            // Mouse is outside deadzone
            if (wasInDeadzone) {
                // just left deadzone: start smooth transition
                wasInDeadzone = false;
                deadzoneExitTimer = 0.0f;
                // deadzoneStartAngle already set to the frozen angle while inside
            }

            if (deadzoneExitTimer < deadzoneExitDuration) {
                // Smoothly interpolate from deadzoneStartAngle -> angleToMouseDegrees
                deadzoneExitTimer += deltaTime;
                float t = std::min(1.0f, deadzoneExitTimer / deadzoneExitDuration);
                // smoothstep
                float w = t * t * (3.0f - 2.0f * t);
                // shortest angular difference
                float start = deadzoneStartAngle;
                float target = angleToMouseDegrees;
                float diff = target - start;
                while (diff > 180.0f) diff -= 360.0f;
                while (diff < -180.0f) diff += 360.0f;
                float angle = start + diff * w;
                sprite.setRotation(angle + rotationOffset);
                feetSprite.setRotation(angle + rotationOffset);
            } else {
                // finished transition, snap to target
                sprite.setRotation(angleToMouseDegrees + rotationOffset);
                feetSprite.setRotation(angleToMouseDegrees + rotationOffset);
            }
        }
    }

    // Keep sprite positioned at the authoritative physics body here so muzzle computation and visuals align.
    sprite.setPosition(body.position.x, body.position.y);

    // Allow sprinting only when not aiming (aiming flag is driven by Game)
    bool canSprint = (moveDir.x != 0.f || moveDir.y != 0.f) && !aiming;

    // Choose feet state
    if (moveDir.x == 0.f && moveDir.y == 0.f) {
        setFeetStateInternal(FeetState::IDLE);
    } else {
        // strafing: large signed angle magnitude near +/-90
        if (std::abs(moveAngle) > 45.f && std::abs(moveAngle) < 135.f) {
            // If player is attempting to sprint while strafing and has stamina, use RUN animation to show faster movement
            if (desiredSprint && canSprint && stamina > 0) {
                setFeetStateInternal(FeetState::RUN);
            } else {
                if (moveAngle > 0) setFeetStateInternal(FeetState::STRAFE_LEFT);
                else setFeetStateInternal(FeetState::STRAFE_RIGHT);
            }
        } else {
            // forward/back/diagonal -> walk or run
            if (desiredSprint && canSprint && stamina > 0) setFeetStateInternal(FeetState::RUN);
            else setFeetStateInternal(FeetState::WALK);
        }
    }

    // Apply movement magnitude
    Vec2 velVec(0.f,0.f);
    if (moveDir.x != 0.f || moveDir.y != 0.f) {
        float effectiveSpeed = speed;
        if (aiming) effectiveSpeed *= aimSpeedMultiplier; // slower while aiming
        if (desiredSprint && canSprint && stamina > 0) {
            velVec.x = moveDir.x * sprintSpeed;
            velVec.y = moveDir.y * sprintSpeed;
        } else {
            velVec.x = moveDir.x * effectiveSpeed;
            velVec.y = moveDir.y * effectiveSpeed;
        }
    }

    // Manage stamina based on desiredSprint provided by Game
    // Note: remove dependency on reloading so sprint input can cancel reload
    bool actuallySprinting = (desiredSprint && canSprint && (moveDir.x != 0.f || moveDir.y != 0.f) && stamina > 0 && !aiming);
    // If player began sprinting while reloading, cancel reload immediately (always allowed)
    if (actuallySprinting && reloading) {
        cancelReload();
    }

    if (actuallySprinting) {
        sprinting = true;
        stamina = std::max(0.0f, stamina - staminaDrainRate * deltaTime);
        isStaminaRegenerating = false;
        timeSinceStaminaUse = 0.0f;
        walkingSound.setPitch(1.5f);
        if (walkingSound.getStatus() != sf::Sound::Playing) walkingSound.play();
    } else {
        // Not sprinting
        sprinting = false;
        timeSinceStaminaUse += deltaTime;
        if (timeSinceStaminaUse >= staminaRegenDelay) isStaminaRegenerating = true;
        if (isStaminaRegenerating) stamina = std::min(maxStamina, stamina + staminaRegenRate * deltaTime);
        if (velVec.x != 0.f || velVec.y != 0.f) {
            walkingSound.setPitch(1.0f);
            if (walkingSound.getStatus() != sf::Sound::Playing) walkingSound.play();
        } else {
            if (walkingSound.getStatus() == sf::Sound::Playing) walkingSound.stop();
        }
    }

    // Footstep sounds: play stepping samples when the player is moving
    bool isMovingForSteps = (velVec.x != 0.f || velVec.y != 0.f) && !dead && !attacking;
    if (isMovingForSteps && !stepBuffers.empty()) {
        // step interval shortens when sprinting
        float interval = baseStepInterval;
        // apply sprint multiplier directly so timing responds immediately
        if (actuallySprinting) interval *= (speed / sprintSpeed);

        // If sprint state changed since last frame, reset timer so the new cadence applies immediately
        if (lastStepSprinting != actuallySprinting) {
            stepTimer = 0.0f; // force immediate step with new interval
            lastStepSprinting = actuallySprinting;
        }

        stepTimer -= deltaTime;
        if (stepTimer <= 0.0f) {
            // advance to next buffer and play using next instance in pool
            if (!stepSoundInstances.empty()) {
                lastStepInstance = (lastStepInstance + 1) % stepSoundInstances.size();
                // ensure buffer for this instance matches the stepIndex
                stepSoundInstances[lastStepInstance].setBuffer(stepBuffers[stepIndex % stepBuffers.size()]);
                stepSoundInstances[lastStepInstance].play();
            }
            stepIndex = (stepIndex + 1) % stepBuffers.size();
            // schedule next
            stepTimer += interval;
        }
    } else {
        // reset timer so first step plays immediately when movement resumes
        stepTimer = 0.0f;
    }

    // Update physics body; if knockback active, reduce player control by multiplier
    if (knockbackTimer > 0.0f) {
        body.velocity = Vec2(body.velocity.x * 0.6f + velVec.x * knockbackMoveMultiplier * 0.4f,
                             body.velocity.y * 0.6f + velVec.y * knockbackMoveMultiplier * 0.4f);
    } else {
        body.velocity = Vec2(velVec.x, velVec.y);
    }
    body.position += body.velocity * deltaTime;
    // do not directly sync sprite here; store current position for interpolation
    currPos = sf::Vector2f(body.position.x, body.position.y);

    // Decay knockback timer
    if (knockbackTimer > 0.0f) {
        knockbackTimer -= deltaTime;
        if (knockbackTimer < 0.0f) knockbackTimer = 0.0f;
    }

    // (rotation applied above)
    // Attack handling unchanged
    timeSinceLastMeleeAttack += deltaTime;
    // Decay recoil over time
    if (recoil > 0.0f) {
        recoil = std::max(0.0f, recoil - recoilDecayRate * deltaTime);
    }
    updateAnimation(deltaTime);

    // Handle reloading timer (in case reloadDuration independent of animation length)
    if (reloading) {
        reloadTimer += deltaTime;
        if (reloadTimer >= reloadDuration) {
            reloading = false;
            currentAmmo = magazineSize;
            // store into per-weapon
            if (currentWeapon == WeaponType::PISTOL) pistolAmmoInMag = currentAmmo;
            else if (currentWeapon == WeaponType::RIFLE) rifleAmmoInMag = currentAmmo;
            upperShooting = false;
            // Note: reload sound is played when reload starts (or when animation starts). Do not play again here.
        }
    }

    // Update active muzzle flashes (decay by real deltaTime so visibility is frame-rate independent)
    if (!activeMuzzles.empty()) {
        for (auto it = activeMuzzles.begin(); it != activeMuzzles.end(); ) {
            it->life -= deltaTime;
            if (it->life <= 0.0f) {
                it = activeMuzzles.erase(it);
            } else {
                // Compute elapsed fraction (0..1)
                float t = it->life / it->maxLife; // remaining fraction
                float elapsed = 1.0f - t; // 0..1
                // alpha follows sin(pi * elapsed): fades in then out
                float alphaFactor = std::sin(3.14159265f * elapsed);
                if (alphaFactor < 0.0f) alphaFactor = 0.0f;
                sf::Uint8 alpha = static_cast<sf::Uint8>(255.0f * alphaFactor);
                it->sprite.setColor(sf::Color(255,255,255, alpha));
                // Slight scale change tied to alpha for a pop effect
                float s = muzzleFlashScale * (0.6f + 0.4f * alphaFactor);
                it->sprite.setScale(s, s);
                ++it;
            }
        }
    }
}

void Player::shoot(const sf::Vector2f& target, PhysicsWorld& physicsWorld, std::vector<std::unique_ptr<Bullet>>& bullets) {
    if (dead) return;
    if (sprinting) return;
    if (currentWeapon != WeaponType::PISTOL && currentWeapon != WeaponType::RIFLE) return;
    // Cancel reload when shooting if we have ammo (user requested behavior)
    if (reloading) {
        if (currentAmmo > 0) {
            cancelReload();
        } else {
            return;
        }
    }

    // Weapon parameters (defaults)
    float bulletDamage = 20.f;
    float bulletSpeed = 1200.f;
    float bulletMass = 1.0f;
    int maxPenetrations = 1;
    float inaccuracyDeg = 2.0f;

    if (currentWeapon == WeaponType::PISTOL) {
        bulletDamage = 15.f; bulletSpeed = 2000.f; bulletMass = 0.2f; maxPenetrations = 2; inaccuracyDeg = 2.0f;
        inaccuracyDeg = aiming ? baseInaccuracyAimedPistol : baseInaccuracyHipPistol;
    } else if (currentWeapon == WeaponType::RIFLE) {
        bulletDamage = 20.f; bulletSpeed = 2200.f; bulletMass = 0.2f; maxPenetrations = 3; inaccuracyDeg = 3.0f;
        inaccuracyDeg = aiming ? baseInaccuracyAimedRifle : baseInaccuracyHipRifle;
    }

    // Add recoil to spread
    inaccuracyDeg += recoil;

    // Spawn position at muzzle
    sf::Vector2f spawnPos = getMuzzlePosition(target);

    // Sprite rotation/facing for visual muzzle flash fallback
    float spriteRotDeg = sprite.getRotation();
    float spriteRotRad = spriteRotDeg * 3.14159265f / 180.0f;
    sf::Vector2f spriteFacing(std::cos(spriteRotRad), std::sin(spriteRotRad));

    // Aim direction from muzzle to target (prefer this over spriteFacing)
    sf::Vector2f aimVec = target - spawnPos;
    float aimLen = std::sqrt(aimVec.x*aimVec.x + aimVec.y*aimVec.y);
    sf::Vector2f aimDir = (aimLen > 1e-6f) ? sf::Vector2f(aimVec.x/aimLen, aimVec.y/aimLen) : spriteFacing;

    // RNG
    static thread_local std::mt19937 gen((unsigned)std::random_device{}());

    // Visual params
    sf::Vector2f spriteOffset(0.f, 0.f);
    float spriteScale = 0.07f;
    float spriteRotationOffset = 90.0f;
    if (currentWeapon == WeaponType::PISTOL) { spriteOffset = sf::Vector2f(-6.f, 0.f); spriteScale = 0.03f; spriteRotationOffset = 0.0f; }
    else if (currentWeapon == WeaponType::RIFLE) { spriteOffset = sf::Vector2f(-6.f, 0.f); spriteScale = 0.03f; spriteRotationOffset = 0.0f; }

    // Ensure ammo
    if (currentAmmo <= 0) return;

    // Fire
    {
        std::uniform_real_distribution<float> dist(-inaccuracyDeg, inaccuracyDeg);
        float angDegRnd = dist(gen);
        float angRad = angDegRnd * 3.14159265f / 180.0f;
        float c = std::cos(angRad), s = std::sin(angRad);
        sf::Vector2f finalDir(aimDir.x * c - aimDir.y * s, aimDir.x * s + aimDir.y * c);
        sf::Vector2f vel = finalDir * bulletSpeed;
        auto bullet = std::make_unique<Bullet>(Vec2(spawnPos.x, spawnPos.y), Vec2(vel.x, vel.y), bulletMass, maxPenetrations, bulletDamage,
                                               spriteOffset, spriteScale, spriteRotationOffset);
        physicsWorld.addBody(&bullet->getBody(), false);
        bullets.push_back(std::move(bullet));
        currentAmmo = std::max(0, currentAmmo - 1);
    }

    // Recoil
    if (aiming) recoil = std::min(maxRecoilAimed, recoil + recoilPerShotAimed);
    else recoil = std::min(maxRecoilHip, recoil + recoilPerShotHip);

    sprinting = false;

    // Sounds
    if (currentWeapon == WeaponType::PISTOL) {
        if (!pistolSoundPool.empty()) { pistolSoundPool[pistolSoundIndex].play(); pistolSoundIndex = (pistolSoundIndex + 1) % pistolSoundPool.size(); }
    } else if (currentWeapon == WeaponType::RIFLE) {
        if (!rifleSoundPool.empty()) { rifleSoundPool[rifleSoundIndex].play(); rifleSoundIndex = (rifleSoundIndex + 1) % rifleSoundPool.size(); }
    }

    // Upper animation
    upperShooting = true;
    if (currentWeapon == WeaponType::PISTOL) {
        if (pistolUpperSheets[2] && !pistolUpperFramesByState.empty() && !pistolUpperFramesByState[2].empty()) {
            upperAnimator.setFrames(pistolUpperSheets[2], pistolUpperFramesByState[2], pistolUpperFrameTimes[2], false);
            upperAnimator.play(true);
            upperAnimator.setOnComplete([this]() { this->upperShooting = false; });
        }
    } else if (currentWeapon == WeaponType::RIFLE) {
        if (rifleUpperSheets[2] && !rifleUpperFramesByState.empty() && !rifleUpperFramesByState[2].empty()) {
            upperAnimator.setFrames(rifleUpperSheets[2], rifleUpperFramesByState[2], rifleUpperFrameTimes[2], false);
            upperAnimator.play(true);
            upperAnimator.setOnComplete([this]() { this->upperShooting = false; });
        }
    }

    timeSinceLastShot = 0.0f;

    // Debug muzzle marker
    if (debugDrawOrigins) g_recentMuzzles.push_back({ spawnPos, 0.5f });

    // Muzzle flash
    if (muzzleTexture) {
        Player::MuzzleFlash mf;
        mf.maxLife = muzzleFlashLife;
        mf.life = mf.maxLife;
        mf.sprite.setTexture(*muzzleTexture);
        float forwardOffset = 6.0f;
        sf::Vector2f face(std::cos(spriteRotRad), std::sin(spriteRotRad));
        sf::Vector2f pos = spawnPos + face * forwardOffset;
        mf.sprite.setOrigin(muzzleTexture->getSize().x * 0.5f, muzzleTexture->getSize().y * 0.5f);
        mf.sprite.setPosition(pos);
        float baseRot = spriteRotDeg + muzzleFlashRotationOffset;
        static thread_local std::mt19937 rg((unsigned)std::random_device{}());
        std::uniform_real_distribution<float> jitterDist(-muzzleFlashRotationJitterDeg, muzzleFlashRotationJitterDeg);
        float jitter = jitterDist(rg);
        mf.sprite.setRotation(baseRot + jitter);
        mf.sprite.setScale(muzzleFlashScale, muzzleFlashScale);
        activeMuzzles.push_back(std::move(mf));
    }
}

void Player::startReload() {
    if (reloading) return;
    if (currentWeapon != WeaponType::PISTOL && currentWeapon != WeaponType::RIFLE) return;
    if (currentAmmo >= magazineSize) return; // magazine full

    reloading = true;
    reloadTimer = 0.0f;

    // trigger reload animation frame set if available
    const std::vector<std::vector<sf::IntRect>>* upperFrames = nullptr;
    const std::array<const sf::Texture*,4>* sheets = nullptr;
    if (currentWeapon == WeaponType::PISTOL) { upperFrames = &pistolUpperFramesByState; sheets = &pistolUpperSheets; }
    else if (currentWeapon == WeaponType::RIFLE) { upperFrames = &rifleUpperFramesByState; sheets = &rifleUpperSheets; }

    if (upperFrames && !upperFrames->empty() && (*sheets)[3]) {
        const sf::Texture* reloadSheet = (*sheets)[3];
        const auto& reloadFrames = (*upperFrames)[3];
        float rt = 0.1f;
        if (currentWeapon == WeaponType::PISTOL) rt = pistolUpperFrameTimes[3];
        else if (currentWeapon == WeaponType::RIFLE) rt = rifleUpperFrameTimes[3];
        upperAnimator.setFrames(reloadSheet, reloadFrames, rt, false);
        upperAnimator.play(true);

        // Play reload sound immediately when animation starts
        if (currentWeapon == WeaponType::PISTOL) {
            if (pistolReloadBuffer.getSampleCount() > 0) pistolReloadSound.play();
        } else if (currentWeapon == WeaponType::RIFLE) {
            if (rifleReloadBuffer.getSampleCount() > 0) rifleReloadSound.play();
        }

        // When the reload animation completes, finish reload immediately
        upperAnimator.setOnComplete([this]() {
            // refill magazine from infinite reserve
            this->reloading = false;
            this->currentAmmo = this->magazineSize;
            this->upperShooting = false;
            this->attacking = false;
            if (this->body.velocity.x != 0.0f || this->body.velocity.y != 0.0f) this->setState(PlayerState::WALK);
            else this->setState(PlayerState::IDLE);

            // note: reload sound already played at animation start to match user request
        });
    } else {
        // No animation available: still play reload sound immediately
        if (currentWeapon == WeaponType::PISTOL) {
            if (pistolReloadBuffer.getSampleCount() > 0) pistolReloadSound.play();
        } else if (currentWeapon == WeaponType::RIFLE) {
            if (rifleReloadBuffer.getSampleCount() > 0) rifleReloadSound.play();
        }
    }
}

void Player::cancelReload() {
    if (!reloading) return;
    reloading = false;
    reloadTimer = 0.0f;
    // stop reload animation if playing by returning to idle/move state
    upperAnimator.stop();
    upperShooting = false;
    // Stop any reload sounds that may be playing
    if (pistolReloadSound.getStatus() == sf::Sound::Playing) pistolReloadSound.stop();
    if (rifleReloadSound.getStatus() == sf::Sound::Playing) rifleReloadSound.stop();
    // ensure correct upper state for movement
    if (body.velocity.x != 0.0f || body.velocity.y != 0.0f) setState(PlayerState::WALK);
    else setState(PlayerState::IDLE);
}

void Player::attack() {
    if (dead) return;
    attacking = true;
}

void Player::sprint(bool isSprinting) {
    // Input indicates desired sprint state; actual sprinting is determined in update() using stamina/aiming rules
    desiredSprint = isSprinting;
}

void Player::takeDamage(float amount) {
    if (dead) return;
    health -= amount;
    timeSinceLastDamage = 0.0f;
    if (health <= 0.0f) {
        health = 0.0f;
        kill();
    }
}

void Player::setPosition(float x, float y) {
    body.position.x = x;
    body.position.y = y;
    sprite.setPosition(x, y);
    feetSprite.setPosition(x, y + 8.0f);
    prevPos = currPos = sf::Vector2f(x, y);
}

sf::Vector2f Player::getPosition() const {
    return sprite.getPosition();
}

sf::Vector2f Player::getPhysicsPosition() const {
    return sf::Vector2f(body.position.x, body.position.y);
}

sf::Vector2f Player::getMuzzlePosition(const sf::Vector2f& aimTarget) const {
    // Use the sprite's current rotation (visual facing) to compute muzzle world position.
    sf::Vector2f base(body.position.x, body.position.y);

    // sprite rotation is in degrees; convert to radians
    float rotDeg = sprite.getRotation();
    float rotRad = rotDeg * 3.14159265f / 180.0f;
    sf::Vector2f face(std::cos(rotRad), std::sin(rotRad));
    // Right-perpendicular relative to facing. Use (-y, x) so positive lateralOffset moves to the sprite's right.
    sf::Vector2f rightPerp(-face.y, face.x);

    // Prefer explicit vector offsets if available
    sf::Vector2f localOffset(0.f, 0.f);
    if (currentWeapon == WeaponType::PISTOL) localOffset = muzzleOffsetPistol;
    else if (currentWeapon == WeaponType::RIFLE) localOffset = muzzleOffsetRifle;

    // If vector offsets are zero, fall back to scalar fields
    if (localOffset.x == 0.f && localOffset.y == 0.f) {
        float forwardOffset = 0.f;
        float lateralOffset = 0.f;
        if (currentWeapon == WeaponType::PISTOL) {
            forwardOffset = muzzleForwardPistol;
            lateralOffset = muzzleLateralPistol;
        } else if (currentWeapon == WeaponType::RIFLE) {
            forwardOffset = muzzleForwardRifle;
            lateralOffset = muzzleLateralRifle;
        }
        return base + face * forwardOffset + rightPerp * lateralOffset;
    }

    // Rotate local offset by sprite rotation
    float lx = localOffset.x;
    float ly = localOffset.y;
    sf::Vector2f worldOffset = face * lx + rightPerp * ly;
    return base + worldOffset;
}

bool Player::isAttacking() const {
    return attacking;
}

float Player::getAttackDamage() const {
    return 25.0f;
}

// Player hitbox centered on physics body
sf::FloatRect Player::getHitbox() const {
    const float desiredW = 50.0f;
    const float desiredH = 50.0f;
    float cx = body.position.x;
    float cy = body.position.y;
    return sf::FloatRect(cx - desiredW/2.0f, cy - desiredH/2.0f, desiredW, desiredH);
}

sf::FloatRect Player::getAttackHitbox() const {
    return attackBounds;
}

sf::FloatRect Player::getAttackBounds() const {
    return attackBounds;
}

void Player::applyKnockback(const sf::Vector2f& direction, float force) {
    // Apply an instantaneous impulse to the physics body so collisions and physics handle response
    body.velocity.x += direction.x * force;
    body.velocity.y += direction.y * force;
    // start a short knockback timer to reduce player control
    knockbackTimer = knockbackDuration;
}

void Player::draw(sf::RenderWindow& window) {
    // For compatibility: draw is same as render here
    render(window);
}

void Player::reset() {
    health = maxHealth;
    stamina = maxStamina;
    dead = false;
    attacking = false;
    currentFeetState = FeetState::IDLE;
    feetAnimator.stop();
    upperAnimator.stop();
    syncSpriteWithBody();
}

float Player::getCurrentHealth() const { return health; }
float Player::getMaxHealth() const { return maxHealth; }
float Player::getCurrentStamina() const { return stamina; }
float Player::getMaxStamina() const { return maxStamina; }

void Player::setWeapon(WeaponType weapon) {
    // Save current ammo into per-weapon storage before switching
    if (currentWeapon == WeaponType::PISTOL) pistolAmmoInMag = currentAmmo;
    else if (currentWeapon == WeaponType::RIFLE) rifleAmmoInMag = currentAmmo;

    currentWeapon = weapon;
    switch (weapon) {
        case WeaponType::PISTOL:
            fireCooldown = 0.13f;
            magazineSize = 12;
            // restore pistol ammo
            currentAmmo = std::clamp(pistolAmmoInMag, 0, magazineSize);
            reloadDuration = 1.3f;
            break;
        case WeaponType::RIFLE:
            fireCooldown = 0.11f;
            magazineSize = 30;
            // restore rifle ammo
            currentAmmo = std::clamp(rifleAmmoInMag, 0, magazineSize);
            reloadDuration = 2.25f;
            break;
        default:
            fireCooldown = 0.5f;
            magazineSize = 0;
            currentAmmo = 0;
            reloadDuration = 1.5f;
            break;
    }
}

void Player::render(sf::RenderWindow& window) {
    // interpolate position
    sf::Vector2f interp = prevPos + (currPos - prevPos) * renderAlpha;
    sprite.setPosition(interp.x, interp.y);
    feetSprite.setPosition(interp.x + feetOffsetX, interp.y + feetOffsetY);

    // draw feet first so torso renders over them
    window.draw(feetSprite);
    window.draw(sprite);

    // Draw muzzle flashes (use additive blending)
    if (!activeMuzzles.empty()) {
        sf::RenderStates rs;
        rs.blendMode = sf::BlendAdd;
        for (auto it = activeMuzzles.begin(); it != activeMuzzles.end(); ) {
            float t = it->life / it->maxLife;
            sf::Color c = sf::Color(255, 255, 255, static_cast<sf::Uint8>(255 * t));
            it->sprite.setColor(c);
            window.draw(it->sprite, rs);
            // decay (we now decrement in update(), keep this as a safer fallback but much smaller)
            it->life -= 1.0f/60.0f;
            if (it->life <= 0.0f) it = activeMuzzles.erase(it);
            else ++it;
        }
    }

    // draw shadow under the player if available
    if (shadowTexture) {
        sf::Sprite sh;
        sh.setTexture(*shadowTexture);
        // position slightly below feet
        float shadowScale = 1.0f;
        sh.setOrigin(shadowTexture->getSize().x * 0.5f, shadowTexture->getSize().y * 0.5f);
        sh.setPosition(interp.x, interp.y + 12.0f);
        sh.setScale(shadowScale, shadowScale);
        // DARKER SHADOW: increased alpha for a stronger, darker shadow
        sh.setColor(sf::Color(0,0,0,220));
        window.draw(sh);
    }

    if (debugDrawOrigins) {
        sf::CircleShape upperMarker(4.0f);
        upperMarker.setFillColor(sf::Color::Red);
        sf::Vector2f upperPos = sprite.getPosition();
        upperMarker.setPosition(upperPos.x - upperMarker.getRadius(), upperPos.y - upperMarker.getRadius());
        window.draw(upperMarker);

        sf::CircleShape feetMarker(4.0f);
        feetMarker.setFillColor(sf::Color::Green);
        sf::Vector2f feetPos = feetSprite.getPosition();
        feetMarker.setPosition(feetPos.x - feetMarker.getRadius(), feetPos.y - feetMarker.getRadius());
        window.draw(feetMarker);

        // Draw recent muzzle spawn markers (frame-rate independent decay handled in update)
        for (const auto &m : g_recentMuzzles) {
            sf::CircleShape s(3.0f);
            s.setFillColor(sf::Color(255, 0, 255, 200));
            s.setPosition(m.first.x - s.getRadius(), m.first.y - s.getRadius());
            window.draw(s);
        }

        // Draw debug line projecting out from the muzzle position in the sprite facing direction
        {
            sf::Vector2f muzzlePos = getMuzzlePosition(sprite.getPosition());
            float rotDeg = sprite.getRotation();
            float rotRad = rotDeg * 3.14159265f / 180.0f;
            sf::Vector2f dir(std::cos(rotRad), std::sin(rotRad));
            float lineLen = 200.0f;
            sf::Vertex line[2] = {
                sf::Vertex(muzzlePos, sf::Color(255, 0, 255, 200)),
                sf::Vertex(sf::Vector2f(muzzlePos.x + dir.x * lineLen, muzzlePos.y + dir.y * lineLen), sf::Color(255, 0, 255, 120))
            };
            window.draw(line, 2, sf::Lines);
        }

        // Debug: draw accuracy/spread cone for current weapon (base inaccuracy + recoil)
        {
            // compute inaccuracy degrees same as used in shoot()
            float inaccuracyDeg = recoil; // recoil already applied elsewhere
            if (currentWeapon == WeaponType::PISTOL) {
                inaccuracyDeg += (aiming ? baseInaccuracyAimedPistol : baseInaccuracyHipPistol);
            } else if (currentWeapon == WeaponType::RIFLE) {
                inaccuracyDeg += (aiming ? baseInaccuracyAimedRifle : baseInaccuracyHipRifle);
            }

            // skip if no meaningful spread
            if (inaccuracyDeg > 0.0001f) {
                sf::Vector2f muzzlePos = getMuzzlePosition(sprite.getPosition());
                float baseDeg = sprite.getRotation();
                float baseRad = baseDeg * 3.14159265f / 180.0f;
                float spreadRad = inaccuracyDeg * 3.14159265f / 180.0f;

                // visual distance for cone
                float coneLen = 400.0f;
                const int segments = 32;
                sf::Color coneColor(255, 200, 0, 64); // translucent yellow
                sf::VertexArray fan(sf::TriangleFan);
                fan.append(sf::Vertex(muzzlePos, coneColor));
                for (int i = 0; i <= segments; ++i) {
                    float t = (float)i / (float)segments;
                    float ang = baseRad - spreadRad + t * (2.0f * spreadRad);
                    sf::Vector2f p(muzzlePos.x + std::cos(ang) * coneLen, muzzlePos.y + std::sin(ang) * coneLen);
                    fan.append(sf::Vertex(p, coneColor));
                }
                window.draw(fan);

                // boundary lines
                sf::Color lineC(255, 200, 0, 200);
                sf::Vector2f leftP(muzzlePos.x + std::cos(baseRad - spreadRad) * coneLen, muzzlePos.y + std::sin(baseRad - spreadRad) * coneLen);
                sf::Vector2f rightP(muzzlePos.x + std::cos(baseRad + spreadRad) * coneLen, muzzlePos.y + std::sin(baseRad + spreadRad) * coneLen);
                sf::Vertex bl[2] = { sf::Vertex(muzzlePos, lineC), sf::Vertex(leftP, lineC) };
                sf::Vertex br[2] = { sf::Vertex(muzzlePos, lineC), sf::Vertex(rightP, lineC) };
                window.draw(bl, 2, sf::Lines);
                window.draw(br, 2, sf::Lines);
            }
        }
    }
}

void Player::kill() {
    if (dead) return;
    dead = true;
    attacking = false;
    // stop animators
    feetAnimator.stop();
    upperAnimator.stop();
    setState(PlayerState::DEATH);
}

void Player::syncSpriteWithBody() {
    sprite.setPosition(body.position.x, body.position.y);
    feetSprite.setPosition(body.position.x + feetOffsetX, body.position.y + feetOffsetY);
    prevPos = currPos = sf::Vector2f(body.position.x, body.position.y);
}

void Player::loadTextures() {
    // Intentionally left empty: textures are loaded in Game::Game and passed via setters.
}

void Player::setPistolSoundBuffer(const sf::SoundBuffer& buf) {
    pistolAttackBuffer = buf;
    pistolAttackSound.setBuffer(pistolAttackBuffer);
    pistolAttackSound.setVolume(100);
    pistolSoundIndex = 0;
    pistolSoundPool.clear();
    pistolSoundPool.resize(6);
    for (auto& s : pistolSoundPool) { s.setBuffer(pistolAttackBuffer); s.setVolume(50); }
}

void Player::setRifleSoundBuffer(const sf::SoundBuffer& buf) {
    rifleAttackBuffer = buf;
    rifleAttackSound.setBuffer(rifleAttackBuffer);
    rifleAttackSound.setVolume(100);
    rifleSoundIndex = 0;
    rifleSoundPool.clear();
    rifleSoundPool.resize(6);
    for (auto& s : rifleSoundPool) { s.setBuffer(rifleAttackBuffer); s.setVolume(100); s.setAttenuation(0.f); s.setRelativeToListener(true); s.setMinDistance(1.f); }
}

// Walking sound
void Player::setWalkingSoundBuffer(const sf::SoundBuffer& buf) {
    walkingBuffer = buf;
    walkingSound.setBuffer(walkingBuffer);
    walkingSound.setLoop(true);
    walkingSound.setVolume(90);
}

void Player::setStepSoundBuffers(const std::vector<sf::SoundBuffer>& bufs) {
    stepBuffers = bufs;
    stepSoundInstances.clear();
    // create a few instances to allow overlapping playback
    size_t poolSize = std::max<size_t>(4, stepBuffers.size());
    stepSoundInstances.resize(poolSize);
    for (size_t i = 0; i < poolSize; ++i) {
        if (!stepBuffers.empty()) stepSoundInstances[i].setBuffer(stepBuffers[i % stepBuffers.size()]);
        stepSoundInstances[i].setVolume(80);
    }
    stepTimer = 0.0f;
    stepIndex = 0;
    lastStepInstance = 0;
}

// --- Missing method implementations (linker fixes) ---
void Player::setPistolReloadSoundBuffer(const sf::SoundBuffer& buf) {
    pistolReloadBuffer = buf;
    pistolReloadSound.setBuffer(pistolReloadBuffer);
    pistolReloadSound.setVolume(100);
    pistolReloadSound.setAttenuation(0.f);
    pistolReloadSound.setRelativeToListener(true);
}

void Player::setRifleReloadSoundBuffer(const sf::SoundBuffer& buf) {
    rifleReloadBuffer = buf;
    rifleReloadSound.setBuffer(rifleReloadBuffer);
    rifleReloadSound.setVolume(100);
    rifleReloadSound.setAttenuation(0.f);
    rifleReloadSound.setRelativeToListener(true);
}

void Player::setState(PlayerState newState) {
    if (currentState == newState) return;
    currentState = newState;

    switch (newState) {
        case PlayerState::IDLE:
            if (currentFeetState != FeetState::IDLE) setFeetStateInternal(FeetState::IDLE);
            attacking = false;
            break;
        case PlayerState::WALK:
            if (currentFeetState != FeetState::WALK) setFeetStateInternal(FeetState::WALK);
            attacking = false;
            break;
        case PlayerState::ATTACK:
            attacking = true;
            break;
        case PlayerState::DEATH:
            dead = true;
            attacking = false;
            feetAnimator.stop();
            upperAnimator.stop();
            break;
        default:
            break;
    }
}