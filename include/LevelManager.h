#ifndef LEVELMANAGER_H
#define LEVELMANAGER_H

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <vector>
#include <string>
#include <optional>
#include "Player.h"
#include "Zombie.h"
#include "Doctor.h"
#include "ZombieWalker.h"
#include "ZombieCrawler.h"
#include "ZombieTank.h"
#include "ZombieZoom.h"
#include "ZombieKing.h"

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

    void spawnDoctor(float x, float y);
    void drawDoctor(sf::RenderWindow& window);
    bool isPlayerNearDoctor(const Player& player) const;
    
    void spawnZombies(int count, sf::Vector2f playerPos);
    void updateZombies(float deltaTime, const Player& player);
    void drawZombies(sf::RenderWindow& window) const;
    std::vector<std::unique_ptr<BaseZombie>>& getZombies();
    
    void drawHUD(sf::RenderWindow& window, const Player& player);
    
    void showTutorialDialog(sf::RenderWindow& window);
    void advanceDialog();
    
    sf::Vector2f getMapSize() const;

    void setPhysicsWorld(PhysicsWorld* world);

private:
    PhysicsWorld* physicsWorld = nullptr;
    GameState gameState;
    int currentLevel;
    int previousLevel;
    int currentRound;
    bool tutorialComplete;
    bool pendingLevelTransition = false;
    
    std::optional<Doctor> doctor;
    bool tutorialZombiesSpawned;
    std::vector<std::string> tutorialDialogs;
    int currentDialogIndex;
    sf::Text dialogText;
    sf::Font font;
    sf::RectangleShape dialogBox;
    bool showingDialog;

    std::vector<std::unique_ptr<BaseZombie>> zombies;
    std::vector<std::unique_ptr<BaseZombie>> zombiesToSpawn;

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
    
public:
    bool isLevelTransitionPending() const { return pendingLevelTransition; }
    void clearLevelTransitionPending() { pendingLevelTransition = false; }
    bool isLevelTransitioning() const { return levelTransitioning; }
};
#endif
