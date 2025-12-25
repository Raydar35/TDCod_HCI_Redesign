#ifndef CUTSCENE_H
#define CUTSCENE_H

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
    sf::Font font2;
    sf::Font dialogueDisplayFont; // Font for dialogue text
    sf::Text titleText;
    sf::Text startButtonText;
    sf::Text controlsButtonText; // new Controls button
    sf::Text settingsButtonText; // new Settings button
    sf::Text exitButtonText; // new exit button
    sf::Text dialogueText;
    sf::Text skipPromptText;
    sf::Text controlsText; // content for controls overlay
    sf::Text backButtonText; // Back button inside controls panel

    sf::RectangleShape titleBorder;
    sf::RectangleShape startButtonBorder;
    sf::RectangleShape controlsButtonBorder; // border for controls button
    sf::RectangleShape settingsButtonBorder; // border for settings button
    sf::RectangleShape exitButtonBorder; // border for exit button
    sf::RectangleShape backButtonBorder; // border for back button inside controls
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
    bool showingControls = false; // whether the controls overlay is visible
    bool backWasHovering = false; // track hover for back button
    bool showingSettings = false; // whether the settings overlay is visible

    float fadeAlpha;
    float fadeDuration;
    bool startFading;

    float normalShipSpeed;
    float fastShipSpeed;
    float currentShipSpeed;

    sf::View cameraView;

    // icon assets for controls panel
    std::vector<sf::Texture> iconTextures;
    std::vector<sf::Sprite> iconSprites;

    // dedicated back-button icon
    sf::Texture backIconTexture;
    sf::Sprite backIconSprite;
    // Audio settings
    float masterVolume = 100.f;
    float musicVolume = 80.f;
    float sfxVolume = 85.f;
    bool masterMuted = false;
    bool musicMuted = false;
    bool sfxMuted = false;
    int draggingSlider = -1; // 0=master,1=music,2=sfx

    // Apply current audio settings to music/sfx
    void applyAudioSettings();
};

} // namespace TDCod

#endif // CUTSCENE_H