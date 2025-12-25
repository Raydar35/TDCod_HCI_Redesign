#include "Game.h"
#include "Explosion.hpp"
#include "Guts.hpp"
#include "ExplosionProvider.hpp"
#include <iostream>
#include <algorithm>
#include <random>
#include <cmath>
#include <chrono>

// Forward declaration for OBB vs AABB helper used by collision checks
static bool obbIntersectsAabb(const sf::Vector2f& c, const sf::Vector2f& he, float rotDeg, const sf::FloatRect& aabb);

// Back button margin from panel edge (keeps it fully inside) - same as scene.cpp
static constexpr float BACK_PANEL_MARGIN = 12.f;

// clickable hit rect for Victory EXIT (UI/default-view coords)
static sf::FloatRect s_victoryExitRect(0.f, 0.f, 0.f, 0.f);
static bool s_victoryWasHovered = false;

Game::Game()
    : window(),
    player(Vec2(400, 300)),
    points(0),
    zombiesToNextLevel(15),
    gameOverTriggered(false),
    gameOverAlpha(0.0f),
    gameOverFadeDuration(1.5f) // 1.5 seconds for game over fade
{
    // Create fullscreen window using desktop resolution and use letter/pillarbox
    // so content isn't stretched.
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    window.create(desktop, "Echoes of Valkyrie", sf::Style::Fullscreen);
    window.setFramerateLimit(60);
    // Hide OS cursor; we'll draw a small dot as the custom cursor
    window.setMouseCursorVisible(false);

    // Set the game view to match the desktop/fullscreen resolution
    float desktopW = static_cast<float>(desktop.width);
    float desktopH = static_cast<float>(desktop.height);
    gameView.setSize(desktopW, desktopH);
    gameView.setCenter(desktopW * 0.5f, desktopH * 0.5f);
    // Use full viewport
    gameView.setViewport(sf::FloatRect(0.f, 0.f, 1.f, 1.f));

    // Zoom the camera in slightly (factor < 1.0f zooms in). Adjust to taste.
    const float CAMERA_ZOOM = 0.85f; // 0.85 -> ~15% closer
    gameView.zoom(CAMERA_ZOOM);
    // record base view size after initial zoom
    baseViewSize = gameView.getSize();

    background.setFillColor(sf::Color(50, 50, 50));
    background.setSize(sf::Vector2f(2560.f, 2560.f)); // Set to map size (pixels)

    if (!mapTexture1.loadFromFile("assets/Map/ground1.jpg")) {
        std::cerr << "Error loading map1 texture!" << std::endl;
    } else {
        // This texture is a single tile (512x512). Enable repeating and
        // instruct the sprite to cover the full map area (2560x2560).
        mapTexture1.setRepeated(true);
        mapSprite1.setTexture(mapTexture1);
        mapSprite1.setTextureRect(sf::IntRect(0, 0, 2560, 2560));
        int startLevelForCanvas = levelManager.getCurrentLevel();
        sf::Vector2u initialMapSize = getMapSize(startLevelForCanvas);
        Props::Explosion::setGroundCanvasSize(initialMapSize.x, initialMapSize.y);
    }
    
    if (!font.loadFromFile("assets/Call of Ops Duty.otf")) {
        std::cerr << "Error loading font 'assets/Call of Ops Duty.otf'! Trying fallback." << std::endl;
    }
    if (!font2.loadFromFile("assets/SwanseaBold-D0ox.ttf")) {
        std::cerr << "Error loading fallback font 'assets/SwanseaBold-D0ox.ttf'! Points text will not be displayed." << std::endl;
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
        // Initialize explosion ground canvas to match map pixels so decals align with world coordinates
        Props::Explosion::setGroundCanvasSize(mapSize.x, mapSize.y);
    }

    if (!backgroundMusic.openFromFile("assets/Audio/atmosphere.mp3")) {
        std::cerr << "Error loading background music! Path: assets/Audio/atmosphere.mp3" << std::endl;
    } else {
        backgroundMusic.setVolume(15);
    }

    if (!zombieBiteBuffer.loadFromFile("assets/Audio/zombie_bite.mp3")) {
        std::cerr << "Error loading zombie bite sound!" << std::endl;
    } else {
        // create a small pool of sound instances so multiple bites can overlap
        const size_t poolSize = 6;
        zombieBiteSounds.resize(poolSize);
        for (size_t i = 0; i < poolSize; ++i) {
            zombieBiteSounds[i].setBuffer(zombieBiteBuffer);
            zombieBiteSounds[i].setVolume(100.f);
        }
        nextBiteSoundIndex = 0;
    }

    if (!Bullet::bulletTexture.loadFromFile("assets/Top_Down_Survivor/bullet.png")) {
        std::cerr << "Failed to load bullet texture!" << std::endl;
    }

    // Load muzzle flash texture (provide your own path here)
    if (!muzzleFlashTexture.loadFromFile("assets/Top_Down_Survivor/muzzle_flash.png")) {
        std::cerr << "Could not load muzzle flash texture" << std::endl;
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
    // Feet Strafes may be different sizes; set left/right explicitly
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

    // Load icon textures for pause controls panel to match scene icons
    pauseIconTextures.clear(); pauseIconSprites.clear();
    size_t totalIcons = 10; // same as scene
    pauseIconTextures.resize(totalIcons);
    pauseIconSprites.resize(totalIcons);
    for (size_t i = 0; i < totalIcons; ++i) {
        std::string path = "assets/scene/Icons/icon" + std::to_string(i) + ".png";
        if (pauseIconTextures[i].loadFromFile(path)) {
            pauseIconTextures[i].setSmooth(true);
            pauseIconSprites[i].setTexture(pauseIconTextures[i]);
        } else {
            // leave empty; panel will draw placeholder slot
        }
    }
    // Load back icon (escape icon) used by the back button in the panel
    std::string backPath = "assets/scene/Icons/esc.png";
    if (!backIconTexture.loadFromFile(backPath)) {
        // Not fatal; panel will draw placeholder slot instead
    } else {
        backIconTexture.setSmooth(true);
        backIconSprite.setTexture(backIconTexture);
    }

    // Load HUD weapon icons and pass to LevelManager
    if (!pistolIconTexture.loadFromFile("assets/pistol_icon.png")) {
        std::cerr << "Warning: could not load pistol icon" << std::endl;
    } else {
        pistolIconTexture.setSmooth(true);
        levelManager.setPistolIcon(pistolIconTexture);
    }

    if (!rifleIconTexture.loadFromFile("assets/rifle_icon.png")) {
        std::cerr << "Warning: could not load rifle icon" << std::endl;
    } else {
        rifleIconTexture.setSmooth(true);
        levelManager.setRifleIcon(rifleIconTexture);
    }

    // Load key icons for equip buttons (optional)
    if (keyIcon1Texture.loadFromFile("assets/1icon.png")) {
        keyIcon1Texture.setSmooth(true);
        levelManager.setKeyIcon1(keyIcon1Texture);
    }
    if (keyIcon2Texture.loadFromFile("assets/2icon.png")) {
        keyIcon2Texture.setSmooth(true);
        levelManager.setKeyIcon2(keyIcon2Texture);
    }

    // Load R key icon for on-screen reload prompt (optional). This is non-fatal.
    if (reloadKeyTexture.loadFromFile("assets/R_icon.png")) {
        reloadKeyTexture.setSmooth(true);
        reloadKeySprite.setTexture(reloadKeyTexture);
        // set a default origin so positioning is straightforward when drawing
        reloadKeySprite.setOrigin(0.f, 0.f);
    }

    // Load Esc key icon for tutorial dialog (optional)
    if (escKeyTexture.loadFromFile("assets/Esc_icon.png")) {
        escKeyTexture.setSmooth(true);
        // pass to level manager as keyIcon1 (use setter to keep ownership rules consistent)
        levelManager.setKeyIconEsc(escKeyTexture);
    }

    // Load blood overlay (optional). This image should be a fullscreen PNG with transparent center.
    if (bloodTexture.loadFromFile("assets/blood_overlay.png")) {
        bloodTexture.setSmooth(true);
        bloodSprite.setTexture(bloodTexture);
        bloodSprite.setPosition(0.f, 0.f);
        // start invisible by default
        bloodSprite.setColor(sf::Color(255,255,255,0));
    } else {
        // not fatal
        // std::cerr << "Warning: could not load blood overlay texture" << std::endl;
    }

    // Load guts texture (optional) and pass to Guts system
    if (gutsTexture.loadFromFile("assets/guts.png")) {
        Guts::setTexture(gutsTexture);
    } else {
        // not fatal, Guts will render primitives
    }

    // Load blood particle texture (optional) and pass to Explosion system
    if (bloodParticleTexture.loadFromFile("assets/blood.png")) {
        Props::Explosion::setTexture(bloodParticleTexture);
    } else {
        // not fatal, Explosion will render colored quads
    }

    // Initialize ExplosionProvider (precomputes random tables and Guts)
    ExplosionProvider::initProvider();

    // Load desaturation shader (used to grey-out world as health decreases)
    const std::string fragShader = R"(
        uniform sampler2D texture;
        uniform float u_desat; // 0 = full color, 1 = grayscale
        void main()
        {
            vec4 col = texture2D(texture, gl_TexCoord[0].xy);
            float lum = dot(col.rgb, vec3(0.2126, 0.7152, 0.0722));
            vec3 gray = vec3(lum);
            vec3 outcol = mix(col.rgb, gray, clamp(u_desat, 0.0, 1.0));
            gl_FragColor = vec4(outcol, col.a);
        }
    )";
    desaturateShaderLoaded = desaturateShader.loadFromMemory(fragShader, sf::Shader::Fragment);
    if (desaturateShaderLoaded) desaturateShader.setUniform("u_desat", 0.f);

    // Load placeholder tally textures for round indicators (0..4)
    for (int i = 0; i < 5; ++i) {
        std::string path = "assets/tally" + std::to_string(i) + ".png";
        if (tallyTextures[i].loadFromFile(path)) {
            tallyTextures[i].setSmooth(true);
            levelManager.setTallyTexture(i, tallyTextures[i]);
        } else {
            std::cerr << "Warning: failed to load tally texture: " << path << std::endl;
        }
    }

    // Load UI sounds (hover and click)
    if (!uiHoverBuffer.loadFromFile("assets/Audio/menubutton.mp3")) {
        // fallback: no hover sound
    } else {
        uiHoverSound.setBuffer(uiHoverBuffer);
        uiHoverSound.setVolume(50.f);
    }
    if (!uiClickBuffer.loadFromFile("assets/Audio/menuclick.mp3")) {
        // fallback: no click sound
    } else {
        uiClickSound.setBuffer(uiClickBuffer);
        uiClickSound.setVolume(60.f);
    }
    applyAudioSettings();
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

        // If game is paused, reset accumulator so physics doesn't catch up while paused
        if (paused) {
            accumulator = 0.0f;
        } else {
            accumulator += deltaTime;
        }

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
        while (!paused && accumulator >= PHYS_STEP) {
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
        if (event.type == sf::Event::Closed) {
            window.close();
            continue;
        }

        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Escape) {
                GameState st = levelManager.getCurrentState();
                // If tutorial dialog specifically requires ESC to advance, advance the dialog.
                if (st == GameState::TUTORIAL && levelManager.isRequireEscToAdvanceDialog()) {
                    levelManager.advanceDialog();
                }
                if (st == GameState::TUTORIAL || (st >= GameState::LEVEL1 && st <= GameState::LEVEL5)) {
                    if (paused && showingControls) {
                        showingControls = false;
                        window.setMouseCursorVisible(true);
                    }
                    else if (paused && showingSettings) {
                        showingSettings = false;
                        window.setMouseCursorVisible(true);
                    }
                    else {
                        paused = !paused;
                        showingControls = false;
                        showingSettings = false;
                        window.setMouseCursorVisible(paused);
                        shootRequested = false;
                    }
                }
                else {
                    window.close();
                }
            }

            if (paused) continue;

            if (event.key.code == sf::Keyboard::Space) {
                // If the current tutorial dialog requires ESC to advance, ignore SPACE
                if (!levelManager.isRequireEscToAdvanceDialog()) levelManager.advanceDialog();
            }
            if (event.key.code == sf::Keyboard::R) player.startReload();
            //if (event.key.code == sf::Keyboard::Tilde) debugDrawHitboxes = !debugDrawHitboxes;
            if (event.key.code == sf::Keyboard::Num1) player.setWeapon(WeaponType::RIFLE);
            if (event.key.code == sf::Keyboard::Num2) player.setWeapon(WeaponType::PISTOL);
        }

        if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
            // Victory EXIT button handling
            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                // Map mouse to default view (UI) coords and check Victory EXIT button
                sf::Vector2f mouseUI = window.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y), window.getDefaultView());
                if (levelManager.getCurrentState() == GameState::VICTORY) {
                    if (s_victoryExitRect.contains(mouseUI)) {
                        if (uiClickSound.getBuffer()) uiClickSound.play();
                        window.close();
                        continue; // consume click
                    }
                }

                // Pause overlay handling
                if (paused && !showingControls && !showingSettings) {
                    std::vector<std::string> labels = { "Resume", "Controls", "Settings", "Exit" };
                    unsigned int fontSize = static_cast<unsigned int>(std::min(160.f, static_cast<float>(window.getSize().y) / 15.f));
                    float spacing = static_cast<float>(fontSize) * 0.25f;
                    float centerX = static_cast<float>(window.getSize().x) * 0.5f;
                    float centerY = static_cast<float>(window.getSize().y) * 0.5f;

                    sf::Text proto; proto.setFont(font); proto.setCharacterSize(fontSize); proto.setString("Ay");
                    sf::FloatRect ph = proto.getLocalBounds();
                    float lineH = ph.height + ph.top;
                    float totalH = labels.size() * lineH + (labels.size() - 1) * spacing;
                    float startY = centerY - totalH * 0.5f;

                    sf::Vector2f world = window.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y), window.getDefaultView());
                    for (size_t i = 0; i < labels.size(); ++i) {
                        sf::Text t; t.setFont(font); t.setCharacterSize(fontSize); t.setString(labels[i]);
                        sf::FloatRect tb = t.getLocalBounds();
                        float tx = centerX - (tb.left + tb.width / 2.f);
                        float ty = startY + static_cast<float>(i) * (lineH + spacing) + lineH * 0.5f - tb.top;
                        sf::FloatRect hitRect(tx + tb.left, ty + tb.top, tb.width, tb.height);
                        if (hitRect.contains(world)) {
                            if (labels[i] == "Exit") {
                                if (uiClickSound.getBuffer()) uiClickSound.play();
                                window.close();
                            }
                            else if (labels[i] == "Resume") {
                                if (uiClickSound.getBuffer()) uiClickSound.play();
                                paused = false; showingControls = false; window.setMouseCursorVisible(false);
                            }
                            else if (labels[i] == "Controls") {
                                if (uiClickSound.getBuffer()) uiClickSound.play();
                                showingControls = !showingControls; window.setMouseCursorVisible(showingControls);
                            }
                            else if (labels[i] == "Settings") {
                                if (uiClickSound.getBuffer()) uiClickSound.play();
                                showingSettings = !showingSettings; window.setMouseCursorVisible(showingSettings);
                            }
                            break;
                        }
                    }
                    continue; // consume click
                }

                // Controls panel Back button
                if (paused && showingControls) {
                    sf::Vector2f panelSize(window.getSize().x * 0.6f, window.getSize().y * 0.6f);
                    float titleBottom = static_cast<float>(window.getSize().y) / 5.0f;
                    float gap = 20.f;
                    const float PANEL_EXTRA_OFFSET = 80.f;
                    float panelCenterY = titleBottom + gap + PANEL_EXTRA_OFFSET + panelSize.y / 2.f;
                    sf::Vector2f panelPos(window.getSize().x / 2.f, panelCenterY);

                    const unsigned int BACK_CHAR_SIZE = 36u;
                    const float ICON_W = 52.f;
                    const float ICON_PAD = 12.f;
                    sf::Text tmpBack; tmpBack.setFont(font); tmpBack.setCharacterSize(BACK_CHAR_SIZE); tmpBack.setString("Back");
                    sf::FloatRect backBounds = tmpBack.getLocalBounds();
                    float backWidth = backBounds.width + backBounds.left + 28.f + ICON_W + ICON_PAD;
                    float backHeight = backBounds.height + backBounds.top + 20.f;
                    sf::Vector2f backPos(panelPos.x + panelSize.x / 2.f - BACK_PANEL_MARGIN - backWidth,
                        panelPos.y + panelSize.y / 2.f - BACK_PANEL_MARGIN - backHeight);

                    sf::Vector2f world = window.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y), window.getDefaultView());
                    sf::FloatRect backRect(backPos.x, backPos.y, backWidth, backHeight);
                    if (backRect.contains(world)) {
                        if (uiClickSound.getBuffer()) uiClickSound.play();
                        showingControls = false; window.setMouseCursorVisible(true);
                        continue;
                    }
                }

                // Settings panel hit tests
                if (paused && showingSettings) {
                    sf::Vector2f panelSize(window.getSize().x * 0.6f, window.getSize().y * 0.6f);
                    float titleBottom = static_cast<float>(window.getSize().y) / 5.0f;
                    float gap = 20.f;
                    const float PANEL_EXTRA_OFFSET = 80.f;
                    float panelCenterY = titleBottom + gap + PANEL_EXTRA_OFFSET + panelSize.y / 2.f;
                    sf::Vector2f panelPos(window.getSize().x / 2.f, panelCenterY);

                    float sliderWidth = panelSize.x * 0.55f;
                    float sliderX = panelPos.x - sliderWidth / 2.f;
                    float panelTop = panelPos.y - panelSize.y / 2.f;
                    float rowYStart = panelTop + 130.f;
                    float rowSpacing = 60.f;

                    sf::Vector2f mousef = window.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y), window.getDefaultView());

                    float hr_left = 4.f; float hr_topOff = 6.f; float hr_extraW = 8.f; float hr_h = 20.f;
                    sf::FloatRect masterBarRect(sliderX - hr_left, rowYStart - hr_topOff, sliderWidth + hr_extraW, hr_h);
                    sf::FloatRect musicBarRect(sliderX - hr_left, rowYStart + rowSpacing - hr_topOff, sliderWidth + hr_extraW, hr_h);
                    sf::FloatRect sfxBarRect(sliderX - hr_left, rowYStart + rowSpacing * 2 - hr_topOff, sliderWidth + hr_extraW, hr_h);

                    if (masterBarRect.contains(mousef)) { draggingSlider = 0; float p = (mousef.x - sliderX) / sliderWidth; masterVolume = std::clamp(p * 100.f, 0.f, 100.f); masterMuted = (masterVolume <= 0.0f); applyAudioSettings(); if (uiClickSound.getBuffer()) uiClickSound.play(); continue; }
                    if (musicBarRect.contains(mousef)) { draggingSlider = 1; float p = (mousef.x - sliderX) / sliderWidth; musicVolume = std::clamp(p * 100.f, 0.f, 100.f); musicMuted = (musicVolume <= 0.0f); applyAudioSettings(); if (uiClickSound.getBuffer()) uiClickSound.play(); continue; }
                    if (sfxBarRect.contains(mousef)) { draggingSlider = 2; float p = (mousef.x - sliderX) / sliderWidth; sfxVolume = std::clamp(p * 100.f, 0.f, 100.f); sfxMuted = (sfxVolume <= 0.0f); applyAudioSettings(); if (uiClickSound.getBuffer()) uiClickSound.play(); continue; }

                    float muteX = sliderX + sliderWidth + 60.f;
                    float muteW = 22.f, muteH = 22.f;
                    sf::FloatRect masterMuteRect(muteX, rowYStart - 6.f, muteW, muteH);
                    sf::FloatRect musicMuteRect(muteX, rowYStart + rowSpacing - 6.f, muteW, muteH);
                    sf::FloatRect sfxMuteRect(muteX, rowYStart + rowSpacing * 2 - 6.f, muteW, muteH);
                    if (masterMuteRect.contains(mousef)) { masterMuted = !masterMuted; applyAudioSettings(); if (uiClickSound.getBuffer()) uiClickSound.play(); continue; }
                    if (musicMuteRect.contains(mousef)) { musicMuted = !musicMuted; applyAudioSettings(); if (uiClickSound.getBuffer()) uiClickSound.play(); continue; }
                    if (sfxMuteRect.contains(mousef)) { sfxMuted = !sfxMuted; applyAudioSettings(); if (uiClickSound.getBuffer()) uiClickSound.play(); continue; }

                    // Back button in settings
                    const unsigned int BACK_CHAR_SIZE = 36u;
                    const float ICON_W = 52.f;
                    const float ICON_PAD = 12.f;
                    sf::Text tmpBack; tmpBack.setFont(font); tmpBack.setCharacterSize(BACK_CHAR_SIZE); tmpBack.setString("Back");
                    sf::FloatRect backBounds = tmpBack.getLocalBounds();
                    float backWidth = backBounds.width + backBounds.left + 28.f + ICON_W + ICON_PAD;
                    float backHeight = backBounds.height + backBounds.top + 20.f;
                    sf::Vector2f backPos(panelPos.x + panelSize.x / 2.f - BACK_PANEL_MARGIN - backWidth,
                        panelPos.y + panelSize.y / 2.f - BACK_PANEL_MARGIN - backHeight);
                    sf::FloatRect backRect(backPos.x, backPos.y, backWidth, backHeight);
                    if (backRect.contains(mousef)) { if (uiClickSound.getBuffer()) uiClickSound.play(); showingSettings = false; window.setMouseCursorVisible(true); continue; }
                }

                // GAME OVER menu handling (clicks)
                if (gameOverTriggered && gameOverMenuAlpha > 0.05f) {
                    unsigned int itemSize = static_cast<unsigned int>(std::min(124.f, static_cast<float>(window.getSize().y) * 0.08f));
                    float startY = static_cast<float>(window.getSize().y) * 0.50f;
                    float lineSpacing = static_cast<float>(window.getSize().y) * 0.1f;
                    std::vector<std::string> items = { "RETRY ROUND", "EXIT" };

                    sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
                    sf::Vector2f mouseUI = window.mapPixelToCoords(mousePixel, window.getDefaultView());

                    for (size_t i = 0; i < items.size(); ++i) {
                        sf::Text it; it.setFont(font); it.setCharacterSize(itemSize); it.setStyle(sf::Text::Bold);
                        it.setString(items[i]);
                        sf::FloatRect ib = it.getLocalBounds();
                        it.setOrigin(ib.left + ib.width / 2.f, ib.top + ib.height / 2.f);
                        float y = startY + static_cast<float>(i) * lineSpacing;
                        it.setPosition(static_cast<float>(window.getSize().x) * 0.5f, y);
                        sf::FloatRect hit(it.getPosition().x - ib.width / 2.f - 8.f, it.getPosition().y - ib.height / 2.f - 6.f, ib.width + 16.f, ib.height + 12.f);
                        if (hit.contains(mouseUI)) {
                            if (uiClickSound.getBuffer()) uiClickSound.play();
                            if (items[i] == "EXIT") { window.close(); }
                            else if (items[i] == "RETRY ROUND") { reset(); }
                            break;
                        }
                    }
                    continue; // consume click
                }

                // Default: defer shooting
                sf::Vector2i mousePosition = sf::Mouse::getPosition(window);
                shootRequested = true;
                shootTarget = window.mapPixelToCoords(mousePosition, gameView);
            }

            else if (event.type == sf::Event::MouseButtonReleased) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    if (draggingSlider != -1) {
                        draggingSlider = -1;
                        if (uiClickSound.getBuffer()) uiClickSound.play();
                    }
                }
            }

            else if (event.type == sf::Event::MouseMoved) {
                if (paused && showingSettings && draggingSlider != -1) {
                    sf::Vector2f panelSize(window.getSize().x * 0.6f, window.getSize().y * 0.6f);
                    float titleBottom = static_cast<float>(window.getSize().y) / 5.0f;
                    float gap = 20.f;
                    const float PANEL_EXTRA_OFFSET = 80.f;
                    float panelCenterY = titleBottom + gap + PANEL_EXTRA_OFFSET + panelSize.y / 2.f;
                    sf::Vector2f panelPos(window.getSize().x / 2.f, panelCenterY);

                    float sliderWidth = panelSize.x * 0.55f;
                    float sliderX = panelPos.x - sliderWidth / 2.f;

                    sf::Vector2f mousef = window.mapPixelToCoords(sf::Vector2i(event.mouseMove.x, event.mouseMove.y), window.getDefaultView());
                    float p = (mousef.x - sliderX) / sliderWidth;
                    p = std::clamp(p, 0.f, 1.f);
                    if (draggingSlider == 0) { masterVolume = p * 100.f; masterMuted = (masterVolume <= 0.0f); }
                    else if (draggingSlider == 1) { musicVolume = p * 100.f; musicMuted = (musicVolume <= 0.0f); }
                    else if (draggingSlider == 2) { sfxVolume = p * 100.f; sfxMuted = (sfxVolume <= 0.0f); }
                    applyAudioSettings();
                }
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

    // Update camera to follow player with optional aiming zoom and slight shift toward facing direction
    sf::Vector2f playerPosition = player.getPosition();

    // Smoothly lerp zoom multiplier toward target (depending on aiming state)
    targetZoomMul = wantsToAim ? aimZoomMul : 1.0f;
    float zLerp = 1.0f - std::exp(-zoomLerpSpeed * deltaTime);
    currentZoomMul = currentZoomMul + (targetZoomMul - currentZoomMul) * zLerp;
    // Apply zoom by setting view size relative to base size
    gameView.setSize(baseViewSize * currentZoomMul);

    // Compute facing direction using player's sprite rotation (degrees)
    float rotDeg = player.getSprite().getRotation();
    float rotRad = rotDeg * 3.14159265f / 180.0f;
    sf::Vector2f face(std::cos(rotRad), std::sin(rotRad));

    // Target center is slightly shifted toward facing direction when zoomed in
    float shiftFactor = (1.0f - currentZoomMul); // 0 when not zoomed
    sf::Vector2f targetCenter = playerPosition + face * (aimOffsetDistance * shiftFactor);

    // Smoothly interpolate center
    sf::Vector2f curCenter = gameView.getCenter();
    float cLerp = 1.0f - std::exp(-centerLerpSpeed * deltaTime);
    sf::Vector2f newCenter(curCenter.x + (targetCenter.x - curCenter.x) * cLerp,
                          curCenter.y + (targetCenter.y - curCenter.y) * cLerp);

    // Clamp camera to map bounds using current view size
    sf::Vector2f viewSize = gameView.getSize();
    float halfW = viewSize.x * 0.5f;
    float halfH = viewSize.y * 0.5f;
    newCenter.x = std::clamp(newCenter.x, halfW, static_cast<float>(mapSize.x) - halfW);
    newCenter.y = std::clamp(newCenter.y, halfH, static_cast<float>(mapSize.y) - halfH);
    gameView.setCenter(newCenter);

    // Provide LevelManager the current camera view rect (world coords)
    sf::Vector2f vc = gameView.getCenter();
    sf::Vector2f vs = gameView.getSize();
    sf::FloatRect viewRect(vc.x - vs.x/2.0f, vc.y - vs.y/2.0f, vs.x, vs.y);
    levelManager.setCameraViewRect(viewRect);

    // Automatic fire for rifle: allow continuous shooting while left mouse button is held.
    // Pistol remains single-shot
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

    // Update explosion and guts particle systems
    Props::Explosion::updateAll(deltaTime);
    Guts::updateAll(deltaTime);

    // If player died this frame, notify zombies once to stop attacking/moving
    if (player.isDead() && !playerDeathNotified) {
        levelManager.notifyPlayerDeath();
        playerDeathNotified = true;
    }

    // Start or advance game over fade when player is dead
    if (player.isDead()) {
        if (!gameOverTriggered) {
            gameOverTriggered = true;
            gameOverAlpha = 0.0f;
            // reset menu timer/alpha
            gameOverMenuTimer = 0.0f;
            gameOverMenuAlpha = 0.0f;
            gameOverHoveredIndex = -1;
            lastGameOverHovered = -1;
        }
        // advance toward 1.0 over gameOverFadeDuration seconds
        if (gameOverAlpha < 1.0f) {
            gameOverAlpha = std::min(1.0f, gameOverAlpha + deltaTime / gameOverFadeDuration);
        }
        // advance menu timer and fade the menu after a short delay
        gameOverMenuTimer += deltaTime;
        if (gameOverMenuTimer > gameOverMenuDelay) {
            float t = (gameOverMenuTimer - gameOverMenuDelay) / gameOverMenuFadeDuration;
            gameOverMenuAlpha = std::clamp(t, 0.0f, 1.0f);
        }
    }

    auto u2 = clock::now();

    // accumulate measured times into Game::run static accumulators
    using msd = std::chrono::duration<double, std::milli>;
    extern double physicsTimeMs; // forward to static in run (can't access directly); workaround: use static locals in run scope? Simpler: store to global statics defined above in anonymous namespace

    // Instead of extern, use static globals defined here to accumulate
    static double localPhysicsAccum = 0.0;
    static double localLevelAccum = 0.0;
    localPhysicsAccum += msd(u1 - u0).count();
    localLevelAccum += msd(u2 - u1).count();

    // debug
    if (levelManager.getDebugLogging()) {
        std::cout << "[UpdateTiming] physics(ms)=" << msd(u1 - u0).count()
                  << " level(ms)=" << msd(u2 - u1).count()
                  << std::endl;
    }

    checkZombiePlayerCollisions();
    checkPlayerBoundaries();
}

