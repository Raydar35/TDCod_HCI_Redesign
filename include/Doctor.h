#ifndef DOCTOR_H
#define DOCTOR_H

#include <SFML/Graphics.hpp>
#include <vector>

enum class DoctorState {
    IDLE,
    WALK
};

class Doctor {
public:
    Doctor();
    Doctor(float x, float y);
    
    void update(float deltaTime, sf::Vector2f playerPosition);
    void draw(sf::RenderWindow& window) const;
    
    sf::Vector2f getPosition() const;
    sf::FloatRect getBounds() const;
    sf::FloatRect getHitbox() const; // Added for debug drawing
    
private:
    sf::Sprite sprite;
    sf::Vector2f position;
    DoctorState currentState;
    
    // Movement properties
    float speed;
    float idleTimer;
    float moveTimer;
    sf::Vector2f moveDirection;
    
    // Animation properties
    std::vector<sf::Texture> idleTextures;
    std::vector<sf::Texture> walkTextures;
    int currentFrame;
    float animationTimer;
    float animationFrameTime;
    
    // Follow range
    const float FOLLOW_DISTANCE = 300.0f;
    const float MIN_DISTANCE = 100.0f;
    
    // Update state
    void setState(DoctorState newState);
    void updateAnimation(float deltaTime);
    void loadTextures();
};

#endif
