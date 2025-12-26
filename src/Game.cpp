#include "Game.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <cmath>
#include <chrono>

// Forward declaration for OBB vs AABB helper used by collision checks
static bool obbIntersectsAabb(const sf::Vector2f& c, const sf::Vector2f& he, float rotDeg, const sf::FloatRect& aabb);

Game::Game()
    : window(),
    player(Vec2(400, 300)),
    points(0),
    zombiesToNextLevel(15)
{
    // Create fullscreen window using desktop resolution but keep a fixed
    // logical game size (800x600). Use view viewport to letterbox/pillarbox
    // so content isn't stretched.
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    window.create(desktop, "Echoes of Valkyrie", sf::Style::Fullscreen);
    window.setFramerateLimit(60);

    // Set the game view to match the desktop/fullscreen resolution so the
    // render target matches the window pixel size. This will stretch sprites
    // to fill the screen but matches the user's native resolution as requested.
    float desktopW = static_cast<float>(desktop.width);
    float desktopH = static_cast<float>(desktop.height);
    gameView.setSize(desktopW, desktopH);
    gameView.setCenter(desktopW * 0.5f, desktopH * 0.5f);
    // Use full viewport
    gameView.setViewport(sf::FloatRect(0.f, 0.f, 1.f, 1.f));

    // Zoom the camera in slightly (factor < 1.0f zooms in). Adjust to taste.
    const float CAMERA_ZOOM = 0.85f; // 0.85 -> ~15% closer
    gameView.zoom(CAMERA_ZOOM);

    background.setFillColor(sf::Color(50, 50, 50));
    background.setSize(sf::Vector2f(2048.f, 2048.f)); // Set to map size (pixels)

    if (!mapTexture1.loadFromFile("assets/Map/ground1.jpg")) {
        std::cerr << "Error loading map1 texture!" << std::endl;
    } else {
        // This texture is a single tile (512x512). Enable repeating and
        // instruct the sprite to cover the full map area (2048x2048).
        mapTexture1.setRepeated(true);
        mapSprite1.setTexture(mapTexture1);
        mapSprite1.setTextureRect(sf::IntRect(0, 0, 2048, 2048));
    }
    
    if (!font.loadFromFile("assets/Call of Ops Duty.otf")) {
        std::cerr << "Error loading font 'assets/Call of Ops Duty.otf'! Trying fallback." << std::endl;
    }
    
    pointsText.setFont(font);
    pointsText.setCharacterSize(24);
    pointsText.setFillColor(sf::Color::White);
    pointsText.setPosition(10, 10);
    
    physics.addBody(&player.getBody(), false);

    levelManager.setPhysicsWorld(&physics);
    physics.setDebugLogging(false);
    levelManager.setDebugLogging(false);
    levelManager.initialize();

    // Place player at map center so they don't spawn in the top-right corner
    {
        int startLevel = levelManager.getCurrentLevel();
        sf::Vector2u mapSize = getMapSize(startLevel);
        player.setPosition(static_cast<float>(mapSize.x) / 2.0f, static_cast<float>(mapSize.y) / 2.0f);
    }

    if (!backgroundMusic.openFromFile("assets/Audio/atmosphere.mp3")) {
        std::cerr << "Error loading background music! Path: assets/Audio/atmosphere.mp3" << std::endl;
    } else {
        backgroundMusic.setVolume(15);
    }

    if (!zombieBiteBuffer.loadFromFile("assets/Audio/zombie_bite.mp3")) {
        std::cerr << "Error loading zombie bite sound!" << std::endl;
    } else {
        zombieBiteSound.setBuffer(zombieBiteBuffer);
    }

    if (!Bullet::bulletTexture.loadFromFile("assets/Top_Down_Survivor/bullet.png")) {
        std::cerr << "Failed to load bullet texture!" << std::endl;
    }

    // Load muzzle flash texture (provide your own path here)
    if (!muzzleFlashTexture.loadFromFile("assets/Top_Down_Survivor/muzzle_flash.png")) {
        std::cerr << "Could not load muzzle flash texture (placeholder path)" << std::endl;
    } else {
        player.setMuzzleTexture(muzzleFlashTexture);
    }

    // Load shadow texture and pass to player and LevelManager
    if (!shadowTexture.loadFromFile("assets/shadow.png")) {
        std::cerr << "Warning: could not load shadow texture" << std::endl;
    } else {
        player.setShadowTexture(shadowTexture);
        levelManager.setShadowTexture(shadowTexture);
    }

    // Load weapon and walking sounds here and pass to Player
    sf::SoundBuffer pistolBuf, rifleBuf, pistolReloadBuf, rifleReloadBuf;
    if (!pistolBuf.loadFromFile("assets/Audio/pistol_shot.mp3")) {
        std::cerr << "Warning: could not load pistol sound" << std::endl;
    } else player.setPistolSoundBuffer(pistolBuf);
    if (!rifleBuf.loadFromFile("assets/Audio/rifle_shot.mp3")) {
        std::cerr << "Warning: could not load rifle sound" << std::endl;
    } else player.setRifleSoundBuffer(rifleBuf);

    // Load two footstep sounds and pass to player (alternate between them)
    {
        std::array<std::string,2> stepPaths = {
            "assets/Audio/steps/step1.mp3",
            "assets/Audio/steps/step2.mp3"
        };

        std::vector<sf::SoundBuffer> stepBufs;
        stepBufs.reserve(stepPaths.size());
        for (const auto &p : stepPaths) {
            sf::SoundBuffer b;
            if (!b.loadFromFile(p)) {
                std::cerr << "Warning: could not load step sound: " << p << std::endl;
            } else {
                stepBufs.push_back(std::move(b));
            }
        }
        if (!stepBufs.empty()) {
            player.setStepSoundBuffers(stepBufs);
        }
    }

    // Reload sounds for weapons
    if (!pistolReloadBuf.loadFromFile("assets/Audio/pistol_reload.mp3")) {
        std::cerr << "Warning: could not load pistol reload sound" << std::endl;
    } else player.setPistolReloadSoundBuffer(pistolReloadBuf);

    if (!rifleReloadBuf.loadFromFile("assets/Audio/rifle_reload.wav")) {
        std::cerr << "Warning: could not load rifle reload sound" << std::endl;
    } else player.setRifleReloadSoundBuffer(rifleReloadBuf);
    
    // --- New: attempt to load five separate feet spritesheet and configure frames ---
    // Placeholder paths -- replace with real ones.
    std::array<std::string,5> feetPaths = {
        "assets/Top_Down_Survivor/feet_idle.png",
        "assets/Top_Down_Survivor/feet_walk.png",
        "assets/Top_Down_Survivor/feet_run.png",
        "assets/Top_Down_Survivor/feet_strafe_left.png",
        "assets/Top_Down_Survivor/feet_strafe_right.png"
    };

    // Example frame sizes (update to your real sizes)
    int idleW = 132, idleH = 155; // idle single image
    int walkW = 172, walkH = 124;
    int runW = 204, runH = 124;
    // Feet strafes may be different sizes; set left/right explicitly
    int strafeLeftW = 155, strafeLeftH = 174;
    int strafeRightW = 154, strafeRightH = 176; // update if different

    // pack into arrays by FeetState order: IDLE, WALK, RUN, STRAFE_LEFT, STRAFE_RIGHT
    std::array<int,5> frameW = { idleW, walkW, runW, strafeLeftW, strafeRightW };
    std::array<int,5> frameH = { idleH, walkH, runH, strafeLeftH, strafeRightH };

    // Load each and set state sheets (auto-slice full grid; no framesPerRow overrides)
    for (int s = 0; s < 5; ++s) {
        if (playerFeetStateTextures[s].loadFromFile(feetPaths[s])) {
            std::vector<sf::IntRect> frames;
            int w = frameW[s];
            int h = frameH[s];
            if (s == static_cast<int>(FeetState::IDLE)) {
                // single frame
                frames.emplace_back(0,0,w,h);
                player.setFeetStateSheet(FeetState::IDLE, playerFeetStateTextures[s], frames, 0.1f);
            } else {
                // auto-calc cols/rows from texture size and slice all rows
                sf::Vector2u ts = playerFeetStateTextures[s].getSize();
                int cols = (w > 0) ? (ts.x / w) : 0;
                int rows = (h > 0) ? (ts.y / h) : 0;
                for (int r = 0; r < rows; ++r) {
                    for (int c = 0; c < cols; ++c) {
                        frames.emplace_back(c * w, r * h, w, h);
                    }
                }
                FeetState fs = static_cast<FeetState>(s);
                player.setFeetStateSheet(fs, playerFeetStateTextures[s], frames, (fs == FeetState::RUN) ? 0.035f : 0.055f);
            }
        } else {
            std::cerr << "Could not load feet state texture: " << feetPaths[s] << std::endl;
        }
    }

    // --- New: load upper sheets per weapon (pistol and rifle), each has 4 sheets: idle, move, shoot, reload ---
    std::array<std::string,4> pistolPaths = {
        "assets/Top_Down_Survivor/hunter_pistol_idle.png",
        "assets/Top_Down_Survivor/hunter_pistol_move.png",
        "assets/Top_Down_Survivor/hunter_pistol_shoot.png",
        "assets/Top_Down_Survivor/hunter_pistol_reload.png"
    };
    std::array<std::string,4> riflePaths = {
        "assets/Top_Down_Survivor/hunter_rifle_idle.png",
        "assets/Top_Down_Survivor/hunter_rifle_move.png",
        "assets/Top_Down_Survivor/hunter_rifle_shoot.png",
        "assets/Top_Down_Survivor/hunter_rifle_reload.png"
    };

    std::array<float,4> pistolTimes = {0.1f, 0.1f, 0.05f, 0.08f};
    std::array<float,4> rifleTimes = {0.1f, 0.1f, 0.04f, 0.14f};

    std::array<int,4> pistolFrameW = {253, 258, 255, 260};
    std::array<int,4> pistolFrameH = {216, 220, 215, 230};
    std::array<int,4> rifleFrameW = {313, 313, 312, 322};
    std::array<int,4> rifleFrameH = {207, 206, 206, 217};

    // Helper to build frames from a sheet (multi-row)
    auto buildFrames = [&](const sf::Texture& tex, int w, int h) {
        std::vector<sf::IntRect> out;
        sf::Vector2u ts = tex.getSize();
        int cols = (w > 0) ? ts.x / w : 0;
        int rows = (h > 0) ? ts.y / h : 0;
        for (int r = 0; r < rows; ++r) for (int c = 0; c < cols; ++c) out.emplace_back(c*w, r*h, w, h);
        return out;
    };

    // Load pistol sheets and frames
    std::vector<std::vector<sf::IntRect>> pistolFrames(4);
    for (int i = 0; i < 4; ++i) {
        if (playerPistolSheets[i].loadFromFile(pistolPaths[i])) {
            int w = pistolFrameW[i];
            int h = pistolFrameH[i];
            pistolFrames[i] = buildFrames(playerPistolSheets[i], w, h);
        } else std::cerr << "Could not load pistol sheet: " << pistolPaths[i] << std::endl;
    }

    // Build pointer arrays for pistol and rifle sheets
    std::array<const sf::Texture*,4> pistolSheetPtrs = { &playerPistolSheets[0], &playerPistolSheets[1], &playerPistolSheets[2], &playerPistolSheets[3] };
    player.setUpperWeaponSheet(WeaponType::PISTOL, pistolSheetPtrs, pistolFrames, pistolTimes);

    // Load rifle sheets and frames
    std::vector<std::vector<sf::IntRect>> rifleFrames(4);
    for (int i = 0; i < 4; ++i) {
        if (playerRifleSheets[i].loadFromFile(riflePaths[i])) {
            int w = rifleFrameW[i];
            int h = rifleFrameH[i];
            rifleFrames[i] = buildFrames(playerRifleSheets[i], w, h);
        } else std::cerr << "Could not load rifle sheet: " << riflePaths[i] << std::endl;
    }
    std::array<const sf::Texture*,4> rifleSheetPtrs = { &playerRifleSheets[0], &playerRifleSheets[1], &playerRifleSheets[2], &playerRifleSheets[3] };
    player.setUpperWeaponSheet(WeaponType::RIFLE, rifleSheetPtrs, rifleFrames, rifleTimes);
}

