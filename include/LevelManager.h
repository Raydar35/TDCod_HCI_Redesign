#ifndef LEVELMANAGER_H
#define LEVELMANAGER_H

#include <SFML/Graphics.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Audio.hpp>
#include <vector>
#include <string>
#include <optional>
#include <deque>
#include <unordered_map>
#include "Player.h"
#include "ZombieWalker.h"
#include "Bullet.h"
#include <chrono>
#include <iomanip>

enum class GameState {
    TUTORIAL,
    LEVEL1,
    LEVEL2,
    LEVEL3,
    LEVEL4,
    LEVEL5,
    BOSS_FIGHT,
    GAME_OVER,
    VICTORY
};

enum class TransitionState {
    NONE,
    FADE_IN,
    SHOW_TEXT,
    FADE_OUT
};

class PhysicsWorld;

class LevelManager {
public:
    LevelManager();
    
    void initialize();
    void update(float deltaTime, Player& player);
    void draw(sf::RenderWindow& window);
    
    void render(sf::RenderWindow& window);
    void renderUI(sf::RenderWindow& window, sf::Font& font);
    void nextLevel();
    void reset();
    
    int getPreviousLevel() const;
    GameState getCurrentState() const;
    void setGameState(GameState state);
    
    void startTutorial();
    bool isTutorialComplete() const;
    
    void loadLevel(int levelNumber);
    int getCurrentLevel() const;
    int getCurrentRound() const;
    void restartCurrentRound(const sf::Vector2f& playerPos);
    int getTotalRoundsForLevel(int level) const;
    
    void spawnZombies(int count, sf::Vector2f playerPos);
    // Configure per-round zombie stats
    struct ZombieRoundConfig {
        int count = 5;
        float health = 50.0f;
        float damage = 5.0f;
        float speed = 50.0f;
        // Walk animation frame time in seconds (lower = faster animation)
        float animSpeed = 0.1f;
    };
    void setTutorialConfig(const ZombieRoundConfig& cfg) { tutorialConfig = cfg; }
    void setRoundConfig(int roundIndex, const ZombieRoundConfig& cfg) { if (roundIndex >= 0 && roundIndex < (int)roundConfigs.size()) roundConfigs[roundIndex] = cfg; }
    
    void updateZombies(float deltaTime, const Player& player);
    void drawZombies(sf::RenderWindow& window) const;
    std::vector<BaseZombie*>& getZombies();

    // Render bullets managed by Game (LevelManager will draw them so ordering is managed centrally)
    void renderBullets(sf::RenderWindow& window, std::vector<std::unique_ptr<Bullet>>& bullets, float renderAlpha);

    void drawHUD(sf::RenderWindow& window, const Player& player);
    
    void showTutorialDialog(sf::RenderWindow& window);
    void advanceDialog();
    
    sf::Vector2f getMapSize() const;

