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
    // Load per-round configs from a simple CSV-like file. See TDCod/Config/rounds.cfg for format.
    void loadConfigs(const std::string& path);

    // Debug helpers
    void setDebugLogging(bool enabled) { debugLogging = enabled; }
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
    sf::Font font;
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
    // Weapon equip request: when a round starts LevelManager can request the Player
    // to equip a specific weapon. This is set inside spawnZombies and applied in
    // the next update() call where a Player reference is available.
    //WeaponType startingWeaponForRound = WeaponType::PISTOL;
    //bool equipWeaponPending = false;
    // Previously we forced weapon equips at round start; now we only ensure the
    // player starts the tutorial with the pistol once. This flag prevents
    // repeatedly re-equipping the player while allowing them to swap during
    // the tutorial via input.
    bool tutorialWeaponForced = false;
    
    // Debugging
    bool debugLogging = false;

public:
    bool isLevelTransitionPending() const { return pendingLevelTransition; }
    void clearLevelTransitionPending() { pendingLevelTransition = false; }
    bool isLevelTransitioning() const { return levelTransitioning; }
};
#endif
