#include "LevelManager.h"
#include "PhysicsWorld.h"
#include <memory>
#include <iostream>
#include <random>
#include <cmath>

LevelManager::LevelManager()
    : gameState(GameState::TUTORIAL),
      currentLevel(0),
      previousLevel(0),
      currentRound(0),
      tutorialComplete(false),
      tutorialZombiesSpawned(false),
      currentDialogIndex(0),
      showingDialog(false),
      roundTransitionTimer(0.0f),
      inRoundTransition(false),
      levelTransitionTimer(0.0f),
      levelTransitionDuration(3.0f),
      levelTransitioning(false),
      transitionState(TransitionState::NONE),
      totalZombiesInRound(0),
      zombiesSpawnedInRound(0),
      zombiesKilledInRound(0),
      zombieSpawnTimer(0.0f),
      zombieSpawnInterval(1.0f),
      roundStarted(false)
{
    tutorialDialogs = {
        "Welcome to Echoes of Valkyrie! I'm Dr. Mendel, the lead researcher.",
        "The infection is spreading rapidly. We need your help!",
        "Use WASD keys to move around. Press SHIFT to sprint, but watch your stamina.",
        "Left click to attack zombies with your knife.",
        "Defeat zombies to earn points and progress to the next level.",
        "Good luck! The fate of humanity depends on you.",
        "Complete this tutorial by exploring the area and eliminating all zombies."
    };
    
    if (!font.loadFromFile("TDCod/Assets/Call of Ops Duty.otf")) {
        std::cerr << "Error loading font for dialog! Trying fallback." << std::endl;
    }
    
    dialogBox.setSize(sf::Vector2f(600, 150));
    dialogBox.setFillColor(sf::Color(0, 0, 0, 200));
    dialogBox.setOutlineColor(sf::Color::White);
    dialogBox.setOutlineThickness(2);
    
    dialogText.setFont(font);
    dialogText.setCharacterSize(18);
    dialogText.setFillColor(sf::Color::White);
    
    mapBounds.setFillColor(sf::Color::Transparent);
    mapBounds.setOutlineColor(sf::Color::Red);
    mapBounds.setOutlineThickness(2);
    mapBounds.setPosition(0, 0);
    
    if (!levelStartBuffer.loadFromFile("TDCod/Assets/Audio/wavestart.mp3")) {
        std::cerr << "Error loading level start sound!" << std::endl;
    }
    levelStartSound.setBuffer(levelStartBuffer);
    
    transitionRect.setFillColor(sf::Color::Black);
    transitionRect.setPosition(0, 0);
    
    levelStartText.setFont(font);
    levelStartText.setCharacterSize(48);
    levelStartText.setFillColor(sf::Color::White);
    levelStartText.setPosition(300, 500);
}

void LevelManager::initialize() {
    setGameState(GameState::TUTORIAL);
    startTutorial();
}