    void setPhysicsWorld(PhysicsWorld* world);
    // Provide the current camera view rectangle (world coords) so spawn logic
    // can place zombies just outside the visible area.
    void setCameraViewRect(const sf::FloatRect& viewRect);
    // Shadow support: set a texture that will be assigned to spawned zombies
    void setShadowTexture(const sf::Texture& tex) { shadowTexture = &tex; }
    // HUD weapon icons (set by Game when loading assets)
    void setPistolIcon(const sf::Texture& tex) { pistolIcon = &tex; }
    void setRifleIcon(const sf::Texture& tex) { rifleIcon = &tex; }
    // Optional key icons for HUD equip buttons (setters below)
    void setKeyIcon1(const sf::Texture& tex);
    void setKeyIcon2(const sf::Texture& tex);
	void setKeyIconEsc(const sf::Texture& tex);
    // Tally textures for rounds (0..4)
    const sf::Texture* tallyTextures[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
    void setTallyTexture(int index, const sf::Texture& tex) { if (index >= 0 && index < 5) tallyTextures[index] = &tex; }
    // Per-icon scale multipliers (1.0 = fit to panel). Setters provided to tune different icon artwork sizes.
    void setPistolIconScale(float s) { pistolIconScale = s; }
    void setRifleIconScale(float s) { rifleIconScale = s; }
    // Per-weapon per-panel scales: allows tuning pistol/rifle sizes separately when shown in the top or bottom panel
    void setPistolTopScale(float s) { pistolTopScale = s; }
    void setPistolBottomScale(float s) { pistolBottomScale = s; }
    void setRifleTopScale(float s) { rifleTopScale = s; }
    void setRifleBottomScale(float s) { rifleBottomScale = s; }

    // Debug helpers
    void setDebugLogging(bool enabled) { debugLogging = enabled; }
    // Notify active zombies that the player has died so they can stop attacking/moving
    void notifyPlayerDeath();
    int getActiveZombieCount() const { return static_cast<int>(zombies.size()); }
    int getQueuedZombieCount() const { return static_cast<int>(zombiesToSpawn.size()); }
    bool getDebugLogging() const { return debugLogging; }

private:
    PhysicsWorld* physicsWorld = nullptr;
    GameState gameState;
    int currentLevel;
    int previousLevel;
    int currentRound;
    bool tutorialComplete;
    bool pendingLevelTransition = false;
    
    bool tutorialZombiesSpawned;
    std::vector<std::string> tutorialDialogs;
    int currentDialogIndex;
    sf::Text dialogText;
    sf::Font font4;
    sf::Font font2;
    sf::Font font3;
    sf::RectangleShape dialogBox;
    bool showingDialog;

    // Active zombies are pointers into the object pool (no ownership here)
    std::vector<BaseZombie*> zombies;

    // Simple spawn request describing a zombie to create (deferred)
    struct SpawnRequest {
        float x, y;
        float health;
        float damage;
        float speed;
        float animSpeed = 0.1f;
    };

    // Queue of spawn requests (deferred allocation)
    std::deque<SpawnRequest> zombiesToSpawn;

    // Object pool of zombie instances (owns objects). Currently pooling ZombieWalker instances.
    std::vector<std::unique_ptr<ZombieWalker>> zombiePool;
    // indices of free entries in zombiePool
    std::deque<int> freeZombieIndices;
    // map from active pointer -> pool index for quick recycling
    std::unordered_map<BaseZombie*, int> poolIndexByPtr;

    // Activate up to N queued zombies (adds physics bodies). Called from update().
    // Modified to accept player position so activation can be deferred until zombies are near the player/camera.
    void activateQueuedZombies(int maxToActivate = 1, const sf::Vector2f& playerPos = sf::Vector2f(0.f,0.f));
    // Ensure pool capacity at least N
    void ensurePoolSize(int desired);

    int totalZombiesInRound;
    int zombiesSpawnedInRound;
    int zombiesKilledInRound;
    float zombieSpawnTimer;
    float zombieSpawnInterval;
    bool roundStarted;

    sf::RectangleShape mapBounds;
    
    float roundTransitionTimer;
    bool inRoundTransition;
    float roundTransitionDuration = 2.2f; // total duration: move + hold + return
    
    float levelTransitionTimer;
    float levelTransitionDuration;
    bool levelTransitioning;
    TransitionState transitionState;
    sf::RectangleShape transitionRect;
    sf::Text levelStartText;
    sf::Sound levelStartSound;
    sf::SoundBuffer levelStartBuffer;
    
    int getZombieCountForLevel(int level, int round);
    // Per-round configuration (rounds 0..4) and tutorial config
    ZombieRoundConfig tutorialConfig;
    std::array<ZombieRoundConfig,5> roundConfigs;

    // Initializes the default per-round configs. Edit the values in the implementation
    // in LevelManager.cpp under the clearly marked "CONFIGURABLE ROUND SETTINGS" region.
    void initializeDefaultConfigs();

    // Current camera view rect used when computing spawn positions
    sf::FloatRect cameraViewRect = sf::FloatRect(0.f, 0.f, 800.f, 600.f);
    const sf::Texture* shadowTexture = nullptr;
    const sf::Texture* pistolIcon = nullptr;
    const sf::Texture* rifleIcon = nullptr;
    const sf::Texture* keyIcon1 = nullptr;
    const sf::Texture* keyIcon2 = nullptr;
    const sf::Texture* keyIconEsc = nullptr;
    int lastTallyIndex = -1; // previous round index used for cross-fade animation
    float pistolIconScale = 1.5f; // legacy/global multiplier
    float rifleIconScale = 3.3f;  // legacy/global multiplier
    // Per-panel per-weapon multipliers (1.0 = fit to panel). These control icon size when shown in top/bottom panels.
    float pistolTopScale = 1.3f;
    float pistolBottomScale = 1.3f;
    float rifleTopScale = 2.6f;
    float rifleBottomScale = 2.8f;
    bool tutorialWeaponForced = false;
    
    // Debugging
    bool debugLogging = false;

    // Health damage flash: when player loses health show the missing portion as red briefly
    float prevFrameHealthPercent = 1.0f; // health percent observed previous frame
    float damageFlashStartPercent = 1.0f; // percent before damage
    float damageFlashRemaining = 0.0f; // seconds left for flash
    float damageFlashDuration = 0.8f; // total duration of red flash
    // Hold time before the flash begins shrinking
    float damageFlashHold = 0.45f; // seconds to hold full red before shrinking
    float damageFlashHoldRemaining = 0.0f; // remaining hold time
    
public:
    bool isRequireEscToAdvanceDialog() const {
        if (currentDialogIndex < 0) return false;
        if (currentDialogIndex >= static_cast<int>(tutorialDialogs.size())) return false;
        const std::string token = "----";
        return tutorialDialogs[currentDialogIndex].find(token) != std::string::npos;
    }
    bool isLevelTransitionPending() const { return pendingLevelTransition; }
    void clearLevelTransitionPending() { pendingLevelTransition = false; }
    bool isLevelTransitioning() const { return levelTransitioning; }
};
#endif
