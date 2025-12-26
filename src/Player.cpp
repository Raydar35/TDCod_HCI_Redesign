#include "Player.h"
#include "Bullet.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>

Player::Player(Vec2 position)
    : Entity(EntityType::Player, position, Vec2(60.f, 60.f), false, 1.f, true),
    currentState(PlayerState::IDLE),
    speed(100.0f),
    sprintSpeed(180.0f),
    health(100.0f),
    maxHealth(100.0f),
    stamina(100.0f),
    maxStamina(100.0f),
    staminaRegenRate(20.0f),
    staminaDrainRate(30.0f),
    staminaRegenDelay(1.5f),
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
    currentWeapon(WeaponType::KNIFE)
{
    loadTextures();
    if (!knifeIdleTextures.empty()) {
        sprite.setTexture(knifeIdleTextures[0]);
        sf::Vector2u textureSize = knifeIdleTextures[0].getSize();
        sprite.setOrigin(textureSize.x / 2.0f, textureSize.y * 0.25f);
        sprite.setScale(scaleFactor, scaleFactor);
        sprite.setPosition(100, 100);
    }

    // Load knife attack sound
    if (!knifeAttackBuffer.loadFromFile("TDCod/Assets/Audio/knife.mp3")) {
        std::cerr << "Error loading knife attack sound!" << std::endl;
    } else {
        knifeAttackSound.setBuffer(knifeAttackBuffer);
        knifeAttackSound.setVolume(50);
    }

    // Load bat attack sound
    if (!batAttackBuffer.loadFromFile("TDCod/Assets/Audio/bat.mp3")) {
        std::cerr << "Error loading bat attack sound!" << std::endl;
    } else {
        batAttackSound.setBuffer(batAttackBuffer);
        batAttackSound.setVolume(50);
    }

    // Load pistol attack sound
    if (!pistolAttackBuffer.loadFromFile("TDCod/Assets/Audio/pistolshot.mp3")) {
        std::cerr << "Error loading pistol attack sound!" << std::endl;
    } else {
        pistolAttackSound.setBuffer(pistolAttackBuffer);
        pistolAttackSound.setVolume(50);
    }

    // Load rifle attack sound
    if (!rifleAttackBuffer.loadFromFile("TDCod/Assets/Audio/rifleshot.mp3")) {
        std::cerr << "Error loading rifle attack sound!" << std::endl;
    } else {
        rifleAttackSound.setBuffer(rifleAttackBuffer);
        rifleAttackSound.setVolume(50);
    }

    // Load flamethrower attack sound
    if (!flamethrowerAttackBuffer.loadFromFile("TDCod/Assets/Audio/flamethrower.mp3")) {
        std::cerr << "Error loading flamethrower attack sound!" << std::endl;
    } else {
        flamethrowerAttackSound.setBuffer(flamethrowerAttackBuffer);
        flamethrowerAttackSound.setVolume(50);
    }

    // Load walking sound
    if (!walkingBuffer.loadFromFile("TDCod/Assets/Audio/walking.mp3")) {
        std::cerr << "Error loading walking sound!" << std::endl;
    } else {
        walkingSound.setBuffer(walkingBuffer);
        walkingSound.setLoop(true);
        walkingSound.setVolume(90);
    }
}