void LevelManager::update(float deltaTime, Player& player) {
    previousLevel = currentLevel;

    sf::Vector2f mapSize = getMapSize();
    mapBounds.setSize(mapSize);
    transitionRect.setSize(mapSize);

    if (levelTransitioning) {
        levelTransitionTimer += deltaTime;
        if (transitionState == TransitionState::FADE_IN) {
            float alpha = 255 * (levelTransitionTimer / levelTransitionDuration);
            transitionRect.setFillColor(sf::Color(0, 0, 0, static_cast<int>(alpha)));
            if (levelTransitionTimer >= levelTransitionDuration) {
                transitionState = TransitionState::SHOW_TEXT;
                levelTransitionTimer = 0.0f;
            }
        } else if (transitionState == TransitionState::SHOW_TEXT) {
            if (levelTransitionTimer >= 1.0f) {
                transitionState = TransitionState::FADE_OUT;
                levelTransitionTimer = 0.0f;
            }
        } else if (transitionState == TransitionState::FADE_OUT) {
            float alpha = 255 - (255 * (levelTransitionTimer / levelTransitionDuration));
            transitionRect.setFillColor(sf::Color(0, 0, 0, static_cast<int>(alpha)));
            if (levelTransitionTimer >= levelTransitionDuration) {
                levelTransitioning = false;
                transitionState = TransitionState::NONE;
                levelTransitionTimer = 0.0f;
                spawnZombies(getZombieCountForLevel(currentLevel, currentRound), player.getPosition());
            }
        }
        return;
    }

    if (inRoundTransition) {
        roundTransitionTimer += deltaTime;
        if (roundTransitionTimer >= 2.0f) {
            inRoundTransition = false;
            roundTransitionTimer = 0.0f;
            spawnZombies(getZombieCountForLevel(currentLevel, currentRound), player.getPosition());
        }
        return;
    }

    if (zombiesSpawnedInRound < totalZombiesInRound && !zombiesToSpawn.empty()) {
        zombieSpawnTimer += deltaTime;
        if (zombieSpawnTimer >= zombieSpawnInterval) {
            zombies.push_back(std::move(zombiesToSpawn.front()));
            zombiesToSpawn.erase(zombiesToSpawn.begin());
            zombiesSpawnedInRound++;
            zombieSpawnTimer = 0.0f;
        }
    }

    switch (gameState) {
        case GameState::TUTORIAL:
            if (isPlayerNearDoctor(player) && !showingDialog) {
                showingDialog = true;
            }

            if (tutorialZombiesSpawned) {
                updateZombies(deltaTime, player);
            }

            if (currentDialogIndex >= tutorialDialogs.size() && !tutorialZombiesSpawned) {
                if (doctor) {
                    zombies.push_back(std::make_unique<ZombieWalker>(doctor->getPosition().x + 100, doctor->getPosition().y));
                    zombies.push_back(std::make_unique<ZombieWalker>(doctor->getPosition().x - 100, doctor->getPosition().y));
                    zombies.push_back(std::make_unique<ZombieWalker>(doctor->getPosition().x, doctor->getPosition().y + 100));
                    totalZombiesInRound = 3;
                    zombiesSpawnedInRound = 3;
                }
                tutorialZombiesSpawned = true;
            }

            if (zombiesKilledInRound == totalZombiesInRound && tutorialZombiesSpawned) {
                tutorialComplete = true;
                pendingLevelTransition = true;
                showingDialog = false;
                player.setWeapon(WeaponType::BAT);
            }
            break;

        case GameState::LEVEL1:
        case GameState::LEVEL2:
        case GameState::LEVEL3:
        case GameState::LEVEL4:
        case GameState::LEVEL5:
            updateZombies(deltaTime, player);

            if (roundStarted && zombiesKilledInRound == totalZombiesInRound && zombiesSpawnedInRound == totalZombiesInRound && !inRoundTransition && !levelTransitioning && transitionState == TransitionState::NONE) {
                if (currentLevel == 4 && currentRound == 1) { // After Level 4, Round 2
                    setGameState(GameState::BOSS_FIGHT);
                    loadLevel(5);
                    player.setWeapon(WeaponType::FLAMETHROWER);
                    roundStarted = false;
                } else if (currentRound < getTotalRoundsForLevel(currentLevel) - 1) {
                    currentRound++;
                    inRoundTransition = true;
                    roundStarted = false;
                } else {
                    int nextLevel = currentLevel + 1;
                    switch (nextLevel) {
                        case 2: player.setWeapon(WeaponType::PISTOL); break;
                        case 3: player.setWeapon(WeaponType::RIFLE); break;
                        case 4: player.setWeapon(WeaponType::FLAMETHROWER); break;
                    }
                    loadLevel(nextLevel);
                    roundStarted = false;
                }
            }
            break;

        case GameState::BOSS_FIGHT:
            updateZombies(deltaTime, player);
            if (zombiesKilledInRound == totalZombiesInRound && zombiesSpawnedInRound == totalZombiesInRound) {
                setGameState(GameState::VICTORY);
            }
            break;

        case GameState::GAME_OVER:
        case GameState::VICTORY:
            break;
    }

    if (doctor) {
        doctor->update(deltaTime, player.getPosition());
    }
}