void Game::run() {
    const float PHYS_STEP = 1.0f / 120.0f; // 120 Hz physics
    float accumulator = 0.0f;
    sf::Clock frameClock;

    // Simple profiler accumulators (low-overhead)
    using clock = std::chrono::steady_clock;
    static double physicsTimeMs = 0.0;
    static double levelTimeMs = 0.0;
    static double renderTimeMs = 0.0;
    static uint32_t samples = 0;
    static auto windowStart = clock::now();
    const double sampleWindowSec = 5.0;

    while (window.isOpen()) {
        processInput();
        float deltaTime = frameClock.restart().asSeconds();
        accumulator += deltaTime;

        if (levelManager.isLevelTransitionPending()) {
            levelManager.loadLevel(levelManager.getCurrentLevel() + 1);
            levelManager.setGameState(GameState::LEVEL1);
            levelManager.clearLevelTransitionPending();
        }

        GameState currentState = levelManager.getCurrentState();
        if (currentState != GameState::GAME_OVER && currentState != GameState::VICTORY) {
            if (backgroundMusic.getStatus() != sf::Music::Playing) {
                backgroundMusic.setLoop(true);
                backgroundMusic.play();
            }
        } else {
            if (backgroundMusic.getStatus() == sf::Music::Playing) backgroundMusic.stop();
        }

        // Fixed-step update loop
        while (accumulator >= PHYS_STEP) {
            update(PHYS_STEP);
            accumulator -= PHYS_STEP;
        }

        renderAlpha = accumulator / PHYS_STEP;

        // Measure render time
        auto r0 = clock::now();
        render();
        auto r1 = clock::now();
        renderTimeMs += std::chrono::duration<double, std::milli>(r1 - r0).count();

        // Count this frame as a sample
        samples++;

        // Print profiling summary every sampleWindowSec seconds
        auto now = clock::now();
        double elapsedSec = std::chrono::duration<double>(now - windowStart).count();
        if (elapsedSec >= sampleWindowSec) {
            double avgPhysics = (samples > 0) ? physicsTimeMs / samples : 0.0;
            double avgLevel = (samples > 0) ? levelTimeMs / samples : 0.0;
            double avgRender = (samples > 0) ? renderTimeMs / samples : 0.0;
            std::cout << "[Profiler] samples=" << samples
                      << " avgPhysics(ms)=" << avgPhysics
                      << " avgLevel(ms)=" << avgLevel
                      << " avgRender(ms)=" << avgRender
                      << " activeZombies=" << levelManager.getActiveZombieCount()
                      << " queuedZombies=" << levelManager.getQueuedZombieCount();
            if (&physics) {
                std::cout << " physicsBodies=" << physics.getDynamicBodyCount()
                          << " lastChecks=" << physics.getLastCollisionChecks();
            }
            std::cout << std::endl;

            // reset counters
            samples = 0;
            physicsTimeMs = 0.0;
            levelTimeMs = 0.0;
            renderTimeMs = 0.0;
            windowStart = now;
        }
    }
}