void Game::drawVictoryScreen() {
    // Use default view coordinates (screen/UI space)
    sf::Vector2u windowSize = window.getSize();

    // Title: large centered "VICTORY"
    sf::Text title;
    title.setFont(font);
    unsigned int titleSize = static_cast<unsigned int>(std::min(300.f, static_cast<float>(windowSize.y) * 0.15f));
    title.setCharacterSize(titleSize);
    title.setStyle(sf::Text::Bold);
    title.setString("VICTORY");
    title.setFillColor(sf::Color(255,255,255,255));
    title.setOutlineColor(sf::Color(0,0,0,255));
    title.setOutlineThickness(3.f);
    sf::FloatRect tbTitle = title.getLocalBounds();
    title.setOrigin(tbTitle.left + tbTitle.width * 0.5f, tbTitle.top + tbTitle.height * 0.5f);
    title.setPosition(static_cast<float>(windowSize.x) * 0.5f, static_cast<float>(windowSize.y) * 0.34f);
    window.draw(title);

    // Label
    sf::Text exitText;
    exitText.setFont(font);
    unsigned int exitCharSize = static_cast<unsigned int>(std::max(70.f, static_cast<float>(windowSize.y) * 0.055f));
    exitText.setCharacterSize(exitCharSize);
    exitText.setStyle(sf::Text::Bold);
    exitText.setString("EXIT");
    exitText.setFillColor(sf::Color::White);
    exitText.setOutlineColor(sf::Color::Black);
    exitText.setOutlineThickness(2.f);

    sf::FloatRect tb = exitText.getLocalBounds();
    exitText.setOrigin(tb.left + tb.width * 0.5f, tb.top + tb.height * 0.5f);
    // Position EXIT below the title
    sf::Vector2f pos(static_cast<float>(windowSize.x) * 0.5f, static_cast<float>(windowSize.y) * 0.55f);
    exitText.setPosition(pos);

    // padded hit rect
    const float padX = 18.f;
    const float padY = 10.f;
    s_victoryExitRect.left = pos.x - tb.width * 0.5f - padX;
    s_victoryExitRect.top = pos.y - tb.height * 0.5f - padY;
    s_victoryExitRect.width = tb.width + padX * 2.f;
    s_victoryExitRect.height = tb.height + padY * 2.f;

    // Determine hover using default-view mouse coords. Use a slightly enlarged
    // hover zone so the user has an easier target when the text scales up.
    const float hoverScale = 1.06f;
    sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
    sf::Vector2f mouseUI = window.mapPixelToCoords(mousePixel, window.getDefaultView());
    float scaledW = tb.width * hoverScale;
    float scaledH = tb.height * hoverScale;
    sf::FloatRect hoverRect(pos.x - scaledW * 0.5f - padX, pos.y - scaledH * 0.5f - padY, scaledW + padX * 2.f, scaledH + padY * 2.f);
    bool hovered = hoverRect.contains(mouseUI);
    if (hovered && !s_victoryWasHovered) { if (uiHoverSound.getBuffer()) uiHoverSound.play(); }
    s_victoryWasHovered = hovered;

    // Choose final scale based on hover state and update the stored hit rect
    float finalScale = hovered ? hoverScale : 1.0f;
    float displayW = tb.width * finalScale;
    float displayH = tb.height * finalScale;
    s_victoryExitRect.left = pos.x - displayW * 0.5f - padX;
    s_victoryExitRect.top = pos.y - displayH * 0.5f - padY;
    s_victoryExitRect.width = displayW + padX * 2.f;
    s_victoryExitRect.height = displayH + padY * 2.f;

    // Tint and scale the text when hovered (no box)
    exitText.setScale(finalScale, finalScale);
    exitText.setFillColor(hovered ? sf::Color::Yellow : sf::Color::White);
    window.draw(exitText);
}

