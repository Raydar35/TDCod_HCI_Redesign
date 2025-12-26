#include "BaseZombie.h"
#include <iostream>
#include <cmath>

BaseZombie::BaseZombie(float x, float y, float health, float attackDamage, float speed, float attackRange, float attackCooldown)
    : Entity(EntityType::Enemy, Vec2(x, y), Vec2(50.f, 50.f), false, 1.0f, true),
    currentState(ZombieState::WALK),
    speed(speed),
    // animator will handle frames
    currentFrame(0),
    attacking(false),
    currentAttackFrame(0),
    attackFrameTime(0.1f),
    attackCooldown(attackCooldown),
    timeSinceLastAttack(0.0f),
    currentDeathFrame(0),
    deathFrameTime(0.15f),
    attackDamage(attackDamage),
    attackRange(attackRange),
    dead(false),
    health(health),
    maxHealth(health),
    m_hasDealtDamageInAttack(false) {
    // attach animator to sprite
    animator.setSprite(&sprite);
}

void BaseZombie::update(float deltaTime, sf::Vector2f playerPosition) {
    // store previous pos for interpolation
    prevPos = currPos;

    if (dead) {
        updateAnimation(deltaTime);
        return;
    }

    Vec2 playerPos(playerPosition.x, playerPosition.y);
    Vec2 direction = playerPos - body.position;
    float distance = direction.length();

    // record latest player pos for derived classes
    lastSeenPlayerPos = playerPos;

    timeSinceLastAttack += deltaTime;

    if (distance <= attackRange && timeSinceLastAttack >= attackCooldown) {
        attack();
        timeSinceLastAttack = 0.0f;
        // Do not immediately zero velocity here; derived classes may want to continue
        // moving until a specific attack frame (pre-lunge).
    }
    else if (!attacking) {
        if (distance > 5.0f) {
            setState(ZombieState::WALK);

            if (distance > 0) direction.normalize();
            if (!isMovementLocked()) body.velocity = direction * speed;
        }
        else {
            body.velocity = Vec2(0, 0);
        }
    }
    else {
        body.velocity = Vec2(0, 0);
    }

    // Allow animator and derived classes to update state (e.g., start a lunge) before applying rotation
    animator.update(deltaTime);
    updateAnimation(deltaTime); // keep any logic (like attack timing)

    // Store current physics position (do not directly set sprite position here)
    currPos = sf::Vector2f(body.position.x, body.position.y);
    // Only rotate if allowed and movement isn't locked by derived behavior (e.g., lunging)
    if (distance > 1.0f && canRotate() && !isMovementLocked()) {
        float angle = std::atan2(direction.y, direction.x) * 180 / 3.14159265f;
        sprite.setRotation(angle + rotationOffset);
    }
}

void BaseZombie::draw(sf::RenderWindow& window) const {
    // interpolate position
    sf::Vector2f interp = prevPos + (currPos - prevPos) * renderAlpha;
    sf::Sprite temp = sprite;

    // draw shadow if available
    if (shadowTexture) {
        sf::Sprite sh;
        sh.setTexture(*shadowTexture);
        sh.setOrigin(shadowTexture->getSize().x * 0.5f, shadowTexture->getSize().y * 0.5f);
        sh.setPosition(interp.x, interp.y + 10.0f);
        sh.setScale(1.0f, 1.0f);
        sh.setColor(sf::Color(0,0,0,140));
        window.draw(sh);
    }
    temp.setPosition(interp);
    window.draw(temp);

    if (!dead) {
        float barWidth = 50.0f;
        float barHeight = 5.0f;
        sf::RectangleShape healthBarBackground;
        healthBarBackground.setSize(sf::Vector2f(barWidth, barHeight));
        healthBarBackground.setFillColor(sf::Color(100, 100, 100, 150));
        healthBarBackground.setPosition(body.position.x - barWidth / 2, body.position.y - 50);

        sf::RectangleShape healthBar;
        float healthPercent = health / maxHealth;
        if (healthPercent < 0) healthPercent = 0;
        healthBar.setSize(sf::Vector2f(barWidth * healthPercent, barHeight));
        healthBar.setFillColor(sf::Color::Red);
        healthBar.setPosition(body.position.x - barWidth / 2, body.position.y - 50);

        window.draw(healthBarBackground);
        window.draw(healthBar);
    }
}