void Game::processInput() {
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed)
            window.close();
            
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Escape)
                window.close();
            if (event.key.code == sf::Keyboard::Space) {
                levelManager.advanceDialog();
            }
            if (event.key.code == sf::Keyboard::R) {
                player.startReload();
            }
            // Weapon selection: make Num1 -> RIFLE, Num2 -> PISTOL (simple mapping)
            if (event.key.code == sf::Keyboard::Num1) {
                player.setWeapon(WeaponType::RIFLE);
            }
            if (event.key.code == sf::Keyboard::Num2) {
                player.setWeapon(WeaponType::PISTOL);
            }
        }
        
        if (event.type == sf::Event::MouseButtonPressed) {
            if (event.mouseButton.button == sf::Mouse::Left) {
                // Defer the shooting request to the update() loop, where sprint/aim rules are enforced
                sf::Vector2i mousePosition = sf::Mouse::getPosition(window);
                shootRequested = true;
                shootTarget = window.mapPixelToCoords(mousePosition, gameView);
            }
        }
    }
}

void Game::update(float deltaTime) {
    sf::Vector2i mousePosition = sf::Mouse::getPosition(window);
    sf::Vector2f worldMousePosition = window.mapPixelToCoords(mousePosition, gameView);

    int currentLevel = levelManager.getCurrentLevel();
    sf::Vector2u mapSize = getMapSize(currentLevel);

    using clock = std::chrono::steady_clock;
    auto u0 = clock::now();

    physics.update(deltaTime);

    auto u1 = clock::now();

    // Centralize input-driven states: pass sprint/aim intent into Player
    bool wantsToAim = sf::Mouse::isButtonPressed(sf::Mouse::Right);
    bool wantsToSprint = sf::Keyboard::isKeyPressed(sf::Keyboard::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::RShift);
    player.setAiming(wantsToAim);
    player.sprint(wantsToSprint);

    player.update(deltaTime, window, mapSize, worldMousePosition);

    // Update camera to follow player so LevelManager knows the current visible area
    sf::Vector2f playerPosition = player.getPosition();
    gameView.setCenter(playerPosition);
    // clamp camera to map
    float cameraLeft = gameView.getCenter().x - gameView.getSize().x / 2;
    float cameraTop = gameView.getCenter().y - gameView.getSize().y / 2;
    float cameraRight = gameView.getCenter().x + gameView.getSize().x / 2;
    float cameraBottom = gameView.getCenter().y + gameView.getSize().y / 2;
    if (cameraLeft < 0) gameView.setCenter(gameView.getSize().x / 2, gameView.getCenter().y);
    if (cameraTop < 0) gameView.setCenter(gameView.getCenter().x, gameView.getSize().y / 2);
    if (cameraRight > mapSize.x) gameView.setCenter(mapSize.x - gameView.getSize().x / 2, gameView.getCenter().y);
    if (cameraBottom > mapSize.y) gameView.setCenter(gameView.getCenter().x, mapSize.y - gameView.getSize().y / 2);

    // Provide LevelManager the current camera view rect (world coords)
    sf::Vector2f vc = gameView.getCenter();
    sf::Vector2f vs = gameView.getSize();
    sf::FloatRect viewRect(vc.x - vs.x/2.0f, vc.y - vs.y/2.0f, vs.x, vs.y);
    levelManager.setCameraViewRect(viewRect);

    // Automatic fire for rifle: allow continuous shooting while left mouse button is held.
    // Pistol remains single-shot via the MouseButtonPressed event in processInput.
    if (sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
        if (player.getCurrentWeapon() == WeaponType::RIFLE && player.timeSinceLastShot >= player.fireCooldown) {
            player.shoot(worldMousePosition, physics, bullets);
        }
    }

    // Check and process deferred shoot request
    if (shootRequested) {
        if (player.getCurrentWeapon() == WeaponType::PISTOL || player.getCurrentWeapon() == WeaponType::RIFLE) {
            if (player.timeSinceLastShot >= player.fireCooldown) {
                player.shoot(shootTarget, physics, bullets);
            }
        }
        shootRequested = false; // Reset the flag
    }

    // Update bullets
    for (auto it = bullets.begin(); it != bullets.end(); ) {
        (*it)->update(deltaTime);

        if (!(*it)->isAlive()) {
            physics.removeBody(&(*it)->getBody()); // remove from physics first
            it = bullets.erase(it); // unique_ptr deletes the bullet automatically
        }
        else {
            ++it;
        }
    }

    levelManager.update(deltaTime, player);

    auto u2 = clock::now();

    // accumulate measured times into Game::run static accumulators
    using msd = std::chrono::duration<double, std::milli>;
    extern double physicsTimeMs; // forward to static in run (can't access directly); workaround: use static locals in run scope? Simpler: store to global statics defined above in anonymous namespace

    // Instead of extern, use static globals defined here to accumulate
    static double localPhysicsAccum = 0.0;
    static double localLevelAccum = 0.0;
    localPhysicsAccum += msd(u1 - u0).count();
    localLevelAccum += msd(u2 - u1).count();

    // Transfer local accumulators to the ones in run by printing them to cout (we'll push them via a simple aggregator)
    // To avoid modifying run's statics directly from here, we will cache the values in Game object fields and pick them up inside run when printing.
    // Add fields to Game in header would be needed, but simplest approach: print per-sample here for debug as well.
    if (levelManager.getDebugLogging()) {
        std::cout << "[UpdateTiming] physics(ms)=" << msd(u1 - u0).count()
                  << " level(ms)=" << msd(u2 - u1).count()
                  << std::endl;
    }

    checkZombiePlayerCollisions();
    checkPlayerBoundaries();
}