void Game::render() {
    window.clear();
    window.setView(gameView);

    if (!levelManager.isLevelTransitioning()) {
        int currentLevel = levelManager.getCurrentLevel();
        window.draw(getMapSprite(currentLevel));
        // draw persistent ground decals (stains) under entities
        Props::Explosion::renderGround(window);
        
        // Set interpolation alpha for player and zombies before they are rendered
        player.setRenderAlpha(renderAlpha);
        auto& zombies = levelManager.getZombies();
        for (auto* z : zombies) {
            z->setRenderAlpha(renderAlpha);
        }

        // Render the full level (tiles, zombies, props) so zombies are visible
        levelManager.render(window);

        // Draw spread cone under the player upper sprite (world-space)
        // Skip drawing if player is dead
        if (!player.isDead()) {
            sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
            sf::Vector2f worldMouse = window.mapPixelToCoords(mousePixel, gameView);
            float inaccuracyDeg = player.getCurrentSpreadDeg();
            if (inaccuracyDeg > 0.0001f) {
                float spreadRad = inaccuracyDeg * 3.14159265f / 180.0f;
                float coneLen = 240.0f; // shorter cone
                sf::Vector2f muzzlePos = player.getMuzzlePosition(worldMouse);
                float baseDeg = player.getSprite().getRotation();
                float baseRad = baseDeg * 3.14159265f / 180.0f;
                sf::Vector2f leftP(muzzlePos.x + std::cos(baseRad - spreadRad) * coneLen, muzzlePos.y + std::sin(baseRad - spreadRad) * coneLen);
                sf::Vector2f rightP(muzzlePos.x + std::cos(baseRad + spreadRad) * coneLen, muzzlePos.y + std::sin(baseRad + spreadRad) * coneLen);

                sf::VertexArray fan(sf::TriangleFan);
                sf::Color centerC(255,255,255,120);
                sf::Color outerC(255,255,255,16);
                fan.append(sf::Vertex(muzzlePos, centerC));
                fan.append(sf::Vertex(leftP, outerC));
                fan.append(sf::Vertex(rightP, outerC));
                window.draw(fan);

                sf::Vertex side1[2] = { sf::Vertex(muzzlePos, sf::Color(255,255,255,120)), sf::Vertex(leftP, sf::Color(255,255,255,80)) };
                sf::Vertex side2[2] = { sf::Vertex(muzzlePos, sf::Color(255,255,255,120)), sf::Vertex(rightP, sf::Color(255,255,255,80)) };
                window.draw(side1, 2, sf::Lines);
                window.draw(side2, 2, sf::Lines);
            }
        }

        // Sync player's debug origins flag with global debug toggle so muzzle markers follow backtick
        player.debugDrawOrigins = debugDrawHitboxes;
        player.render(window);
        // Now draw bullets so they appear over the player sprite
        levelManager.renderBullets(window, bullets, renderAlpha);

        // Draw reload prompt panel under the player if they are out of ammo and not currently reloading
        if (player.getCurrentAmmo() <= 0 && !player.isReloading()) {
            sf::Vector2f ppos = player.getPosition();
            // Panel dimensions in world space
            float panelW = 95.f;
            float panelH = 28.f;
            float yOffset = 54.f; // distance below player

            sf::RectangleShape panel(sf::Vector2f(panelW, panelH));
            panel.setOrigin(panelW * 0.5f, panelH * 0.5f);
            panel.setPosition(ppos.x, ppos.y + yOffset);
            // Use a more transparent background so it doesn't block the view
            panel.setFillColor(sf::Color(0, 0, 0, 120));
            // Softer outline
            panel.setOutlineColor(sf::Color(255, 255, 255, 140));
            panel.setOutlineThickness(1.5f);
            window.draw(panel);

            // Draw key icon on left (if available)
            float iconPad = 8.f;
            float iconH = panelH * 0.72f;
            if (reloadKeyTexture.getSize().x > 0 && reloadKeySprite.getTexture() != nullptr) {
                sf::Sprite ks = reloadKeySprite;
                sf::Vector2u kts = reloadKeyTexture.getSize();
                float scale = iconH / static_cast<float>(kts.y);
                ks.setScale(scale, scale);
                float iconX = ppos.x - panelW * 0.5f + iconPad;
                float iconY = ppos.y + yOffset - panelH * 0.5f + (panelH - kts.y * scale) * 0.5f;
                ks.setPosition(iconX, iconY);
                window.draw(ks);
            } else {
                // fallback: draw a simple 'R' box
                sf::RectangleShape keyBox(sf::Vector2f(iconH, iconH));
                keyBox.setFillColor(sf::Color(30,30,30,140));
                keyBox.setOutlineColor(sf::Color(255,255,255,140));
                keyBox.setOutlineThickness(1.f);
                float iconX = ppos.x - panelW * 0.5f + iconPad;
                float iconY = ppos.y + yOffset - panelH * 0.5f + (panelH - iconH) * 0.5f;
                keyBox.setPosition(iconX, iconY);
                window.draw(keyBox);
                sf::Text keyLetter;
                keyLetter.setFont(font);
                keyLetter.setCharacterSize(static_cast<unsigned int>(iconH * 0.6f));
                keyLetter.setFillColor(sf::Color::White);
                keyLetter.setString("R");
                sf::FloatRect kb = keyLetter.getLocalBounds();
                keyLetter.setPosition(iconX + (iconH - kb.width) * 0.5f - kb.left, iconY + (iconH - kb.height) * 0.5f - kb.top);
                window.draw(keyLetter);
            }

            // Draw reload prompt text to the right of the icon
            sf::Text reloadText;
            if (font2.getInfo().family.empty()) reloadText.setFont(font);
            else reloadText.setFont(font2);
            reloadText.setCharacterSize(12);
            reloadText.setFillColor(sf::Color::White);
            reloadText.setOutlineColor(sf::Color::Black);
            reloadText.setOutlineThickness(1.f);
            reloadText.setString("RELOAD");
            // compute text position
            float textX = ppos.x - panelW * 0.5f + iconPad + iconH + 8.f;
            float textY = ppos.y + yOffset - panelH * 0.5f + (panelH - reloadText.getLocalBounds().height) * 0.5f - reloadText.getLocalBounds().top;
            reloadText.setPosition(textX, textY);
            window.draw(reloadText);
        }

        // Draw explosions and guts splatters (world-space) AFTER entities so airborne particles appear above zombies
        // They use world coordinates, so keep the gameView when rendering them.
        Props::Explosion::renderAll(window);
        Guts::renderAll(window);
    }

    // Draw world-space overlays (HUD/UI) using default view
    window.setView(window.getDefaultView());

    // Apply a simple grayscale overlay to desaturate the world (keeps UI and blood overlay unaffected).
    float healthPercent = 1.0f;
    if (player.getMaxHealth() > 0.0f) healthPercent = std::clamp(player.getCurrentHealth() / player.getMaxHealth(), 0.0f, 1.0f);
    // non-linear ramp for desaturation (increase toward low health)
    // Use desaturatePow to slow the ramp: t = (1 - health)^desaturatePow
    float desatAmount = std::pow(1.0f - healthPercent, desaturatePow);
    desatAmount = std::clamp(desatAmount, 0.0f, 1.0f);
    if (desatAmount > 0.001f) {
        // Reduce maximum strength so it never becomes too gray. Use a lighter base and lower max alpha.
        const float MAX_ALPHA = 100.0f; // slightly lower for smoother ramp
        sf::Uint8 a = static_cast<sf::Uint8>(std::clamp(desatAmount * MAX_ALPHA, 0.0f, MAX_ALPHA));
        sf::RectangleShape desatRect(sf::Vector2f(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)));
        // Use a lighter gray so the effect is subtle
        desatRect.setFillColor(sf::Color(180,180,180,a));
        desatRect.setPosition(0.f, 0.f);
        window.draw(desatRect);
    }

    // Draw blood overlay (screen-space) based on player's health (below UI)
    if (bloodTexture.getSize().x > 0) {
        float healthPercent = 1.0f;
        if (player.getMaxHealth() > 0.0f) healthPercent = std::clamp(player.getCurrentHealth() / player.getMaxHealth(), 0.0f, 1.0f);
        // intensity grows as health decreases.
        float t = std::pow(1.0f - healthPercent, bloodIntensityPow);
        t = std::clamp(t, 0.0f, 1.0f);
        float alphaF = bloodMaxAlpha * t; // 0..bloodMaxAlpha
        sf::Uint8 a = static_cast<sf::Uint8>(std::clamp(alphaF, 0.0f, 255.0f));
        bloodSprite.setColor(sf::Color(255,255,255,a));
        // scale sprite to cover current window size
        sf::Vector2u ts = bloodTexture.getSize();
        if (ts.x > 0 && ts.y > 0) {
            float sx = static_cast<float>(window.getSize().x) / static_cast<float>(ts.x);
            float sy = static_cast<float>(window.getSize().y) / static_cast<float>(ts.y);
            bloodSprite.setScale(sx, sy);
            bloodSprite.setPosition(0.f, 0.f);
            window.draw(bloodSprite);
        }
    }

    // Draw explosions and guts splatters (world-space) AFTER entities so airborne particles appear above zombies
    // They use world coordinates, so keep the gameView when rendering them.
    Props::Explosion::renderAll(window);
    Guts::renderAll(window);

    // Draw zombie count in top-left corner
    levelManager.renderUI(window, font);
    levelManager.drawHUD(window, player);

    if (levelManager.getCurrentState() == GameState::VICTORY) {
        // Ensure UI/default view is active
        window.setView(window.getDefaultView());
        drawVictoryScreen();
    }

    // Determine whether OS cursor should be visible (menus, cutscenes, game-over)
    {
        GameState st = levelManager.getCurrentState();
        // Show OS cursor for level transitions and terminal states (game over / victory).
        // Tutorial and regular levels should use the custom dot cursor.
        bool showOSCursor = levelManager.isLevelTransitioning() || st == GameState::GAME_OVER || st == GameState::VICTORY;
        window.setMouseCursorVisible(showOSCursor || paused);

        // Draw custom mouse cursor only when OS cursor is hidden (gameplay)
        if (!showOSCursor && !paused) {
            sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
            sf::CircleShape cursorDot(4.0f);
            cursorDot.setOrigin(4.0f, 4.0f);
            cursorDot.setPosition(static_cast<float>(mousePixel.x), static_cast<float>(mousePixel.y));
            cursorDot.setFillColor(sf::Color::White);
            cursorDot.setOutlineColor(sf::Color::Black);
            cursorDot.setOutlineThickness(0.5f);
            window.draw(cursorDot);
        }
    }

    // If paused, draw a transparent overlay with centered pause menu text entries
    if (paused) {
        sf::RectangleShape overlay(sf::Vector2f(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)));
        overlay.setFillColor(sf::Color(0, 0, 0, 140));
        window.draw(overlay);

        // Draw simple vertical text list: Resume, Controls, Settings, Exit (no button boxes)
        std::vector<std::string> labels = {"Resume", "Controls", "Settings", "Exit"};
        // Make font size scale with window height and clamp to a reasonable max
        unsigned int fontSize = static_cast<unsigned int>(std::min(160.f, static_cast<float>(window.getSize().y) / 15.f));
        float spacing = static_cast<float>(fontSize) * 0.25f;
        float centerX = static_cast<float>(window.getSize().x) * 0.5f;
        float centerY = static_cast<float>(window.getSize().y) * 0.5f;

        // Measure total height using local bounds of a prototype text
        sf::Text proto;
        proto.setFont(font);
        proto.setCharacterSize(fontSize);
        proto.setString("Ay"); // sample to get height
        sf::FloatRect ph = proto.getLocalBounds();
        float lineH = ph.height + ph.top;
        float totalH = labels.size() * lineH + (labels.size() - 1) * spacing;
        float startY = centerY - totalH * 0.5f;

        // Pre-compute text hitboxes for all label positions
        std::vector<sf::FloatRect> labelBounds;
        labelBounds.reserve(labels.size());
        for (size_t i = 0; i < labels.size(); ++i) {
            sf::Text t;
            t.setFont(font);
            t.setCharacterSize(fontSize);
            t.setString(labels[i]);
            sf::FloatRect tb = t.getLocalBounds();
            float tx = centerX - (tb.left + tb.width / 2.f);
            float ty = startY + static_cast<float>(i) * (lineH + spacing) + lineH * 0.5f - tb.top;
            sf::FloatRect hitRect(tx + tb.left, ty + tb.top, tb.width, tb.height);
            labelBounds.push_back(hitRect);
        }

        // Determine mouse position in default view coordinates
        sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
        sf::Vector2f mouseWorld = window.mapPixelToCoords(mousePixel, window.getDefaultView());

        int hoveredIndex = -1;
        // Check which label is hovered by comparing mouse position with pre-computed hitboxes
        for (size_t i = 0; i < labels.size(); ++i) {
            if (labelBounds[i].contains(mouseWorld)) {
                hoveredIndex = static_cast<int>(i);
                break;
            }
        }
        // Play hover sound when hovered index changes
        if (hoveredIndex != lastHoveredMenuIndex) {
            if (hoveredIndex != -1 && uiHoverSound.getBuffer()) uiHoverSound.play();
            lastHoveredMenuIndex = hoveredIndex;
        }
        
        for (size_t i = 0; i < labels.size(); ++i) {
            sf::Text t;
            t.setFont(font);
            t.setCharacterSize(fontSize);
            t.setStyle(sf::Text::Bold);
            // Highlight hovered entry
            if (static_cast<int>(i) == hoveredIndex) t.setFillColor(sf::Color::Yellow);
            else t.setFillColor(sf::Color::White);
            t.setString(labels[i]);
            sf::FloatRect tb = t.getLocalBounds();
            float tx = centerX - (tb.left + tb.width / 2.f);
            float ty = startY + static_cast<float>(i) * (lineH + spacing) + lineH * 0.5f - tb.top;
            t.setPosition(tx, ty);
            window.draw(t);
        }
    }

    // If paused and showingControls, draw the controls panel matching scene layout
    if (paused && showingControls) {
        sf::Vector2f panelSize(window.getSize().x * 0.6f, window.getSize().y * 0.6f);
        sf::RectangleShape panel(panelSize);
        panel.setFillColor(sf::Color(20,20,20,255));
        panel.setOutlineColor(sf::Color::White);
        panel.setOutlineThickness(2.f);
        panel.setOrigin(panel.getSize().x/2.f, panel.getSize().y/2.f);

        float titleBottom = static_cast<float>(window.getSize().y) / 5.0f; // approximate title area
        float gap = 20.f;
        const float PANEL_EXTRA_OFFSET = 80.f;
        float panelCenterY = titleBottom + gap + PANEL_EXTRA_OFFSET + panel.getSize().y / 2.f;
        panel.setPosition(window.getSize().x/2.f, panelCenterY);
        window.draw(panel);

        sf::Text title;
        title.setFont(font); title.setCharacterSize(52); title.setFillColor(sf::Color::White);
        title.setString("Controls");
        sf::FloatRect tbb = title.getLocalBounds();
        title.setOrigin(tbb.left + tbb.width/2.f, tbb.top + tbb.height/2.f);
        title.setPosition(panel.getPosition().x, panel.getPosition().y - panel.getSize().y/2.f + 40.f);
        window.draw(title);

        // Draw a short separator line below the title with fade to transparent at the edges (matches scene.cpp)
        {
            float outline = panel.getOutlineThickness();
            float sepThickness = 2.f;
            float sepWidth = panel.getSize().x - outline * 2.f; // inner width
            float xL = panel.getPosition().x - sepWidth/2.f;
            float xR = panel.getPosition().x + sepWidth/2.f;
            float edgeFade = std::min(80.f, sepWidth * 0.18f);
            float centerLeft = xL + edgeFade;
            float centerRight = xR - edgeFade;
            float sepY = title.getPosition().y + tbb.height/2.f + 12.f;
            float yTop = sepY;
            float yBottom = sepY + sepThickness;

            sf::Color colOpaque(200,200,200,220);
            sf::Color colTransparent = colOpaque; colTransparent.a = 0;

            // center solid segment
            if (centerRight > centerLeft) {
                sf::RectangleShape centerRect(sf::Vector2f(centerRight - centerLeft, sepThickness));
                centerRect.setPosition(centerLeft, yTop);
                centerRect.setFillColor(colOpaque);
                window.draw(centerRect);
            }

            // left fading quad (two triangles)
            if (centerLeft > xL + 1.f) {
                sf::VertexArray leftGrad(sf::Triangles, 6);
                leftGrad[0] = sf::Vertex(sf::Vector2f(xL, yTop), colTransparent);
                leftGrad[1] = sf::Vertex(sf::Vector2f(centerLeft, yTop), colOpaque);
                leftGrad[2] = sf::Vertex(sf::Vector2f(centerLeft, yBottom), colOpaque);
                leftGrad[3] = sf::Vertex(sf::Vector2f(xL, yTop), colTransparent);
                leftGrad[4] = sf::Vertex(sf::Vector2f(centerLeft, yBottom), colOpaque);
                leftGrad[5] = sf::Vertex(sf::Vector2f(xL, yBottom), colTransparent);
                window.draw(leftGrad);
            }

            // right fading quad (two triangles)
            if (centerRight < xR - 1.f) {
                sf::VertexArray rightGrad(sf::Triangles, 6);
                rightGrad[0] = sf::Vertex(sf::Vector2f(centerRight, yTop), colOpaque);
                rightGrad[1] = sf::Vertex(sf::Vector2f(xR, yTop), colTransparent);
                rightGrad[2] = sf::Vertex(sf::Vector2f(xR, yBottom), colTransparent);
                rightGrad[3] = sf::Vertex(sf::Vector2f(centerRight, yTop), colOpaque);
                rightGrad[4] = sf::Vertex(sf::Vector2f(xR, yBottom), colTransparent);
                rightGrad[5] = sf::Vertex(sf::Vector2f(centerRight, yBottom), colOpaque);
                window.draw(rightGrad);
            }
        }

        // draw controls two-column list
        float panelPadding = 28.f;
        float columnGap = 24.f;
        float innerLeft = panel.getPosition().x - panel.getSize().x/2.f + panelPadding;
        float innerRight = panel.getPosition().x + panel.getSize().x/2.f - panelPadding;
        float panelInnerWidth = innerRight - innerLeft;
        float colWidth = (panelInnerWidth - columnGap) * 0.5f;
        const float iconSlotW = 88.f;
        float leftColX = innerLeft;
        float rightColX = innerLeft + colWidth + columnGap;

        std::vector<std::string> leftLabels = {"Up","Left","Down","Right","Sprint (Hold)"};
        std::vector<std::string> rightLabels = {"Fire","Aim (Hold)","Reload","Rifle","Pistol"};

        sf::Text label; label.setFont(font2); label.setCharacterSize(24); label.setFillColor(sf::Color::White);
        float headerY = title.getPosition().y + 100.f;
        // Column headers (draw above the separator lines)
        sf::Text leftHeader; leftHeader.setFont(font); leftHeader.setCharacterSize(36); leftHeader.setFillColor(sf::Color::White); leftHeader.setString("Movement");
        sf::FloatRect lh = leftHeader.getLocalBounds(); leftHeader.setOrigin(lh.left + lh.width/2.f, lh.top + lh.height/2.f);
        leftHeader.setPosition(leftColX + colWidth * 0.25f, headerY);
        window.draw(leftHeader);

        sf::Text rightHeader; rightHeader.setFont(font); rightHeader.setCharacterSize(36); rightHeader.setFillColor(sf::Color::White); rightHeader.setString("Combat");
        sf::FloatRect rh = rightHeader.getLocalBounds(); rightHeader.setOrigin(rh.left + rh.width/2.f, rh.top + rh.height/2.f);
        rightHeader.setPosition(rightColX + colWidth * 0.25f, headerY);
        window.draw(rightHeader);

        float startY = headerY + 24.f;
        // Shift content (bars, icons, vertical separators) downward between
        // the top separator (below the title) and the back-button separator.
        // Adjust this value to control spacing.
        const float contentShift = 8.f;
        startY += contentShift;

        float barHeight = 56.f; float spacing = 14.f;

        // Helper to draw a faded horizontal line (center segment + fades at edges)
        auto drawFadedHLine = [&](float xLeft, float width, float y, float thickness) {
            float xRight = xLeft + width;
            float edgeFade = std::min(40.f, width * 0.18f);
            float centerL = xLeft + edgeFade;
            float centerR = xRight + 8.f;
            float sepY = y + thickness / 2.f;

            sf::Color colOpaque(200,200,200,220);
            sf::Color colTransparent = colOpaque; colTransparent.a = 0;

            // center solid segment
            if (centerR > centerL) {
                sf::RectangleShape centerRect(sf::Vector2f(centerR - centerL, thickness));
                centerRect.setPosition(centerL, y);
                centerRect.setFillColor(colOpaque);
                window.draw(centerRect);
            }

            // left fading quad (two triangles)
            if (centerL > xLeft + 1.f) {
                sf::VertexArray leftGrad(sf::Triangles, 6);
                leftGrad[0] = sf::Vertex(sf::Vector2f(xLeft, y), colTransparent);
                leftGrad[1] = sf::Vertex(sf::Vector2f(centerL, y), colOpaque);
                leftGrad[2] = sf::Vertex(sf::Vector2f(centerL, y + thickness), colOpaque);
                leftGrad[3] = sf::Vertex(sf::Vector2f(xLeft, y), colTransparent);
                leftGrad[4] = sf::Vertex(sf::Vector2f(centerL, y + thickness), colOpaque);
                leftGrad[5] = sf::Vertex(sf::Vector2f(xLeft, y + thickness), colTransparent);
                window.draw(leftGrad);
            }
        };

        // Draw top horizontal lines above both columns
        float topLineY = startY - 12.f; // slightly above first row
        drawFadedHLine(leftColX, colWidth, topLineY, 2.f);
        drawFadedHLine(rightColX, colWidth, topLineY, 2.f);

        // Helper to draw a faded vertical line (center segment + fades at top/bottom)
        auto drawFadedVLine = [&](float x, float yTop, float height, float thickness){
            float edgeFade = std::min(40.f, height * 0.18f);
            float cyT = yTop;
            float cyB = yTop + height - edgeFade;
            sf::Color colOpaque(200,200,200,200);
            sf::Color colTransparent = colOpaque; colTransparent.a = 0;
            // center solid segment
            if (cyB > cyT) {
                sf::RectangleShape centerRect(sf::Vector2f(thickness, cyB - cyT));
                centerRect.setPosition(x - thickness * 0.5f + 8.f, cyT);
                centerRect.setFillColor(colOpaque);
                window.draw(centerRect);
            }
            // bottom fade
            if (edgeFade >= 1.f) {
                const float FADE_H = 36.f;
                float yB = yTop + height + FADE_H;
                sf::VertexArray botGrad(sf::Triangles, 6);
                botGrad[0] = sf::Vertex(sf::Vector2f(x - thickness*0.5f + 8.f, cyB), colOpaque);
                botGrad[1] = sf::Vertex(sf::Vector2f(x + thickness*0.5f + 8.f, cyB), colOpaque);
                botGrad[2] = sf::Vertex(sf::Vector2f(x + thickness*0.5f + 8.f, yB), colTransparent);
                botGrad[3] = sf::Vertex(sf::Vector2f(x - thickness*0.5f + 8.f, cyB), colOpaque);
                botGrad[4] = sf::Vertex(sf::Vector2f(x + thickness*0.5f + 8.f, yB), colTransparent);
                botGrad[5] = sf::Vertex(sf::Vector2f(x - thickness*0.5f + 8.f, yB), colTransparent);
                window.draw(botGrad);
            }
        };

        // Draw vertical lines at the right edge of each column
        float vTop = startY - 12.f;
        float vHeight = std::min(140.f, panel.getSize().y * 0.04f);
        float vThickness = 2.f;
        // right edge of left column
        float xLeftColEdge = leftColX + colWidth;
        drawFadedVLine(xLeftColEdge, vTop, vHeight, vThickness);
        // right edge of right column
        float xRightColEdge = rightColX + colWidth;
        drawFadedVLine(xRightColEdge, vTop, vHeight, vThickness);

        for (size_t i = 0; i < leftLabels.size(); ++ i) {
            float y = startY + static_cast<float>(i) * (barHeight + spacing);
            // bar background
            sf::RectangleShape bar(sf::Vector2f(colWidth, barHeight));
            bar.setPosition(leftColX, y);
            bar.setFillColor(sf::Color(36,36,36,220));
            bar.setOutlineColor(sf::Color(100,100,100,200));
            bar.setOutlineThickness(1.f);
            window.draw(bar);

            // icon slot placeholder on left (draw icon if available)
            float slotH = barHeight - 12.f;
            sf::RectangleShape iconSlot(sf::Vector2f(iconSlotW, slotH));
            iconSlot.setPosition(leftColX + 8.f, y + 6.f);
            iconSlot.setFillColor(sf::Color(24,24,24,200));
            iconSlot.setOutlineThickness(1.f);
            iconSlot.setOutlineColor(sf::Color(80,80,80,200));
            if (i < pauseIconSprites.size() && pauseIconTextures[i].getSize().x > 0) {
                sf::Sprite s = pauseIconSprites[i];
                sf::Vector2u tsize = pauseIconTextures[i].getSize();
                float scale = std::min(slotH / static_cast<float>(tsize.y), iconSlotW / static_cast<float>(tsize.x));
                s.setScale(scale, scale);
                float spriteW = tsize.x * scale;
                float spriteX = leftColX + 8.f + (iconSlotW - spriteW) * 0.5f;
                float spriteY = y + 6.f + (slotH - tsize.y * scale) * 0.5f;
                s.setPosition(spriteX, spriteY);
                window.draw(s);
            }
            else {
                window.draw(iconSlot);
            }
            // label (right-aligned within bar, leaving padding)
            label.setString(leftLabels[i]);
            sf::FloatRect lb = label.getLocalBounds();
            float textRightPadding = 12.f;
            float textX = leftColX + colWidth - textRightPadding - lb.width - lb.left;
            float textY = y + (barHeight - lb.height) / 2.f - lb.top;
            label.setPosition(textX, textY);
            window.draw(label);
        }

        for (size_t i = 0; i < rightLabels.size(); ++i) {
            float y = startY + static_cast<float>(i) * (barHeight + spacing);
            // bar background
            sf::RectangleShape bar(sf::Vector2f(colWidth, barHeight));
            bar.setPosition(rightColX, y);
            bar.setFillColor(sf::Color(36,36,36,220));
            bar.setOutlineColor(sf::Color(100,100,100,200));
            bar.setOutlineThickness(1.f);
            window.draw(bar);


            // icon slot placeholder on left of this bar (draw icon if available)
            float slotH_R = barHeight - 12.f;
            sf::RectangleShape iconSlotR(sf::Vector2f(iconSlotW, slotH_R));
            iconSlotR.setPosition(rightColX + 8.f, y + 6.f);
            iconSlotR.setFillColor(sf::Color(24,24,24,200));
            iconSlotR.setOutlineThickness(1.f);
            iconSlotR.setOutlineColor(sf::Color(80,80,80,200));
            size_t iconIndexR = leftLabels.size() + i;
            if (iconIndexR < pauseIconSprites.size() && pauseIconTextures[iconIndexR].getSize().x > 0) {
                sf::Sprite s = pauseIconSprites[iconIndexR];
                sf::Vector2u tsize = pauseIconTextures[iconIndexR].getSize();
                float scale = std::min(slotH_R / static_cast<float>(tsize.y), iconSlotW / static_cast<float>(tsize.x));
                s.setScale(scale, scale);
                float spriteW = tsize.x * scale;
                float spriteX = rightColX + 8.f + (iconSlotW - spriteW) * 0.5f;
                float spriteY = y + 6.f + (slotH_R - tsize.y * scale) * 0.5f;
                s.setPosition(spriteX, spriteY);
                window.draw(s);
            } else {
                window.draw(iconSlotR);
            }

            // label (right-aligned within bar)
            label.setString(rightLabels[i]);
            sf::FloatRect lb = label.getLocalBounds();
            float textRightPadding = 12.f;
            float textX = rightColX + colWidth - textRightPadding - lb.width - lb.left;
            float textY = y + (barHeight - lb.height) / 2.f - lb.top;
            label.setPosition(textX, y + (barHeight - lb.height) / 2.f - lb.top);
            window.draw(label);
        }

        // Back button at bottom-right inside panel
        const unsigned int BACK_CHAR_SIZE = 36u;
        const float ICON_W = 52.f;
        const float ICON_PAD = 12.f;
        sf::Text tmpBack; tmpBack.setFont(font); tmpBack.setCharacterSize(BACK_CHAR_SIZE); tmpBack.setString("Back");
        sf::FloatRect backBounds = tmpBack.getLocalBounds();
        float backWidth = backBounds.width + backBounds.left + 28.f + ICON_W + ICON_PAD;
        float backHeight = backBounds.height + backBounds.top + 20.f;
        sf::Vector2f backPos(panel.getPosition().x + panel.getSize().x/2.f - BACK_PANEL_MARGIN - backWidth,
                             panel.getPosition().y + panel.getSize().y/2.f - BACK_PANEL_MARGIN - backHeight);

        // Draw a full-width faded separator immediately above the back button (spans panel inner bounds)
        {
            float lineThickness = 2.f;
            float lineY = backPos.y - 12.f; // gap above the button
            float lx_full = innerLeft;
            float rx_full = innerRight;
            float edgeFadeLocal = std::min(40.f, (rx_full - lx_full) * 0.18f);
            float centerL = lx_full + edgeFadeLocal;
            float centerR = rx_full - edgeFadeLocal;
            sf::Color colOpaque(200,200,200,220);
            sf::Color colTransparentLocal = colOpaque; colTransparentLocal.a = 0;
            // center solid segment
            if (centerR > centerL) {
                sf::RectangleShape centerRect(sf::Vector2f(centerR - centerL, lineThickness));
                centerRect.setPosition(centerL, lineY);
                centerRect.setFillColor(colOpaque);
                window.draw(centerRect);
            }
            // left fade
            if (centerL > lx_full + 1.f) {
                sf::VertexArray leftGrad(sf::Triangles, 6);
                leftGrad[0] = sf::Vertex(sf::Vector2f(lx_full, lineY), colTransparentLocal);
                leftGrad[1] = sf::Vertex(sf::Vector2f(centerL, lineY), colOpaque);
                leftGrad[2] = sf::Vertex(sf::Vector2f(centerL, lineY + lineThickness), colOpaque);
                leftGrad[3] = sf::Vertex(sf::Vector2f(lx_full, lineY), colTransparentLocal);
                leftGrad[4] = sf::Vertex(sf::Vector2f(centerL, lineY + lineThickness), colOpaque);
                leftGrad[5] = sf::Vertex(sf::Vector2f(lx_full, lineY + lineThickness), colTransparentLocal);
                window.draw(leftGrad);
            }
            // right fade
            if (centerR < rx_full - 1.f) {
                sf::VertexArray rightGrad(sf::Triangles, 6);
                rightGrad[0] = sf::Vertex(sf::Vector2f(centerR, lineY), colOpaque);
                rightGrad[1] = sf::Vertex(sf::Vector2f(rx_full, lineY), colTransparentLocal);
                rightGrad[2] = sf::Vertex(sf::Vector2f(rx_full, lineY + lineThickness), colTransparentLocal);
                rightGrad[3] = sf::Vertex(sf::Vector2f(centerR, lineY), colOpaque);
                rightGrad[4] = sf::Vertex(sf::Vector2f(rx_full, lineY + lineThickness), colTransparentLocal);
                rightGrad[5] = sf::Vertex(sf::Vector2f(centerR, lineY + lineThickness), colOpaque);
                window.draw(rightGrad);
            }
        }

        // detect hover over back button using default view coordinates
        sf::Vector2i mousePixelUI = sf::Mouse::getPosition(window);
        sf::Vector2f mouseUI = window.mapPixelToCoords(mousePixelUI, window.getDefaultView());
        sf::FloatRect backRect(backPos.x, backPos.y, backWidth, backHeight);
        bool backHovered = backRect.contains(mouseUI);
        // play hover sound once when hovering begins
        if (backHovered && !lastBackHovered) { if (uiHoverSound.getBuffer()) uiHoverSound.play(); }
        lastBackHovered = backHovered;

        // draw back button border (highlight when hovered)
        sf::RectangleShape backBorder(sf::Vector2f(backWidth, backHeight));
        backBorder.setFillColor(sf::Color::Transparent);
        backBorder.setOutlineColor(backHovered ? sf::Color::Yellow : sf::Color::White);
        backBorder.setOutlineThickness(2.f);
        backBorder.setOrigin(0.f, 0.f);
        backBorder.setPosition(backPos);
        window.draw(backBorder);

        // Icon slot (left side of back button)
        float slotW = ICON_W;
        float slotH = backHeight - 12.f;
        sf::RectangleShape iconSlot(sf::Vector2f(slotW, slotH));
        iconSlot.setPosition(backPos.x + 8.f, backPos.y + 6.f);
        iconSlot.setFillColor(sf::Color(24,24,24,200));
        iconSlot.setOutlineThickness(1.f);
        iconSlot.setOutlineColor(sf::Color(80,80,80,200));
        if (backIconTexture.getSize().x > 0) {
            sf::Vector2u bts = backIconTexture.getSize();
            float scale = std::min(slotH / static_cast<float>(bts.y), slotW / static_cast<float>(bts.x));
            backIconSprite.setScale(scale, scale);
            float spriteW = bts.x * scale;
            float spriteX = backPos.x + 8.f + (slotW - spriteW) * 0.5f;
            float spriteY = backPos.y + 6.f + (slotH - bts.y * scale) * 0.5f;
            backIconSprite.setPosition(spriteX, spriteY);
            if (backHovered) { sf::Color prevCol = backIconSprite.getColor(); backIconSprite.setColor(sf::Color(255,255,180)); window.draw(backIconSprite); backIconSprite.setColor(prevCol); }
            else window.draw(backIconSprite);
        } else {
            window.draw(iconSlot);
        }

        sf::Text backText; backText.setFont(font); backText.setCharacterSize(BACK_CHAR_SIZE); backText.setString("Back"); backText.setFillColor(backHovered ? sf::Color::Yellow : sf::Color::White);
        sf::FloatRect tb = backText.getLocalBounds();
        float tx = backPos.x + 8.f + ICON_W + ICON_PAD;
        float ty = backPos.y + (backHeight - tb.height) / 2.f - tb.top;
        backText.setPosition(tx, ty);
        window.draw(backText);
    }

    // If paused and showingSettings, draw the settings panel (matches controls layout style)
    if (paused && showingSettings) {
        sf::Vector2f panelSize(window.getSize().x * 0.6f, window.getSize().y * 0.6f);
        sf::RectangleShape panel(panelSize);
        panel.setFillColor(sf::Color(20, 20, 20, 255));
        panel.setOutlineColor(sf::Color::White);
        panel.setOutlineThickness(2.f);
        panel.setOrigin(panel.getSize().x / 2.f, panel.getSize().y / 2.f);

        float titleBottom = static_cast<float>(window.getSize().y) / 5.0f; // approximate title area
        float gap = 20.f;
        const float PANEL_EXTRA_OFFSET = 80.f;
        float panelCenterY = titleBottom + gap + PANEL_EXTRA_OFFSET + panel.getSize().y / 2.f;
        panel.setPosition(window.getSize().x / 2.f, panelCenterY);
        window.draw(panel);

        sf::Text title;
        title.setFont(font); title.setCharacterSize(52); title.setFillColor(sf::Color::White);
        title.setString("Settings");
        sf::FloatRect tbb = title.getLocalBounds();
        title.setOrigin(tbb.left + tbb.width / 2.f, tbb.top + tbb.height / 2.f);
        title.setPosition(panel.getPosition().x, panel.getPosition().y - panel.getSize().y / 2.f + 40.f);
        window.draw(title);

        // compute layout for sliders
        float sliderWidth = panelSize.x * 0.55f;
        float sliderX = panel.getPosition().x - sliderWidth / 2.f;
        float panelTop = panel.getPosition().y - panelSize.y / 2.f;
        float rowYStart = panelTop + 130.f;
        float rowSpacing = 60.f;

        // draw horizontal rule for header (reuse faded style)
        {
            float sepThickness = 2.f;
            float sepWidth = panel.getSize().x - panel.getOutlineThickness() * 2.f;
            float xL = panel.getPosition().x - sepWidth / 2.f;
            float xR = panel.getPosition().x + sepWidth / 2.f;
            float edgeFade = std::min(80.f, sepWidth * 0.18f);
            float centerLeft = xL + edgeFade;
            float centerRight = xR - edgeFade;
            float sepY = title.getPosition().y + tbb.height / 2.f + 12.f;
            sf::Color colOpaque(200, 200, 200, 220);
            sf::Color colTransparent = colOpaque; colTransparent.a = 0;
            if (centerRight > centerLeft) {
                sf::RectangleShape centerRect(sf::Vector2f(centerRight - centerLeft, sepThickness));
                centerRect.setPosition(centerLeft, sepY);
                centerRect.setFillColor(colOpaque);
                window.draw(centerRect);
            }
            if (centerLeft > xL + 1.f) {
                sf::VertexArray leftGrad(sf::Triangles, 6);
                leftGrad[0] = sf::Vertex(sf::Vector2f(xL, sepY), colTransparent);
                leftGrad[1] = sf::Vertex(sf::Vector2f(centerLeft, sepY), colOpaque);
                leftGrad[2] = sf::Vertex(sf::Vector2f(centerLeft, sepY + sepThickness), colOpaque);
                leftGrad[3] = sf::Vertex(sf::Vector2f(xL, sepY), colTransparent);
                leftGrad[4] = sf::Vertex(sf::Vector2f(centerLeft, sepY + sepThickness), colOpaque);
                leftGrad[5] = sf::Vertex(sf::Vector2f(xL, sepY + sepThickness), colTransparent);
                window.draw(leftGrad);
            }
            if (centerRight < xR - 1.f) {
                sf::VertexArray rightGrad(sf::Triangles, 6);
                rightGrad[0] = sf::Vertex(sf::Vector2f(centerRight, sepY), colOpaque);
                rightGrad[1] = sf::Vertex(sf::Vector2f(xR, sepY), colTransparent);
                rightGrad[2] = sf::Vertex(sf::Vector2f(xR, sepY + sepThickness), colTransparent);
                rightGrad[3] = sf::Vertex(sf::Vector2f(centerRight, sepY), colOpaque);
                rightGrad[4] = sf::Vertex(sf::Vector2f(xR, sepY + sepThickness), colTransparent);
                rightGrad[5] = sf::Vertex(sf::Vector2f(centerRight, sepY + sepThickness), colOpaque);
                window.draw(rightGrad);
            }
        }

        // Helper drawing values
        auto drawSlider = [&](const std::string& labelStr, float valuePct, float y) {
            // background bar
            float barH = 12.f;
            sf::RectangleShape bg(sf::Vector2f(sliderWidth, barH));
            bg.setPosition(sliderX, y - barH / 2.f);
            bg.setFillColor(sf::Color(60, 60, 60, 220));
            bg.setOutlineColor(sf::Color(120, 120, 120, 200));
            bg.setOutlineThickness(1.f);
            window.draw(bg);

            // filled portion
            float fillW = sliderWidth * std::clamp(valuePct, 0.f, 1.f);
            if (fillW > 0.0f) {
                sf::RectangleShape fill(sf::Vector2f(fillW, barH));
                fill.setPosition(sliderX, y - barH / 2.f);
                fill.setFillColor(sf::Color(200, 200, 200, 220));
                window.draw(fill);
            }

            // thumb
            float thumbX = sliderX + fillW;
            float thumbR = 8.f;
            sf::CircleShape thumb(thumbR);
            thumb.setOrigin(thumbR, thumbR);
            thumb.setPosition(thumbX, y);
            thumb.setFillColor(sf::Color::White);
            thumb.setOutlineColor(sf::Color(40, 40, 40));
            thumb.setOutlineThickness(1.f);
            window.draw(thumb);

            // label on left of slider
            sf::Text lbl; lbl.setFont(font2); lbl.setCharacterSize(20); lbl.setFillColor(sf::Color::White); lbl.setString(labelStr);
            sf::FloatRect lb = lbl.getLocalBounds();
            lbl.setPosition(sliderX - lb.width - 16.f, y - lb.height / 2.f - lb.top);
            window.draw(lbl);

            // numeric percent on right of slider
            sf::Text pct; pct.setFont(font2); pct.setCharacterSize(18); pct.setFillColor(sf::Color::White);
            int iv = static_cast<int>(std::round(valuePct * 100.f));
            pct.setString(std::to_string(iv) + "%");
            sf::FloatRect pb = pct.getLocalBounds();
            pct.setPosition(sliderX + sliderWidth + 16.f, y - pb.height / 2.f - pb.top);
            window.draw(pct);
            };

        // Rows: Master, Music, SFX
        float y0 = rowYStart;
        drawSlider("Master", masterMuted ? 0.f : (masterVolume / 100.f), y0);
        drawSlider("Music", musicMuted ? 0.f : (musicVolume / 100.f), y0 + rowSpacing);
        drawSlider("SFX", sfxMuted ? 0.f : (sfxVolume / 100.f), y0 + rowSpacing * 2.f);

        // Draw mute checkboxes to the right of sliders
        float muteX = sliderX + sliderWidth + 60.f;
        float muteW = 22.f, muteH = 22.f;
        auto drawMuteBox = [&](bool muted, float x, float y) {
            sf::RectangleShape box(sf::Vector2f(muteW, muteH));
            box.setPosition(x, y - muteH / 2.f);
            box.setFillColor(sf::Color(30, 30, 30, 200));
            box.setOutlineThickness(1.f);
            box.setOutlineColor(sf::Color(120, 120, 120));
            window.draw(box);
            if (muted) {
                // draw an X
                sf::VertexArray vx(sf::Lines, 4);
                sf::Color col(255, 200, 80);
                vx[0] = sf::Vertex(sf::Vector2f(x + 4.f, y - 4.f), col);
                vx[1] = sf::Vertex(sf::Vector2f(x + muteW - 4.f, y + 4.f), col);
                vx[2] = sf::Vertex(sf::Vector2f(x + 4.f, y + 4.f), col);
                vx[3] = sf::Vertex(sf::Vector2f(x + muteW - 4.f, y - 4.f), col);
                window.draw(vx);
            }
            };

        drawMuteBox(masterMuted, muteX, y0);
        drawMuteBox(musicMuted, muteX, y0 + rowSpacing);
        drawMuteBox(sfxMuted, muteX, y0 + rowSpacing * 2.f);

        // Back button at bottom-right inside settings panel (match controls)
        const unsigned int BACK_CHAR_SIZE = 36u;
        const float ICON_W = 52.f;
        const float ICON_PAD = 12.f;
        sf::Text tmpBack2; tmpBack2.setFont(font); tmpBack2.setCharacterSize(BACK_CHAR_SIZE); tmpBack2.setString("Back");
        sf::FloatRect backBounds2 = tmpBack2.getLocalBounds();
        float backWidth2 = backBounds2.width + backBounds2.left + 28.f + ICON_W + ICON_PAD;
        float backHeight2 = backBounds2.height + backBounds2.top + 20.f;
        sf::Vector2f backPos2(panel.getPosition().x + panel.getSize().x / 2.f - BACK_PANEL_MARGIN - backWidth2,
            panel.getPosition().y + panel.getSize().y / 2.f - BACK_PANEL_MARGIN - backHeight2);

        // separator above back button
        {
            float lineThickness = 2.f;
            float lineY = backPos2.y - 12.f;
            float lx_full = panel.getPosition().x - panel.getSize().x / 2.f + 28.f;
            float rx_full = panel.getPosition().x + panel.getSize().x / 2.f - 28.f;
            float edgeFadeLocal = std::min(40.f, (rx_full - lx_full) * 0.18f);
            float centerL = lx_full + edgeFadeLocal;
            float centerR = rx_full - edgeFadeLocal;
            sf::Color colOpaque(200, 200, 200, 220);
            sf::Color colTransparentLocal = colOpaque; colTransparentLocal.a = 0;
            if (centerR > centerL) {
                sf::RectangleShape centerRect(sf::Vector2f(centerR - centerL, lineThickness));
                centerRect.setPosition(centerL, lineY);
                centerRect.setFillColor(colOpaque);
                window.draw(centerRect);
            }
            if (centerL > lx_full + 1.f) {
                sf::VertexArray leftGrad(sf::Triangles, 6);
                leftGrad[0] = sf::Vertex(sf::Vector2f(lx_full, lineY), colTransparentLocal);
                leftGrad[1] = sf::Vertex(sf::Vector2f(centerL, lineY), colOpaque);
                leftGrad[2] = sf::Vertex(sf::Vector2f(centerL, lineY + lineThickness), colOpaque);
                leftGrad[3] = sf::Vertex(sf::Vector2f(lx_full, lineY), colTransparentLocal);
                leftGrad[4] = sf::Vertex(sf::Vector2f(centerL, lineY + lineThickness), colOpaque);
                leftGrad[5] = sf::Vertex(sf::Vector2f(lx_full, lineY + lineThickness), colTransparentLocal);
                window.draw(leftGrad);
            }
            if (centerR < rx_full - 1.f) {
                sf::VertexArray rightGrad(sf::Triangles, 6);
                rightGrad[0] = sf::Vertex(sf::Vector2f(centerR, lineY), colOpaque);
                rightGrad[1] = sf::Vertex(sf::Vector2f(rx_full, lineY), colTransparentLocal);
                rightGrad[2] = sf::Vertex(sf::Vector2f(rx_full, lineY + lineThickness), colTransparentLocal);
                rightGrad[3] = sf::Vertex(sf::Vector2f(centerR, lineY), colOpaque);
                rightGrad[4] = sf::Vertex(sf::Vector2f(rx_full, lineY + lineThickness), colTransparentLocal);
                rightGrad[5] = sf::Vertex(sf::Vector2f(centerR, lineY + lineThickness), colOpaque);
                window.draw(rightGrad);
            }
        }

        // detect hover over back button
        sf::Vector2i mousePixelUI2 = sf::Mouse::getPosition(window);
        sf::Vector2f mouseUI2 = window.mapPixelToCoords(mousePixelUI2, window.getDefaultView());
        sf::FloatRect backRect2(backPos2.x, backPos2.y, backWidth2, backHeight2);
        bool backHovered2 = backRect2.contains(mouseUI2);
        if (backHovered2 && !lastBackHovered) { if (uiHoverSound.getBuffer()) uiHoverSound.play(); }
        lastBackHovered = backHovered2;

        sf::RectangleShape backBorder2(sf::Vector2f(backWidth2, backHeight2));
        backBorder2.setFillColor(sf::Color::Transparent);
        backBorder2.setOutlineColor(backHovered2 ? sf::Color::Yellow : sf::Color::White);
        backBorder2.setOutlineThickness(2.f);
        backBorder2.setOrigin(0.f, 0.f);
        backBorder2.setPosition(backPos2);
        window.draw(backBorder2);

        // icon slot
        float slotW2 = ICON_W;
        float slotH2 = backHeight2 - 12.f;
        sf::RectangleShape iconSlot2(sf::Vector2f(slotW2, slotH2));
        iconSlot2.setPosition(backPos2.x + 8.f, backPos2.y + 6.f);
        iconSlot2.setFillColor(sf::Color(24, 24, 24, 200));
        iconSlot2.setOutlineThickness(1.f);
        iconSlot2.setOutlineColor(sf::Color(80, 80, 80, 200));
        if (backIconTexture.getSize().x > 0) {
            sf::Vector2u bts = backIconTexture.getSize();
            float scale = std::min(slotH2 / static_cast<float>(bts.y), slotW2 / static_cast<float>(bts.x));
            backIconSprite.setScale(scale, scale);
            float spriteW = bts.x * scale;
            float spriteX = backPos2.x + 8.f + (slotW2 - spriteW) * 0.5f;
            float spriteY = backPos2.y + 6.f + (slotH2 - bts.y * scale) * 0.5f;
            backIconSprite.setPosition(spriteX, spriteY);
            if (backHovered2) { sf::Color prevCol = backIconSprite.getColor(); backIconSprite.setColor(sf::Color(255, 255, 180)); window.draw(backIconSprite); backIconSprite.setColor(prevCol); }
            else window.draw(backIconSprite);
        }
        else {
            window.draw(iconSlot2);
        }

        sf::Text backText2; backText2.setFont(font); backText2.setCharacterSize(BACK_CHAR_SIZE); backText2.setString("Back"); backText2.setFillColor(backHovered2 ? sf::Color::Yellow : sf::Color::White);
        sf::FloatRect tb2 = backText2.getLocalBounds();
        float tx2 = backPos2.x + 8.f + ICON_W + ICON_PAD;
        float ty2 = backPos2.y + (backHeight2 - tb2.height) / 2.f - tb2.top;
        backText2.setPosition(tx2, ty2);
        window.draw(backText2);
    }

    // Draw GAME OVER text when triggered (fades in)
    if (gameOverTriggered && gameOverAlpha > 0.001f) {
        sf::Text goText;
        goText.setFont(font);
        // Make size proportional to window height
        unsigned int size = static_cast<unsigned int>(std::min(300.f, static_cast<float>(window.getSize().y) * 0.15f));
        goText.setCharacterSize(size);
        goText.setStyle(sf::Text::Bold);
        goText.setString("GAME OVER");
        // fade alpha
        sf::Uint8 ia = static_cast<sf::Uint8>(std::clamp(gameOverAlpha * 255.0f, 0.0f, 255.0f));
        goText.setFillColor(sf::Color(255, 255, 255, ia));
        goText.setOutlineColor(sf::Color(0, 0, 0, ia));
        goText.setOutlineThickness(2.f);
        sf::FloatRect gb = goText.getLocalBounds();
        goText.setOrigin(gb.left + gb.width / 2.f, gb.top + gb.height / 2.f);
        goText.setPosition(static_cast<float>(window.getSize().x) * 0.5f, static_cast<float>(window.getSize().y) * 0.30f);
        window.draw(goText);

        // Compute the title world rect (accounts for origin used above)
        sf::FloatRect titleWorldRect(
            goText.getPosition().x - gb.width * 0.5f,
            goText.getPosition().y - gb.height * 0.5f,
            gb.width,
            gb.height
        );

        // Underline parameters (scale with text size but clamped for stability)
        float underlinePadX = std::clamp(static_cast<float>(size) * 0.12f, 12.f, 64.f); // horizontal padding each side
        float underlineGap = std::clamp(static_cast<float>(size) * 0.08f, 8.f, 48.f);   // gap between text bottom and underline
        float underlineThickness = std::clamp(static_cast<float>(size) * 0.04f, 4.f, 14.f);

        // Build underline rect (draw a thin black backing first for contrast)
        sf::Vector2f ulSize(titleWorldRect.width + underlinePadX * 2.f, underlineThickness);
        sf::Vector2f ulPos(titleWorldRect.left - underlinePadX, titleWorldRect.top + titleWorldRect.height + underlineGap);

        // Backing (subtle outline/shadow)
        sf::RectangleShape ulBack(ulSize + sf::Vector2f(6.f, 3.f));
        ulBack.setPosition(ulPos - sf::Vector2f(3.f, 1.5f));
        ulBack.setFillColor(sf::Color(0, 0, 0, ia));
        window.draw(ulBack);

        // Foreground underline (white or hover-accent if you want)
        sf::RectangleShape underline(ulSize);
        underline.setPosition(ulPos);
        underline.setFillColor(sf::Color(255, 255, 255, ia));
        window.draw(underline);

  
        // Draw two simple centered menu entries under the GAME OVER title
        unsigned int itemSize = static_cast<unsigned int>(std::min(124.f, static_cast<float>(window.getSize().y) * 0.08f));
        float startY = static_cast<float>(window.getSize().y) * 0.50f;
        float lineSpacing = static_cast<float>(window.getSize().y) * 0.1f;
        const std::vector<std::string> items = { "RETRY ROUND", "EXIT" };
        // Determine mouse UI position (default view)
        sf::Vector2i mousePixel = sf::Mouse::getPosition(window);
        sf::Vector2f mouseUI = window.mapPixelToCoords(mousePixel, window.getDefaultView());

        float centerX = static_cast<float>(window.getSize().x) * 0.5f;
        int hoveredIndex = -1;

        // Use same alpha as the GAME OVER title so menu fades consistently
        sf::Uint8 itemAlpha = ia; // 'ia' was computed for GAME OVER above

        // Colors with alpha
        sf::Color fillNormal(255, 255, 255, itemAlpha);
        sf::Color fillHover(255, 255, 0, itemAlpha);
        sf::Color outlineCol(0, 0, 0, itemAlpha);
        const float outlineThickness = 2.f;

        // First pass: detect hover only (click handling stays in processInput)
        for (size_t i = 0; i < items.size(); ++i) {
            sf::Text it;
            it.setFont(font);
            it.setCharacterSize(itemSize);
            it.setStyle(sf::Text::Bold);
            it.setString(items[i]);

            sf::FloatRect ib = it.getLocalBounds();
            float y = startY + static_cast<float>(i) * lineSpacing;

            // build a centered hit rect (expanded slightly for easier targeting)
            sf::FloatRect hit(centerX - ib.width * 0.5f - 8.f,
                y - ib.height * 0.5f - 6.f,
                ib.width + 16.f,
                ib.height + 12.f);

            if (hit.contains(mouseUI)) hoveredIndex = static_cast<int>(i);
        }

        // Draw items and apply hover highlight + outline like the GAME OVER title
        for (size_t i = 0; i < items.size(); ++i) {
            sf::Text it;
            it.setFont(font);
            it.setCharacterSize(itemSize);
            it.setStyle(sf::Text::Bold);
            it.setString(items[i]);

            sf::FloatRect ib = it.getLocalBounds();
            float y = startY + static_cast<float>(i) * lineSpacing;

            // Center text by origin and position
            it.setOrigin(ib.left + ib.width * 0.5f, ib.top + ib.height * 0.5f);
            it.setPosition(centerX, y);

            // Outline and fill (outline matches GAME OVER style)
            it.setOutlineColor(outlineCol);
            it.setOutlineThickness(outlineThickness);
            it.setFillColor(static_cast<int>(i) == hoveredIndex ? fillHover : fillNormal);

            window.draw(it);
        }

        // Update hover-tracking state used by hover-sound logic elsewhere
        gameOverHoveredIndex = hoveredIndex;

        // Play hover sound when hover index changes (only when menu visible)
        if (gameOverMenuAlpha > 0.05f && gameOverHoveredIndex != lastGameOverHovered) {
            if (gameOverHoveredIndex != -1 && uiHoverSound.getBuffer()) uiHoverSound.play();
            lastGameOverHovered = gameOverHoveredIndex;
        }
    }

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
            return sf::Vector2u(2560, 2560);
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

        sf::FloatRect playerHitbox = player.getHitbox();

        // Use oriented OB box for attack -> accurate direction-dependent collision
        sf::Vector2f attackCenter, attackHalf; float attackRot;
        zb->getAttackOBB(attackCenter, attackHalf, attackRot);
        bool pzCollision = obbIntersectsAabb(attackCenter, attackHalf, attackRot, playerHitbox);
        if (pzCollision && zb->isAttacking()) {
            float damageToApply = zb->tryDealDamage();
            if (damageToApply > 0) {
                player.takeDamage(damageToApply);
                // play via pooled sounds to allow overlap
                playZombieBite();

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
    // Restart the player at the same level/round they died on.
    // Reset player state and physics, then tell LevelManager to restart the current round.
    player.reset();
    // Re-register player's physics body with the physics world in case it was removed on death
    physics.addBody(&player.getBody(), false);
    // Restart current round where the player died; use player's position for spawn reference
    levelManager.restartCurrentRound(player.getPosition());

    // Debug: print player motion state to help diagnose lingering slowdown after retry
    std::cout << "[DEBUG] Player reset: knockbackTimer=" << player.knockbackTimer
              << " knockbackMoveMultiplier=" << player.knockbackMoveMultiplier
              << " velocity=(" << player.getPhysicsPosition().x <<","<< player.getPhysicsPosition().y <<")"
              << " body.vel=(" << player.getBody().velocity.x <<","<< player.getBody().velocity.y <<")"
              << " externalImpulse=(" << player.getBody().externalImpulse.x <<","<< player.getBody().externalImpulse.y <<")"
              << " stamina=" << player.getCurrentStamina() << std::endl;
     // Reset state to allow death callback to remove body on next death
     playerBodyRemoved = false;
     playerDeathNotified = false;
     // reset game over fade state
     gameOverTriggered = false;
     gameOverAlpha = 0.0f;
     // Reset game over menu state so the UI fully hides after retry
     gameOverMenuTimer = 0.0f;
     gameOverMenuAlpha = 0.0f;
     gameOverHoveredIndex = -1;
     lastGameOverHovered = -1;
     // clear deferred input/shoot requests
     shootRequested = false;
}

sf::RenderWindow& Game::getWindow() {
    return window;
}

void Game::playZombieBite() {
    if (zombieBiteSounds.empty()) return;
    zombieBiteSounds[nextBiteSoundIndex].play();
    nextBiteSoundIndex = (nextBiteSoundIndex + 1) % zombieBiteSounds.size();
}

void Game::applyAudioSettings() {
    float masterMul = masterMuted ? 0.f : (masterVolume / 100.f);
    float musicMul = musicMuted ? 0.f : (musicVolume / 100.f);
    float sfxMul = sfxMuted ? 0.f : (sfxVolume / 100.f);

    float volMusic = masterMul * musicMul * 100.f;
    backgroundMusic.setVolume(volMusic);

    float volSfx = masterMul * sfxMul * 100.f;
    uiHoverSound.setVolume(volSfx);
    uiClickSound.setVolume(volSfx);
    for (auto &s : zombieBiteSounds) s.setVolume(volSfx);

    float combinedPercent = (masterVolume * sfxVolume) / 100.0f;
    player.setMasterSfxVolume(combinedPercent);
    player.setMasterSfxMuted(masterMuted || sfxMuted);

    // NEW: propagate to Bullet so hit/kill pools follow the same combined SFX setting.
    // Note: Bullet::setGlobalSfxVolume expects 0..100 percent
    Bullet::setGlobalSfxVolume(combinedPercent);
    Bullet::setGlobalSfxMuted(masterMuted || sfxMuted);
}