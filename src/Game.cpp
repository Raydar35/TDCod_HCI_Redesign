#include "Game.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <cmath>

Game::Game()
    : window(sf::VideoMode(800, 600), "Echoes of Valkyrie"),
    player(Vec2(400, 300)), // <-- Add this!
    points(0),
    zombiesToNextLevel(15) 
{
    window.setFramerateLimit(60);
    
    background.setFillColor(sf::Color(50, 50, 50));
    background.setSize(sf::Vector2f(1200, 1250)); // Set to max map size
    
    if (!mapTexture1.loadFromFile("TDCod/Assets/Map.png")) {
        std::cerr << "Error loading map1 texture!" << std::endl;
    }
    mapSprite1.setTexture(mapTexture1);
    
    if (!mapTexture2.loadFromFile("TDCod/Assets/Map/Map2/Map2.png")) {
        std::cerr << "Error loading map2 texture!" << std::endl;
    }
    mapSprite2.setTexture(mapTexture2);
    
    if (!mapTexture3.loadFromFile("TDCod/Assets/Map/Map3/Map3.png")) {
        std::cerr << "Error loading map3 texture!" << std::endl;
    }
    mapSprite3.setTexture(mapTexture3);
    
    if (!font.loadFromFile("TDCod/Assets/Call of Ops Duty.otf")) {
        std::cerr << "Error loading font 'TDCod/Assets/Call of Ops Duty.otf'! Trying fallback." << std::endl;
    }
    
    pointsText.setFont(font);
    pointsText.setCharacterSize(24);
    pointsText.setFillColor(sf::Color::White);
    pointsText.setPosition(10, 10);
    
    gameView.setSize(800, 600);
    gameView.setCenter(400, 300);
    
    physics.addBody(&player.getBody(), false);

    levelManager.setPhysicsWorld(&physics);
    levelManager.initialize();
    
    if (!backgroundMusic.openFromFile("TDCod/Assets/Audio/atmosphere.mp3")) {
        std::cerr << "Error loading background music! Path: TDCod/Assets/Audio/atmosphere.mp3" << std::endl;
    } else {
        backgroundMusic.setVolume(15);
    }

    if (!zombieBiteBuffer.loadFromFile("TDCod/Assets/Audio/zombiebite.mp3")) {
        std::cerr << "Error loading zombie bite sound!" << std::endl;
    } else {
        zombieBiteSound.setBuffer(zombieBiteBuffer);
    }

    if (!Bullet::bulletTexture.loadFromFile("TDCod/Assets/WeaponsItems/Bullet/Fire_small_asset.png")) {
        std::cerr << "Failed to load bullet texture!" << std::endl;
    }
}

void Game::run() {
    while (window.isOpen()) {
        processInput();
        float deltaTime = clock.restart().asSeconds();

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

        update(deltaTime);
        render();
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
        }
        
        if (event.type == sf::Event::MouseButtonPressed) {
            if (event.mouseButton.button == sf::Mouse::Left) {
                // Get mouse position in world coordinates
                sf::Vector2i mousePosition = sf::Mouse::getPosition(window);
                sf::Vector2f worldMousePosition = window.mapPixelToCoords(mousePosition, gameView);

                // Check weapon type
                if (player.getCurrentWeapon() == WeaponType::PISTOL || player.getCurrentWeapon() == WeaponType::RIFLE) {
                    if (player.timeSinceLastShot >= player.fireCooldown) {
                        player.shoot(worldMousePosition, physics, bullets);
                    }
                }
                else {
                    player.attack();
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
    physics.update(deltaTime);
    player.update(deltaTime, window, mapSize, worldMousePosition);
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
    
    checkZombiePlayerCollisions();
    checkPlayerBoundaries();
    
    sf::Vector2f playerPosition = player.getPosition();
    gameView.setCenter(playerPosition);
    
    float cameraLeft = gameView.getCenter().x - gameView.getSize().x / 2;
    float cameraTop = gameView.getCenter().y - gameView.getSize().y / 2;
    float cameraRight = gameView.getCenter().x + gameView.getSize().x / 2;
    float cameraBottom = gameView.getCenter().y + gameView.getSize().y / 2;
    
    if (cameraLeft < 0) {
        gameView.setCenter(gameView.getSize().x / 2, gameView.getCenter().y);
    }
    if (cameraTop < 0) {
        gameView.setCenter(gameView.getCenter().x, gameView.getSize().y / 2);
    }
    if (cameraRight > mapSize.x) {
        gameView.setCenter(mapSize.x - gameView.getSize().x / 2, gameView.getCenter().y);
    }
    if (cameraBottom > mapSize.y) {
        gameView.setCenter(gameView.getCenter().x, mapSize.y - gameView.getSize().y / 2);
    }
}

void Game::render() {
    window.clear();
    
    window.setView(gameView);

    if (!levelManager.isLevelTransitioning()) {
        int currentLevel = levelManager.getCurrentLevel();
        window.draw(getMapSprite(currentLevel));
        levelManager.render(window);
        for (const auto& bullet : bullets) {
            bullet->render(window); 
        }
        player.render(window);
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
            return sf::Vector2u(1000, 1250);
        case 2:
            return sf::Vector2u(1200, 1000);
        case 3:
            return sf::Vector2u(1200, 800);
        default:
            return sf::Vector2u(1000, 1250);
    }
}

void Game::checkZombiePlayerCollisions() {
    std::vector<std::unique_ptr<BaseZombie>>& zombies = levelManager.getZombies();
    
    for (auto it = zombies.begin(); it != zombies.end(); ) {
        if (!(*it)->isAlive()) {
            it = zombies.erase(it);
            continue;
        }

        sf::FloatRect playerHitbox = player.getHitbox();
        sf::FloatRect zombieHitbox = (*it)->getHitbox();

        bool pzCollision = isCollision(playerHitbox, zombieHitbox);
        if (pzCollision && (*it)->isAttacking()) {
            float damageToApply = (*it)->tryDealDamage();
            if (damageToApply > 0) {
                player.takeDamage(damageToApply);
                if (zombieBiteSound.getStatus() != sf::Sound::Playing) {
                    zombieBiteSound.play();
                }

                sf::Vector2f direction = player.getPosition() - (*it)->getPosition();
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
            bool paZCollision = isCollision(playerAttackBox, zombieHitbox);
            if (paZCollision) {
                (*it)->takeDamage(player.getAttackDamage());
                
                if (!(*it)->isAlive()) {
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