void Game::render() {
    window.clear();
    window.setView(gameView);

    if (!levelManager.isLevelTransitioning()) {
        int currentLevel = levelManager.getCurrentLevel();
        window.draw(getMapSprite(currentLevel));
        
        // Set interpolation alpha for player and zombies before they are rendered
        player.setRenderAlpha(renderAlpha);
        auto& zombies = levelManager.getZombies();
        for (auto* z : zombies) {
            z->setRenderAlpha(renderAlpha);
        }

            levelManager.render(window);
            
            if (debugDrawHitboxes) {
                // Debug: draw physics bodies for player and zombies
                PhysicsBody& pb = player.getBody();
                if (pb.isCircle) {
                    float r = pb.size.x * 0.5f;
                    sf::CircleShape circle(r);
                    circle.setOrigin(r, r);
                    circle.setPosition(pb.position.x, pb.position.y);
                    circle.setFillColor(sf::Color(0, 0, 255, 48));
                    circle.setOutlineColor(sf::Color::Blue);
                    circle.setOutlineThickness(1.0f);
                    window.draw(circle);
                } else {
                    sf::RectangleShape rect(sf::Vector2f(pb.size.x, pb.size.y));
                    rect.setOrigin(pb.size.x * 0.5f, pb.size.y * 0.5f);
                    rect.setPosition(pb.position.x, pb.position.y);
                    rect.setFillColor(sf::Color(0, 0, 255, 48));
                    rect.setOutlineColor(sf::Color::Blue);
                    rect.setOutlineThickness(1.0f);
                    window.draw(rect);
                }

                for (auto& z : zombies) {
                    PhysicsBody& zb = z->getBody();
                    if (zb.isCircle) {
                        float r = zb.size.x * 0.5f;
                        sf::CircleShape circle(r);
                        circle.setOrigin(r, r);
                        circle.setPosition(zb.position.x, zb.position.y);
                        circle.setFillColor(sf::Color(0, 255, 0, 48));
                        circle.setOutlineColor(sf::Color::Green);
                        circle.setOutlineThickness(1.0f);
                        window.draw(circle);
                    } else {
                        sf::RectangleShape rect(sf::Vector2f(zb.size.x, zb.size.y));
                        rect.setOrigin(zb.size.x * 0.5f, zb.size.y * 0.5f);
                        rect.setPosition(zb.position.x, zb.position.y);
                        rect.setFillColor(sf::Color(0, 255, 0, 48));
                        rect.setOutlineColor(sf::Color::Green);
                        rect.setOutlineThickness(1.0f);
                        window.draw(rect);
                    }
                }
            }
         // Draw bullets via LevelManager so draw order is centralized
         // Render player first so bullets can be drawn on top
         // Sync player's debug origins flag with global debug toggle so muzzle markers follow backtick
         player.debugDrawOrigins = debugDrawHitboxes;
         player.render(window);
         // Now draw bullets so they appear over the player sprite
         levelManager.renderBullets(window, bullets, renderAlpha);
     }

    window.setView(window.getDefaultView());

    pointsText.setString("Points: " + std::to_string(points));
    window.draw(pointsText);

    levelManager.renderUI(window, font);
    levelManager.drawHUD(window, player);

    window.display();
}