void LevelManager::draw(sf::RenderWindow& window) {
    drawZombies(window);
    if (doctor) {
        doctor->draw(window);
    }
}

void LevelManager::render(sf::RenderWindow& window) {
    draw(window);
}

void LevelManager::renderUI(sf::RenderWindow& window, sf::Font& font) {
    if (showingDialog && currentDialogIndex < tutorialDialogs.size()) {
        dialogText.setFont(this->font);
        showTutorialDialog(window);
    }
    
    if (inRoundTransition && gameState != GameState::TUTORIAL) {
        sf::Text transitionText;
        transitionText.setFont(this->font);
        transitionText.setCharacterSize(24);
        transitionText.setFillColor(sf::Color::Yellow);
        transitionText.setString("Round " + std::to_string(currentRound + 1) + " Starting Soon...");
        sf::Vector2u windowSize = window.getSize();
        transitionText.setPosition((windowSize.x - transitionText.getGlobalBounds().width) / 2, windowSize.y / 2);
        window.draw(transitionText);
    }
    
    if (levelTransitioning) {
        window.draw(transitionRect);
    }
    
    if (levelTransitioning && (transitionState == TransitionState::FADE_IN || transitionState == TransitionState::SHOW_TEXT)) {
        std::string levelTextString;
        if (currentLevel >= 1 && currentLevel <= 4) {
            levelTextString = "Level " + std::to_string(currentLevel) + " Starting";
        } else if (gameState == GameState::BOSS_FIGHT) {
            levelTextString = "Boss Fight: Zombie King";
        } else {
            levelTextString = "Starting Level";
        }
        levelStartText.setString(levelTextString);
        sf::Vector2u windowSize = window.getSize();
        levelStartText.setPosition((windowSize.x - levelStartText.getGlobalBounds().width) / 2, windowSize.y / 2 - 50);
        window.draw(levelStartText);
    }
}

void LevelManager::nextLevel() {
    int nextLevelNumber = currentLevel + 1;
    if (nextLevelNumber > 5) {
        setGameState(GameState::VICTORY);
    } else {
        loadLevel(nextLevelNumber);
    }
}

void LevelManager::reset() {
    currentLevel = 0;
    currentRound = 0;
    zombies.clear();
    zombiesToSpawn.clear();
    doctor.reset();
    tutorialComplete = false;
    tutorialZombiesSpawned = false;
    currentDialogIndex = 0;
    showingDialog = false;
    roundTransitionTimer = 0.0f;
    inRoundTransition = false;
    levelTransitionTimer = 0.0f;
    levelTransitioning = false;
    transitionState = TransitionState::NONE;
    totalZombiesInRound = 0;
    zombiesSpawnedInRound = 0;
    zombiesKilledInRound = 0;
    zombieSpawnTimer = 0.0f;
    zombieSpawnInterval = 1.0f;
    roundStarted = false;
    setGameState(GameState::TUTORIAL);
}

GameState LevelManager::getCurrentState() const {
    return gameState;
}

void LevelManager::setGameState(GameState state) {
    gameState = state;
}

void LevelManager::startTutorial() {
    currentLevel = 0;
    currentRound = 0;
    tutorialComplete = false;
    currentDialogIndex = 0;
    showingDialog = true;
    spawnDoctor(400, 300);
}

bool LevelManager::isTutorialComplete() const {
    return tutorialComplete;
}

int LevelManager::getCurrentLevel() const {
    return currentLevel;
}

int LevelManager::getCurrentRound() const {
    return currentRound;
}

int LevelManager::getPreviousLevel() const {
    return previousLevel;
}