void Player::loadTextures() {
    // Knife textures
    for (int i = 0; i < 8; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroKnife/HeroIdle/Idle_Knife_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading knife idle texture: " << filePath << std::endl;
        }
        knifeIdleTextures.push_back(texture);
    }
    for (int i = 0; i < 6; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroKnife/HeroWalk/Walk_knife_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading knife walk texture: " << filePath << std::endl;
        }
        knifeWalkingTextures.push_back(texture);
    }
    for (int i = 0; i < 8; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroKnife/HeroAttack/Knife_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading knife attack texture: " << filePath << std::endl;
        }
        knifeAttackTextures.push_back(texture);
    }

    // Bat textures
    for (int i = 0; i < 8; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroBat/HeroIdle/Idle_Bat_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading bat idle texture: " << filePath << std::endl;
        }
        batIdleTextures.push_back(texture);
    }
    for (int i = 0; i < 6; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroBat/HeroWalk/Walk_bat_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading bat walk texture: " << filePath << std::endl;
        }
        batWalkingTextures.push_back(texture);
    }
    for (int i = 0; i < 8; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroBat/HeroAttack/Bat_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading bat attack texture: " << filePath << std::endl;
        }
        batAttackTextures.push_back(texture);
    }

    // Pistol textures
    for (int i = 0; i < 8; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroPistol/HeroIdle/Idle_gun_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading pistol idle texture: " << filePath << std::endl;
        }
        pistolIdleTextures.push_back(texture);
    }
    for (int i = 0; i < 6; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroPistol/HeroWalk/Walk_gun_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading pistol walk texture: " << filePath << std::endl;
        }
        pistolWalkingTextures.push_back(texture);
    }
    for (int i = 0; i < 5; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroPistol/HeroAttack/Gun_Shot_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading pistol attack texture: " << filePath << std::endl;
        }
        pistolAttackTextures.push_back(texture);
    }

    // Rifle textures
    for (int i = 0; i < 8; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroRifle/HeroIdle/Idle_riffle_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading rifle idle texture: " << filePath << std::endl;
        }
        rifleIdleTextures.push_back(texture);
    }
    for (int i = 0; i < 6; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroRifle/HeroWalk/Walk_riffle_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading rifle walk texture: " << filePath << std::endl;
        }
        rifleWalkingTextures.push_back(texture);
    }
    for (int i = 0; i < 8; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroRifle/HeroAttack/Riffle_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading rifle attack texture: " << filePath << std::endl;
        }
        rifleAttackTextures.push_back(texture);
    }

    // Flamethrower textures
    for (int i = 0; i < 8; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroFlame/HeroIdle/Idle_firethrower_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading flamethrower idle texture: " << filePath << std::endl;
        }
        flamethrowerIdleTextures.push_back(texture);
    }
    for (int i = 0; i < 6; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroFlame/HeroWalk/Walk_firethrower_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading flamethrower walk texture: " << filePath << std::endl;
        }
        flamethrowerWalkingTextures.push_back(texture);
    }
    for (int i = 0; i < 8; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroFlame/HeroAttack/FlameThrower_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading flamethrower attack texture: " << filePath << std::endl;
        }
        flamethrowerAttackTextures.push_back(texture);
    }

    // Death textures
    for (int i = 0; i < 6; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Hero/HeroDeath/death_000" + std::to_string(i) + "_Man.png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading player death texture: " << filePath << std::endl;
        }
        deathTextures.push_back(texture);
    }
}

void Player::setWeapon(WeaponType weapon) {
    currentWeapon = weapon;
    currentFrame = 0;
    animationTimer = 0.0f;
    if (currentState == PlayerState::IDLE) {
        switch (currentWeapon) {
            case WeaponType::KNIFE:
                if (!knifeIdleTextures.empty()) sprite.setTexture(knifeIdleTextures[0]);
                break;
            case WeaponType::BAT:
                if (!batIdleTextures.empty()) sprite.setTexture(batIdleTextures[0]);
                break;
            case WeaponType::PISTOL:
                if (!pistolIdleTextures.empty()) sprite.setTexture(pistolIdleTextures[0]);
                break;
            case WeaponType::RIFLE:
                if (!rifleIdleTextures.empty()) sprite.setTexture(rifleIdleTextures[0]);
                break;
            case WeaponType::FLAMETHROWER:
                if (!flamethrowerIdleTextures.empty()) sprite.setTexture(flamethrowerIdleTextures[0]);
                break;
        }
    }
}

float Player::getAttackDamage() const {
    switch (currentWeapon) {
        case WeaponType::KNIFE: return 10.0f;
        case WeaponType::BAT: return 15.0f;
        case WeaponType::PISTOL: return 20.0f;
        case WeaponType::RIFLE: return 30.0f;
        case WeaponType::FLAMETHROWER: return 50.0f;
        default: return 10.0f;
    }
}

void Player::sprint(bool isSprinting) {
    if (isSprinting && stamina > 0) {
        isStaminaRegenerating = false;
        timeSinceStaminaUse = 0.0f;
    }
}

