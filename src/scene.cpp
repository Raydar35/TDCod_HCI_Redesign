#include "Cutscene.h"
#include <iostream> // For error reporting

// Helper function for text wrapping (can be moved into the class as static if preferred)
namespace TDCod {

// Forward declaration for wrapText if it's used by the class and defined later,
// or ensure it's defined before its first use within the class methods.
std::string wrapText(sf::Text& textObject, const std::string& string, float maxWidth, const sf::Font& font, unsigned int charSize) {
    std::string wrappedString;
    std::string currentLine;
    std::string word;
    sf::Text tempText;
    tempText.setFont(font);
    tempText.setCharacterSize(charSize);

    for (char character : string) {
        if (character == ' ' || character == '\n' || character == '\t') {
            tempText.setString(currentLine + word);
            if (tempText.getLocalBounds().width > maxWidth && !currentLine.empty()) {
                wrappedString += currentLine + '\n';
                currentLine = word;
            } else {
                currentLine += word;
            }
            
            if (character == '\n') {
                 wrappedString += currentLine + '\n';
                 currentLine.clear();
            } else {
                 currentLine += " ";
            }
            word.clear();
        } else {
            word += character;
        }
    }
    tempText.setString(currentLine + word);
    if (tempText.getLocalBounds().width > maxWidth && !currentLine.empty()) {
        wrappedString += currentLine + '\n' + word;
    } else {
        wrappedString += currentLine + word;
    }
    
    size_t first = wrappedString.find_first_not_of("\n \t");
    if (std::string::npos == first) {
        return "";
    }
    size_t last = wrappedString.find_last_not_of("\n \t");
    return wrappedString.substr(first, (last - first + 1));
}


Cutscene::Cutscene() : currentLineIndex(0), textDisplayTimer(0.0f), textDisplayDelay(0.02f), cutsceneFinished(false),
                       // Initialize other members that have default values
                       currentCharIndex(0), currentLineComplete(false), dialogueActive(false),
                       fadeAlpha(0.0f), fadeDuration(1.0f), startFading(false),
                       normalShipSpeed(150.0f), fastShipSpeed(400.0f),
                       isMenuState(true) // Start in menu state
{
    init(); // Call init to load assets and setup initial state
}

Cutscene::~Cutscene() {
    // SFML resources are managed by their own destructors
}

void Cutscene::init() {
    // Load assets
    if (!font.loadFromFile("TDCod/Assets/Call of Ops Duty.otf")) {
        std::cerr << "Error loading font TDCod/Assets/Call of Ops Duty.otf" << std::endl;
        // Handle error appropriately, maybe set cutsceneFinished = true
    }
    if (!backgroundTexture.loadFromFile("TDCod/Scene/Assets2/Space Background.png")) {
        std::cerr << "Error loading texture TDCod/Scene/Assets2/Space Background.png" << std::endl;
    }
    backgroundSprite.setTexture(backgroundTexture);

    if (!planetTexture.loadFromFile("TDCod/Scene/Assets2/planet.png")) {
        std::cerr << "Error loading texture TDCod/Scene/Assets2/planet.png" << std::endl;
    }
    planetSprite.setTexture(planetTexture);

    if (!shipMoveTexture.loadFromFile("TDCod/Scene/Assets2/Corvette/Move.png")) {
        std::cerr << "Error loading texture TDCod/Scene/Assets2/Corvette/Move.png" << std::endl;
    }
    if (!shipIdleTexture.loadFromFile("TDCod/Scene/Assets2/Corvette/Idle.png")) {
        std::cerr << "Error loading texture TDCod/Scene/Assets2/Corvette/Idle.png" << std::endl;
    }
    shipSprite.setTexture(shipIdleTexture); // Initial texture

    if (!menuButtonBuffer.loadFromFile("TDCod/Scene/Assets2/menubutton.mp3")) {
        std::cerr << "Error loading sound TDCod/Scene/Assets2/menubutton.mp3" << std::endl;
    }
    menuButtonSound.setBuffer(menuButtonBuffer);

    if (!menuClickBuffer.loadFromFile("TDCod/Scene/Assets2/menuclick.mp3")) {
        std::cerr << "Error loading sound TDCod/Scene/Assets2/menuclick.mp3" << std::endl;
    }
    menuClickSound.setBuffer(menuClickBuffer);

    // Load menu music
    if (!backgroundMusic.openFromFile("TDCod/Scene/Assets2/01 - Damned.mp3")) {
        std::cerr << "Error loading menu music TDCod/Scene/Assets2/01 - Damned.mp3" << std::endl;
    }
    backgroundMusic.setLoop(true);

    // Load cutscene music
    if (!cutsceneMusic.openFromFile("TDCod/Scene/Assets2/cutscene.mp3")) {
        std::cerr << "Error loading cutscene music TDCod/Scene/Assets2/cutscene.mp3" << std::endl;
    }
    cutsceneMusic.setLoop(false); // Cutscene music usually doesn't loop

    // Setup dialogue script (adapted to std::vector<std::string> dialogueLines)
    dialogueScript = {
        {"MC", "What the hell was that shake? That did not feel like turbulence."},
        {"Doctor", "It was not. The primary thruster cooling system just failed. We have got maybe five minutes before it overheats completely."},
        {"MC", "Can we fix it mid flight?"},
        {"Doctor", "Not a chance. We need to land right now. Find somewhere stable, or we are both going down with this thing."},
        {"MC", "Hold on... scanning for a surface. Found one. It is rough terrain, but it will have to do."},
        {"Doctor", "Then what are you waiting for? Get us down there before this ship turns into a fireball."}
    };
    // The dialogueLines member from Cutscene.h is std::vector<std::string>
    // This might need adjustment if dialogueScript (with speaker and line separate) is preferred.
    // For now, let's assume dialogueLines will be populated in update/render from dialogueScript.

    // Setup text elements (font and char size set here, position/string in run or render)
    titleText.setFont(font);
    titleText.setCharacterSize(60);
    titleText.setFillColor(sf::Color::White);
    titleText.setStyle(sf::Text::Bold);

    startButtonText.setFont(font);
    startButtonText.setCharacterSize(40);
    startButtonText.setFillColor(sf::Color::White);

    // Attempt to load a standard font for dialogue, fallback to original if not found
    // Fallback to the original game font if the primary font fails
    dialogueDisplayFont = font;
    dialogueText.setFont(dialogueDisplayFont);
    dialogueText.setCharacterSize(24);
    dialogueText.setFillColor(sf::Color::White);

    skipPromptText.setFont(dialogueDisplayFont);
    skipPromptText.setCharacterSize(18);
    skipPromptText.setFillColor(sf::Color(200, 200, 200));
    skipPromptText.setString("Press Space to Skip/Continue");
    
    currentShipSpeed = normalShipSpeed;
}


void Cutscene::run(sf::RenderWindow& window) {
    // Setup elements that depend on window size (if not already handled or if window size can change)
    titleText.setString("Echo's of Valkyrie");
    sf::FloatRect titleTextBounds = titleText.getLocalBounds();
    titleText.setOrigin(titleTextBounds.left + titleTextBounds.width / 2.0f, titleTextBounds.top + titleTextBounds.height / 2.0f);
    titleText.setPosition(window.getSize().x / 2.0f, window.getSize().y / 4.0f);

    titleBorder.setSize(sf::Vector2f(titleTextBounds.width + 20, titleTextBounds.height + 20));
    titleBorder.setFillColor(sf::Color::Transparent);
    titleBorder.setOutlineColor(sf::Color::White);
    titleBorder.setOutlineThickness(2);
    titleBorder.setOrigin(titleBorder.getSize().x / 2.0f, titleBorder.getSize().y / 2.0f);
    titleBorder.setPosition(titleText.getPosition());

    startButtonText.setString("Start");
    sf::FloatRect startButtonBounds = startButtonText.getLocalBounds();
    startButtonText.setOrigin(startButtonBounds.left + startButtonBounds.width / 2.0f, startButtonBounds.top + startButtonBounds.height / 2.0f);
    startButtonText.setPosition(window.getSize().x / 2.0f, window.getSize().y / 1.5f);

    startButtonBorder.setSize(sf::Vector2f(startButtonBounds.width + 20, startButtonBounds.height + 20));
    startButtonBorder.setFillColor(sf::Color::Transparent);
    startButtonBorder.setOutlineColor(sf::Color::White);
    startButtonBorder.setOutlineThickness(2);
    startButtonBorder.setOrigin(startButtonBorder.getSize().x / 2.0f, startButtonBorder.getSize().y / 2.0f);
    startButtonBorder.setPosition(startButtonText.getPosition());
    
    planetSprite.setPosition(backgroundTexture.getSize().x - planetTexture.getSize().x - 50, window.getSize().y / 2.0f - planetTexture.getSize().y / 2.0f);
    shipSprite.setPosition(-shipSprite.getGlobalBounds().width, window.getSize().y / 2.0f - shipSprite.getGlobalBounds().height / 2.0f);

    dialogueBox.setSize(sf::Vector2f(window.getSize().x * 0.9f, window.getSize().y * 0.25f));
    dialogueBox.setFillColor(sf::Color(0, 0, 0, 180));
    dialogueBox.setOutlineColor(sf::Color::White);
    dialogueBox.setOutlineThickness(3.0f);
    dialogueBox.setOrigin(dialogueBox.getSize().x / 2.0f, dialogueBox.getSize().y / 2.0f);
    dialogueBox.setPosition(window.getSize().x / 2.0f, window.getSize().y * 0.8f);

    cameraView.setSize(window.getSize().x, window.getSize().y);
    cameraView.setCenter(window.getSize().x / 2.0f, window.getSize().y / 2.0f);
    
    fadeRect.setSize(sf::Vector2f(window.getSize().x, window.getSize().y));
    fadeRect.setFillColor(sf::Color(0, 0, 0, 0));

    if (isMenuState) {
        backgroundMusic.play();
    }
    
    sf::Clock frameClock; // For calculating deltaTime

    while (window.isOpen() && !cutsceneFinished) {
        float deltaTime = frameClock.restart().asSeconds();
        textDisplayTimer += deltaTime; // Accumulate time for dialogue

        handleEvents(window);
        update(deltaTime, window); // Pass window for size-dependent updates if any
        render(window);

        // Condition to end the cutscene (e.g., after fading)
        if (startFading && fadeAlpha >= 255.0f) {
             // Add a small delay after full fade before finishing
            static sf::Clock endDelayClock;
            if (!fadeEndTimerStarted) {
                endDelayClock.restart();
                fadeEndTimerStarted = true;
            }
            if (endDelayClock.getElapsedTime().asSeconds() > 0.5f) { // 0.5 second delay
                cutsceneFinished = true;
            }
        }
    }
    backgroundMusic.stop(); // Ensure menu music is stopped if it was playing
    cutsceneMusic.stop(); // Ensure cutscene music is stopped when cutscene ends
}

void Cutscene::handleEvents(sf::RenderWindow& window) {
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            window.close();
            cutsceneFinished = true; // Ensure loop terminates if window is closed
            return;
        }