void LevelManager::loadLevel(int levelNumber) {
    previousLevel = currentLevel;
    currentLevel = levelNumber;
    currentRound = 0;
    zombies.clear();
    zombiesToSpawn.clear();
    totalZombiesInRound = 0;
    zombiesSpawnedInRound = 0;
    zombiesKilledInRound = 0;
    roundStarted = false;

    bool triggerTransition = false;
    if ((previousLevel >= 1 && previousLevel <= 3) || (previousLevel == 4 && currentLevel == 5)) {
        triggerTransition = true;
    }

    if (triggerTransition) {
        levelTransitioning = true;
        levelTransitionTimer = 0.0f;
        transitionState = TransitionState::FADE_IN;
        levelStartSound.play();
    } else {
        spawnZombies(getZombieCountForLevel(currentLevel, currentRound), sf::Vector2f(400, 300));
    }
}

int LevelManager::getTotalRoundsForLevel(int level) const {
    switch (level) {
        case 1: return 2;
        case 2: return 2;
        case 3: return 2;
        case 4: return 2;
        case 5: return 1; // Single boss fight
        default: return 1;
    }
}

void LevelManager::spawnDoctor(float x, float y) {
    doctor.emplace(x, y);
}

void LevelManager::drawDoctor(sf::RenderWindow& window) {
    if (doctor) {
        doctor->draw(window);
    }
}

bool LevelManager::isPlayerNearDoctor(const Player& player) const {
    if (!doctor) return false;
    
    sf::Vector2f playerPos = player.getPosition();
    sf::Vector2f doctorPos = doctor->getPosition();
    
    float dx = playerPos.x - doctorPos.x;
    float dy = playerPos.y - doctorPos.y;
    float distance = std::sqrt(dx*dx + dy*dy);
    
    return distance < 100.0f;
}

