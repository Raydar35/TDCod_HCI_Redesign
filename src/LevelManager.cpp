#include "LevelManager.h"
#include "PhysicsWorld.h"
#include <memory>
#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <vector>

// Helper: wrap text to fit within a given width for an sf::Text prototype
static std::string wrapText(const sf::Text& prototype, const std::string& input, float maxWidth, int maxLines = -1) {
    // Preserve existing paragraph breaks
    std::istringstream paragraphStream(input);
    std::string paragraph;
    std::string result;
    bool firstParagraph = true;
    int lineCount = 0;

    while (std::getline(paragraphStream, paragraph)) {
        if (!firstParagraph) {
            // if adding a blank line would exceed maxLines, stop
            if (maxLines >= 0 && lineCount + 1 > maxLines) {
                result += "...";
                return result;
            }
            result += '\n';
            lineCount += 1;
        }
        firstParagraph = false;

        std::istringstream wordStream(paragraph);
        std::string word;
        std::string line;

        while (wordStream >> word) {
            std::string candidate = line.empty() ? word : (line + " " + word);
            sf::Text meas = prototype;
            meas.setString(candidate);
            // getLocalBounds().width gives width without transform
            if (meas.getLocalBounds().width <= maxWidth) {
                line = candidate;
            } else {
                if (!line.empty()) {
                    // current line is full, flush it and start new line with the word
                    if (maxLines >= 0 && lineCount + 1 > maxLines) {
                        // no space for this line -> append ellipsis to previous and return
                        // Trim trailing whitespace
                        while (!result.empty() && (result.back() == '\n' || result.back() == ' ')) result.pop_back();
                        result += "...";
                        return result;
                    }
                    result += line + '\n';
                    lineCount += 1;
                    line = word;
                } else {
                    // single word longer than maxWidth: break the word by characters
                    std::string part;
                    for (char c : word) {
                        std::string test = part + c;
                        meas.setString(test);
                        if (meas.getLocalBounds().width <= maxWidth) {
                            part = test;
                        } else {
                            if (!part.empty()) {
                                if (maxLines >= 0 && lineCount + 1 > maxLines) {
                                    // append ellipsis and return
                                    while (!result.empty() && (result.back() == '\n' || result.back() == ' ')) result.pop_back();
                                    result += "...";
                                    return result;
                                }
                                result += part + '\n';
                                lineCount += 1;
                            }
                            part = std::string(1, c);
                        }
                    }
                    line = part;
                }
            }
        }

        if (!line.empty()) {
            if (maxLines >= 0 && lineCount + 1 > maxLines) {
                while (!result.empty() && (result.back() == '\n' || result.back() == ' ')) result.pop_back();
                result += "...";
                return result;
            }
            result += line;
            lineCount += 1;
        }
    }

    return result;
}

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
        "Hello, this is Randy speaking. As the chief engineer I'll tell you right now, we are going to need some time to repair the ship.",
        "This planet seems to be infested by some sort of half-dead mindless creatures.",
        "Use 'WASD' keys to move around the area and use 'SHIFT' to sprint.",
        "If you come across any of those...",
        "Things...",
        "Shoot first and think later. Use 'LEFT CLICK' to fire your gun, hold 'RIGHT CLICK' to aim, and press 'R' to reload.",
        "Don't forget you can switch to your rifle by pressing '1' and can switch back to your pistol by pressing '2'.",
        "Try to stall for as long as you can, I'll let you know when we are ready to escape. Good luck."
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

    // CONFIGURABLE ROUND SETTINGS - edit values here to change per-round zombie behavior
    initializeDefaultConfigs();

    // Pre-allocate the zombie pool to the maximum count we'll need for any round
    // This avoids large allocations during round start which can cause frame hitches.
    int maxCount = tutorialConfig.count;
    for (const auto& cfg : roundConfigs) maxCount = std::max(maxCount, cfg.count);
    ensurePoolSize(maxCount);
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

    // Debug logging throttle (print once per second)
    static float lmDebugTimer = 0.0f;
    if (debugLogging) {
        lmDebugTimer += deltaTime;
        if (lmDebugTimer >= 1.0f) {
            lmDebugTimer = 0.0f;
            std::cout << "[LevelManager] active=" << zombies.size()
                      << " queued=" << zombiesToSpawn.size()
                      << " poolFree=" << freeZombieIndices.size()
                      << " poolTotal=" << zombiePool.size()
                      << " level=" << currentLevel
                      << " round=" << currentRound
                      << " pendingTransition=" << (pendingLevelTransition ? 1 : 0)
                      << " inRoundTransition=" << (inRoundTransition ? 1 : 0)
                      << " levelTransitioning=" << (levelTransitioning ? 1 : 0)
                      << std::endl;
            if (physicsWorld) {
                std::cout << "             physics dynamic=" << physicsWorld->getDynamicBodyCount()
                          << " static=" << physicsWorld->getStaticBodyCount()
                          << " lastChecks=" << physicsWorld->getLastCollisionChecks() << std::endl;
            }
        }
    }

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
                // spawn using configured counts for current gameState
                spawnZombies(0, player.getPosition());
            }
        }
        return;
    }

    if (inRoundTransition) {
        roundTransitionTimer += deltaTime;
        if (roundTransitionTimer >= 2.0f) {
            inRoundTransition = false;
            roundTransitionTimer = 0.0f;
            spawnZombies(0, player.getPosition());
        }
        return;
    }

    // Move queued zombies into the active list at intervals.
    if (zombiesSpawnedInRound < totalZombiesInRound && !zombiesToSpawn.empty()) {
        // Activate a limited number of queued zombies per frame to smooth CPU cost.
        // We still respect zombieSpawnInterval for pacing but also allow a small
        // burst if the queue grows large.
        zombieSpawnTimer += deltaTime;
        // Dynamic activation rate: increase activations when queue is large
        int dynamicActivate = 1;
        // Increase activation if queue is large
        size_t queued = zombiesToSpawn.size();
        if (queued > 30) dynamicActivate = 3;
        else if (queued > 15) dynamicActivate = 2;

        // Respect pacing interval
        if (zombieSpawnTimer >= zombieSpawnInterval) {
            // activate up to dynamicActivate (controlled pacing)
            activateQueuedZombies(dynamicActivate, player.getPosition());
            zombieSpawnTimer = 0.0f;
        }
    }

    switch (gameState) {
        case GameState::TUTORIAL:
            if (!tutorialWeaponForced) {
                player.setWeapon(WeaponType::PISTOL);
                tutorialWeaponForced = true;
            }
            if (!showingDialog) showingDialog = true;

            if (tutorialZombiesSpawned) updateZombies(deltaTime, player);

            if (currentDialogIndex >= tutorialDialogs.size() && !tutorialZombiesSpawned) {
                // Start the tutorial round using the configured tutorialConfig values
                spawnZombies(tutorialConfig.count, player.getPosition());
                tutorialZombiesSpawned = true;
            }

            if (zombiesKilledInRound == totalZombiesInRound && tutorialZombiesSpawned) {
                tutorialComplete = true;
                pendingLevelTransition = true;
                showingDialog = false;
                // Do not force the player's weapon when the tutorial ends --- keep whatever
                // the player currently has selected so they aren't forced back to pistol.
            }
            break;

        case GameState::LEVEL1:
        case GameState::LEVEL2:
        case GameState::LEVEL3:
        case GameState::LEVEL4:
        case GameState::LEVEL5:
            updateZombies(deltaTime, player);

            if (roundStarted && zombiesKilledInRound >= totalZombiesInRound && zombiesSpawnedInRound >= totalZombiesInRound && !inRoundTransition && !levelTransitioning && transitionState == TransitionState::NONE) {
                if (currentRound < 4) {
                    currentRound++;
                    inRoundTransition = true;
                    roundStarted = false;
                } else {
                    setGameState(GameState::VICTORY);
                }
            }
            break;

        case GameState::BOSS_FIGHT:
            updateZombies(deltaTime, player);
            if (zombiesKilledInRound == totalZombiesInRound && zombiesSpawnedInRound == totalZombiesInRound) setGameState(GameState::VICTORY);
            break;

        case GameState::GAME_OVER:
        case GameState::VICTORY:
            break;
    }
}

