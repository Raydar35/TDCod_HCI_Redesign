#ifndef TDCOD_CUTSCENE_H
#define TDCOD_CUTSCENE_H

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <string>
#include <vector>

namespace TDCod {

class Cutscene {
public:
    Cutscene();
    ~Cutscene();

    void run(sf::RenderWindow& window);

private:
    void init();
    // Changed update to take RenderWindow for mouse position, if needed for UI elements
    void update(float deltaTime, sf::RenderWindow& window);
    void render(sf::RenderWindow& window);
    void handleEvents(sf::RenderWindow& window);

    // Helper struct for dialogue lines if you want to keep speaker and line separate
    struct DialogueLine {
        std::string speaker;
        std::string line;
    };

    // Member variables for cutscene state, assets, etc.
    sf::Font font;
    sf::Font dialogueDisplayFont; // Font for dialogue text
    sf::Text titleText;
    sf::Text startButtonText;
    sf::Text dialogueText;
    sf::Text skipPromptText;

    sf::RectangleShape titleBorder;
    sf::RectangleShape startButtonBorder;
    sf::RectangleShape dialogueBox;
    sf::RectangleShape fadeRect;

    sf::Texture backgroundTexture;
    sf::Texture planetTexture;
    sf::Texture shipMoveTexture;
    sf::Texture shipIdleTexture;

    sf::Sprite backgroundSprite;
    sf::Sprite planetSprite;
    sf::Sprite shipSprite;

    sf::Music backgroundMusic; // Music for the menu
    sf::Music cutsceneMusic; // Music for the cutscene
    sf::SoundBuffer menuButtonBuffer;
    sf::Sound menuButtonSound;
    sf::SoundBuffer menuClickBuffer;
    sf::Sound menuClickSound;

    sf::Clock movementClock;
    sf::Clock animationClock;
    sf::Clock fadeClock;
    sf::Clock dialogueClock;
    sf::Clock autoAdvanceClock;
    bool fadeEndTimerStarted = false;


    std::vector<DialogueLine> dialogueScript; // Using the struct for richer dialogue
    // std::vector<std::string> dialogueLines; // Original, can be removed if using dialogueScript
    
    int currentLineIndex;
    int currentCharIndex; // For typewriter effect
    float textDisplayTimer; // Can be removed if dialogueClock is used for timing chars
    float textDisplayDelay;

    bool cutsceneFinished;
    bool isMenuState; // To differentiate between menu and actual cutscene
    bool dialogueActive;
    bool currentLineComplete; // For dialogue progression
    
    float fadeAlpha;
    float fadeDuration;
    bool startFading;

    float normalShipSpeed;
    float fastShipSpeed;
    float currentShipSpeed;

    sf::View cameraView;
};

} // namespace TDCod

#endif // TDCOD_CUTSCENE_H