void LevelManager::spawnZombies(int count, sf::Vector2f playerPos) {
    zombiesToSpawn.clear();
    totalZombiesInRound = count;
    zombiesSpawnedInRound = 0;
    zombiesKilledInRound = 0;
    zombieSpawnTimer = 0.0f;
    zombieSpawnInterval = 1.0f;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> sideDist(0, 3); // 0: left, 1: right, 2: top, 3: bottom
    std::uniform_int_distribution<> typeDist(0, 3); // For non-boss levels, up to 4 types

    sf::Vector2f mapSize = getMapSize();
    float spawnMargin = 50.0f;

    if (gameState == GameState::BOSS_FIGHT) {
        totalZombiesInRound = 10; // 1 ZombieKing + 9 other zombies
        float x, y;
        float minDistance = 200.0f;

        // Spawn ZombieKing
        do {
            int side = sideDist(gen);
            switch (side) {
                case 0: x = -spawnMargin; y = std::uniform_real_distribution<>(0, mapSize.y)(gen); break;
                case 1: x = mapSize.x + spawnMargin; y = std::uniform_real_distribution<>(0, mapSize.y)(gen); break;
                case 2: x = std::uniform_real_distribution<>(0, mapSize.x)(gen); y = -spawnMargin; break;
                case 3: x = std::uniform_real_distribution<>(0, mapSize.x)(gen); y = mapSize.y + spawnMargin; break;
            }
            float dx = x - playerPos.x;
            float dy = y - playerPos.y;
            if (std::sqrt(dx*dx + dy*dy) >= minDistance) break;
        } while (true);
        auto king = std::make_unique<ZombieKing>(x, y);
        if (physicsWorld) physicsWorld->addBody(&king->getBody(), false);
        zombiesToSpawn.push_back(std::move(king));

        // Spawn other zombies (2 of each type)
        for (int i = 0; i < 2; ++i) {
            for (int type = 0; type < 4; ++type) {
                do {
                    int side = sideDist(gen);
                    switch (side) {
                        case 0: x = -spawnMargin; y = std::uniform_real_distribution<>(0, mapSize.y)(gen); break;
                        case 1: x = mapSize.x + spawnMargin; y = std::uniform_real_distribution<>(0, mapSize.y)(gen); break;
                        case 2: x = std::uniform_real_distribution<>(0, mapSize.x)(gen); y = -spawnMargin; break;
                        case 3: x = std::uniform_real_distribution<>(0, mapSize.x)(gen); y = mapSize.y + spawnMargin; break;
                    }
                    float dx = x - playerPos.x;
                    float dy = y - playerPos.y;
                    if (std::sqrt(dx*dx + dy*dy) >= minDistance) break;
                } while (true);

                switch (type) {
                case 0: {
                    auto z = std::make_unique<ZombieWalker>(x, y);
                    if (physicsWorld) physicsWorld->addBody(&z->getBody(), false);
                    zombiesToSpawn.push_back(std::move(z));
                    break;
                }
                case 1: {
                    auto z = std::make_unique<ZombieCrawler>(x, y);
                    if (physicsWorld) physicsWorld->addBody(&z->getBody(), false);
                    zombiesToSpawn.push_back(std::move(z));
                    break;
                }
                case 2: {
                    auto z = std::make_unique<ZombieTank>(x, y);
                    if (physicsWorld) physicsWorld->addBody(&z->getBody(), false);
                    zombiesToSpawn.push_back(std::move(z));
                    break;
                }
                case 3: {
                    auto z = std::make_unique<ZombieZoom>(x, y);
                    if (physicsWorld) physicsWorld->addBody(&z->getBody(), false);
                    zombiesToSpawn.push_back(std::move(z));
                    break;
                }
                }

            }
        }
    } else {
        for (int i = 0; i < count; ++i) {
            float x, y;
            float minDistance = 200.0f;

            do {
                int side = sideDist(gen);
                switch (side) {
                    case 0: x = -spawnMargin; y = std::uniform_real_distribution<>(0, mapSize.y)(gen); break;
                    case 1: x = mapSize.x + spawnMargin; y = std::uniform_real_distribution<>(0, mapSize.y)(gen); break;
                    case 2: x = std::uniform_real_distribution<>(0, mapSize.x)(gen); y = -spawnMargin; break;
                    case 3: x = std::uniform_real_distribution<>(0, mapSize.x)(gen); y = mapSize.y + spawnMargin; break;
                }
                float dx = x - playerPos.x;
                float dy = y - playerPos.y;
                if (std::sqrt(dx*dx + dy*dy) >= minDistance) break;
            } while (true);

            if (gameState == GameState::TUTORIAL) {
                auto z = std::make_unique<ZombieWalker>(x, y);
                if (physicsWorld) physicsWorld->addBody(&z->getBody(), false);
                zombiesToSpawn.push_back(std::move(z));
            } else {
                int type = typeDist(gen);
                switch (type % (currentLevel > 0 ? currentLevel : 1)) {
                case 0: {
                    auto z = std::make_unique<ZombieWalker>(x, y);
                    if (physicsWorld) physicsWorld->addBody(&z->getBody(), false);
                    zombiesToSpawn.push_back(std::move(z));
                    break;
                }
                case 1: {
                    if (currentLevel >= 2) {
                        auto z = std::make_unique<ZombieCrawler>(x, y);
                        if (physicsWorld) physicsWorld->addBody(&z->getBody(), false);
                        zombiesToSpawn.push_back(std::move(z));
                    }
                    else {
                        auto z = std::make_unique<ZombieWalker>(x, y);
                        if (physicsWorld) physicsWorld->addBody(&z->getBody(), false);
                        zombiesToSpawn.push_back(std::move(z));
                    }
                    break;
                }
                case 2: {
                    if (currentLevel >= 3) {
                        auto z = std::make_unique<ZombieTank>(x, y);
                        if (physicsWorld) physicsWorld->addBody(&z->getBody(), false);
                        zombiesToSpawn.push_back(std::move(z));
                    }
                    else {
                        auto z = std::make_unique<ZombieWalker>(x, y);
                        if (physicsWorld) physicsWorld->addBody(&z->getBody(), false);
                        zombiesToSpawn.push_back(std::move(z));
                    }
                    break;
                }
                case 3: {
                    if (currentLevel >= 4) {
                        auto z = std::make_unique<ZombieZoom>(x, y);
                        if (physicsWorld) physicsWorld->addBody(&z->getBody(), false);
                        zombiesToSpawn.push_back(std::move(z));
                    }
                    else {
                        auto z = std::make_unique<ZombieWalker>(x, y);
                        if (physicsWorld) physicsWorld->addBody(&z->getBody(), false);
                        zombiesToSpawn.push_back(std::move(z));
                    }
                    break;
                }
                default: {
                    auto z = std::make_unique<ZombieWalker>(x, y);
                    if (physicsWorld) physicsWorld->addBody(&z->getBody(), false);
                    zombiesToSpawn.push_back(std::move(z));
                    break;
                }
                }
            }
        }
    }
    roundStarted = true;
}

