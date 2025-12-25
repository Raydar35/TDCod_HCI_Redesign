#include "LevelManager.h"
#include "PhysicsWorld.h"
#
 // Implement setters declared in header
void LevelManager::setKeyIcon1(const sf::Texture& tex) { keyIcon1 = &tex; }
void LevelManager::setKeyIcon2(const sf::Texture& tex) { keyIcon2 = &tex; }
void LevelManager::setKeyIconEsc(const sf::Texture& tex) { keyIconEsc = &tex; }

#include <memory>
#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <vector>

// Easing helper for smoother (slower at endpoints) animations
static float easeInOutCubic(float t) {
    if (t <= 0.f) return 0.f;
    if (t >= 1.f) return 1.f;
    if (t < 0.5f) return 4.0f * t * t * t;
    float f = -2.0f * t + 2.0f;
    return 1.0f - (f * f * f) / 2.0f;
}

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
        "We need you to buy us some time so we can get the ship back up and running.",
        "If you need any help, you can bring up the menu by pressing ---- . Try it now.",
        "Thats all, good luck..."
    };

    if (!font4.loadFromFile("assets/CallOfDuty.ttf")) {
        std::cerr << "Error loading font for dialog! Trying fallback." << std::endl;
    }
    if (!font2.loadFromFile("assets/SwanseaBold-D0ox.ttf")) {
        std::cerr << "Error loading font for dialog!" << std::endl;
	}
    if (!font3.loadFromFile("assets/Martius.ttf")) {
        std::cerr << "Error loading fallback font for dialog!" << std::endl;
    }

    dialogBox.setSize(sf::Vector2f(600, 150));
    dialogBox.setFillColor(sf::Color(0, 0, 0, 200));
    dialogBox.setOutlineColor(sf::Color::White);
    dialogBox.setOutlineThickness(2);

    dialogText.setFont(font2);
    dialogText.setCharacterSize(20);
    dialogText.setFillColor(sf::Color::White);

    mapBounds.setFillColor(sf::Color::Transparent);
    mapBounds.setOutlineColor(sf::Color::Red);
    mapBounds.setOutlineThickness(2);
    mapBounds.setPosition(0, 0);

    if (!levelStartBuffer.loadFromFile("assets/Audio/wavestart.mp3")) {
        std::cerr << "Error loading level start sound!" << std::endl;
    }
    levelStartSound.setBuffer(levelStartBuffer);

    transitionRect.setFillColor(sf::Color::Black);
    transitionRect.setPosition(0, 0);

    levelStartText.setFont(font4);
    levelStartText.setCharacterSize(48);
    levelStartText.setFillColor(sf::Color::White);
    levelStartText.setPosition(300, 500);

    // FIGURABLE ROUND SETTINGS - edit values here to change per-round zombie behavior
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

    // --- Health damage flash handling (track when player loses health) ---
    float currHealthPercent = 1.0f;
    if (player.getMaxHealth() > 0.0f) currHealthPercent = std::clamp(player.getCurrentHealth() / player.getMaxHealth(), 0.0f, 1.0f);
    // If health dropped since last observed percent, start the damage flash
    if (currHealthPercent < prevFrameHealthPercent - 1e-6f) {
        damageFlashStartPercent = prevFrameHealthPercent;
        damageFlashRemaining = damageFlashDuration;
        damageFlashHoldRemaining = damageFlashHold;
    }
    // Decrement flash timer
    // First consume hold time, then reduce the shrinking timer
    if (damageFlashHoldRemaining > 0.0f) {
        damageFlashHoldRemaining -= deltaTime;
        if (damageFlashHoldRemaining < 0.0f) damageFlashHoldRemaining = 0.0f;
    } else if (damageFlashRemaining > 0.0f) {
        damageFlashRemaining -= deltaTime;
        if (damageFlashRemaining < 0.0f) damageFlashRemaining = 0.0f;
    }
    // Update prevFrameHealthPercent for next frame
    prevFrameHealthPercent = currHealthPercent;

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
        if (roundTransitionTimer >= roundTransitionDuration) {
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

            // The dialog advance requirement for ESC is computed dynamically by LevelManager::isRequireEscToAdvanceDialog()

            if (tutorialZombiesSpawned) updateZombies(deltaTime, player);

            if (currentDialogIndex >= tutorialDialogs.size() && !tutorialZombiesSpawned) {
                // If the tutorial round is configured with zombies, start that round.
                // Otherwise the tutorial is purely dialog-driven and should transition
                // directly to Level 1 (round 1) once the dialog completes.
                if (tutorialConfig.count > 0) {
                    spawnZombies(tutorialConfig.count, player.getPosition());
                    tutorialZombiesSpawned = true;
                } else {
                    // No tutorial zombies: queue a transition to the next level (Level 1)
                    pendingLevelTransition = true;
                    showingDialog = false;
                }
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
                    // advance round index and begin round transition animation
                    currentRound++;
                    // record previous tally so we can animate it moving to center
                    lastTallyIndex = std::clamp(currentRound - 1, 0, 4);
                    inRoundTransition = true;
                    roundTransitionTimer = 0.0f; // reset timer
                    // Play round-start sound when tally transition begins
                    levelStartSound.play();
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
    // Existing dialog handling (unchanged)
    if (showingDialog && currentDialogIndex < tutorialDialogs.size()) {
        dialogText.setFont(this->font2);
        showTutorialDialog(window);
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

    // Draw round tally mark in top-right with cross-fade to upcoming texture during transitions.
    {
        sf::Vector2u windowSize = window.getSize();
        float winW = static_cast<float>(windowSize.x);
        float winH = static_cast<float>(windowSize.y);

        int currIdx = std::clamp(currentRound, 0, 4);
        int prevIdx = lastTallyIndex;

        const sf::Texture* currTex = (currIdx >= 0) ? tallyTextures[currIdx] : nullptr;
        const sf::Texture* prevTex = (prevIdx >= 0) ? tallyTextures[prevIdx] : nullptr;

        // small top-right scale and larger center scale
        const float topRightScale = 0.6f; // smaller when shown in top-right
        const float centerScale = 1.4f;   // scale when centered (reduced from 1.8)
        auto drawTallySprite = [&](const sf::Texture* tex, float alphaMul, float t) {
            if (!tex) return;
            sf::Sprite s; s.setTexture(*tex);
            sf::Vector2u ts = tex->getSize();
            float baseW = static_cast<float>(ts.x);
            float baseH = static_cast<float>(ts.y);
            float margin = 12.f;
            sf::Vector2f topRightPos(winW - margin - baseW, margin);
            sf::Vector2f topMidPos((winW - baseW) * 0.5f, winH * 0.15f);
            // interpolate position and scale based on t (0..1)
            float drawX = topRightPos.x + (topMidPos.x - topRightPos.x) * t;
            float drawY = topRightPos.y + (topMidPos.y - topRightPos.y) * t;
            // scale interpolates between a smaller top-right scale and a larger center scale
            float scale = topRightScale + (centerScale - topRightScale) * t;
            s.setScale(scale, scale);
            s.setPosition(drawX, drawY);
            sf::Color prev = s.getColor();
            // compute alpha and apply only alpha so PNG colors remain intact
            sf::Uint8 a = static_cast<sf::Uint8>(255 * alphaMul);
            s.setColor(sf::Color(255, 255, 255, a));
            window.draw(s);
            s.setColor(prev);
            };

        // If a transition is active we animate t from 0->1 across the levelTransitionDuration
        // For round transitions, use roundTransitionTimer. Otherwise fall back to levelTransitioning.
        bool inTrans = inRoundTransition || (levelTransitioning && (transitionState == TransitionState::FADE_IN || transitionState == TransitionState::SHOW_TEXT || transitionState == TransitionState::FADE_OUT));
        float t = 0.f;
        if (inRoundTransition) {
            t = std::clamp(roundTransitionTimer / std::max(0.0001f, roundTransitionDuration), 0.f, 1.f);
        }
        else if (levelTransitioning) {
            t = std::clamp(levelTransitionTimer / std::max(0.0001f, levelTransitionDuration), 0.f, 1.f);
        }

        // Phase timings (absolute seconds) - increased to slow the animation down
        const float moveInT = 1.0f;      // previous moves to center over 1.0s (was 0.5s)
        const float prevHoldT = 0.6f;    // previous holds at center for 0.6s (was 0.3s)
        const float newFadeInT = 0.4f;   // new fades in at center over 0.4s (was 0.2s)
        const float newHoldT = 2.0f;     // new holds at center for 2.0s (was 1.0s)
        const float moveBackT = 1.0f;    // new shrinks/moves back over 1.0s (was 0.5s)
        // Ensure member duration matches sum so update() uses same value
        float totalDur = moveInT + prevHoldT + newFadeInT + newHoldT + moveBackT;
        // keep roundTransitionDuration in sync
        roundTransitionDuration = totalDur; // sync duration used by update()

        if (inTrans) {
            float rt = t * roundTransitionDuration; // convert normalized t back to seconds
            if (rt < moveInT) {
                float tm = rt / moveInT; // 0..1 moving to center
                float eased = easeInOutCubic(tm);
                const sf::Texture* prevToMove = (lastTallyIndex >= 0) ? tallyTextures[lastTallyIndex] : prevTex;
                drawTallySprite(prevToMove, 1.0f, eased);
            }
            else if (rt < moveInT + prevHoldT) {
                const sf::Texture* prevToMove = (lastTallyIndex >= 0) ? tallyTextures[lastTallyIndex] : prevTex;
                drawTallySprite(prevToMove, 1.0f, 1.0f);
            }
            else if (rt < moveInT + prevHoldT + newFadeInT) {
                float sub = rt - (moveInT + prevHoldT);
                float fadeT = sub / newFadeInT; // 0..1 crossfade
                float easedFade = easeInOutCubic(fadeT);
                const sf::Texture* prevToMove = (lastTallyIndex >= 0) ? tallyTextures[lastTallyIndex] : prevTex;
                drawTallySprite(prevToMove, 1.0f - easedFade, 1.0f);
                drawTallySprite(currTex, easedFade, 1.0f);
            }
            else if (rt < moveInT + prevHoldT + newFadeInT + newHoldT) {
                // new fully visible at center for newHoldT
                drawTallySprite(currTex, 1.0f, 1.0f);
            }
            else {
                // move back phase: new shrinks/moves from center back to top-right
                float sub = rt - (moveInT + prevHoldT + newFadeInT + newHoldT);
                float mbt = std::max(0.0001f, moveBackT);
                float pm = std::clamp(sub / mbt, 0.f, 1.f);
                float easedPM = easeInOutCubic(pm);
                drawTallySprite(currTex, 1.0f, 1.0f - easedPM);
            }

            // finalize when done
            if (inRoundTransition && t >= 1.0f) {
                lastTallyIndex = currIdx;
            }
        }
        else {
            drawTallySprite(currTex, 1.0f, 0.0f);
            lastTallyIndex = currIdx;
        }
    }

    // Single zombie count draw (top-left). Kept here to avoid duplicate draws.
    if (!levelTransitioning || (levelTransitioning && (transitionState == TransitionState::FADE_OUT || transitionState == TransitionState::FADE_IN))) {
        // Only show zombie count in top-left; remove level/round text
        sf::Text zombieCountText;
        zombieCountText.setFont(font4);
        zombieCountText.setCharacterSize(40);
        // use an explicit padding constant for clarity
        const float uiPadding = 10.f;
        zombieCountText.setPosition(uiPadding + 2.f, uiPadding + 6.f);
        // add a black outline so the text is readable over varying backgrounds
        zombieCountText.setOutlineThickness(2.f);

        sf::Uint8 alpha = 255;
        if (levelTransitioning) {
            if (transitionState == TransitionState::FADE_IN) alpha = static_cast<sf::Uint8>(255 * (1.0f - (levelTransitionTimer / levelTransitionDuration)));
            else if (transitionState == TransitionState::FADE_OUT) alpha = static_cast<sf::Uint8>(255 * (levelTransitionTimer / levelTransitionDuration));
            else alpha = 0;
        }

        // apply fill and outline with the current alpha so both fade correctly during transitions
        zombieCountText.setFillColor(sf::Color(255, 255, 255, alpha));
        zombieCountText.setOutlineColor(sf::Color(0, 0, 0, alpha));
        zombieCountText.setString("Zombies Left: " + std::to_string(zombies.size() + zombiesToSpawn.size()));
        window.draw(zombieCountText);
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

void LevelManager::restartCurrentRound(const sf::Vector2f& playerPos) {
        // Clear active zombies & recycle pool indices (same pattern used in loadLevel/reset)
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
    
    // Reset per-round counters but keep currentLevel and currentRound unchanged
    zombiesSpawnedInRound = 0;
    zombiesKilledInRound = 0;
    zombieSpawnTimer = 0.0f;
    roundStarted = false;
    
    // Ensure pool is large enough for this round and requeue spawns for the current round
    ZombieRoundConfig cfg;
    if (gameState == GameState::TUTORIAL) cfg = tutorialConfig;
    else cfg = roundConfigs[std::min(std::max(0, currentRound), 4)];
    ensurePoolSize(cfg.count);
    
    // spawnZombies will read currentRound and pick the correct round config
    spawnZombies(0, playerPos);
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

    // Draw health and stamina bars in bottom-left
    // Longer and skinnier bars
    float hudBarWidth = 340.f;
    float hudBarHeight = 10.f;
    float hudPadding = 20.f;
    float hudSpacing = 8.f;
    // Vertical raise for HUD elements (positive moves them upward)
    float hudVerticalRaise = 20.f; // tweak this value to move bars/panels up/down
    float hudStaminaY = static_cast<float>(windowSize.y) - hudBarHeight - hudPadding - hudVerticalRaise;
    float hudHealthY = hudStaminaY - hudBarHeight - hudSpacing;

    // Container backdrop behind bars: draw a left-opaque -> right-transparent gradient
    float containerPad = 10.f;
    float containerX = hudPadding - containerPad;
    float containerY = hudHealthY - containerPad;
    float containerW = hudBarWidth + containerPad * 2.f;
    float containerH = hudBarHeight * 2.f + hudSpacing + containerPad * 2.f;

    // Make left side start more transparent (lower alpha)
    sf::Color opaqueCol(10,10,10,100);
    sf::Color transparentCol(10,10,10,0);
    // Build two triangles to form a rectangle with horizontal gradient
    sf::VertexArray grad(sf::Triangles, 6);
    // Triangle 1: tl, bl, tr
    grad[0].position = sf::Vector2f(containerX, containerY); grad[0].color = opaqueCol; // top-left
    grad[1].position = sf::Vector2f(containerX, containerY + containerH); grad[1].color = opaqueCol; // bottom-left
    grad[2].position = sf::Vector2f(containerX + containerW, containerY); grad[2].color = transparentCol; // top-right
    // Triangle 2: tr, bl, br
    grad[3].position = sf::Vector2f(containerX + containerW, containerY); grad[3].color = transparentCol; // top-right
    grad[4].position = sf::Vector2f(containerX, containerY + containerH); grad[4].color = opaqueCol; // bottom-left
    grad[5].position = sf::Vector2f(containerX + containerW, containerY + containerH); grad[5].color = transparentCol; // bottom-right

    window.draw(grad);

    // Backgrounds
    sf::RectangleShape healthBg(sf::Vector2f(hudBarWidth, hudBarHeight));
    healthBg.setFillColor(sf::Color(40, 40, 40, 200));
    healthBg.setOutlineColor(sf::Color(100,100,100,180));
    healthBg.setOutlineThickness(1.f);
    healthBg.setPosition(hudPadding, hudHealthY);

    sf::RectangleShape staminaBg(sf::Vector2f(hudBarWidth, hudBarHeight));
    staminaBg.setFillColor(sf::Color(40, 40, 40, 200));
    staminaBg.setOutlineColor(sf::Color(100,100,100,180));
    staminaBg.setOutlineThickness(1.f);
    staminaBg.setPosition(hudPadding, hudStaminaY);

    // Filled bars
    float healthPercent = 0.0f;
    if (player.getMaxHealth() > 0.0f) healthPercent = player.getCurrentHealth() / player.getMaxHealth();
    healthPercent = std::clamp(healthPercent, 0.0f, 1.0f);
    sf::RectangleShape healthFill(sf::Vector2f(hudBarWidth * healthPercent, hudBarHeight));
    healthFill.setFillColor(sf::Color::Green);
    healthFill.setPosition(hudPadding, hudHealthY);

    // Precompute stamina fill so we can draw it after the health flash overlay
    float staminaPercent = 0.0f;
    if (player.getMaxStamina() > 0.0f) staminaPercent = player.getCurrentStamina() / player.getMaxStamina();
    staminaPercent = std::clamp(staminaPercent, 0.0f, 1.0f);
    sf::RectangleShape staminaFill(sf::Vector2f(hudBarWidth * staminaPercent, hudBarHeight));
    staminaFill.setFillColor(sf::Color::White);
    staminaFill.setPosition(hudPadding, hudStaminaY);

    window.draw(healthBg);
    window.draw(healthFill);
    // If a damage flash is active, draw the missing portion as red on top of the health bar
    if ((damageFlashRemaining > 0.0f || damageFlashHoldRemaining > 0.0f) && damageFlashStartPercent > healthPercent) {
        float t = 1.0f;
        if (damageFlashHoldRemaining > 0.0f) {
            // hold phase: t==1.0, flash at full width
            t = 1.0f;
        } else {
            // shrinking phase uses remaining/duration
            t = damageFlashRemaining / damageFlashDuration; // 1->0
        }
        // current flash edge moves from damageFlashStartPercent -> healthPercent as t goes 1->0
        float currentFlashEdge = healthPercent + (damageFlashStartPercent - healthPercent) * t;
        float redWidth = hudBarWidth * (currentFlashEdge - healthPercent);
        if (redWidth > 0.0f) {
            sf::RectangleShape redFill(sf::Vector2f(redWidth, hudBarHeight));
            redFill.setFillColor(sf::Color(200, 40, 40, 220));
            redFill.setPosition(hudPadding + hudBarWidth * healthPercent, hudHealthY);
            window.draw(redFill);
        }
    }
    window.draw(staminaBg);
    window.draw(staminaFill);

    // Two stacked dark transparent panels in bottom-right with padding between them
    {
        const float panelWidth = 240.f;
        const float panelHeight = 64.f;
        const float panelPadding = 16.f; // distance from screen edges
        const float gap = 5.f; // space between stacked panels

        float xTop = static_cast<float>(windowSize.x) - panelPadding - panelWidth;
        // smaller dimensions for bottom panel
        float panelBWidth = panelWidth * 0.82f;
        float panelBHeight = panelHeight * 0.55f;
        float xBottom = static_cast<float>(windowSize.x) - panelPadding - panelBWidth; // smaller than top
        // bottom panel (panel B) anchored to bottom edge
        float yBottom = static_cast<float>(windowSize.y) - panelPadding - panelBHeight - hudVerticalRaise;
        // top panel (panel A) sits above bottom panel with gap
        float yTop = yBottom - gap - panelHeight;

        sf::RectangleShape panelA(sf::Vector2f(panelWidth, panelHeight));
        panelA.setPosition(xTop, yTop);
        panelA.setFillColor(sf::Color(0, 0, 0, 160)); // dark, semi-transparent

        sf::RectangleShape panelB(sf::Vector2f(panelBWidth, panelBHeight));
        panelB.setPosition(xBottom, yBottom);
        panelB.setFillColor(sf::Color(0, 0, 0, 160));

        window.draw(panelA);
        // Draw lower panel so it is visible (must be drawn before top panel outlines)
        window.draw(panelB);

        // Draw weapon icons INSIDE the panels. Draw these before the outlines/bracket lines
        // so the white accent lines remain visible on top.
        const float iconPadding = 2.0f; // small padding inside panel
        // drawIconInPanel now takes an optional size multiplier. It clamps the final scale
        // so the sprite never draws outside the available panel area.
        auto drawIconInPanel = [&](const sf::Texture* tex, float px, float py, float pW, float pH, float mul = 1.0f) {
             if (!tex) return;
             sf::Sprite s; s.setTexture(*tex);
             float texW = static_cast<float>(tex->getSize().x);
             float texH = static_cast<float>(tex->getSize().y);
             float availW = pW - iconPadding * 2.0f;
             float availH = pH - iconPadding * 2.0f;
             if (availW <= 0 || availH <= 0 || texW <= 0 || texH <= 0) return;
             // base scale that fits the texture into the available box
             float baseScale = std::min(availW / texW, availH / texH);
             // apply multiplier but clamp to the maximum that still fits
             float appliedScale = baseScale * mul;
             s.setScale(appliedScale, appliedScale);
             float drawW = texW * appliedScale;
             float drawH = texH * appliedScale;
             // center inside panel area (respect padding)
             float posX = px + (pW - drawW) * 0.5f;
             float posY = py + (pH - drawH) * 0.5f;
             s.setPosition(posX, posY);
             window.draw(s);
         };

        const sf::Texture* topTex = nullptr;
        const sf::Texture* bottomTex = nullptr;
        if (player.getCurrentWeapon() == WeaponType::PISTOL) {
            topTex = pistolIcon;
            bottomTex = rifleIcon;
        } else {
            topTex = rifleIcon;
            bottomTex = pistolIcon;
        }
        // Use per-icon scale multipliers (1.0 = fit to panel). Apply the correct scale
        // to the top (equipped) and bottom (other) icon depending on the player's weapon.
        float topMul = 1.0f;
        float bottomMul = 1.0f;
        if (player.getCurrentWeapon() == WeaponType::PISTOL) {
            // pistol is on top panel
            topMul = pistolTopScale;
            bottomMul = rifleBottomScale;
        } else {
            // rifle is on top panel
            topMul = rifleTopScale;
            bottomMul = pistolBottomScale;
        }

        // Compute debug line X (same fraction used later) so we can center icons between panel left and the line
        const float debugFrac = 0.55f; // same fraction used for TEMP debug line
        float lx = xTop + panelWidth * debugFrac;

        // Draw an icon centered between the left edge of the panel (px) and the vertical line lx.
        auto drawIconCenteredBetween = [&](const sf::Texture* tex, float px, float py, float pW, float pH, float mul, float lineX) {
             if (!tex) return;
             sf::Sprite s; s.setTexture(*tex);
             float texW = static_cast<float>(tex->getSize().x);
             float texH = static_cast<float>(tex->getSize().y);

             const float pad = 2.0f;
             // horizontal slot is from px+pad .. lineX - pad
             float slotLeft = px + pad;
             float slotRight = lineX - pad;
             float slotW = slotRight - slotLeft;
             if (slotW < 8.f) slotW = std::max(8.f, pW - pad*2.f);
             // vertical slot uses panel height
             float slotH = pH - pad * 2.0f;
             if (slotH < 8.f) slotH = pH - pad*2.f;

             // Compute baseScale to fit inside the slot (respecting both width and height)
             float baseScale = 1.0f;
             if (texW > 0 && texH > 0) baseScale = std::min(slotW / texW, slotH / texH);
             float appliedScale = baseScale * mul;
             s.setScale(appliedScale, appliedScale);

             float drawW = texW * appliedScale;
             float drawH = texH * appliedScale;

             // center inside the horizontal slot and vertically inside panel
             float posX = slotLeft + (slotW - drawW) * 0.5f;
             float posY = py + pad + (slotH - drawH) * 0.5f;
             s.setPosition(posX, posY);
             window.draw(s);
         };

        // Draw icons centered between their panel left edge and the debug line lx
        drawIconCenteredBetween(topTex, xTop, yTop, panelWidth, panelHeight, topMul, lx);
        drawIconCenteredBetween(bottomTex, xBottom, yBottom, panelBWidth, panelBHeight, bottomMul, lx);

        // Draw key icon to the left of the bottom panel indicating which key equips the unequipped weapon
        // Draw the PNG at its original pixel size (no scaling). If texture missing, draw nothing.
        {
            WeaponType topW = player.getCurrentWeapon();
            WeaponType bottomW = (topW == WeaponType::PISTOL) ? WeaponType::RIFLE : WeaponType::PISTOL;
            const sf::Texture* ktex = (bottomW == WeaponType::RIFLE) ? keyIcon1 : keyIcon2;
            if (ktex && ktex->getSize().x > 0 && ktex->getSize().y > 0) {
                sf::Sprite ks; ks.setTexture(*ktex);
                // place sprite to the left of the bottom panel with a small gap
                float gapBox = 8.0f;
                // use original texture size in pixels
                sf::Vector2u ts = ktex->getSize();
                float spriteW = static_cast<float>(ts.x);
                float spriteH = static_cast<float>(ts.y);
                // compute position so sprite is vertically centered relative to bottom panel
                float boxX = xBottom - gapBox - spriteW;
                float boxY = yBottom + (panelBHeight - spriteH) * 0.5f;
                ks.setScale(1.0f, 1.0f);
                ks.setPosition(boxX, boxY);
                window.draw(ks);
            }
        }

        // For the top panel, draw the equipped weapon's ammo right-aligned inside the panel
        {
            int topAmmo = player.getCurrentAmmo();
            int topMag = player.getMagazineSize();

            sf::Text leftAmmoTop;
            sf::Text rightAmmoTop;
            leftAmmoTop.setFont(this->font3);
            rightAmmoTop.setFont(this->font3);
            unsigned int topCharSize = static_cast<unsigned int>(std::max(10.f, panelHeight * 0.75f));
            leftAmmoTop.setCharacterSize(topCharSize);
            rightAmmoTop.setCharacterSize(topCharSize);
            leftAmmoTop.setFillColor(sf::Color::White);
            rightAmmoTop.setFillColor(sf::Color(160,160,160));
            leftAmmoTop.setString(std::to_string(topAmmo));
            rightAmmoTop.setString(std::string("/") + std::to_string(topMag));

            sf::FloatRect lbTop = leftAmmoTop.getLocalBounds();
            sf::FloatRect rbTop = rightAmmoTop.getLocalBounds();
            float totalWTop = (lbTop.width + lbTop.left) + (rbTop.width + rbTop.left);
            float textStartXTop = xTop + panelWidth - 10.f - totalWTop; // 10px right padding
            float textYTop = yTop + (panelHeight - (lbTop.height)) * 0.5f - lbTop.top;
            leftAmmoTop.setPosition(textStartXTop - 4.f, textYTop);
            rightAmmoTop.setPosition(textStartXTop + (lbTop.width + lbTop.left), textYTop);
            window.draw(leftAmmoTop);
            window.draw(rightAmmoTop);
        }

        // For the bottom panel, draw the unequipped weapon's ammo right-aligned inside the panel
        {
            WeaponType topW = player.getCurrentWeapon();
            WeaponType bottomW = (topW == WeaponType::PISTOL) ? WeaponType::RIFLE : WeaponType::PISTOL;
            int ammoVal = 0;
            int magVal = 0;
            if (bottomW == WeaponType::PISTOL) {
                ammoVal = player.getPistolAmmoInMag();
                magVal = 12; // pistol mag size
            } else {
                ammoVal = player.getRifleAmmoInMag();
                magVal = 30; // rifle mag size
            }
            std::string ammoStr = std::to_string(ammoVal) + "/" + std::to_string(magVal);

            // Render ammo as two parts: current ammo (white) and "/mag" (grey) so the slash and right numbers are grey
            sf::Text leftAmmoText;
            leftAmmoText.setFont(this->font3);
            sf::Text rightAmmoText;
            rightAmmoText.setFont(this->font3);
            unsigned int charSize = static_cast<unsigned int>(std::max(10.f, panelBHeight * 0.75f));
            leftAmmoText.setCharacterSize(charSize);
            rightAmmoText.setCharacterSize(charSize);
            leftAmmoText.setFillColor(sf::Color::White);
            rightAmmoText.setFillColor(sf::Color(160,160,160)); // grey for slash and mag
            leftAmmoText.setString(std::to_string(ammoVal));
            rightAmmoText.setString(std::string("/") + std::to_string(magVal));

            // Measure both parts to right-align the pair inside the bottom panel
            sf::FloatRect lb = leftAmmoText.getLocalBounds();
            sf::FloatRect rb = rightAmmoText.getLocalBounds();
            float totalW = (lb.width + lb.left) + (rb.width + rb.left);
            float textStartX = xBottom + panelBWidth - 10.f - totalW; // 10px right padding
            float textY = yBottom + (panelBHeight - (lb.height)) * 0.5f - lb.top;
            leftAmmoText.setPosition(textStartX - 4.f, textY);
            rightAmmoText.setPosition(textStartX + (lb.width + lb.left), textY);
            window.draw(leftAmmoText);
            window.draw(rightAmmoText);
         }

        // Solid white side outlines on top panel
         float outlineW = 2.0f;
         sf::RectangleShape leftOutline(sf::Vector2f(outlineW, panelHeight));
         leftOutline.setPosition(xTop, yTop);
         leftOutline.setFillColor(sf::Color::White);
         window.draw(leftOutline);

         sf::RectangleShape rightOutline(sf::Vector2f(outlineW, panelHeight));
         rightOutline.setPosition(xTop + panelWidth - outlineW, yTop);
         rightOutline.setFillColor(sf::Color::White);
         window.draw(rightOutline);

         // Horizontal bracket lines that extend slightly inward from each side
         float horizLen = panelWidth * 0.03f; // how far the bracket extends inward
         float horizTh = outlineW; // thickness matches side outline
         float vertInset = 0.0f; // vertical inset from top/bottom edges

         // Top-left horizontal
         sf::RectangleShape topLeftHor(sf::Vector2f(horizLen, horizTh));
         topLeftHor.setPosition(xTop + outlineW, yTop + vertInset);
         topLeftHor.setFillColor(sf::Color::White);
         window.draw(topLeftHor);

         // Bottom-left horizontal
         sf::RectangleShape bottomLeftHor(sf::Vector2f(horizLen, horizTh));
         bottomLeftHor.setPosition(xTop + outlineW, yTop + panelHeight - vertInset - horizTh);
         bottomLeftHor.setFillColor(sf::Color::White);
         window.draw(bottomLeftHor);

         // Top-right horizontal (extends leftwards)
         sf::RectangleShape topRightHor(sf::Vector2f(horizLen, horizTh));
         topRightHor.setPosition(xTop + panelWidth - outlineW - horizLen, yTop + vertInset);
         topRightHor.setFillColor(sf::Color::White);
         window.draw(topRightHor);

         // Bottom-right horizontal
         sf::RectangleShape bottomRightHor(sf::Vector2f(horizLen, horizTh));
         bottomRightHor.setPosition(xTop + panelWidth - outlineW - horizLen, yTop + panelHeight - vertInset - horizTh);
         bottomRightHor.setFillColor(sf::Color::White);
         window.draw(bottomRightHor);

        // TEMP: draw a vertical debug line at 45% of the top panel width and continue it through the bottom panel
        {
            float frac = 0.55f; // fraction across the top panel where the line should be
            float lx = xTop + panelWidth * frac;
            // start at top panel top, end at bottom panel bottom (covers the gap between panels)
            float yStart = yTop;
            float yEnd = yBottom + panelBHeight;
            sf::VertexArray vline(sf::Lines, 2);
            vline[0].position = sf::Vector2f(lx, yStart);
            vline[1].position = sf::Vector2f(lx, yEnd);
            // Make debug line invisible by using fully transparent color
            sf::Color dbgCol(255, 64, 64, 0);
            vline[0].color = dbgCol;
            vline[1].color = dbgCol;
            window.draw(vline);
        }
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
        float baseLineHeight = dialogText.getCharacterSize() * 1.2f;
        int maxLines = static_cast<int>((dialogBox.getSize().y - padding * 2.0f) / baseLineHeight);
        if (maxLines < 1) maxLines = 1;
        std::string wrapped = wrapText(dialogText, tutorialDialogs[currentDialogIndex], maxTextWidth, maxLines);

        // Draw dialog box and then each line separately with explicit spacing to avoid SFML auto-spacing issues
        window.draw(dialogBox);

        // If dialog contains the special token "----" draw the Esc icon inline.
        const std::string token = "----";
        std::string original = tutorialDialogs[currentDialogIndex];
        bool dialogHasEscToken = (original.find(token) != std::string::npos);
        if (dialogHasEscToken && keyIconEsc && keyIconEsc->getSize().x > 0) {
            // Manual layout: iterate words and place them, substituting icon for token with wrapping.
            float lineSpacingMul = 1.1f;
            float lineHeight = dialogText.getCharacterSize() * lineSpacingMul;
            float startX = dialogBox.getPosition().x + padding;
            float startY = dialogBox.getPosition().y + padding;
            float maxX = dialogBox.getPosition().x + dialogBox.getSize().x - padding;

            sf::Text tmp = dialogText; // prototype for measurements
            float curX = startX;
            float curY = startY;

            std::istringstream ws(original);
            std::string word;
            while (ws >> word) {
                if (word == token) {
                    // measure icon desired size to match text height
                    sf::Vector2u its = keyIconEsc->getSize();
                    float desiredH = dialogText.getCharacterSize();
                    float scale = (its.y > 0) ? (desiredH / static_cast<float>(its.y)) : 1.0f;
                    float iconW = its.x * scale;
                    float iconH = its.y * scale;
                    // wrap if doesn't fit
                    if (curX + iconW > maxX) {
                        curX = startX; curY += lineHeight;
                    }
                    // draw icon
                    sf::Sprite ks; ks.setTexture(*keyIconEsc);
                    ks.setScale(scale, scale);
                    ks.setPosition(curX, curY + (lineHeight - iconH) * 0.5f);
                    window.draw(ks);
                    curX += iconW + tmp.getLetterSpacing();
                } else {
                    // measure word width including a trailing space
                    tmp.setString(word + " ");
                    float w = tmp.getLocalBounds().width;
                    if (curX + w > maxX) {
                        curX = startX; curY += lineHeight;
                    }
                    tmp.setPosition(curX, curY);
                    window.draw(tmp);
                    curX += w;
                }
            }
        } else {
            std::vector<std::string> lines;
            {
                std::istringstream ss(wrapped);
                std::string line;
                while (std::getline(ss, line)) lines.push_back(line);
            }

            // Use slightly larger spacing to prevent overlap
            float lineSpacingMul = 1.1f; // increase to 1.3x char size
            float lineHeight = dialogText.getCharacterSize() * lineSpacingMul;
            float startX = dialogBox.getPosition().x + padding;
            float startY = dialogBox.getPosition().y + padding;

            sf::Text lineText = dialogText; // copy prototype
            for (size_t i = 0; i < lines.size(); ++i) {
                lineText.setString(lines[i]);
                lineText.setPosition(startX, startY + static_cast<float>(i) * lineHeight);
                window.draw(lineText);
            }
        }
        // Always draw the skip/continue prompt at bottom-right inside the dialog box
        sf::Text skipPrompt;
        skipPrompt.setFont(this->font2);
        skipPrompt.setCharacterSize(16);
        skipPrompt.setFillColor(sf::Color(200,200,200));
        if (dialogHasEscToken) skipPrompt.setString("Press ESC to continue...");
        else skipPrompt.setString("Press SPACE to continue...");
        sf::FloatRect spb = skipPrompt.getLocalBounds();
        float spX = dialogBox.getPosition().x + dialogBox.getSize().x - padding - (spb.width + spb.left);
        float spY = dialogBox.getPosition().y + dialogBox.getSize().y - padding - (spb.height + spb.top);
        skipPrompt.setPosition(spX, spY);
        window.draw(skipPrompt);
    } else {
        // still draw empty box if no dialog
        window.draw(dialogBox);
        // draw the prompt even when dialog is empty
        sf::Text skipPrompt;
        skipPrompt.setFont(this->font2);
        skipPrompt.setCharacterSize(16);
        skipPrompt.setFillColor(sf::Color(200,200,200));
        // If the current (empty) dialog index matches token requirement, show ESC; else SPACE
        {
            const std::string token = "----";
            if (tutorialDialogs[currentDialogIndex].find(token) != std::string::npos) skipPrompt.setString("Press ESC to continue...");
            else skipPrompt.setString("Press SPACE to continue...");
        }
        sf::FloatRect spb = skipPrompt.getLocalBounds();
        float padding = 10.0f;
        float spX = dialogBox.getPosition().x + dialogBox.getSize().x - padding - (spb.width + spb.left);
        float spY = dialogBox.getPosition().y + dialogBox.getSize().y - padding - (spb.height + spb.top);
        skipPrompt.setPosition(spX, spY);
        window.draw(skipPrompt);
    }
}

void LevelManager::advanceDialog() { currentDialogIndex++; showingDialog = (currentDialogIndex < tutorialDialogs.size()); }

void LevelManager::notifyPlayerDeath() {
    for (auto zb : zombies) {
        if (zb) zb->onPlayerDeath();
    }
}

// Add missing method implementations
void LevelManager::render(sf::RenderWindow& window) {
    // Render active world entities (zombies). Drawing of map/background is handled by Game.
    draw(window);
}

void LevelManager::setPhysicsWorld(PhysicsWorld* world) {
    physicsWorld = world;
}

void LevelManager::setCameraViewRect(const sf::FloatRect& viewRect) {
    cameraViewRect = viewRect;
}

sf::Vector2f LevelManager::getMapSize() const {
    // Return the current configured map bounds size (fallback to 800x600 if not initialized)
    sf::Vector2f s = mapBounds.getSize();
    if (s.x <= 0.f || s.y <= 0.f) return sf::Vector2f(800.f, 600.f);
    return s;
}

void LevelManager::ensurePoolSize(int desired) {
    if (desired <= 0) return;
    // Reserve more zombies in the pool up to `desired`.
    int current = static_cast<int>(zombiePool.size());
    if (current >= desired) return;
    zombiePool.reserve(desired);
    for (int i = current; i < desired; ++i) {
        // Create at origin; resetForSpawn will position later when activated
        auto z = std::make_unique<ZombieWalker>(0.0f, 0.0f);
        // Assign shadow texture if available
        if (shadowTexture) z->setShadowTexture(*shadowTexture);
        freeZombieIndices.push_back(static_cast<int>(zombiePool.size()));
        poolIndexByPtr[z.get()] = static_cast<int>(zombiePool.size());
        zombiePool.push_back(std::move(z));
        // Immediately mark as free by popping mapping (we only want poolIndexByPtr for active ones)
        poolIndexByPtr.erase(zombiePool.back().get());
    }
}

void LevelManager::initializeDefaultConfigs() {
    // Reasonable defaults for tutorial and rounds
    tutorialConfig.count = 0;
    tutorialConfig.health = 30.0f;
    tutorialConfig.damage = 10.0f;
    tutorialConfig.speed = 50.0f;
    tutorialConfig.animSpeed = 0.12f;

    // Round configs (5 rounds)
    roundConfigs[0].count = 50; roundConfigs[0].health = 40.0f; roundConfigs[0].damage = 18.0f; roundConfigs[0].speed = 80.0f; roundConfigs[0].animSpeed = 0.11f;
    roundConfigs[1].count = 1; roundConfigs[1].health = 40.0f; roundConfigs[1].damage = 20.0f; roundConfigs[1].speed = 90.0f; roundConfigs[1].animSpeed = 0.10f;
    roundConfigs[2].count = 1; roundConfigs[2].health = 40.0f; roundConfigs[2].damage = 22.0f; roundConfigs[2].speed = 92.0f; roundConfigs[2].animSpeed = 0.095f;
    roundConfigs[3].count = 1; roundConfigs[3].health = 50.0f; roundConfigs[3].damage = 25.0f; roundConfigs[3].speed = 95.0f; roundConfigs[3].animSpeed = 0.09f;
    roundConfigs[4].count = 1; roundConfigs[4].health = 60.0f; roundConfigs[4].damage = 30.0f; roundConfigs[4].speed = 100.0f; roundConfigs[4].animSpeed = 0.085f;
}