void LevelManager::draw(sf::RenderWindow& window) {
    drawZombies(window);
}

void LevelManager::renderBullets(sf::RenderWindow& window, std::vector<std::unique_ptr<Bullet>>& bullets, float renderAlpha) {
    for (auto& b : bullets) {
        b->setRenderAlpha(renderAlpha);
        b->render(window);
    }
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

    // Ensure transition rectangle matches the actual UI/window pixel size (not the world/map size)
    if (levelTransitioning) {
        sf::Vector2u winSize = window.getSize();
        transitionRect.setSize(sf::Vector2f(static_cast<float>(winSize.x), static_cast<float>(winSize.y)));
        transitionRect.setPosition(0.f, 0.f);
        window.draw(transitionRect);
    }

    if (levelTransitioning && (transitionState == TransitionState::FADE_IN || transitionState == TransitionState::SHOW_TEXT)) {
        std::string levelTextString;
        if (currentLevel >= 1 && currentLevel <= 4) levelTextString = "Level " + std::to_string(currentLevel) + " Starting";
        else if (gameState == GameState::BOSS_FIGHT) levelTextString = "Boss Fight: Zombie King";
        else levelTextString = "Starting Level";

        levelStartText.setString(levelTextString);
        sf::Vector2u windowSize = window.getSize();
        levelStartText.setPosition((windowSize.x - levelStartText.getGlobalBounds().width) / 2, windowSize.y / 2 - 50);
        window.draw(levelStartText);
    }
}