void LevelManager::updateZombies(float deltaTime, const Player& player) {
    for (auto& zombie : zombies) {
        zombie->update(deltaTime, player.getPosition());
    }

    auto it = zombies.begin();
    while (it != zombies.end()) {
        bool erased = false;
        if ((*it)->isDead()) {
            physicsWorld->removeBody(&(*it)->getBody()); // Remove physics body before erasing entity
            it = zombies.erase(it);
            erased = true;
            zombiesKilledInRound++;
        } else if (player.isAttacking() && player.getAttackBounds().intersects((*it)->getHitbox())) {
            (*it)->takeDamage(player.getAttackDamage());
            if ((*it)->isDead()) {
                physicsWorld->removeBody(&(*it)->getBody()); // Remove physics body before erasing entity
                it = zombies.erase(it);
                erased = true;
                zombiesKilledInRound++;
            }
        }
        if (!erased) {
            ++it;
        }
    }
}

void LevelManager::drawZombies(sf::RenderWindow& window) const {
    for (const auto& zombie : zombies) {
        zombie->draw(window);
    }
}

std::vector<std::unique_ptr<BaseZombie>>& LevelManager::getZombies() {
    return zombies;
}

void LevelManager::drawHUD(sf::RenderWindow& window, const Player& player) {
    sf::Vector2u windowSize = window.getSize();
    float barWidth = 200;
    float barHeight = 20;
    float padding = 10;

    sf::RectangleShape healthBarBackground;
    healthBarBackground.setSize(sf::Vector2f(barWidth, barHeight));
    healthBarBackground.setFillColor(sf::Color(100, 100, 100, 150));
    healthBarBackground.setPosition(windowSize.x - barWidth - padding, padding);
    
    sf::RectangleShape healthBar;
    float healthPercent = player.getCurrentHealth() / player.getMaxHealth();
    if (healthPercent < 0) healthPercent = 0;
    healthBar.setSize(sf::Vector2f(barWidth * healthPercent, barHeight));
    healthBar.setFillColor(sf::Color::Red);
    healthBar.setPosition(windowSize.x - barWidth - padding, padding);

    window.draw(healthBarBackground);
    window.draw(healthBar);

    sf::RectangleShape staminaBarBackground;
    staminaBarBackground.setSize(sf::Vector2f(barWidth, barHeight));
    staminaBarBackground.setFillColor(sf::Color(100, 100, 100, 150));
    staminaBarBackground.setPosition((windowSize.x - barWidth) / 2.0f, windowSize.y - barHeight - padding);
    
    sf::RectangleShape staminaBar;
    float staminaPercent = player.getCurrentStamina() / player.getMaxStamina();
    if (staminaPercent < 0) staminaPercent = 0;
    staminaBar.setSize(sf::Vector2f(barWidth * staminaPercent, barHeight));
    staminaBar.setFillColor(sf::Color::Green);
    staminaBar.setPosition((windowSize.x - barWidth) / 2.0f, windowSize.y - barHeight - padding);

    window.draw(staminaBarBackground);
    window.draw(staminaBar);

    if (!levelTransitioning || (levelTransitioning && (transitionState == TransitionState::FADE_OUT || transitionState == TransitionState::FADE_IN))) {
        sf::Text levelText;
        levelText.setFont(font);
        levelText.setCharacterSize(18);
        levelText.setPosition(padding, padding + 30);

        sf::Text zombieCountText;
        zombieCountText.setFont(font);
        zombieCountText.setCharacterSize(18);
        zombieCountText.setFillColor(sf::Color::White);
        zombieCountText.setPosition(padding, padding + 50);

        sf::Uint8 alpha = 255;
        if (levelTransitioning) {
            if (transitionState == TransitionState::FADE_IN) {
                alpha = static_cast<sf::Uint8>(255 * (1.0f - (levelTransitionTimer / levelTransitionDuration)));
            } else if (transitionState == TransitionState::FADE_OUT) {
                alpha = static_cast<sf::Uint8>(255 * (levelTransitionTimer / levelTransitionDuration));
            } else {
                alpha = 0;
            }
        }

        levelText.setFillColor(sf::Color(255, 255, 255, alpha));
        zombieCountText.setFillColor(sf::Color(255, 255, 255, alpha));

        if (currentLevel == 0) {
            levelText.setString("Tutorial");
        } else if (gameState == GameState::BOSS_FIGHT) {
            levelText.setString("Boss Level");
        } else {
            levelText.setString("Level " + std::to_string(currentLevel) + " - Round " + std::to_string(currentRound + 1));
        }
        window.draw(levelText);

        zombieCountText.setString("Zombies: " + std::to_string(zombies.size() + zombiesToSpawn.size()));
        window.draw(zombieCountText);
    }
}

