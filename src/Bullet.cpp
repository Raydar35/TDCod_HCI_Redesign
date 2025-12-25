#include "Bullet.h"
#include "BaseZombie.h"
#include "ExplosionProvider.hpp"
#include <iostream>
#include <SFML/Audio.hpp>

// Local static audio resources for hit/kill sounds (lazy-loaded)
static sf::SoundBuffer s_bHitBuf;
static sf::SoundBuffer s_bKillBuf;

// small pools to allow overlapping play without cutting previous sounds
static std::vector<sf::Sound> s_hitSoundsPool;
static std::vector<sf::Sound> s_killSoundsPool;
static size_t s_hitIndex = 0;
static size_t s_killIndex = 0;
static bool s_soundsLoaded = false;

static constexpr size_t SOUND_POOL_SIZE = 8; // allow up to 8 overlapping instances

// Base volumes used as reference; applied with master percent multiplier
static float s_baseHitVolume = 85.f;
static float s_baseKillVolume = 100.f;

// Master percent / mute state applied to pools (0..100)
static float s_masterSfxPercent = 100.f;
static bool s_masterSfxMuted = false;

static void initSoundPoolsIfNeeded() {
    if (!s_hitSoundsPool.empty() && !s_killSoundsPool.empty()) return;
    s_hitSoundsPool.resize(SOUND_POOL_SIZE);
    s_killSoundsPool.resize(SOUND_POOL_SIZE);
}

static void applyMasterToPools() {
    float mul = s_masterSfxMuted ? 0.f : (s_masterSfxPercent / 100.0f);
    float hitVol = s_baseHitVolume * mul;
    float killVol = s_baseKillVolume * mul;
    for (auto& s : s_hitSoundsPool) s.setVolume(hitVol);
    for (auto& s : s_killSoundsPool) s.setVolume(killVol);
}

static void loadHitKillSounds() {
    if (s_soundsLoaded) return;
    // Placeholder paths - replace with real asset paths in your project
    const std::string hitPath = "assets/Audio/hit_zombie.mp3";
    const std::string killPath = "assets/Audio/kill_zombie.wav";

    if (!s_bHitBuf.loadFromFile(hitPath)) {
        std::cerr << "Warning: failed to load hit sound: " << hitPath << std::endl;
    }
    else {
        initSoundPoolsIfNeeded();
        for (auto& s : s_hitSoundsPool) {
            s.setBuffer(s_bHitBuf);
            // set volume according to base * master percent
            s.setVolume(s_baseHitVolume * (s_masterSfxMuted ? 0.f : s_masterSfxPercent / 100.f));
        }
    }

    if (!s_bKillBuf.loadFromFile(killPath)) {
        std::cerr << "Warning: failed to load kill sound: " << killPath << std::endl;
    }
    else {
        initSoundPoolsIfNeeded();
        for (auto& s : s_killSoundsPool) {
            s.setBuffer(s_bKillBuf);
            s.setVolume(s_baseKillVolume * (s_masterSfxMuted ? 0.f : s_masterSfxPercent / 100.f));
        }
    }

    s_soundsLoaded = true;
}

// NEW: public API to control global SFX for hit/kill pools
void Bullet::setGlobalSfxVolume(float percent) {
    s_masterSfxPercent = std::clamp(percent, 0.0f, 100.0f);
    applyMasterToPools();
}

void Bullet::setGlobalSfxMuted(bool muted) {
    s_masterSfxMuted = muted;
    applyMasterToPools();
}

sf::Texture Bullet::bulletTexture;

Bullet::Bullet(Vec2 position, Vec2 velocity, float mass, int maxPenetrations, float damage,
    sf::Vector2f spriteOffsetIn, float spriteScaleIn, float rotationOffsetIn)
    : Entity(EntityType::Bullet, position, Vec2(8.f, 8.f), false, mass, true),
    remainingPenetrations(maxPenetrations),
    initialPenetrations(maxPenetrations),
    damage(damage),
    spriteOffset(spriteOffsetIn),
    spriteScale(spriteScaleIn),
    rotationOffset(rotationOffsetIn)
{
    getBody().isCircle = true;
    body.isTrigger = true;
    body.velocity = velocity;
    body.getCircleShape().setRadius(4.f);
    body.getCircleShape().setOrigin(4.f, 4.f);
    body.getCircleShape().setFillColor(sf::Color::Yellow);

    // Load texture for bullet
    sprite.setTexture(Bullet::bulletTexture);
    sprite.setOrigin(Bullet::bulletTexture.getSize().x / 2.f, Bullet::bulletTexture.getSize().y / 2.f);
    sprite.setScale(spriteScale, spriteScale);
    sprite.setPosition(body.position.x, body.position.y);
    float angle = std::atan2(body.velocity.y, body.velocity.x) * 180.f / 3.14159f; // Get direction from velocity
    sprite.setRotation(angle + rotationOffset);

    prevPos = currPos = sf::Vector2f(body.position.x, body.position.y);

    // shorter default life to avoid accumulating many stray bullets
    lifeTimer = 0.0f;
    maxLife = 2.0f;
}

