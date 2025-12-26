#ifndef PLAYER_H
#define PLAYER_H

#include "PhysicsWorld.h"
#include "Entity.h"
#include "Bullet.h"
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <vector>
#include <array>
#include "include/Animator.h"

enum class PlayerState {
    IDLE,
    WALK,
    ATTACK,
    DEATH
};

enum class WeaponType {
    PISTOL,
    RIFLE
};

enum class FeetState {
    IDLE = 0,
    WALK = 1,
    RUN = 2,
    STRAFE_LEFT = 3,
    STRAFE_RIGHT = 4
};

class Player : public Entity {
public:
    Player(Vec2 position);
    
    void update(float deltaTime, sf::RenderWindow& window, sf::Vector2u mapSize, sf::Vector2f worldMousePosition);
    void draw(sf::RenderWindow& window);
    void render(sf::RenderWindow& window);
    void attack();
    void takeDamage(float amount);
    void kill();
    
    void setPosition(float x, float y);
    sf::Vector2f getPosition() const;
    // Return the physics body's position (accurate world position for AI targeting)
    sf::Vector2f getPhysicsPosition() const;
    
    void sprint(bool isSprinting);
    bool isSprinting() const;
    
    bool isAttacking() const;
    bool isDead() const;

    float getCurrentHealth() const;
    float getMaxHealth() const;
    
    float getCurrentStamina() const;
    float getMaxStamina() const;
    
    sf::FloatRect getAttackBounds() const;
    sf::Sprite& getSprite();
    
    void applyKnockback(const sf::Vector2f& direction, float force);

    // Knockback state (short slow + physics impulse)
    float knockbackTimer = 0.0f;
    // Reduced duration so the slow effect from zombie hits feels shorter
    float knockbackDuration = 0.1f; // seconds
    float knockbackMoveMultiplier = 0.35f; // movement reduced while knocked
    
    float getAttackDamage() const;
    
    sf::FloatRect getHitbox() const;
    sf::FloatRect getAttackHitbox() const;
    
    void reset();
    
    WeaponType getCurrentWeapon() const { return currentWeapon; }
    void setWeapon(WeaponType weapon);
    void shoot(const sf::Vector2f& target, PhysicsWorld& physicsWorld, std::vector<std::unique_ptr<Bullet>>& bullets);

    // Return the world-space muzzle position for the current weapon given an aim target
    sf::Vector2f getMuzzlePosition(const sf::Vector2f& aimTarget) const;

    // Reloading
    void startReload();
    bool isReloading() const { return reloading; }
    int getCurrentAmmo() const { return currentAmmo; }
    int getMagazineSize() const { return magazineSize; }

    float timeSinceLastShot = 0.0f;
    float fireCooldown = 0.0f; // Time (in seconds) between shots

    void syncSpriteWithBody();

    // Aiming controls
    void setAiming(bool a) { aiming = a; }
    bool isAiming() const { return aiming; }

    // Animator sheet helpers
    void setFeetSpriteSheet(const sf::Texture& sheetTexture, const std::vector<std::vector<sf::IntRect>>& framesByState, float frameTime);
    // per-state setter: provide a separate sheet and frames for one feet state
    void setFeetStateSheet(FeetState state, const sf::Texture& sheetTexture, const std::vector<sf::IntRect>& frames, float frameTime);

    // Upper body sheets per weapon (idle, move, shoot, reload)
    void setUpperWeaponSheet(WeaponType weapon, const std::array<const sf::Texture*,4>& sheets, const std::vector<std::vector<sf::IntRect>>& framesByState, const std::array<float,4>& frameTimes);

    // Alignment offsets (tweak these to adjust origins/positioning)
    // feetOffset is applied to feet sprite position relative to body position
    float feetOffsetX = 0.0f;
    float feetOffsetY = 0.0f;
    // upperOriginOffset shifts the sprite origin after centering (useful if sprite art pivot isn't centered)
    float upperOriginOffsetX = -55.0f;
    float upperOriginOffsetY = 18.0f;

    // When true, render small markers at the upper and feet sprite origins to help align sprites visually
    bool debugDrawOrigins = false;

    // Optional muzzle flash texture (set via setter). If set, muzzle flashes will use this texture
    // rendered with additive blending. Texture lifetime must outlive Player.
    const sf::Texture* muzzleTexture = nullptr;
    void setMuzzleTexture(const sf::Texture& tex) { muzzleTexture = &tex; }