void Player::takeDamage(float amount) {
    if (!dead) {
        health -= amount;
        timeSinceLastDamage = 0.0f;
        if (health <= 0) {
            health = 0;
            kill();
        }
    }
}

void Player::kill() {
    if (!dead) {
        dead = true;
        attacking = false;
        currentDeathFrame = 0;
        deathTimer = 0.0f;
        setState(PlayerState::DEATH);
        if (!deathTextures.empty()) {
            sprite.setTexture(deathTextures[currentDeathFrame]);
        }
    }
}

bool Player::isAttacking() const {
    return attacking;
}

bool Player::isDead() const {
    return dead;
}

bool Player::isSprinting() const {
    bool wantsToSprint = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);
    return wantsToSprint && stamina > 0;
}

float Player::getCurrentStamina() const {
    return stamina;
}

float Player::getMaxStamina() const {
    return maxStamina;
}

sf::FloatRect Player::getAttackBounds() const {
    return attackBounds;
}

void Player::setState(PlayerState newState) {
    if (currentState != newState && !dead) {
        currentState = newState;
        if (currentState != PlayerState::ATTACK) {
            currentFrame = 0;
            animationTimer = 0.0f;
        }
    }
}

void Player::updateAnimation(float deltaTime) {
    animationTimer += deltaTime;
    float frameTime = animationFrameTime;

    if (dead) {
        deathTimer += deltaTime;
        if (deathTimer >= deathFrameTime) {
            deathTimer -= deathFrameTime;
            if (currentDeathFrame < deathTextures.size() - 1) {
                currentDeathFrame++;
                if (!deathTextures.empty()) {
                    sprite.setTexture(deathTextures[currentDeathFrame]);
                }
            }
        }
        return;
    }

    if (attacking) {
        attackTimer += deltaTime;
        if (attackTimer >= attackFrameTime) {
            attackTimer -= attackFrameTime;
            currentAttackFrame++;
            std::vector<sf::Texture>* attackTextures = nullptr;
            switch (currentWeapon) {
                case WeaponType::KNIFE: attackTextures = &knifeAttackTextures; break;
                case WeaponType::BAT: attackTextures = &batAttackTextures; break;
                case WeaponType::PISTOL: attackTextures = &pistolAttackTextures; break;
                case WeaponType::RIFLE: attackTextures = &rifleAttackTextures; break;
                case WeaponType::FLAMETHROWER: attackTextures = &flamethrowerAttackTextures; break;
            }
            if (attackTextures && currentAttackFrame < attackTextures->size()) {
                sprite.setTexture((*attackTextures)[currentAttackFrame]);
            } else {
                attacking = false;
                if (body.velocity.x != 0.0f || body.velocity.y != 0.0f) {
                    setState(PlayerState::WALK);
                } else {
                    setState(PlayerState::IDLE);
                }
            }
        }
    } else if (animationTimer >= frameTime) {
        animationTimer -= frameTime;
        std::vector<sf::Texture>* currentTextures = nullptr;
        size_t textureSize = 0;
        switch (currentState) {
            case PlayerState::IDLE:
                switch (currentWeapon) {
                    case WeaponType::KNIFE: currentTextures = &knifeIdleTextures; break;
                    case WeaponType::BAT: currentTextures = &batIdleTextures; break;
                    case WeaponType::PISTOL: currentTextures = &pistolIdleTextures; break;
                    case WeaponType::RIFLE: currentTextures = &rifleIdleTextures; break;
                    case WeaponType::FLAMETHROWER: currentTextures = &flamethrowerIdleTextures; break;
                }
                textureSize = currentTextures ? currentTextures->size() : 0;
                if (currentTextures && !currentTextures->empty()) {
                    currentFrame = (currentFrame + 1) % textureSize;
                    sprite.setTexture((*currentTextures)[currentFrame]);
                }
                break;
            case PlayerState::WALK:
                switch (currentWeapon) {
                    case WeaponType::KNIFE: currentTextures = &knifeWalkingTextures; break;
                    case WeaponType::BAT: currentTextures = &batWalkingTextures; break;
                    case WeaponType::PISTOL: currentTextures = &pistolWalkingTextures; break;
                    case WeaponType::RIFLE: currentTextures = &rifleWalkingTextures; break;
                    case WeaponType::FLAMETHROWER: currentTextures = &flamethrowerWalkingTextures; break;
                }
                textureSize = currentTextures ? currentTextures->size() : 0;
                if (currentTextures && !currentTextures->empty()) {
                    currentFrame = (currentFrame + 1) % textureSize;
                    sprite.setTexture((*currentTextures)[currentFrame]);
                }
                break;
            default:
                break;
        }
    }
}