sf::Sprite& Game::getMapSprite(int level) {
    switch (level) {
        case 0:
        case 1:
        case 4:
        case 5:
            return mapSprite1;
        case 2:
            return mapSprite2;
        case 3:
            return mapSprite3;
        default:
            return mapSprite1;
    }
}

sf::Vector2u Game::getMapSize(int level) {
    switch (level) {
        case 0:
        case 1:
        case 4:
        case 5:
            return sf::Vector2u(2048, 2048);
        case 2:
            return sf::Vector2u(1200, 1000);
        case 3:
            return sf::Vector2u(1200, 800);
        default:
            return sf::Vector2u(3072, 3072);
    }
}

void Game::checkZombiePlayerCollisions() {
    auto& zombiesRef = levelManager.getZombies();
    for (auto it = zombiesRef.begin(); it != zombiesRef.end(); ) {
        BaseZombie* zb = *it;
        // Do not erase zombies here. LevelManager owns the active zombie list and is responsible
        // for removing dead zombies, unregistering physics bodies, and recycling pool indices.
        // Removing pointers from here without coordinating with LevelManager/PhysicsWorld can
        // leak physics bodies and cause the performance issues you've observed.

        sf::FloatRect playerHitbox = player.getHitbox();

        // Use oriented OB box for attack -> accurate direction-dependent collision
        sf::Vector2f attackCenter, attackHalf; float attackRot;
        zb->getAttackOBB(attackCenter, attackHalf, attackRot);
        bool pzCollision = obbIntersectsAabb(attackCenter, attackHalf, attackRot, playerHitbox);
        if (pzCollision && zb->isAttacking()) {
            float damageToApply = zb->tryDealDamage();
            if (damageToApply > 0) {
                player.takeDamage(damageToApply);
                if (zombieBiteSound.getStatus() != sf::Sound::Playing) zombieBiteSound.play();

                sf::Vector2f direction = player.getPosition() - zb->getPosition();
                float magnitude = sqrt(direction.x * direction.x + direction.y * direction.y);
                if (magnitude > 0) {
                    direction.x /= magnitude;
                    direction.y /= magnitude;
                    player.applyKnockback(direction, 50.0f);
                }
            }
        }
        
        if (player.isAttacking()) {
            sf::FloatRect playerAttackBox = player.getAttackHitbox();
            sf::FloatRect zombieBodyHitbox = zb->getHitbox();
            if (isCollision(playerAttackBox, zombieBodyHitbox)) {
                zb->takeDamage(player.getAttackDamage());
                if (!zb->isAlive()) {
                    // Award points and track progression. Do NOT erase the zombie pointer here;
                    // LevelManager will remove the entity and unregister the physics body on its update.
                    points += 10;
                    zombiesToNextLevel--;
                    if (zombiesToNextLevel <= 0) {
                        levelManager.nextLevel();
                        int currentLevel = levelManager.getCurrentLevel();
                        zombiesToNextLevel = 15;
                    }
                }
            }
        }
        ++it;
    }
}