void LevelManager::nextLevel() {
    int nextLevelNumber = currentLevel + 1;
    if (nextLevelNumber > 5) setGameState(GameState::VICTORY);
    else loadLevel(nextLevelNumber);
}

void LevelManager::reset() {
    currentLevel = 0;
    currentRound = 0;

    // Unregister any active zombies' physics bodies and recycle their pool indices
    for (auto zb : zombies) {
        if (physicsWorld) physicsWorld->removeBody(&zb->getBody());
        auto mit = poolIndexByPtr.find(zb);
        if (mit != poolIndexByPtr.end()) {
            freeZombieIndices.push_back(mit->second);
            poolIndexByPtr.erase(mit);
        }
    }

    zombies.clear();
    zombiesToSpawn.clear();
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

GameState LevelManager::getCurrentState() const { return gameState; }

void LevelManager::setGameState(GameState state) { gameState = state; }

void LevelManager::startTutorial() {
    currentLevel = 0;
    currentRound = 0;
    tutorialComplete = false;
    currentDialogIndex = 0;
    showingDialog = true;
    tutorialWeaponForced = false;
    // Ensure tutorial uses the default round configs (in case they were modified at runtime)
    initializeDefaultConfigs();
    // Pre-allocate pool for expected tutorial zombies
    ensurePoolSize(tutorialConfig.count);
}

bool LevelManager::isTutorialComplete() const { return tutorialComplete; }

int LevelManager::getCurrentLevel() const { return currentLevel; }
int LevelManager::getCurrentRound() const { return currentRound; }
int LevelManager::getPreviousLevel() const { return previousLevel; }

void LevelManager::loadLevel(int levelNumber) {
    previousLevel = currentLevel;
    currentLevel = levelNumber;
    currentRound = 0;

    // Unregister any active zombies' physics bodies and recycle their pool indices
    for (auto zb : zombies) {
        if (physicsWorld) physicsWorld->removeBody(&zb->getBody());
        auto mit = poolIndexByPtr.find(zb);
        if (mit != poolIndexByPtr.end()) {
            freeZombieIndices.push_back(mit->second);
            poolIndexByPtr.erase(mit);
        }
    }
    zombies.clear();
    zombiesToSpawn.clear();
    totalZombiesInRound = 0;
    zombiesSpawnedInRound = 0;
    zombiesKilledInRound = 0;
    roundStarted = false;

    bool triggerTransition = false;
    if ((previousLevel >= 1 && previousLevel <= 3) || (previousLevel == 4 && currentLevel == 5)) triggerTransition = true;

    if (triggerTransition) {
        levelTransitioning = true;
        levelTransitionTimer = 0.0f;
        transitionState = TransitionState::FADE_IN;
    }
    // Ensure gameState reflects the level being loaded so spawnZombies picks the right config
    if (levelNumber >= 1 && levelNumber <= 5) {
        // Map 1..5 to LEVEL1..LEVEL5
        gameState = static_cast<GameState>(static_cast<int>(GameState::LEVEL1) + (levelNumber - 1));
    } else if (levelNumber == 0) {
        gameState = GameState::TUTORIAL;
    }
    if (!triggerTransition) {
        spawnZombies(0, sf::Vector2f(400, 300));
    }
    else {
        levelStartSound.play();
    }
}

int LevelManager::getTotalRoundsForLevel(int level) const {
    switch (level) {
        case 1: return 2;
        case 2: return 2;
        case 3: return 2;
        case 4: return 2;
        case 5: return 1;
        default: return 1;
    }
}

void LevelManager::spawnZombies(int count, sf::Vector2f playerPos) {
    // Prevent duplicate spawn requests if this round was already started
    if (roundStarted) return;

    zombiesToSpawn.clear();
    totalZombiesInRound = count;
    zombiesSpawnedInRound = 0;
    zombiesKilledInRound = 0;
    zombieSpawnTimer = 0.0f;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> sideDist(0, 3);

    sf::Vector2f mapSize = getMapSize();
    float spawnMargin = 8.0f;

    const float viewBuffer = 50.0f;
    const float minDistance = 200.0f;
    const int maxAttemptsPerZombie = 18;

    ZombieRoundConfig cfg;
    if (gameState == GameState::TUTORIAL) cfg = tutorialConfig;
    else cfg = roundConfigs[std::min(std::max(0, currentRound), 4)];

    int spawnCount = cfg.count;
    totalZombiesInRound = spawnCount;

    // Ensure pool can hold spawnCount zombies (simple heuristic)
    ensurePoolSize(spawnCount);

    // We'll queue spawn positions and create/activate only a few per frame to avoid stalls.
    // Store spawn positions temporarily in zombiesToSpawn as actual zombie objects (deferred activation)
    for (int i = 0; i < spawnCount; ++i) {
        bool placed = false;
        for (int attempt = 0; attempt < maxAttemptsPerZombie; ++attempt) {
            // Sample a position around the player in polar coordinates so zombies surround the player
            float sx = 0.0f, sy = 0.0f;
            // Choose ring radius so spawns are off-screen: compute minimum radius to be outside camera rectangle
            float halfW = cameraViewRect.width * 0.5f;
            float halfH = cameraViewRect.height * 0.5f;
            // distance from camera center to its corner (half-diagonal)
            float camHalfDiag = std::sqrt(halfW*halfW + halfH*halfH);
            float buffer = 32.0f; // extra buffer so spawns are comfortably off-screen
            float camMinR = camHalfDiag + buffer;
            float camMax = std::max(cameraViewRect.width, cameraViewRect.height);
            float minR = std::max(minDistance, camMinR);
            float maxR = minR + camMax * 0.8f; // allow some spread further out
            std::uniform_real_distribution<float> angDist(0.0f, 2.0f * 3.14159265f);
            std::uniform_real_distribution<float> radDist(minR, maxR);
            float ang = angDist(gen);
            float r = radDist(gen);
            sx = playerPos.x + std::cos(ang) * r;
            sy = playerPos.y + std::sin(ang) * r;

            // Reject samples that land inside the current camera view (spawn must be off-screen)
            if (cameraViewRect.contains(sx, sy)) {
                continue; // try another sample
            }

            // Allow spawns outside the map bounds so zombies can enter from off-map.
            // Do not clamp or reject these samples; they will walk in toward the player.

            // Create zombie but *don't* add to physics world yet. We'll add bodies gradually
            LevelManager::SpawnRequest req; req.x = sx; req.y = sy; req.health = cfg.health; req.damage = cfg.damage; req.speed = cfg.speed;
            // compute per-round animation speed: base cfg.animSpeed minus 0.01 per round index (faster each round)
            float baseAnim = cfg.animSpeed;
            int roundIndex = std::min(std::max(0, currentRound), 4);
            float animForSpawn = std::max(0.01f, baseAnim - 0.01f * static_cast<float>(roundIndex));
            req.animSpeed = animForSpawn;
            zombiesToSpawn.push_back(req);
            placed = true;
            break;
        }

        if (!placed) {
            float x, y;
            int attempts = 0;
            do {
                int side = sideDist(gen);
                switch (side) {
                    case 0: x = -spawnMargin; y = std::uniform_real_distribution<>(0, mapSize.y)(gen); break;
                    case 1: x = mapSize.x + spawnMargin; y = std::uniform_real_distribution<>(0, mapSize.y)(gen); break;
                    case 2: x = std::uniform_real_distribution<>(0, mapSize.x)(gen); y = -spawnMargin; break;
                    default: x = std::uniform_real_distribution<>(0, mapSize.x)(gen); y = mapSize.y + spawnMargin; break;
                }
                float dx = x - playerPos.x;
                float dy = y - playerPos.y;
                if (std::sqrt(dx*dx + dy*dy) >= minDistance) break;
                attempts++;
            } while (attempts < 8);
            LevelManager::SpawnRequest req; req.x = x; req.y = y; req.health = cfg.health; req.damage = cfg.damage; req.speed = cfg.speed;
            float baseAnim = cfg.animSpeed;
            int roundIndex = std::min(std::max(0, currentRound), 4);
            float animForSpawn = std::max(0.01f, baseAnim - 0.01f * static_cast<float>(roundIndex));
            req.animSpeed = animForSpawn;
            zombiesToSpawn.push_back(req);
        }
    }
    roundStarted = true;
 }

// New: activate a small number of queued zombies per frame to avoid large spikes
void LevelManager::activateQueuedZombies(int maxToActivate, const sf::Vector2f& playerPos) {

    int activated = 0;
    // We'll prefer to activate zombies that are nearer to the player first so expensive physics
    // bodies are only created when the zombie is potentially relevant. This reduces CPU & cache
    // pressure when large numbers of zombies are queued far away.
    
    // Simple approach: scan queue up to a limit, find candidates within a threshold distance and activate them.
    const float immediateActivateDist = 1200.0f; // world units
    const int scanLimit = std::min((size_t)50, zombiesToSpawn.size());

    // Try to activate up to maxToActivate using proximity heuristic
    for (int pass = 0; pass < 2 && activated < maxToActivate; ++pass) {
        // pass 0: prefer closer than immediateActivateDist
        // pass 1: fallback to activating any remaining queued
        for (int i = 0; i < scanLimit && activated < maxToActivate && !zombiesToSpawn.empty(); ++i) {
            // Access front element cheaply
            LevelManager::SpawnRequest req = zombiesToSpawn.front();
            // For pass 0, if too far skip by rotating queue
            float dx = req.x - playerPos.x;
            float dy = req.y - playerPos.y;
            float dist2 = dx*dx + dy*dy;
            bool shouldActivate = (pass == 1) || (dist2 <= immediateActivateDist * immediateActivateDist);

            if (!shouldActivate) {
                // rotate to back to avoid repeated checks next frame
                zombiesToSpawn.pop_front();
                zombiesToSpawn.push_back(req);
                continue;
            }

            // Activate this one
            zombiesToSpawn.pop_front();
            if (freeZombieIndices.empty()) break;
            int poolIdx = freeZombieIndices.front();
            freeZombieIndices.pop_front();
            ZombieWalker* z = zombiePool[poolIdx].get();

            // Reset zombie via public API
            z->resetForSpawn(req.x, req.y, req.health, req.damage, req.speed);
            // Apply per-spawn animation speed if supported (computed when queued)
            z->setWalkFrameTime(req.animSpeed);
            if (shadowTexture) z->setShadowTexture(*shadowTexture);

            // Register in physics world now
            if (physicsWorld) physicsWorld->addBody(&z->getBody(), false);

            // Add pointer into active list and index mapping
            zombies.push_back(z);
            poolIndexByPtr[z] = poolIdx;

            zombiesSpawnedInRound++;
            activated++;
        }
    }
}

void LevelManager::updateZombies(float deltaTime, const Player& player) {
    // Update active zombies
    // Use index-based loop since zombies vector may be modified during iteration
    for (size_t i = 0; i < zombies.size(); ++i) {
        zombies[i]->update(deltaTime, player.getPhysicsPosition());
    }

    // Remove dead zombies and recycle them back into the pool
    auto it = zombies.begin();
    while (it != zombies.end()) {
        BaseZombie* zb = *it;
        bool erased = false;
        if (zb->isDead()) {
            // remove physics body
            if (physicsWorld) physicsWorld->removeBody(&zb->getBody());
            // find pool index for this pointer
            auto mit = poolIndexByPtr.find(zb);
            if (mit != poolIndexByPtr.end()) {
                int idx = mit->second;
                poolIndexByPtr.erase(mit);
                freeZombieIndices.push_back(idx);
                // Optionally reset zombie visuals here
            }
            it = zombies.erase(it);
            erased = true;
            zombiesKilledInRound++;
        } else if (player.isAttacking() && player.getAttackBounds().intersects(zb->getHitbox())) {
            zb->takeDamage(player.getAttackDamage());
            if (zb->isDead()) {
                if (physicsWorld) physicsWorld->removeBody(&zb->getBody());
                auto mit = poolIndexByPtr.find(zb);
                if (mit != poolIndexByPtr.end()) {
                    int idx = mit->second;
                    poolIndexByPtr.erase(mit);
                    freeZombieIndices.push_back(idx);
                }
                it = zombies.erase(it);
                erased = true;
                zombiesKilledInRound++;
            }
        }
        if (!erased) ++it;
    }
}

void LevelManager::drawZombies(sf::RenderWindow& window) const {
    for (const auto& zombie : zombies) zombie->draw(window);
}

std::vector<BaseZombie*>& LevelManager::getZombies() { return zombies; }

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
    float healthPercent = 0.0f;
    if (player.getMaxHealth() > 0.0f) healthPercent = player.getCurrentHealth() / player.getMaxHealth();
    healthPercent = std::clamp(healthPercent, 0.0f, 1.0f);
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

    sf::Text ammoText;
    ammoText.setFont(font);
    ammoText.setCharacterSize(18);
    ammoText.setFillColor(sf::Color::White);
    int currAmmo = player.getCurrentAmmo();
    int magSize = player.getMagazineSize();
    ammoText.setString("Ammo: " + std::to_string(currAmmo) + " / " + std::to_string(magSize));
    ammoText.setPosition(windowSize.x - magSize * 6 - 150, padding + 50);
    window.draw(ammoText);

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
            if (transitionState == TransitionState::FADE_IN) alpha = static_cast<sf::Uint8>(255 * (1.0f - (levelTransitionTimer / levelTransitionDuration)));
            else if (transitionState == TransitionState::FADE_OUT) alpha = static_cast<sf::Uint8>(255 * (levelTransitionTimer / levelTransitionDuration));
            else alpha = 0;
        }

        levelText.setFillColor(sf::Color(255, 255, 255, alpha));
        zombieCountText.setFillColor(sf::Color(255, 255, 255, alpha));

        if (currentLevel == 0) levelText.setString("Tutorial");
        else if (gameState == GameState::BOSS_FIGHT) levelText.setString("Boss Level");
        else levelText.setString("Level " + std::to_string(currentLevel) + " - Round " + std::to_string(currentRound + 1));
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
        // Wrap the dialog to fit inside the dialog box (accounting for padding)
        float padding = 10.0f;
        float maxTextWidth = dialogBox.getSize().x - padding * 2.0f;
        // Estimate max lines based on dialog box height and line spacing (character size * 1.2)
        float lineHeight = dialogText.getCharacterSize() * 1.2f;
        int maxLines = static_cast<int>((dialogBox.getSize().y - padding * 2.0f) / lineHeight);
        if (maxLines < 1) maxLines = 1;
        std::string wrapped = wrapText(dialogText, tutorialDialogs[currentDialogIndex], maxTextWidth, maxLines);
        dialogText.setString(wrapped);
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