void Player::updateStamina(float deltaTime) {
    // Stamina logic is handled in update()
}

void Player::update(float deltaTime, sf::RenderWindow& window, sf::Vector2u mapSize, sf::Vector2f worldMousePosition) {
    timeSinceLastMeleeAttack += deltaTime;
    timeSinceLastShot += deltaTime;
    if (dead) {
        updateAnimation(deltaTime);
        if (walkingSound.getStatus() == sf::Sound::Playing) {
            walkingSound.stop();
        }
        return;
    }
    
    timeSinceLastDamage += deltaTime;
    if (timeSinceLastDamage >= HEALTH_REGEN_DELAY && health < maxHealth) {
        health = std::min(maxHealth, health + healthRegenRate * deltaTime);
    }

    body.velocity = Vec2(0.f, 0.f);
    
    bool wantsToSprint = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);
    bool actuallySprinting = false;
    
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) {
        body.velocity.x = -1.0f;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
        body.velocity.x = 1.0f;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) {
        body.velocity.y = -1.0f;
    }
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) {
        body.velocity.y = 1.0f;
    }
    
    if (body.velocity.length() > 0.f) body.velocity.normalize();
    
    if (!attacking && !dead) {
        if (body.velocity.x != 0 || body.velocity.y != 0) {
            setState(PlayerState::WALK);
        } else {
            setState(PlayerState::IDLE);
        }
    }

    if (body.velocity.x != 0 || body.velocity.y != 0) {
        if (wantsToSprint && stamina > 0) {
            body.velocity.x *= sprintSpeed;
            body.velocity.y *= sprintSpeed;
            actuallySprinting = true;
            walkingSound.setPitch(1.5f);
        } else {
            body.velocity.x *= speed;
            body.velocity.y *= speed;
            walkingSound.setPitch(1.0f);
        }
        if (walkingSound.getStatus() != sf::Sound::Playing && !attacking) {
            walkingSound.play();
        }
    } else {
        if (walkingSound.getStatus() == sf::Sound::Playing) {
            walkingSound.stop();
        }
    }
    
    if (actuallySprinting) {
        stamina = std::max(0.0f, stamina - staminaDrainRate * deltaTime);
        isStaminaRegenerating = false;
        timeSinceStaminaUse = 0.0f;
    } else {
        timeSinceStaminaUse += deltaTime;
        if (timeSinceStaminaUse >= staminaRegenDelay) {
            isStaminaRegenerating = true;
        }
        if (isStaminaRegenerating) {
            stamina = std::min(maxStamina, stamina + staminaRegenRate * deltaTime);
        }
    }

    syncSpriteWithBody();
    
    sf::Vector2f playerPosition = sprite.getPosition();
    sf::Vector2f direction = worldMousePosition - playerPosition;
    float distanceToMouse = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    float minDistanceForRotation = 1.0f;

    float angleToMouseDegrees;
    if (distanceToMouse > minDistanceForRotation) {
        angleToMouseDegrees = std::atan2(direction.y, direction.x) * 180 / 3.14159265f;
        sprite.setRotation(angleToMouseDegrees - 90.0f);
    } else {
        angleToMouseDegrees = sprite.getRotation() + 90.0f;
    }
    
    if (attacking) {
        sf::Vector2f attackOffset = sf::Vector2f(
            std::cos(angleToMouseDegrees * 3.14159265f / 180) * 30.0f,
            std::sin(angleToMouseDegrees * 3.14159265f / 180) * 30.0f
        );
        float attackBoxSize = 60.0f;
        attackBounds = sf::FloatRect(
            playerPosition.x + attackOffset.x - (attackBoxSize / 2.0f),
            playerPosition.y + attackOffset.y - (attackBoxSize / 2.0f),
            attackBoxSize, attackBoxSize
        );
    }
    
    updateAnimation(deltaTime);
}