sf::FloatRect BaseZombie::getBounds() const {
    return sprite.getGlobalBounds();
}

void BaseZombie::attack() {
    if (attacking || dead) return;
    std::cout << "Zombie is attacking!" << std::endl; // TEMP LOG

    attacking = true;
    currentAttackFrame = 0;
    m_hasDealtDamageInAttack = false;
    setState(ZombieState::ATTACK);

    // switch animator to attack frames if available
    if (hasAttackSheet && !attackRects.empty()) {
        animator.setFrames(&attackSheetTexture, attackRects, attackFrameTime, false);
        animator.play(true);
    } else if (!attackTextures.empty()) {
        animator.setFrames(&attackTextures, attackFrameTime, false);
        animator.play(true);
    }

    // Attack state initialized (derived classes may implement special attack movement)
}

void BaseZombie::takeDamage(float amount) {
    if (!dead) {
        health -= amount;
        if (health <= 0) {
            health = 0;
            kill();
        }
    }
}

void BaseZombie::kill() {
    if (!dead) {
        // Mark dead and stop any animation. We do not play a death animation — the zombie should disappear.
        dead = true;
        attacking = false;
        // Stop animator and hide the sprite immediately so it appears to despawn
        animator.stop();
        sprite.setColor(sf::Color(255,255,255,0));
        // ensure physics body no longer moves
        body.velocity = Vec2(0,0);
    }
}

bool BaseZombie::isAttacking() const {
    return attacking;
}

bool BaseZombie::isDead() const {
    return dead;
}

bool BaseZombie::isAlive() const {
    return !dead;
}

float BaseZombie::getAttackDamage() const {
    return attackDamage;
}

sf::Vector2f BaseZombie::getPosition() const {
    return sf::Vector2f(body.position.x, body.position.y);
}

sf::FloatRect BaseZombie::getHitbox() const {
    // Use a fixed-size smaller hitbox centered on the physics body so it remains stable
    // regardless of sprite rotation or texture size.
    const float hitW = 40.0f;
    const float hitH = 40.0f;
    return sf::FloatRect(body.position.x - hitW/2.0f, body.position.y - hitH/2.0f, hitW, hitH);
}

sf::FloatRect BaseZombie::getAttackHitbox() const {
    // Build axis-aligned bounding box from oriented attack box for compatibility
    sf::Vector2f center; sf::Vector2f halfExt; float rot;
    getAttackOBB(center, halfExt, rot);

    // Compute the four OBB corners and build AABB
    float rad = rot * 3.14159265f / 180.0f;
    float cs = std::cos(rad), sn = std::sin(rad);
    // local axes
    sf::Vector2f ux(cs, sn);
    sf::Vector2f uy(-sn, cs);

    sf::Vector2f corners[4];
    corners[0] = center + ux * halfExt.x + uy * halfExt.y;
    corners[1] = center - ux * halfExt.x + uy * halfExt.y;
    corners[2] = center - ux * halfExt.x - uy * halfExt.y;
    corners[3] = center + ux * halfExt.x - uy * halfExt.y;

    float minx = corners[0].x, maxx = corners[0].x, miny = corners[0].y, maxy = corners[0].y;
    for (int i = 1; i < 4; ++i) {
        minx = std::min(minx, corners[i].x);
        maxx = std::max(maxx, corners[i].x);
        miny = std::min(miny, corners[i].y);
        maxy = std::max(maxy, corners[i].y);
    }
    return sf::FloatRect(minx, miny, maxx - minx, maxy - miny);
}

void BaseZombie::getAttackOBB(sf::Vector2f& outCenter, sf::Vector2f& outHalfExtents, float& outRotationDeg) const {
    // Oriented attack box: width along forward direction, height perpendicular
    const float attackW = 40.0f; // narrower attack box for smaller zombies
    const float attackH = 25.0f;
    const float forwardOffset = 28.0f;

    // Determine forward using velocity then lastSeenPlayerPos
    float fx = 0.0f, fy = 0.0f;
    const float velEps = 1e-4f;
    if (std::abs(body.velocity.x) > velEps || std::abs(body.velocity.y) > velEps) {
        float len = std::sqrt(body.velocity.x*body.velocity.x + body.velocity.y*body.velocity.y);
        fx = body.velocity.x / len; fy = body.velocity.y / len;
    } else {
        Vec2 toTarget = lastSeenPlayerPos - body.position;
        if (toTarget.length() > velEps) { toTarget.normalize(); fx = toTarget.x; fy = toTarget.y; }
        else { fx = 1.0f; fy = 0.0f; }
    }

    outCenter = sf::Vector2f(body.position.x + fx * forwardOffset, body.position.y + fy * forwardOffset);
    outHalfExtents = sf::Vector2f(attackW * 0.5f, attackH * 0.5f);
    outRotationDeg = std::atan2(fy, fx) * 180.0f / 3.14159265f;
}