    // Shadow support: texture pointer stored on the player (must outlive Player)
    const sf::Texture* shadowTexture = nullptr;
    void setShadowTexture(const sf::Texture& tex) { shadowTexture = &tex; }

    // Sound buffer setters (Game should load buffers and pass them here)
    void setPistolSoundBuffer(const sf::SoundBuffer& buf);
    void setRifleSoundBuffer(const sf::SoundBuffer& buf);
    void setWalkingSoundBuffer(const sf::SoundBuffer& buf);

    // Footstep sounds: provide multiple buffers; player will play them in sequence while moving
    void setStepSoundBuffers(const std::vector<sf::SoundBuffer>& bufs);

    // Reload sound setters
    void setPistolReloadSoundBuffer(const sf::SoundBuffer& buf);
    void setRifleReloadSoundBuffer(const sf::SoundBuffer& buf);

    // Footstep sound state
    std::vector<sf::SoundBuffer> stepBuffers;
    std::vector<sf::Sound> stepSoundInstances; // playback instances to allow overlap
    float stepTimer = 0.0f;
    float baseStepInterval = 0.48f; // seconds at normal walk speed
    size_t stepIndex = 0; // which buffer to play next
    size_t lastStepInstance = 0;
    bool lastStepSprinting = false; // track sprint state to adjust step timing immediately

    // Per-weapon muzzle offsets (in pixels) relative to body forward/right: used to compute bullet spawn
    float muzzleForwardPistol = 45.0f;
    float muzzleLateralPistol = 15.0f;
    float muzzleForwardRifle = 55.0f;
    float muzzleLateralRifle = 15.0f;

    // Explicit sprite-local muzzle offsets (x = forward, y = lateral (positive = right))
    sf::Vector2f muzzleOffsetPistol = sf::Vector2f(45.0f, 14.0f);
    sf::Vector2f muzzleOffsetRifle = sf::Vector2f(60.0f, 11.0f);

    // Interpolation positions for smooth rendering
    sf::Vector2f prevPos = sf::Vector2f(0.f, 0.f);
    sf::Vector2f currPos = sf::Vector2f(0.f, 0.f);
    float renderAlpha = 1.0f;
    void setRenderAlpha(float a) { renderAlpha = a; }

    // Active muzzle flashes spawned when shooting
    struct MuzzleFlash {
        sf::Sprite sprite;
        float life = 0.0f; // remaining life in seconds
        float maxLife = 0.0f;
    };
    std::vector<MuzzleFlash> activeMuzzles;

    // Tuning for muzzle flashes
    float muzzleFlashLife = 0.07f; // seconds (increased for visibility)
    float muzzleFlashScale = 0.016f; // base scale applied to texture (larger for visibility)
    // Rotation offset (degrees) applied to muzzle sprite so art faces the correct direction
    float muzzleFlashRotationOffset = 90.0f;
    // Optional random jitter applied to flash rotation (+/- degrees)
    float muzzleFlashRotationJitterDeg = 0.0f;

private:
    void loadTextures();
    void updateAnimation(float deltaTime);
    void setState(PlayerState newState);
    void updateStamina(float deltaTime);
    void cancelReload();

    // helper to set feet state
    void setFeetStateInternal(FeetState newState);
    
    // Upper body sprite + feet sprite (draw feet first)
    sf::Sprite sprite; // upper body
    sf::Sprite feetSprite; // feet layer

    PlayerState currentState;
    
    float speed;
    float sprintSpeed;
    float rotationSpeed;
    float scaleFactor;
    // Rotation offset (degrees) applied to upper sprite so art faces correct direction
    float rotationOffset = 0.0f;
    // Slight upper-body tilt configuration (visual tweak)
    float upperTiltMaxDegrees = 6.0f; // maximum tilt applied to upper sprite
    float upperTiltScale = 0.06f; // scale applied to signed movement angle to compute tilt
    // current smoothed tilt value (degrees)
    float upperTiltCurrent = 0.0f;
    
    float health;
    float maxHealth;
    
    float healthRegenRate = 5.0f;
    float timeSinceLastDamage = 0.0f;
    const float HEALTH_REGEN_DELAY = 5.0f;

    float stamina;
    float maxStamina;
    float staminaRegenRate;
    float staminaDrainRate;
    float staminaRegenDelay;
    float timeSinceStaminaUse;
    bool isStaminaRegenerating;
    
    int currentFrame;
    float animationTimer;
    float animationFrameTime;
    
    bool attacking;
    int currentAttackFrame;
    float attackTimer;
    float attackFrameTime;
    sf::FloatRect attackBounds;
    