void Game::checkPlayerBoundaries() {
    sf::Vector2f playerPos = player.getPosition();
    int currentLevel = levelManager.getCurrentLevel();
    sf::Vector2u mapSize = getMapSize(currentLevel);
    float playerRadius = 20.0f;
    
    if (playerPos.x - playerRadius < 0) {
        player.setPosition(playerRadius, playerPos.y);
    }
    if (playerPos.y - playerRadius < 0) {
        player.setPosition(playerPos.x, playerRadius);
    }
    if (playerPos.x + playerRadius > mapSize.x) {
        player.setPosition(mapSize.x - playerRadius, playerPos.y);
    }
    if (playerPos.y + playerRadius > mapSize.y) {
        player.setPosition(playerPos.x, mapSize.y - playerRadius);
    }
}

bool Game::isCollision(const sf::FloatRect& rect1, const sf::FloatRect& rect2) {
    return rect1.intersects(rect2);
}

// SAT-based OBB vs AABB test: OBB given by center, half-extents, rotation degrees; AABB given by rect
static bool obbIntersectsAabb(const sf::Vector2f& c, const sf::Vector2f& he, float rotDeg, const sf::FloatRect& aabb) {
    // Build OBB axes
    float rad = rotDeg * 3.14159265f / 180.0f;
    sf::Vector2f ux(std::cos(rad), std::sin(rad));
    sf::Vector2f uy(-ux.y, ux.x);

    // AABB center
    sf::Vector2f ac(aabb.left + aabb.width * 0.5f, aabb.top + aabb.height * 0.5f);
    sf::Vector2f t = ac - c;

    // Projected radii on axes
    float ra, rb;

    // test axis ux
    ra = he.x;
    rb = std::abs(aabb.width * 0.5f * ux.x) + std::abs(aabb.height * 0.5f * ux.y);
    if (std::abs(t.x * ux.x + t.y * ux.y) > ra + rb) return false;

    // test axis uy
    ra = he.y;
    rb = std::abs(aabb.width * 0.5f * uy.x) + std::abs(aabb.height * 0.5f * uy.y);
    if (std::abs(t.x * uy.x + t.y * uy.y) > ra + rb) return false;

    // test AABB axes x and y
    // project OBB onto world X
    float obbPx = std::abs(ux.x * he.x) + std::abs(uy.x * he.y);
    if (std::abs(t.x) > obbPx + aabb.width * 0.5f) return false;
    // project OBB onto world Y
    float obbPy = std::abs(ux.y * he.x) + std::abs(uy.y * he.y);
    if (std::abs(t.y) > obbPy + aabb.height * 0.5f) return false;

    return true;
}

void Game::addPoints(int amount) {
    points += amount;
}

int Game::getPoints() const {
    return points;
}

void Game::reset() {
    points = 0;
    zombiesToNextLevel = 15;
    levelManager.reset();
    player.reset();
}

sf::RenderWindow& Game::getWindow() {
    return window;
}