        if (isMenuState) {
            if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    if (startButtonText.getGlobalBounds().contains(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y))) {
                        menuClickSound.play();
                        isMenuState = false; // Transition from menu to cutscene
                        dialogueActive = true;
                        currentLineIndex = 0;
                        currentCharIndex = 0;
                        currentLineComplete = false;
                        dialogueClock.restart();
                        backgroundMusic.stop(); // Stop menu music
                        cutsceneMusic.play(); // Play cutscene music
                        movementClock.restart();
                        animationClock.restart();
                    }
                }
            }
        } else if (dialogueActive && event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Space) {
            if (currentLineIndex < dialogueScript.size()) {
                if (!currentLineComplete) {
                    currentCharIndex = dialogueScript[currentLineIndex].line.length();
                    // Update logic will handle setting currentLineComplete true
                } else {
                    currentLineIndex++;
                    if (currentLineIndex < dialogueScript.size()) {
                        currentCharIndex = 0;
                        currentLineComplete = false;
                        dialogueClock.restart();
                        autoAdvanceClock.restart();
                    } else {
                        dialogueActive = false;
                        currentShipSpeed = fastShipSpeed;
                        skipPromptText.setString("");
                    }
                }
            }
        }
    }
}

void Cutscene::update(float deltaTime, sf::RenderWindow& window) {
    if (isMenuState) {
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);
        bool isHovering = startButtonText.getGlobalBounds().contains(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
        static bool wasHovering = false;

        if (isHovering) {
            startButtonText.setFillColor(sf::Color::Yellow);
            startButtonBorder.setOutlineColor(sf::Color::Yellow);
            if (!wasHovering) {
                menuButtonSound.play();
            }
        } else {
            startButtonText.setFillColor(sf::Color::White);
            startButtonBorder.setOutlineColor(sf::Color::White);
        }
        wasHovering = isHovering;
    } else { // Actual cutscene part
        shipSprite.move(currentShipSpeed * deltaTime, 0);

        // Camera follow
        float cameraX = shipSprite.getPosition().x + shipSprite.getGlobalBounds().width / 2; // Follow center of ship
        float minCameraX = window.getSize().x / 2.0f;
        float maxCameraX = backgroundTexture.getSize().x - window.getSize().x / 2.0f;
        cameraX = std::max(minCameraX, std::min(cameraX, maxCameraX));
        cameraView.setCenter(cameraX, window.getSize().y / 2.0f);

        // Planet collision for disappearing ship and fading
        sf::FloatRect planetCenterBounds(
            planetSprite.getPosition().x + planetSprite.getGlobalBounds().width * 0.45f,
            planetSprite.getPosition().y + planetSprite.getGlobalBounds().height * 0.45f,
            planetSprite.getGlobalBounds().width * 0.1f,
            planetSprite.getGlobalBounds().height * 0.1f
        );
        if (shipSprite.getGlobalBounds().intersects(planetCenterBounds)) {
            shipSprite.setColor(sf::Color::Transparent);
        }
        if (shipSprite.getGlobalBounds().intersects(planetSprite.getGlobalBounds())) {
            if (!startFading) {
                startFading = true;
                fadeClock.restart();
            }
        }

        // Fading logic
        if (startFading) {
            float elapsedTime = fadeClock.getElapsedTime().asSeconds();
            fadeAlpha = (elapsedTime / fadeDuration) * 255.0f;
            if (fadeAlpha > 255.0f) {
                fadeAlpha = 255.0f;
            }
            fadeRect.setFillColor(sf::Color(0, 0, 0, static_cast<sf::Uint8>(fadeAlpha)));
        }

        // Dialogue update
        if (dialogueActive && currentLineIndex < dialogueScript.size()) {
            if (!currentLineComplete && dialogueClock.getElapsedTime().asSeconds() > textDisplayDelay) {
                if (currentCharIndex < dialogueScript[currentLineIndex].line.length()) {
                    currentCharIndex++;
                    dialogueClock.restart();
                } else {
                    currentLineComplete = true;
                    autoAdvanceClock.restart();
                }
            }

            if (currentLineComplete) {
                 // Auto-advance logic
                float baseDelay = 0.3f; 
                float charDelayFactor = 0.01f; 
                float dynamicDelay = baseDelay + (dialogueScript[currentLineIndex].line.length() * charDelayFactor);
                if (autoAdvanceClock.getElapsedTime().asSeconds() > dynamicDelay) {
                    currentLineIndex++;
                    if (currentLineIndex < dialogueScript.size()) {
                        currentCharIndex = 0;
                        currentLineComplete = false;
                        dialogueClock.restart();
                        autoAdvanceClock.restart();
                    } else {
                        dialogueActive = false;
                        currentShipSpeed = fastShipSpeed;
                        skipPromptText.setString("");
                    }
                }
            }
        }
    }
}