void Bullet::update(float dt) {
    // store previous position for interpolation
    prevPos = currPos;

    body.update(dt); // Add physics updates if needed

    currPos = sf::Vector2f(body.position.x, body.position.y);

    // lifetime decay
    lifeTimer += dt;
    if (lifeTimer >= maxLife) {
        destroy();
    }
}

void Bullet::render(sf::RenderWindow& window) {
    float alpha = renderAlpha;
    // Interpolate position between prevPos and currPos
    sf::Vector2f interp = prevPos + (currPos - prevPos) * alpha;

    // Compute rotation from velocity
    float rad = std::atan2(body.velocity.y, body.velocity.x);
    float deg = rad * 180.f / 3.14159f;
    float cs = std::cos(rad), sn = std::sin(rad);

    // Rotate offset vector by the bullet rotation so offset follows facing
    sf::Vector2f rotatedOffset(spriteOffset.x * cs - spriteOffset.y * sn,
        spriteOffset.x * sn + spriteOffset.y * cs);

    sprite.setPosition(interp.x + rotatedOffset.x, interp.y + rotatedOffset.y);
    sprite.setRotation(deg + rotationOffset);

    window.draw(sprite);
}

void Bullet::onCollision(Entity* other) {
    if (!other || other == this || !other->isAlive()) return;

    EntityType targetType = other->getType();

    if (targetType == EntityType::Enemy) {
        // Avoid multiple hits on the same entity
        if (hitEntities.count(other)) return;
        hitEntities.insert(other);

        // Deal damage
        BaseZombie* zombie = dynamic_cast<BaseZombie*>(other);
        if (zombie) {
            // ensure sounds are loaded (lazy)
            loadHitKillSounds();

            // Apply damage
            zombie->takeDamage(damage); // Use bullet's current damage
            // Play sound only for the first penetration (i.e., if this is the initial hit)
            if (initialPenetrations == remainingPenetrations) {
                if (zombie->isDead()) {
                    if (!s_killSoundsPool.empty() && s_bKillBuf.getSampleCount() > 0) {
                        s_killSoundsPool[s_killIndex % s_killSoundsPool.size()].stop();
                        s_killSoundsPool[s_killIndex % s_killSoundsPool.size()].play();
                        s_killIndex = (s_killIndex + 1) % s_killSoundsPool.size();
                    }
                }
                else {
                    if (!s_hitSoundsPool.empty() && s_bHitBuf.getSampleCount() > 0) {
                        s_hitSoundsPool[s_hitIndex % s_hitSoundsPool.size()].stop();
                        s_hitSoundsPool[s_hitIndex % s_hitSoundsPool.size()].play();
                        s_hitIndex = (s_hitIndex + 1) % s_hitSoundsPool.size();
                    }
                }
            }
        }

        // Calculate the mass ratio between the bullet and the enemy
        float bulletMass = body.mass;
        float enemyMass = other->getBody().mass;

        // Apply pushback scaled by mass ratio
        float pushbackFactor = bulletMass / (bulletMass + enemyMass);  // Small pushback for heavy enemies, bigger for light enemies
        other->getBody().externalImpulse += body.velocity * 0.5f * pushbackFactor;

        // Effects (blood/explosion) are handled by the zombie's hit/takeDamage logic now

        remainingPenetrations--;
        // After penetrating, reduce damage for next potential hit
        damage *= penetrationDamageMultiplier;
    }
    else if (targetType == EntityType::Wall) {
        remainingPenetrations = 0; // Wall stops bullet
    }

    if (remainingPenetrations <= 0) {
        destroy();
    }
}