void BaseZombie::setState(ZombieState newState) {
    if (dead) return;

    bool changed = (currentState != newState);
    // If the state hasn't changed and the animator is already playing for this state,
    // don't reconfigure frames (that would restart the animation each frame).
    if (!changed && animator.isPlaying()) return;
    currentState = newState;

    // When the state changes, reset indices/flags. Even if the state is the same,
    // ensure the animator is configured and playing so initial setup from
    // derived constructors starts the animation.
    if (changed) {
        if (currentState == ZombieState::ATTACK) {
            currentAttackFrame = 0;
            attackTimer = 0.0f;
            m_hasDealtDamageInAttack = false;
        } else if (currentState == ZombieState::WALK) {
            currentFrame = 0;
        }
    }

    // Switch animator frames depending on state (prefer sheets)
    if (currentState == ZombieState::ATTACK) {
        if (hasAttackSheet && !attackRects.empty()) {
            animator.setFrames(&attackSheetTexture, attackRects, attackFrameTime, false);
            animator.play(true);
        } else if (!attackTextures.empty()) {
            animator.setFrames(&attackTextures, attackFrameTime, false);
            animator.play(true);
        }
    } else if (currentState == ZombieState::WALK) {
        if (hasWalkSheet && !walkRects.empty()) {
            animator.setFrames(&walkSheetTexture, walkRects, walkFrameTime, true);
            animator.play(true);
        } else if (!walkTextures.empty()) {
            animator.setFrames(&walkTextures, walkFrameTime, true);
            animator.play(true);
        }
    } else if (currentState == ZombieState::DEATH) {
        // No death animation; stop animator
        animator.stop();
    }
}

void BaseZombie::updateAnimation(float deltaTime) {
    if (dead) return;

    // Animator-driven playback: when animator stops and the state was attack, revert to walk
    if (currentState == ZombieState::ATTACK) {
        if (!animator.isPlaying()) {
            // attack animation ended
            attacking = false;
            m_hasDealtDamageInAttack = false;
            setState(ZombieState::WALK);
        }
    }
}

bool BaseZombie::hasDealtDamageInAttack() const {
    return m_hasDealtDamageInAttack;
}

float BaseZombie::tryDealDamage() {
    if (!m_hasDealtDamageInAttack) {
        m_hasDealtDamageInAttack = true;
        return attackDamage;
    }
    return 0.0f;
}

void BaseZombie::loadTextureSet(std::vector<sf::Texture>& textures, const std::string& basePath,
    const std::string& prefix, int count, bool isBoss) {
    textures.clear();

    for (int i = 0; i < count; ++i) {
        sf::Texture texture;
        std::string filePath = basePath + "/" + prefix + "_";

        if (i < 10) {
            filePath += "0" + std::to_string(i) + ".png";
        }
        else {
            filePath += std::to_string(i) + ".png";
        }

        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading zombie texture: " << filePath << std::endl;
        }
        textures.push_back(texture);
    }
}

void BaseZombie::resetForSpawn(float x, float y, float healthVal, float damageVal, float speedVal) {
    // Position the physics body
    body.position.x = x;
    body.position.y = y;
    body.velocity = Vec2(0,0);

    // Reset stats
    health = healthVal;
    maxHealth = healthVal;
    attackDamage = damageVal;
    speed = speedVal;

    // Reset state
    dead = false;
    attacking = false;
    // Restore sprite visibility (kill() may have made it transparent)
    sprite.setColor(sf::Color(255,255,255,255));
    // Ensure animator is configured for walking and playing
    setState(ZombieState::WALK);
    prevPos = currPos = sf::Vector2f(x, y);
}