void Player::draw(sf::RenderWindow& window) {
    window.draw(sprite);
    
    //sf::RectangleShape mainHitboxShape;
    //sf::FloatRect mainHitbox = getHitbox();
    //mainHitboxShape.setPosition(mainHitbox.left, mainHitbox.top);
    //mainHitboxShape.setSize(sf::Vector2f(mainHitbox.width, mainHitbox.height));
    //mainHitboxShape.setFillColor(sf::Color(0, 255, 0, 70));
    //mainHitboxShape.setOutlineColor(sf::Color::Green);
    //mainHitboxShape.setOutlineThickness(1);
    //window.draw(mainHitboxShape);

    //if (attacking) {
    //    sf::RectangleShape attackRect;
    //    attackRect.setPosition(attackBounds.left, attackBounds.top);
    //    attackRect.setSize(sf::Vector2f(attackBounds.width, attackBounds.height));
    //    attackRect.setFillColor(sf::Color(255, 0, 0, 70));
    //    attackRect.setOutlineColor(sf::Color::Red);
    //    attackRect.setOutlineThickness(1);
    //    window.draw(attackRect);
    //}
}

void Player::setPosition(float x, float y) {
    body.position = Vec2(x, y);
    syncSpriteWithBody();
}

sf::Vector2f Player::getPosition() const {
    return sprite.getPosition();
}

sf::Sprite& Player::getSprite() {
    return sprite;
}

void Player::attack() {
    if (!attacking && !dead && timeSinceLastMeleeAttack >= meleeAttackCooldown) {
        attacking = true;
        currentAttackFrame = 0;
        attackTimer = 0.0f;
        setState(PlayerState::ATTACK);
        timeSinceLastMeleeAttack = 0.0f;

        std::vector<sf::Texture>* attackTextures = nullptr;
        sf::Sound* attackSound = nullptr;
        switch (currentWeapon) {
        case WeaponType::KNIFE:
            attackTextures = &knifeAttackTextures;
            attackSound = &knifeAttackSound;
            break;
        case WeaponType::BAT:
            attackTextures = &batAttackTextures;
            attackSound = &batAttackSound;
            break;
        case WeaponType::PISTOL:
            attackTextures = &pistolAttackTextures;
            attackSound = &pistolAttackSound;
            break;
        case WeaponType::RIFLE:
            attackTextures = &rifleAttackTextures;
            attackSound = &rifleAttackSound;
            break;
        case WeaponType::FLAMETHROWER:
            attackTextures = &flamethrowerAttackTextures;
            attackSound = &flamethrowerAttackSound;
            break;
        }
        if (attackTextures && !attackTextures->empty()) {
            sprite.setTexture((*attackTextures)[currentAttackFrame]);
        }
        if (attackSound) {
            attackSound->stop();
            attackSound->play();
        }
    }
}

void Player::render(sf::RenderWindow& window) {
    draw(window);
}

sf::FloatRect Player::getHitbox() const {
    const float desiredScaledWidth = 60.0f;
    const float desiredScaledHeight = 60.0f;
    float localBoxWidth = desiredScaledWidth / scaleFactor;
    float localBoxHeight = desiredScaledHeight / scaleFactor;
    sf::FloatRect localBoxDefinition(
        -localBoxWidth / 2.0f,
        -localBoxHeight / 2.0f,
        localBoxWidth,
        localBoxHeight
    );
    sf::FloatRect aabbOfTransformedLocalBox = sprite.getTransform().transformRect(localBoxDefinition);
    sf::Vector2f playerCurrentPosition = sprite.getPosition();
    sf::FloatRect finalHitbox(
        playerCurrentPosition.x - aabbOfTransformedLocalBox.width / 2.0f,
        playerCurrentPosition.y - aabbOfTransformedLocalBox.height / 2.0f,
        aabbOfTransformedLocalBox.width,
        aabbOfTransformedLocalBox.height
    );
    return finalHitbox;
}