void LevelManager::advanceDialog() { currentDialogIndex++; showingDialog = (currentDialogIndex < tutorialDialogs.size()); }

int LevelManager::getZombieCountForLevel(int level, int round) {
    if (level == 0) return 3;
    switch (round) {
        case 0: return 5;
        case 1: return 7;
        case 2: return 9;
        case 3: return 11;
        case 4: return 13;
        default: return 5;
    }
}

sf::Vector2f LevelManager::getMapSize() const { return sf::Vector2f(2048.f, 2048.f); }

void LevelManager::setPhysicsWorld(PhysicsWorld* world) { physicsWorld = world; }
void LevelManager::setCameraViewRect(const sf::FloatRect& viewRect) { cameraViewRect = viewRect; }
void LevelManager::render(sf::RenderWindow& window) { draw(window); }

void LevelManager::initializeDefaultConfigs() {
    tutorialConfig = ZombieRoundConfig{1, 40.0f, 10.0f, 55.0f, 0.12f};
    roundConfigs[0] = ZombieRoundConfig{20, 40.0f, 18.0f, 80.0f, 0.11f};
    roundConfigs[1] = ZombieRoundConfig{1, 40.0f, 20.0f, 90.0f, 0.10f};
    roundConfigs[2] = ZombieRoundConfig{1, 40.0f, 22.0f, 92.0f, 0.095f};
    roundConfigs[3] = ZombieRoundConfig{1, 50.0f, 25.0f, 95.0f, 0.09f};
    roundConfigs[4] = ZombieRoundConfig{1, 60.0f, 30.0f, 100.0f, 0.085f};
}

void LevelManager::ensurePoolSize(int desired) {
    if (desired <= (int)zombiePool.size()) return;
    int start = (int)zombiePool.size();
    for (int i = start; i < desired; ++i) {
        // create with default values; will be reset on activation
        auto z = std::make_unique<ZombieWalker>(0.0f, 0.0f, tutorialConfig.health, tutorialConfig.damage, tutorialConfig.speed);
        zombiePool.push_back(std::move(z));
        freeZombieIndices.push_back(i);
    }
}