void LevelManager::showTutorialDialog(sf::RenderWindow& window) {
    sf::Vector2u windowSize = window.getSize();
    dialogBox.setPosition((windowSize.x - dialogBox.getSize().x) / 2, windowSize.y - dialogBox.getSize().y - 20);
    dialogText.setPosition((dialogBox.getPosition().x + 10), dialogBox.getPosition().y + 10);

    if (currentDialogIndex < tutorialDialogs.size()) {
        dialogText.setString(tutorialDialogs[currentDialogIndex]);
    }

    window.draw(dialogBox);
    window.draw(dialogText);

    if (currentDialogIndex < tutorialDialogs.size()) {
        sf::Text continueText;
        continueText.setFont(font);
        continueText.setCharacterSize(18);
        continueText.setFillColor(sf::Color::Yellow);
        continueText.setString("Press Space to Continue...");
        continueText.setPosition(dialogBox.getPosition().x + dialogBox.getSize().x - continueText.getGlobalBounds().width - 10, dialogBox.getPosition().y + dialogBox.getSize().y - continueText.getGlobalBounds().height - 10);
        window.draw(continueText);
    }
}

void LevelManager::advanceDialog() {
    currentDialogIndex++;
    if (currentDialogIndex < tutorialDialogs.size()) {
        showingDialog = true;
    } else {
        showingDialog = false;
    }
}

int LevelManager::getZombieCountForLevel(int level, int round) {
    switch (level) {
        case 0: return 3; // Tutorial
        case 1: return (5 + round) / 2;
        case 2: return 7 / 2;
        case 3: return 7 / 2;
        case 4: return 7 / 2;
        case 5: return 10; // Boss fight: 1 ZombieKing + 9 others
        default: return 1;
    }
}

sf::Vector2f LevelManager::getMapSize() const {
    switch (currentLevel) {
        case 0:
        case 1:
        case 4:
        case 5:
            return sf::Vector2f(1000, 1250);
        case 2:
            return sf::Vector2f(1200, 1000);
        case 3:
            return sf::Vector2f(1200, 800);
        default:
            return sf::Vector2f(1000, 1250);
    }
}

void LevelManager::setPhysicsWorld(PhysicsWorld* world) {
    physicsWorld = world;
}