void Cutscene::render(sf::RenderWindow& window) {
    window.clear();

    if (isMenuState) {
        window.setView(window.getDefaultView());
        window.draw(backgroundSprite);
        window.draw(planetSprite); // Planet visible in menu
        window.draw(titleBorder);
        window.draw(titleText);
        window.draw(startButtonBorder);
        window.draw(startButtonText);
    } else { // Actual cutscene part
        window.setView(cameraView);
        window.draw(backgroundSprite);
        window.draw(planetSprite);
        window.draw(shipSprite);

        // Draw dialogue box and text with default view (overlay)
        window.setView(window.getDefaultView());
        if (dialogueActive && currentLineIndex < dialogueScript.size()) {
            window.draw(dialogueBox);
            
            std::string currentSpeaker = dialogueScript[currentLineIndex].speaker;
            std::string lineContent = dialogueScript[currentLineIndex].line;
            std::string displayedLineContent = currentLineComplete ? lineContent : lineContent.substr(0, currentCharIndex);
            std::string fullStringToDisplay = currentSpeaker + ": " + displayedLineContent;
            
            float dialogueBoxInnerWidth = dialogueBox.getSize().x - 40.0f;
            dialogueText.setString(wrapText(dialogueText, fullStringToDisplay, dialogueBoxInnerWidth, dialogueDisplayFont, dialogueText.getCharacterSize()));
            
            sf::FloatRect dialogueBoxBounds = dialogueBox.getGlobalBounds(); // Use global bounds after positioning
            dialogueText.setPosition(
                dialogueBox.getPosition().x - dialogueBox.getSize().x / 2.0f + 20.0f, // Relative to dialogueBox origin
                dialogueBox.getPosition().y - dialogueBox.getSize().y / 2.0f + 10.0f
            );
            window.draw(dialogueText);

            sf::FloatRect skipPromptBounds = skipPromptText.getLocalBounds();
            skipPromptText.setPosition(
                dialogueBox.getPosition().x + dialogueBox.getSize().x / 2.0f - skipPromptBounds.width - 20.0f,
                dialogueBox.getPosition().y + dialogueBox.getSize().y / 2.0f - skipPromptBounds.height - 15.0f
            );
            window.draw(skipPromptText);
        }
        // Draw fade rectangle (always with default view, covers everything)
        // window.setView(window.getDefaultView()); // Already set if dialogue was drawn
        window.draw(fadeRect);
    }
    window.display();
}

} // namespace TDCod