sf::FloatRect Player::getAttackHitbox() const {
    return attackBounds;
}

void Player::applyKnockback(const sf::Vector2f& direction, float force) {
    body.externalImpulse += Vec2(direction.x, direction.y) * force;
}

void Player::reset() {
    health = maxHealth;
    stamina = maxStamina;
    dead = false;
    attacking = false;
    currentState = PlayerState::IDLE;
    currentWeapon = WeaponType::KNIFE;
    sprite.setPosition(100, 100);
    if (!knifeIdleTextures.empty()) {
        sprite.setTexture(knifeIdleTextures[0]);
    }
}

float Player::getCurrentHealth() const {
    return health;
}

float Player::getMaxHealth() const {
    return maxHealth;
}

void Player::syncSpriteWithBody() {
    sprite.setPosition(body.position.x, body.position.y);
}

// Returns a Vec2 rotated by angleRadians
Vec2 rotateVec2(const Vec2& v, float angleRadians) {
    float cosA = std::cos(angleRadians);
    float sinA = std::sin(angleRadians);
    return Vec2(
        v.x * cosA - v.y * sinA,
        v.x * sinA + v.y * cosA
    );
}

void Player::shoot(const sf::Vector2f& target, PhysicsWorld& physicsWorld, std::vector<std::unique_ptr<Bullet>>& bullets) {
    // Only allow shooting for gun-type weapons
    if (currentWeapon != WeaponType::PISTOL && currentWeapon != WeaponType::RIFLE)
        return;

    // Set fireCooldown based on weapon
    switch (currentWeapon) {
    case WeaponType::PISTOL: fireCooldown = 0.3f; break;
    case WeaponType::RIFLE: fireCooldown = 0.1f; break;
    default: fireCooldown = 0.5f; break;
    }

    // Weapon-specific bullet properties
    float bulletDamage = 25.f; // Default
    float bulletSpeed = 500.f;
    float bulletMass = 1.0f;
    int maxPenetrations = 1;
    float inaccuracyDegrees = 5.0f;

    switch (currentWeapon) {
    case WeaponType::PISTOL:
        bulletDamage = 15.f;
        bulletSpeed = 1000.f;
        bulletMass = 1.0f;
        maxPenetrations = 1;
        inaccuracyDegrees = 2.0f;
        break;
    case WeaponType::RIFLE:
        bulletDamage = 20.f;
        bulletSpeed = 1200.f;
        bulletMass = 1.5f;
        maxPenetrations = 2;
        inaccuracyDegrees = 3.0f;
        break;
    default:
        return;
    }

    // Calculate direction
    sf::Vector2f playerPos = sprite.getPosition();
    float playerRadius = getBody().getCircleShape().getRadius();
    sf::Vector2f dir = target - playerPos;
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len == 0) return;
    dir /= len; // Normalize

    // Add inaccuracy
    float inaccuracyRadians = inaccuracyDegrees * 3.14159265f / 180.0f;
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-inaccuracyRadians, inaccuracyRadians);
    float angle = dis(gen);

    Vec2 velocity = rotateVec2(Vec2(dir.x, dir.y), angle) * bulletSpeed;

    sf::Vector2f bulletSpawnPos = playerPos + dir * playerRadius;

    // Create and add bullet
    auto bullet = std::make_unique<Bullet>(Vec2(bulletSpawnPos.x, bulletSpawnPos.y), velocity, bulletMass, maxPenetrations, bulletDamage);
    physicsWorld.addBody(&bullet->getBody(), false);
    bullets.push_back(std::move(bullet));

    // Play shoot sound
    sf::Sound* shootSound = nullptr;
    switch (currentWeapon) {
    case WeaponType::PISTOL:
        shootSound = &pistolAttackSound;
        break;
    case WeaponType::RIFLE:
        shootSound = &rifleAttackSound;
        break;
    default:
        break; // No sound for KNIFE, BAT, FLAMETHROWER
    }
    if (shootSound) {
        shootSound->stop();
        shootSound->play();
    }

    // Reset shot timer
    timeSinceLastShot = 0.0f;
}
