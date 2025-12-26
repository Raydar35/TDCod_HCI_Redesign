#include "Doctor.h"
#include <iostream>
#include <cmath>
#include <random>

Doctor::Doctor() : position(0, 0), currentState(DoctorState::IDLE), speed(70.0f), 
                  idleTimer(0.0f), moveTimer(0.0f), currentFrame(0), 
                  animationTimer(0.0f), animationFrameTime(0.1f) {
    loadTextures();
}

Doctor::Doctor(float x, float y) : position(x, y), currentState(DoctorState::IDLE), speed(70.0f), 
                                  idleTimer(0.0f), moveTimer(0.0f), currentFrame(0), 
                                  animationTimer(0.0f), animationFrameTime(0.1f) {
    loadTextures();
    
    // Set initial position
    sprite.setPosition(position);
    
    // Initialize with first idle texture
    if (!idleTextures.empty()) {
        sprite.setTexture(idleTextures[0]);
        sf::Vector2u textureSize = idleTextures[0].getSize();
        sprite.setOrigin(textureSize.x / 2.0f, textureSize.y / 2.0f);
        sprite.setScale(0.4f, 0.4f); // Reduced scale from 0.5f to 0.4f
    }
}

void Doctor::loadTextures() {
    // Load idle textures
    for (int i = 0; i < 8; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Doctor/DoctorIdle/Idle_knife_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading doctor idle texture: " << filePath << std::endl;
            // Use placeholder texture if loading fails
            texture.create(64, 64);
        }
        idleTextures.push_back(texture);
    }
    
    // Load walk textures
    for (int i = 0; i < 6; ++i) {
        sf::Texture texture;
        std::string filePath = "TDCod/Assets/Doctor/DoctorWalk/Walk_knife_00" + std::to_string(i) + ".png";
        if (!texture.loadFromFile(filePath)) {
            std::cerr << "Error loading doctor walk texture: " << filePath << std::endl;
            // Use placeholder texture if loading fails
            texture.create(64, 64);
        }
        walkTextures.push_back(texture);
    }
}

void Doctor::update(float deltaTime, sf::Vector2f playerPosition) {
    // Calculate distance to player
    sf::Vector2f direction = playerPosition - position;
    float distance = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    
    // If player is in range but not too close, move towards them
    // If player is in range but not too close, move towards them
    if (distance < FOLLOW_DISTANCE && distance > MIN_DISTANCE) {
        setState(DoctorState::WALK);
        
        // Normalize direction
        if (distance > 0) {
            direction /= distance;
        }
        
        // Move towards player
        position.x += direction.x * speed * deltaTime;
        position.y += direction.y * speed * deltaTime;
        
        // Rotate to face player
        float angle = std::atan2(direction.y, direction.x) * 180 / 3.14159265f;
        sprite.setRotation(angle - 90.0f); // Consistent with Player's downward-facing sprite logic
    } else {
        // Random wandering behavior when idle
        idleTimer += deltaTime;
        
        if (currentState == DoctorState::IDLE && idleTimer > 3.0f) {
            setState(DoctorState::WALK);
            idleTimer = 0.0f;
            moveTimer = 0.0f;
            
            // Generate random direction
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<> distribAngle(0, 2 * 3.14159265f);
            float angle = distribAngle(gen);
            
            moveDirection.x = std::cos(angle);
            moveDirection.y = std::sin(angle);
            
            // Set rotation
            sprite.setRotation(angle * 180 / 3.14159265f);
        } else if (currentState == DoctorState::WALK) {
            moveTimer += deltaTime;
            
            if (moveTimer > 5.0f) { // Increased random move duration
                setState(DoctorState::IDLE);
                moveTimer = 0.0f;
            } else {
                // Continue moving in random direction
                position.x += moveDirection.x * speed * deltaTime;
                position.y += moveDirection.y * speed * deltaTime;
            }
        }
    }
    
    // Update sprite position
    sprite.setPosition(position);
    
    // Update animation
    updateAnimation(deltaTime);
}

void Doctor::draw(sf::RenderWindow& window) const {
    window.draw(sprite);

    // Debug: Draw a circle at sprite's reported position (Now removed)
    // sf::CircleShape posCircle(5.f);
    // posCircle.setPosition(sprite.getPosition());
    // posCircle.setOrigin(5.f, 5.f);
    // posCircle.setFillColor(sf::Color::Cyan);
    // window.draw(posCircle);

    // Debug: Draw hitbox
    //sf::RectangleShape hitboxShape;
    //sf::FloatRect hitbox = getHitbox(); // This is an AABB
    //// To center the AABB shape on the sprite's origin (cyan circle)
    //hitboxShape.setPosition(
    //    sprite.getPosition().x - hitbox.width / 2.0f,
    //    sprite.getPosition().y - hitbox.height / 2.0f
    //);
    //hitboxShape.setSize(sf::Vector2f(hitbox.width, hitbox.height));
    //hitboxShape.setFillColor(sf::Color(0, 255, 255, 70)); // Cyan, semi-transparent
    //hitboxShape.setOutlineColor(sf::Color::Cyan);
    //hitboxShape.setOutlineThickness(1);
    //window.draw(hitboxShape);
}

sf::Vector2f Doctor::getPosition() const {
    return position;
}

sf::FloatRect Doctor::getBounds() const {
    return sprite.getGlobalBounds();
}

void Doctor::setState(DoctorState newState) {
    if (currentState != newState) {
        currentState = newState;
        currentFrame = 0;
        animationTimer = 0.0f;
        
        // Set texture based on state
        if (currentState == DoctorState::IDLE && !idleTextures.empty()) {
            sprite.setTexture(idleTextures[0]);
        } else if (currentState == DoctorState::WALK && !walkTextures.empty()) {
            sprite.setTexture(walkTextures[0]);
        }
    }
}

sf::FloatRect Doctor::getHitbox() const {
    // Define a desired hitbox size in *scaled* pixels
    const float desiredScaledWidth = 30.0f;
    const float desiredScaledHeight = 30.0f;

    // Doctor scale is 0.4f (from constructor)
    float localHitboxWidth = desiredScaledWidth / 0.4f;  // 30 / 0.4 = 75.0
    float localHitboxHeight = desiredScaledHeight / 0.4f; // 30 / 0.4 = 75.0

    // Create a local hitbox centered around the sprite's origin (0,0 in local origin-adjusted space)
    sf::FloatRect localHitbox(
        -localHitboxWidth / 2.0f,
        -localHitboxHeight / 2.0f,
        localHitboxWidth,
        localHitboxHeight
    );
    
    // Transform this local hitbox to global coordinates
    return sprite.getTransform().transformRect(localHitbox);
}

void Doctor::updateAnimation(float deltaTime) {
    animationTimer += deltaTime;
    
    if (animationTimer >= animationFrameTime) {
        animationTimer -= animationFrameTime;
        
        // Update frame based on current state
        if (currentState == DoctorState::IDLE) {
            currentFrame = (currentFrame + 1) % idleTextures.size();
            if (!idleTextures.empty()) {
                sprite.setTexture(idleTextures[currentFrame]);
            }
        } else if (currentState == DoctorState::WALK) {
            currentFrame = (currentFrame + 1) % walkTextures.size();
            if (!walkTextures.empty()) {
                sprite.setTexture(walkTextures[currentFrame]);
            }
        }
    }
}