    float timeSinceLastMeleeAttack = 0.0f;
    float meleeAttackCooldown = 0.8f;

    bool dead;
    int currentDeathFrame;
    float deathTimer;
    float deathFrameTime;
    
    // Current weapon
    WeaponType currentWeapon;

    // Texture collections for each weapon (upper body per-frame textures kept for compatibility)
    // knife and bat no longer use per-frame upper textures; sounds retained

    std::vector<sf::Texture> deathTextures;

    // Feet sprite-sheet support: per-state texture+frames
    std::array<const sf::Texture*,5> feetStateTextures = {nullptr,nullptr,nullptr,nullptr,nullptr};
    std::vector<std::vector<sf::IntRect>> feetFramesByState; // index by FeetState
    std::array<float,5> feetFrameTimes = {0.1f,0.03f,0.03f,0.03f,0.03f};
    FeetState currentFeetState = FeetState::IDLE;

    // Upper body sprite-sheet support for pistol and rifle: states: 0=idle,1=move,2=shoot,3=reload
    std::array<const sf::Texture*,4> pistolUpperSheets = {nullptr,nullptr,nullptr,nullptr};
    std::vector<std::vector<sf::IntRect>> pistolUpperFramesByState; // 4 states
    std::array<float,4> pistolUpperFrameTimes = {0.1f,0.1f,0.05f,0.05f};

    std::array<const sf::Texture*,4> rifleUpperSheets = {nullptr,nullptr,nullptr,nullptr};
    std::vector<std::vector<sf::IntRect>> rifleUpperFramesByState; // 4 states
    std::array<float,4> rifleUpperFrameTimes = {0.1f,0.1f,0.04f,0.08f};

    bool upperShooting = false;

    // Animator instances
    Animator upperAnimator;
    Animator feetAnimator;

    // Track currently applied upper animation state to avoid resetting frames every update
    int currentUpperStateIndex = -1;
    const sf::Texture* currentUpperSheetPtr = nullptr;
    bool currentUpperShootingFlag = false;

    // Aiming / recoil
    bool aiming = false;
    bool sprinting = false;
    // Desired sprint requested by input (use Player::sprint to set)
    bool desiredSprint = false;
    // recoil in degrees added to base inaccuracy
    float recoil = 0.0f;
    float recoilDecayRate = 3.5f; // degrees per second
    float recoilPerShotAimed = 0.6f;
    float recoilPerShotHip = 1.2f;
    float maxRecoilAimed = 3.0f;
    float maxRecoilHip = 10.0f;
    float baseInaccuracyAimedPistol = 1.0f;
    float baseInaccuracyHipPistol = 3.0f;
    float baseInaccuracyAimedRifle = 0.5f;
    float baseInaccuracyHipRifle = 5.0f;
    float aimSpeedMultiplier = 0.65f; // movement speed multiplier while aiming

    // Sound buffers and sounds for each weapon
    sf::SoundBuffer pistolAttackBuffer;
    sf::Sound pistolAttackSound;
    sf::SoundBuffer rifleAttackBuffer;
    sf::Sound rifleAttackSound;
    sf::SoundBuffer walkingBuffer;
    sf::Sound walkingSound;

    // Reload sound buffers and sounds
    sf::SoundBuffer pistolReloadBuffer;
    sf::Sound pistolReloadSound;
    sf::SoundBuffer rifleReloadBuffer;
    sf::Sound rifleReloadSound;

    std::vector<sf::Sound> pistolSoundPool;
    std::vector<sf::Sound> rifleSoundPool;
    size_t pistolSoundIndex = 0;
    size_t rifleSoundIndex = 0;

    // Ammo / reload
    int magazineSize = 0;
    int currentAmmo = 0;   // rounds in current magazine
    bool reloading = false;
    float reloadTimer = 0.0f;
    float reloadDuration = 1.5f; // seconds

    // Per-weapon ammo storage so switching weapons preserves remaining ammo
    int pistolAmmoInMag = 12;
    int rifleAmmoInMag = 30;
    bool weaponAmmoInitialized = false;

    // Deadzone smoothing when leaving muzzle-origin circle
    bool wasInDeadzone = false; // true if in previous update the mouse was inside deadzone
    float deadzoneExitTimer = 0.0f; // timer for smooth transition when leaving
    float deadzoneExitDuration = 0.12f; // seconds to smoothly rotate after leaving deadzone
    float deadzoneStartAngle = 0.0f; // sprite rotation at moment of leaving deadzone
};

#endif // PLAYER_H