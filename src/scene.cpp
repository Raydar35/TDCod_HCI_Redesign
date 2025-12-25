#include "Cutscene.h"
#include <iostream>
#include <sstream>
#include <algorithm>

// Settings panel vertical offset (controls visual + hit-test alignment)
static constexpr float SETTINGS_CONTENT_TOP_OFFSET = 130.f; // decrease to move sliders up, increase to move down
// Temporary debug overlay to visualize hit rectangles for sliders and mute boxes
static constexpr bool DRAW_HIT_DEBUG = false;
// Temporary input Y adjustment (negative moves hit-tests upward). Tweak if needed.
static constexpr float INPUT_HIT_Y_OFFSET = 14.f;
// UI scaling for settings widgets (labels, sliders, checkboxes)
static constexpr float SETTINGS_UI_SCALE = 1.25f; // increase to make controls larger
// Back button margin from panel edge (keeps it fully inside)
static constexpr float BACK_PANEL_MARGIN = 12.f;

namespace TDCod {

static std::string wrapText(sf::Text& textObject, const std::string& string, float maxWidth, const sf::Font& font, unsigned int charSize) {
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
    if (std::string::npos == first) return "";
    size_t last = wrappedString.find_last_not_of("\n \t");
    return wrappedString.substr(first, (last - first + 1));
}

Cutscene::Cutscene()
    : currentLineIndex(0), textDisplayTimer(0.0f), textDisplayDelay(0.02f), cutsceneFinished(false),
      currentCharIndex(0), currentLineComplete(false), dialogueActive(false),
      fadeAlpha(0.0f), fadeDuration(1.0f), startFading(false),
      normalShipSpeed(150.0f), fastShipSpeed(400.0f), isMenuState(true), backWasHovering(false),
      masterVolume(100.f), musicVolume(100.f), sfxVolume(100.f),
      masterMuted(false), musicMuted(false), sfxMuted(false), draggingSlider(-1)
{
    init();
}

Cutscene::~Cutscene() {}

void Cutscene::init() {
    if (!font.loadFromFile("assets/Call of Ops Duty.otf")) {
        std::cerr << "Error loading font TDCod/Assets/Call of Ops Duty.otf" << std::endl;
    }
    if (!font2.loadFromFile("assets/SwanseaBold-D0ox.ttf")) {
        std::cerr << "Error loading font TDCod/Assets/SwanseaBold-D0ox.ttf" << std::endl;
	}
    if (!backgroundTexture.loadFromFile("assets/scene/Space Background.png")) {
        std::cerr << "Error loading texture TDCod/Scene/Assets2/Space Background.png" << std::endl;
    }
    backgroundSprite.setTexture(backgroundTexture);

    if (!planetTexture.loadFromFile("assets/scene/planet.png")) {
        std::cerr << "Error loading texture assets/scene/planet.png" << std::endl;
    }
    planetSprite.setTexture(planetTexture);

    if (!shipMoveTexture.loadFromFile("assets/scene/Corvette/Move.png")) {
        std::cerr << "Error loading texture assets/scene/Corvette/Move.png" << std::endl;
    }
    if (!shipIdleTexture.loadFromFile("assets/scene/Corvette/Idle.png")) {
        std::cerr << "Error loading texture assets/scene/Corvette/Idle.png" << std::endl;
    }
    shipSprite.setTexture(shipIdleTexture);

    if (!menuButtonBuffer.loadFromFile("assets/scene/menubutton.mp3")) {
        std::cerr << "Error loading sound assets/scene/menubutton.mp3" << std::endl;
    }
    menuButtonSound.setBuffer(menuButtonBuffer);

    if (!menuClickBuffer.loadFromFile("assets/scene/menuclick.mp3")) {
        std::cerr << "Error loading sound assets/scene/menuclick.mp3" << std::endl;
    }
    menuClickSound.setBuffer(menuClickBuffer);

    if (!backgroundMusic.openFromFile("assets/scene/01 - Damned.mp3")) {
        std::cerr << "Error loading menu music" << std::endl;
    }
    backgroundMusic.setLoop(true);

    if (!cutsceneMusic.openFromFile("assets/scene/cutscene.mp3")) {
        std::cerr << "Error loading cutscene music" << std::endl;
    }
    cutsceneMusic.setLoop(false);

    // dialogue
    dialogueScript = {
        {"MC", "What the hell was that shake? That did not feel like turbulence."},
        {"Doctor", "It was not. The primary thruster cooling system just failed. We have got maybe five minutes before it overheats completely."},
        {"MC", "Can we fix it mid flight?"},
        {"Doctor", "Not a chance. We need to land right now. Find somewhere stable, or we are both going down with this thing."},
        {"MC", "Hold on... scanning for a surface. Found one. It is rough terrain, but it will have to do."},
        {"Doctor", "Then what are you waiting for? Get us down there before this ship turns into a fireball."}
    };

    titleText.setFont(font);
    titleText.setCharacterSize(96);
    titleText.setFillColor(sf::Color::White);
    titleText.setStyle(sf::Text::Bold);

    startButtonText.setFont(font);
    startButtonText.setCharacterSize(56);
    startButtonText.setFillColor(sf::Color::White);

    controlsButtonText.setFont(font);
    controlsButtonText.setCharacterSize(56);
    controlsButtonText.setFillColor(sf::Color::White);

    // Settings button
    settingsButtonText.setFont(font);
    settingsButtonText.setCharacterSize(56);
    settingsButtonText.setFillColor(sf::Color::White);

    exitButtonText.setFont(font);
    exitButtonText.setCharacterSize(56);
    exitButtonText.setFillColor(sf::Color::White);

    dialogueDisplayFont = font;
    dialogueText.setFont(dialogueDisplayFont);
    dialogueText.setCharacterSize(24);
    dialogueText.setFillColor(sf::Color::White);

    skipPromptText.setFont(dialogueDisplayFont);
    skipPromptText.setCharacterSize(18);
    skipPromptText.setFillColor(sf::Color(200,200,200));
    skipPromptText.setString("Press Space to Skip/Continue");

    currentShipSpeed = normalShipSpeed;

    // load icon textures (placeholders) and create sprites
    iconTextures.clear(); iconSprites.clear();
    // total icons = leftLabels + rightLabels (5 + 5)
    size_t totalIcons = 10;
    iconTextures.resize(totalIcons);
    iconSprites.resize(totalIcons);
    for (size_t i = 0; i < totalIcons; ++i) {
        std::string path = "assets/scene/Icons/icon" + std::to_string(i) + ".png"; // placeholder path
        if (!iconTextures[i].loadFromFile(path)) {
            // Not fatal; leave texture empty and we'll fall back to a rectangle
            // std::cerr << "Warning: failed to load icon " << path << std::endl;
        } else {
            iconTextures[i].setSmooth(true);
            iconSprites[i].setTexture(iconTextures[i]);
        }
    }

    // load back icon texture (placeholder path)
    if (true) {
        std::string backPath = "assets/scene/Icons/esc.png";
        if (!backIconTexture.loadFromFile(backPath)) {
            // fallback: leave sprite empty
            // std::cerr << "Warning: failed to load back icon " << backPath << std::endl;
        } else {
            backIconTexture.setSmooth(true);
            backIconSprite.setTexture(backIconTexture);
        }
    }
}

void Cutscene::run(sf::RenderWindow& window) {
    titleText.setString("Echo's of Valkyrie");
    sf::FloatRect titleTextBounds = titleText.getLocalBounds();
    titleText.setOrigin(titleTextBounds.left + titleTextBounds.width/2.0f, titleTextBounds.top + titleTextBounds.height/2.0f);
    titleText.setPosition(window.getSize().x/2.0f, window.getSize().y/5.0f);

    titleBorder.setSize(sf::Vector2f(titleTextBounds.width + 20, titleTextBounds.height + 20));
    titleBorder.setFillColor(sf::Color::Transparent);
    titleBorder.setOutlineColor(sf::Color::White);
    titleBorder.setOutlineThickness(2);
    titleBorder.setOrigin(titleBorder.getSize().x/2.0f, titleBorder.getSize().y/2.0f);
    titleBorder.setPosition(titleText.getPosition());

    // Move the menu buttons up a bit so they're closer to the title and space evenly
    float menuBaseY = window.getSize().y/1.65f;
    float btnSpacing = 92.0f; // vertical spacing between adjacent buttons (increased slightly)
    float halfSpan = 1.5f * btnSpacing; // distance from center to top/bottom button

    // Ensure all menu buttons use the same font
    startButtonText.setFont(font);
    controlsButtonText.setFont(font);
    settingsButtonText.setFont(font);
    exitButtonText.setFont(font);

    // Compute menu font size dynamically from window height so menu scales on different resolutions
    unsigned int menuFontSize = static_cast<unsigned int>(std::max(48.f, std::min(200.f, window.getSize().y * 0.07f)));
    startButtonText.setCharacterSize(menuFontSize);
    controlsButtonText.setCharacterSize(menuFontSize);
    settingsButtonText.setCharacterSize(menuFontSize);
    exitButtonText.setCharacterSize(menuFontSize);

    startButtonText.setString("Play");
    sf::FloatRect startButtonBounds = startButtonText.getLocalBounds();
    startButtonText.setOrigin(startButtonBounds.left + startButtonBounds.width/2.0f, startButtonBounds.top + startButtonBounds.height/2.0f);
    startButtonText.setPosition(window.getSize().x/2.0f, menuBaseY - halfSpan);

    controlsButtonText.setString("Controls");
    sf::FloatRect controlsButtonBounds = controlsButtonText.getLocalBounds();
    controlsButtonText.setOrigin(controlsButtonBounds.left + controlsButtonBounds.width/2.0f, controlsButtonBounds.top + controlsButtonBounds.height/2.0f);
    controlsButtonText.setPosition(window.getSize().x/2.0f, menuBaseY - 0.5f * btnSpacing);

    // Settings button
    // settingsButtonText.setCharacterSize(56);
    settingsButtonText.setFillColor(sf::Color::White);
    settingsButtonText.setString("Settings");
    sf::FloatRect settingsButtonBounds = settingsButtonText.getLocalBounds();
    settingsButtonText.setOrigin(settingsButtonBounds.left + settingsButtonBounds.width/2.0f, settingsButtonBounds.top + settingsButtonBounds.height/2.0f);
    settingsButtonText.setPosition(window.getSize().x/2.0f, menuBaseY + 0.5f * btnSpacing);

    exitButtonText.setString("Exit");
    sf::FloatRect exitButtonBounds = exitButtonText.getLocalBounds();
    exitButtonText.setOrigin(exitButtonBounds.left + exitButtonBounds.width/2.0f, exitButtonBounds.top + exitButtonBounds.height/2.0f);
    exitButtonText.setPosition(window.getSize().x/2.0f, menuBaseY + halfSpan);

    startButtonBorder.setSize(sf::Vector2f(startButtonBounds.width + 20, startButtonBounds.height + 20));
    startButtonBorder.setFillColor(sf::Color::Transparent);
    startButtonBorder.setOutlineColor(sf::Color::White);
    startButtonBorder.setOutlineThickness(2);
    startButtonBorder.setOrigin(startButtonBorder.getSize().x/2.0f, startButtonBorder.getSize().y/2.0f);
    startButtonBorder.setPosition(startButtonText.getPosition());

    controlsButtonBorder.setSize(sf::Vector2f(controlsButtonBounds.width + 20, controlsButtonBounds.height + 20));
    controlsButtonBorder.setFillColor(sf::Color::Transparent);
    controlsButtonBorder.setOutlineColor(sf::Color::White);
    controlsButtonBorder.setOutlineThickness(2);
    controlsButtonBorder.setOrigin(controlsButtonBorder.getSize().x/2.0f, controlsButtonBorder.getSize().y/2.0f);
    controlsButtonBorder.setPosition(controlsButtonText.getPosition());

    // Settings button border
    settingsButtonBorder.setSize(sf::Vector2f(settingsButtonBounds.width + 20, settingsButtonBounds.height + 20));
    settingsButtonBorder.setFillColor(sf::Color::Transparent);
    settingsButtonBorder.setOutlineColor(sf::Color::White);
    settingsButtonBorder.setOutlineThickness(2);
    settingsButtonBorder.setOrigin(settingsButtonBorder.getSize().x/2.0f, settingsButtonBorder.getSize().y/2.0f);
    settingsButtonBorder.setPosition(settingsButtonText.getPosition());

    exitButtonBorder.setSize(sf::Vector2f(exitButtonBounds.width + 20, exitButtonBounds.height + 20));
    exitButtonBorder.setFillColor(sf::Color::Transparent);
    exitButtonBorder.setOutlineColor(sf::Color::White);
    exitButtonBorder.setOutlineThickness(2);
    exitButtonBorder.setOrigin(exitButtonBorder.getSize().x/2.0f, exitButtonBorder.getSize().y/2.0f);
    exitButtonBorder.setPosition(exitButtonText.getPosition());

    planetSprite.setPosition(backgroundTexture.getSize().x - planetTexture.getSize().x - 50, window.getSize().y/2.0f - planetTexture.getSize().y/2.0f);
    shipSprite.setPosition(-shipSprite.getGlobalBounds().width, window.getSize().y/2.0f - shipSprite.getGlobalBounds().height/2.0f);

    dialogueBox.setSize(sf::Vector2f(window.getSize().x * 0.9f, window.getSize().y * 0.25f));
    dialogueBox.setFillColor(sf::Color(0,0,0,180));
    dialogueBox.setOutlineColor(sf::Color::White);
    dialogueBox.setOutlineThickness(3.0f);
    dialogueBox.setOrigin(dialogueBox.getSize().x/2.0f, dialogueBox.getSize().y/2.0f);
    dialogueBox.setPosition(window.getSize().x/2.0f, window.getSize().y * 0.8f);

    cameraView.setSize(window.getSize().x, window.getSize().y);
    cameraView.setCenter(window.getSize().x/2.0f, window.getSize().y/2.0f);

    fadeRect.setSize(sf::Vector2f(window.getSize().x, window.getSize().y));
    fadeRect.setFillColor(sf::Color(0,0,0,0));

    if (isMenuState) backgroundMusic.play();

    window.setMouseCursorVisible(true);

    sf::Clock frameClock;
    while (window.isOpen() && !cutsceneFinished) {
        float deltaTime = frameClock.restart().asSeconds();
        textDisplayTimer += deltaTime;
        handleEvents(window);
        update(deltaTime, window);
        render(window);

        if (startFading && fadeAlpha >= 255.0f) {
            static sf::Clock endDelay;
            static bool started = false;
            if (!started) { endDelay.restart(); started = true; }
            if (endDelay.getElapsedTime().asSeconds() > 0.5f) cutsceneFinished = true;
        }
    }

    backgroundMusic.stop();
    cutsceneMusic.stop();
}

void Cutscene::handleEvents(sf::RenderWindow& window) {
    sf::Event event;
    while (window.pollEvent(event)) {
        if (event.type == sf::Event::Closed) { window.close(); cutsceneFinished = true; return; }

        if (isMenuState) {
            // Allow ESC to close the controls or settings panel when open
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
                if (showingControls) {
                    menuButtonSound.play();
                    showingControls = false;
                    continue; // consume event
                }
                if (showingSettings) {
                    menuButtonSound.play();
                    showingSettings = false;
                    continue;
                }
            }

            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                if (showingControls || showingSettings) {
                    // compute back rect (same as update/render)
                    sf::Vector2f panelSize(window.getSize().x * 0.6f, window.getSize().y * 0.6f);
                    sf::FloatRect tBounds = titleText.getLocalBounds();
                    float titleBottom = titleText.getPosition().y + tBounds.height / 2.0f;
                    float gap = 20.f;
                    // Extra downward offset so the controls/settings panel sits lower on screen
                    const float PANEL_EXTRA_OFFSET = 80.f;
                    sf::Vector2f panelPos(window.getSize().x/2.f, titleBottom + gap + PANEL_EXTRA_OFFSET + panelSize.y/2.f);

                    // Back button: larger/wider hit rect (match render)
                    const unsigned int BACK_CHAR_SIZE = 36u;
                    const float ICON_W = 52.f; // use same icon slot width as bars
                    const float ICON_PAD = 12.f;
                    sf::Text tmpBack; tmpBack.setFont(font); tmpBack.setCharacterSize(BACK_CHAR_SIZE); tmpBack.setString("Back");
                    sf::FloatRect backBounds = tmpBack.getLocalBounds();
                    float backWidth = backBounds.width + backBounds.left + 28.f + ICON_W + ICON_PAD; // include icon slot and padding
                    float backHeight = backBounds.height + backBounds.top + 20.f;
                    // Place the back button fully inside the panel at bottom-right
                    sf::Vector2f backPos(panelPos.x + panelSize.x/2.f - BACK_PANEL_MARGIN - backWidth,
                                         panelPos.y + panelSize.y/2.f - BACK_PANEL_MARGIN - backHeight);

                    sf::FloatRect backRect(backPos.x, backPos.y, backWidth, backHeight);

                    sf::Vector2i m = sf::Mouse::getPosition(window);
                    sf::Vector2f backMouse = window.mapPixelToCoords(sf::Vector2i(m.x, m.y), window.getDefaultView());
                    backMouse.y += INPUT_HIT_Y_OFFSET; // apply Y offset
                    if (backRect.contains(backMouse.x, backMouse.y)) {
                        menuClickSound.play();
                        if (showingControls) showingControls = false;
                        if (showingSettings) showingSettings = false;
                    }
                    // settings specific hit tests
                    if (showingSettings) {
                        // Compute panel and slider geometry exactly like render (larger hit targets)
                        sf::FloatRect tbb = titleText.getLocalBounds();
                        float sepY = titleText.getPosition().y + tbb.height/2.f + 12.f;
                        sf::Vector2f panelSize(window.getSize().x * 0.6f, window.getSize().y * 0.6f);
                        sf::Vector2f panelPos(window.getSize().x/2.f, titleText.getPosition().y + tbb.height/2.f + 12.f + 20.f + 80.f + panelSize.y/2.f);
                        float sliderWidth = panelSize.x * 0.55f;
                        float sliderX = panelPos.x - sliderWidth/2.f;
                        // use centralized SETTINGS_CONTENT_TOP_OFFSET
                        float panelTop = panelPos.y - panelSize.y/2.f;
                        float rowYStart = panelTop + SETTINGS_CONTENT_TOP_OFFSET;
                        float rowSpacing = 60.f * SETTINGS_UI_SCALE;

                        sf::Vector2f mousef = window.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y), window.getDefaultView());
                        mousef.y += INPUT_HIT_Y_OFFSET; // apply Y offset
                        // tighten hit rects to better match visuals (scaled)
                        float hr_left = 4.f * SETTINGS_UI_SCALE;
                        float hr_topOff = 6.f * SETTINGS_UI_SCALE;
                        float hr_extraW = 8.f * SETTINGS_UI_SCALE;
                        float hr_h = 20.f * SETTINGS_UI_SCALE;
                        sf::FloatRect masterBarRect(sliderX - hr_left, rowYStart - hr_topOff, sliderWidth + hr_extraW, hr_h);
                        sf::FloatRect musicBarRect(sliderX - hr_left, rowYStart + rowSpacing - hr_topOff, sliderWidth + hr_extraW, hr_h);
                        sf::FloatRect sfxBarRect(sliderX - hr_left, rowYStart + rowSpacing*2 - hr_topOff, sliderWidth + hr_extraW, hr_h);
                        if (masterBarRect.contains(mousef)) {
                            draggingSlider = 0;
                            float p = (mousef.x - sliderX) / sliderWidth; masterVolume = std::clamp(p * 100.f, 0.f, 100.f);
                            masterMuted = (masterVolume <= 0.0f);
                            applyAudioSettings(); menuClickSound.play();
                            std::cerr << "[DEBUG] Clicked master bar. pixel(" << event.mouseButton.x << "," << event.mouseButton.y << ") world(" << mousef.x << "," << mousef.y << ") rowYStart=" << rowYStart << "\n";
                            continue;
                        }
                        if (musicBarRect.contains(mousef)) {
                            draggingSlider = 1;
                            float p = (mousef.x - sliderX) / sliderWidth; musicVolume = std::clamp(p * 100.f, 0.f, 100.f);
                            musicMuted = (musicVolume <= 0.0f);
                            applyAudioSettings(); menuClickSound.play();
                            std::cerr << "[DEBUG] Clicked music bar. pixel(" << event.mouseButton.x << "," << event.mouseButton.y << ") world(" << mousef.x << "," << mousef.y << ") rowYStart=" << rowYStart << "\n";
                            continue;
                        }
                        if (sfxBarRect.contains(mousef)) {
                            draggingSlider = 2;
                            float p = (mousef.x - sliderX) / sliderWidth; sfxVolume = std::clamp(p * 100.f, 0.f, 100.f);
                            sfxMuted = (sfxVolume <= 0.0f);
                            applyAudioSettings(); menuClickSound.play();
                            std::cerr << "[DEBUG] Clicked sfx bar. pixel(" << event.mouseButton.x << "," << event.mouseButton.y << ") world(" << mousef.x << "," << mousef.y << ") rowYStart=" << rowYStart << "\n";
                            continue;
                        }

                        float muteX = sliderX + sliderWidth + 60.f * SETTINGS_UI_SCALE; // slightly closer
                        float muteW = 22.f * SETTINGS_UI_SCALE, muteH = 22.f * SETTINGS_UI_SCALE;
                        sf::FloatRect masterMuteRect(muteX, rowYStart - 6.f * SETTINGS_UI_SCALE, muteW, muteH);
                        sf::FloatRect musicMuteRect(muteX, rowYStart + rowSpacing - 6.f * SETTINGS_UI_SCALE, muteW, muteH);
                        sf::FloatRect sfxMuteRect(muteX, rowYStart + rowSpacing*2 - 6.f * SETTINGS_UI_SCALE, muteW, muteH);
                        if (masterMuteRect.contains(mousef)) { masterMuted = !masterMuted; applyAudioSettings(); menuClickSound.play(); std::cerr << "[DEBUG] Clicked master mute. pixel(" << event.mouseButton.x << "," << event.mouseButton.y << ") world(" << mousef.x << "," << mousef.y << ")\n"; continue; }
                        if (musicMuteRect.contains(mousef)) { musicMuted = !musicMuted; applyAudioSettings(); menuClickSound.play(); std::cerr << "[DEBUG] Clicked music mute. pixel(" << event.mouseButton.x << "," << event.mouseButton.y << ") world(" << mousef.x << "," << mousef.y << ")\n"; continue; }
                        if (sfxMuteRect.contains(mousef)) { sfxMuted = !sfxMuted; applyAudioSettings(); menuClickSound.play(); std::cerr << "[DEBUG] Clicked sfx mute. pixel(" << event.mouseButton.x << "," << event.mouseButton.y << ") world(" << mousef.x << "," << mousef.y << ")\n"; continue; }
                    }
                 } else {
                    if (startButtonText.getGlobalBounds().contains(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y))) {
                        menuClickSound.play();
                        isMenuState = false; dialogueActive = true; currentLineIndex = 0; currentCharIndex = 0; currentLineComplete = false;
                        dialogueClock.restart(); backgroundMusic.stop(); cutsceneMusic.play(); movementClock.restart(); animationClock.restart();
                    } else if (controlsButtonText.getGlobalBounds().contains(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y))) {
                        menuClickSound.play(); showingControls = !showingControls;
                    } else if (settingsButtonText.getGlobalBounds().contains(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y))) {
                        menuClickSound.play(); showingSettings = !showingSettings;
                    } else if (exitButtonText.getGlobalBounds().contains(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y))) {
                        menuClickSound.play(); window.close(); cutsceneFinished = true;
                    }
                 }
             }
        } else if (dialogueActive && event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Space) {
            if (currentLineIndex < dialogueScript.size()) {
                if (!currentLineComplete) {
                    currentCharIndex = dialogueScript[currentLineIndex].line.length();
                } else {
                    currentLineIndex++;
                    if (currentLineIndex < dialogueScript.size()) {
                        currentCharIndex = 0; currentLineComplete = false; dialogueClock.restart(); autoAdvanceClock.restart();
                    } else {
                        dialogueActive = false; currentShipSpeed = fastShipSpeed; skipPromptText.setString("");
                    }
                }
            }
        }
    }

    // Post-event polling: handle dragging update while mouse isheld and settings panel visible
    if (isMenuState && showingSettings && draggingSlider >= 0) {
        sf::Vector2f panelSize(window.getSize().x * 0.6f, window.getSize().y * 0.6f);
        sf::FloatRect tbb = titleText.getLocalBounds();
        float sepY = titleText.getPosition().y + tbb.height/2.f + 12.f;
        sf::Vector2f panelPos(window.getSize().x/2.f, titleText.getPosition().y + tbb.height/2.f + 12.f + 20.f + 80.f + panelSize.y/2.f);
        float sliderWidth = panelSize.x * 0.55f;
        float sliderX = panelPos.x - sliderWidth/2.f;
        // use centralized SETTINGS_CONTENT_TOP_OFFSET
        float panelTop = panelPos.y - panelSize.y/2.f;
        float rowYStart = panelTop + SETTINGS_CONTENT_TOP_OFFSET;

        sf::Vector2i m = sf::Mouse::getPosition(window);
        sf::Vector2f mousef = window.mapPixelToCoords(m, window.getDefaultView());
        mousef.y += INPUT_HIT_Y_OFFSET; // apply Y offset
        float mx = mousef.x;
        float p = (mx - sliderX) / sliderWidth;
        p = std::clamp(p, 0.f, 1.f);
        if (draggingSlider == 0) { masterVolume = p * 100.f; masterMuted = (masterVolume <= 0.f); }
        else if (draggingSlider == 1) { musicVolume = p * 100.f; musicMuted = (musicVolume <= 0.f); }
        else if (draggingSlider == 2) { sfxVolume = p * 100.f; sfxMuted = (sfxVolume <= 0.f); }
        applyAudioSettings();

        // If mouse released, stop dragging
        if (!sf::Mouse::isButtonPressed(sf::Mouse::Left)) draggingSlider = -1;
    }
}

void Cutscene::applyAudioSettings() {
    float masterMul = masterMuted ? 0.f : (masterVolume / 100.f);
    float musicMul = musicMuted ? 0.f : (musicVolume / 100.f);
    float sfxMul = sfxMuted ? 0.f : (sfxVolume / 100.f);

    float volMusic = masterMul * musicMul * 100.f;
    backgroundMusic.setVolume(volMusic);
    cutsceneMusic.setVolume(volMusic);

    float volSfx = masterMul * sfxMul * 100.f;
    menuButtonSound.setVolume(volSfx);
    menuClickSound.setVolume(volSfx);
}

void Cutscene::update(float deltaTime, sf::RenderWindow& window) {
    if (isMenuState) {
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);

        sf::Vector2f mouseWorld = window.mapPixelToCoords(mousePos, window.getDefaultView());
        mouseWorld.y += INPUT_HIT_Y_OFFSET; // apply Y offset to hover checks

        bool isHoveringStart = startButtonText.getGlobalBounds().contains(mouseWorld.x, mouseWorld.y);
        static bool wasHoveringStart = false;
        if (!showingControls && !showingSettings && isHoveringStart) {
            startButtonText.setFillColor(sf::Color::Yellow);
            startButtonBorder.setOutlineColor(sf::Color::Yellow);
            if (!wasHoveringStart) menuButtonSound.play();
            wasHoveringStart = true;
        } else {
            startButtonText.setFillColor(sf::Color::White);
            startButtonBorder.setOutlineColor(sf::Color::White);
            wasHoveringStart = false;
        }

        if (!showingControls && !showingSettings) {
            bool hoverControls = controlsButtonText.getGlobalBounds().contains(mouseWorld.x, mouseWorld.y);
            static bool wasHoveringControls = false;
            if (hoverControls) {
                controlsButtonText.setFillColor(sf::Color::Yellow);
                controlsButtonBorder.setOutlineColor(sf::Color::Yellow);
                if (!wasHoveringControls) menuButtonSound.play();
                wasHoveringControls = true;
            } else {
                controlsButtonText.setFillColor(sf::Color::White);
                controlsButtonBorder.setOutlineColor(sf::Color::White);
                wasHoveringControls = false;
            }

            bool hoverSettings = settingsButtonText.getGlobalBounds().contains(mouseWorld.x, mouseWorld.y);
            static bool wasHoveringSettings = false;
            if (hoverSettings) {
                settingsButtonText.setFillColor(sf::Color::Yellow);
                settingsButtonBorder.setOutlineColor(sf::Color::Yellow);
                if (!wasHoveringSettings) menuButtonSound.play();
                wasHoveringSettings = true;
            } else {
                settingsButtonText.setFillColor(sf::Color::White);
                settingsButtonBorder.setOutlineColor(sf::Color::White);
                wasHoveringSettings = false;
            }

            bool hoverExit = exitButtonText.getGlobalBounds().contains(mouseWorld.x, mouseWorld.y);
            static bool wasHoveringExit = false;
            if (hoverExit) {
                exitButtonText.setFillColor(sf::Color::Yellow);
                exitButtonBorder.setOutlineColor(sf::Color::Yellow);
                if (!wasHoveringExit) menuButtonSound.play();
                wasHoveringExit = true;
            } else {
                exitButtonText.setFillColor(sf::Color::White);
                exitButtonBorder.setOutlineColor(sf::Color::White);
                wasHoveringExit = false;
            }
        } else {
            controlsButtonText.setFillColor(sf::Color::White);
            controlsButtonBorder.setOutlineColor(sf::Color::White);
            exitButtonText.setFillColor(sf::Color::White);
            exitButtonBorder.setOutlineColor(sf::Color::White);
        }

        if (showingControls || showingSettings) {
            sf::Vector2f panelSize(window.getSize().x * 0.6f, window.getSize().y * 0.6f);
            sf::FloatRect tBounds = titleText.getLocalBounds();
            float titleBottom = titleText.getPosition().y + tBounds.height / 2.0f;
            float gap = 20.f;
            const float PANEL_EXTRA_OFFSET = 80.f;
            float panelCenterY = titleBottom + gap + PANEL_EXTRA_OFFSET + panelSize.y / 2.f;
            sf::Vector2f panelPos(window.getSize().x/2.f, panelCenterY);

            // compute back rect for hover (match render)
            const unsigned int BACK_CHAR_SIZE = 36u;
            const float ICON_W = 52.f;
            const float ICON_PAD = 12.f;
            sf::Text tmpBack; tmpBack.setFont(font); tmpBack.setCharacterSize(BACK_CHAR_SIZE); tmpBack.setString("Back");
            sf::FloatRect backBounds = tmpBack.getLocalBounds();
            float backWidth = backBounds.width + backBounds.left + 28.f + ICON_W + ICON_PAD; // include icon slot and padding
            float backHeight = backBounds.height + backBounds.top + 20.f;
            // Place the back button fully inside the panel at bottom-right
            sf::Vector2f backPos(panelPos.x + panelSize.x/2.f - BACK_PANEL_MARGIN - backWidth,
                                 panelPos.y + panelSize.y/2.f - BACK_PANEL_MARGIN - backHeight);
            sf::FloatRect backRect(backPos.x, backPos.y, backWidth, backHeight);

            sf::Vector2i m = sf::Mouse::getPosition(window);
            sf::Vector2f backMouse = window.mapPixelToCoords(sf::Vector2i(m.x, m.y), window.getDefaultView());
            backMouse.y += INPUT_HIT_Y_OFFSET; // apply Y offset
            bool overBack = backRect.contains(backMouse.x, backMouse.y);
            if (overBack) {
                if (!backWasHovering) menuButtonSound.play();
                backWasHovering = true;
                backButtonText.setFillColor(sf::Color::Yellow);
                backButtonBorder.setOutlineColor(sf::Color::Yellow);
            } else {
                backWasHovering = false;
                backButtonText.setFillColor(sf::Color::White);
                backButtonBorder.setOutlineColor(sf::Color::White);
            }
        }

    } else {
        shipSprite.move(currentShipSpeed * deltaTime, 0);

        float cameraX = shipSprite.getPosition().x + shipSprite.getGlobalBounds().width / 2;
        float minCameraX = window.getSize().x / 2.0f;
        float maxCameraX = backgroundTexture.getSize().x - window.getSize().x / 2.0f;
        cameraX = std::max(minCameraX, std::min(cameraX, maxCameraX));
        cameraView.setCenter(cameraX, window.getSize().y / 2.0f);

        sf::FloatRect planetCenterBounds(
            planetSprite.getPosition().x + planetSprite.getGlobalBounds().width * 0.45f,
            planetSprite.getPosition().y + planetSprite.getGlobalBounds().height * 0.45f,
            planetSprite.getGlobalBounds().width * 0.1f,
            planetSprite.getGlobalBounds().height * 0.1f
        );
        if (shipSprite.getGlobalBounds().intersects(planetCenterBounds)) shipSprite.setColor(sf::Color::Transparent);
        if (shipSprite.getGlobalBounds().intersects(planetSprite.getGlobalBounds())) {
            if (!startFading) { startFading = true; fadeClock.restart(); }
        }

        if (startFading) {
            float elapsedTime = fadeClock.getElapsedTime().asSeconds();
            fadeAlpha = (elapsedTime / fadeDuration) * 255.0f;
            if (fadeAlpha > 255.0f) fadeAlpha = 255.0f;
            fadeRect.setFillColor(sf::Color(0,0,0,static_cast<sf::Uint8>(fadeAlpha)));
        }

        if (dialogueActive && currentLineIndex < dialogueScript.size()) {
            if (!currentLineComplete && dialogueClock.getElapsedTime().asSeconds() > textDisplayDelay) {
                if (currentCharIndex < dialogueScript[currentLineIndex].line.length()) { currentCharIndex++; dialogueClock.restart(); }
                else { currentLineComplete = true; autoAdvanceClock.restart(); }
            }

            if (currentLineComplete) {
                float baseDelay = 0.3f; float charDelayFactor = 0.01f;
                float dynamicDelay = baseDelay + (dialogueScript[currentLineIndex].line.length() * charDelayFactor);
                if (autoAdvanceClock.getElapsedTime().asSeconds() > dynamicDelay) {
                    currentLineIndex++;
                    if (currentLineIndex < dialogueScript.size()) { currentCharIndex = 0; currentLineComplete = false; dialogueClock.restart(); autoAdvanceClock.restart(); }
                    else { dialogueActive = false; currentShipSpeed = fastShipSpeed; skipPromptText.setString(""); }
                }
            }
        }
    }
}

void TDCod::Cutscene::render(sf::RenderWindow& window) {
    window.clear();

    if (isMenuState) {
        window.setView(window.getDefaultView());
        backgroundSprite.setPosition(0.f,0.f);
        sf::Vector2u texSize = backgroundTexture.getSize();
        if (texSize.x>0 && texSize.y>0) {
            float sx = static_cast<float>(window.getSize().x)/static_cast<float>(texSize.x);
            float sy = static_cast<float>(window.getSize().y)/static_cast<float>(texSize.y);
            backgroundSprite.setScale(sx, sy);
        } else backgroundSprite.setScale(1.f,1.f);

        window.draw(backgroundSprite);
        planetSprite.setScale(1.f,1.f);
        planetSprite.setPosition(static_cast<float>(window.getSize().x) - static_cast<float>(planetTexture.getSize().x) - 50.f,
                                  window.getSize().y/2.0f - static_cast<float>(planetTexture.getSize().y)/2.0f);
        window.draw(planetSprite);
        window.draw(titleBorder);
        window.draw(titleText);

        window.draw(startButtonText);
        window.draw(controlsButtonText);
        window.draw(settingsButtonText);
        window.draw(exitButtonText);

        if (showingControls || showingSettings) {
            sf::Vector2f panelSize(window.getSize().x * 0.6f, window.getSize().y * 0.6f);
            sf::RectangleShape panel(panelSize);
            panel.setFillColor(sf::Color(20,20,20,255));
            panel.setOutlineColor(sf::Color::White);
            panel.setOutlineThickness(2.f);
            panel.setOrigin(panel.getSize().x/2.f, panel.getSize().y/2.f);

            sf::FloatRect tBounds = titleText.getLocalBounds();
            float titleBottom = titleText.getPosition().y + tBounds.height / 2.0f;
            float gap = 20.f;
            const float PANEL_EXTRA_OFFSET = 80.f;
            float panelCenterY = titleBottom + gap + PANEL_EXTRA_OFFSET + panel.getSize().y / 2.f;
            panel.setPosition(window.getSize().x/2.f, panelCenterY);
            window.draw(panel);

            sf::Text title;
            title.setFont(font); title.setCharacterSize(52); title.setFillColor(sf::Color::White);
            bool isSettings = showingSettings && !showingControls; // prefer controls if both true
            title.setString(isSettings ? "Settings" : "Controls");
            sf::FloatRect tbb = title.getLocalBounds();
            // center the title inside the panel
            title.setOrigin(tbb.left + tbb.width/2.f, tbb.top + tbb.height/2.f);
            title.setPosition(panel.getPosition().x, panel.getPosition().y - panel.getSize().y/2.f + 40.f);
            window.draw(title);

            // Draw a short separator line below the title with fade to transparent at the edges
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

            // Reserve icon slot and sizing vars (used by back button & content) - unscaled for general UI
            float iconSlotW = 88.f; // reserve 88px to fit wide icons like 88x44
            float iconPadding = 12.f; // padding between icon slot and text
            float barHeight = 56.f;
            float spacing = 14.f;

            // Shared slider geometry (used by both controls layout and settings sliders)
            float panelPaddingInner = 28.f;
             float sliderWidth = panel.getSize().x * 0.55f;
             float sliderX = panel.getPosition().x - sliderWidth/2.f; // center under panel
             // use centralized SETTINGS_CONTENT_TOP_OFFSET
             float panelTop = panel.getPosition().y - panel.getSize().y/2.f;
             float rowYStart = panelTop + SETTINGS_CONTENT_TOP_OFFSET;
             float rowSpacing = 60.f;

            // If showing Settings, skip the detailed controls layout. We'll draw only the title and back button area.
            if (!isSettings) {
                // Two-column layout with icon area on left of each bar
                std::vector<std::string> leftLabels, rightLabels;
                if (isSettings) {
                    leftLabels = {"Music Volume", "SFX Volume", "Master Volume", "Resolution", "Fullscreen"};
                    rightLabels = {"", "", "", "", ""};
                } else {
                    leftLabels = {"Up", "Left", "Down", "Right", "Sprint (Hold)"};
                    rightLabels = {"Fire", "Aim (Hold)", "Reload", "Rifle", "Pistol"};
                }

                // panel inner padding and column sizes
                float panelPadding = 28.f;
                float columnGap = 24.f;
                float innerLeft = panel.getPosition().x - panel.getSize().x/2.f + panelPadding;
                float innerRight = panel.getPosition().x + panel.getSize().x/2.f - panelPadding;
                float panelInnerWidth = innerRight - innerLeft;
                float colWidth = (panelInnerWidth - columnGap) * 0.5f;
                float leftColX = innerLeft;
                float rightColX = innerLeft + colWidth + columnGap;

                // place headers just below the separator line (sepY defined above)
                sf::Text leftHeader;
                leftHeader.setFont(font);
                leftHeader.setCharacterSize(36);
                leftHeader.setFillColor(sf::Color::White);
                leftHeader.setString("Movement");
                sf::FloatRect lhBounds = leftHeader.getLocalBounds();
                leftHeader.setOrigin(lhBounds.left + lhBounds.width/2.f, lhBounds.top + lhBounds.height/2.f);

                sf::Text rightHeader;
                rightHeader.setFont(font);
                rightHeader.setCharacterSize(36);
                rightHeader.setFillColor(sf::Color::White);
                rightHeader.setString("Combat");
                sf::FloatRect rhBounds = rightHeader.getLocalBounds();
                rightHeader.setOrigin(rhBounds.left + rhBounds.width/2.f, rhBounds.top + rhBounds.height/2.f);

                float headerY = sepY + 84.f;
                leftHeader.setPosition(leftColX + colWidth/4.f, headerY);
                rightHeader.setPosition(rightColX + colWidth/4.f, headerY);
                // startY for listing bars beneath headers
                float startY = headerY + 24.f;
                window.draw(leftHeader);
                window.draw(rightHeader);

                // Draw a short separator line across the top of each column (with fade to transparent at edges)
                // Place this separator immediately above the first bars in the columns so it visually groups headers/bars.
                float panelTopCol = panel.getPosition().y - panel.getSize().y/2.f;
                float colSepThickness = 2.f;
                // compute separator Y so it sits just above the first bars. The first bars start at (headerY + 36.f)
                // subtract a small gap so the separator sits slightly above the bars.
                const float BAR_TOP_GAP = 8.f;
                float colSepY = headerY + 24.f - BAR_TOP_GAP;
                 float colYTop = colSepY;
                 float colYBottom = colSepY + colSepThickness;
                 sf::Color colOpaqueTop(200,200,200,220);
                 sf::Color colTransparentTop = colOpaqueTop; colTransparentTop.a = 0;
                 float edgeFadeCol = std::min(40.f, colWidth * 0.18f);

                 // Left column line
                 float leftX0 = leftColX;
                 float leftXR = leftColX + colWidth;
                 float leftCenterL = leftX0 + edgeFadeCol;
                 float leftCenterR = leftXR + 7;
                 if (leftCenterR > leftCenterL) {
                     sf::RectangleShape centerRectLeft(sf::Vector2f(leftCenterR - leftCenterL, colSepThickness));
                     centerRectLeft.setPosition(leftCenterL, colYTop);
                     centerRectLeft.setFillColor(colOpaqueTop);
                     window.draw(centerRectLeft);
                 }
                 if (leftCenterL > leftX0 + 1.f) {
                     sf::VertexArray leftGradCol(sf::Triangles, 6);
                     leftGradCol[0] = sf::Vertex(sf::Vector2f(leftX0, colYTop), colTransparentTop);
                     leftGradCol[1] = sf::Vertex(sf::Vector2f(leftCenterL, colYTop), colOpaqueTop);
                     leftGradCol[2] = sf::Vertex(sf::Vector2f(leftCenterL, colYBottom), colOpaqueTop);
                     leftGradCol[3] = sf::Vertex(sf::Vector2f(leftX0, colYTop), colTransparentTop);
                     leftGradCol[4] = sf::Vertex(sf::Vector2f(leftCenterL, colYBottom), colOpaqueTop);
                     leftGradCol[5] = sf::Vertex(sf::Vector2f(leftX0, colYBottom), colTransparentTop);
                     window.draw(leftGradCol);
                 }

                // Draw a short vertical extension down the right side of the left column
                {
                    const float colDownLen = std::min(140.f, panel.getSize().y * 0.04f);
                    // shift the vertical down-line outward from the column edge for better visual separation
                    const float VERT_SHIFT = 8.f; // pixels to shift right
                    sf::RectangleShape vert(sf::Vector2f(colSepThickness, colDownLen));
                    // place slightly to the right of the column's right edge
                    float vertX = leftXR - colSepThickness + VERT_SHIFT;
                    vert.setPosition(vertX, colYTop);
                    vert.setFillColor(colOpaqueTop);
                    window.draw(vert);

                    // stronger vertical fade at the bottom of the vertical extension (fade downward)
                    const float FADE_H = 36.f; // stronger contrast at top
                    sf::Color fadeOpaque = colOpaqueTop; fadeOpaque.a = 255;
                    // make a small rectangle under the vertical: top opaque, bottom transparent
                    sf::VertexArray fadeV(sf::Triangles, 6);
                    float yTopFade = colYTop + colDownLen;
                    float yBottomFade = yTopFade + FADE_H;
                    // triangle 1
                    fadeV[0] = sf::Vertex(sf::Vector2f(vertX, yTopFade), fadeOpaque);
                    fadeV[1] = sf::Vertex(sf::Vector2f(vertX + colSepThickness, yTopFade), fadeOpaque);
                    fadeV[2] = sf::Vertex(sf::Vector2f(vertX + colSepThickness, yBottomFade), colTransparentTop);
                    // triangle 2
                    fadeV[3] = sf::Vertex(sf::Vector2f(vertX, yTopFade), fadeOpaque);
                    fadeV[4] = sf::Vertex(sf::Vector2f(vertX + colSepThickness, yBottomFade), colTransparentTop);
                    fadeV[5] = sf::Vertex(sf::Vector2f(vertX, yBottomFade), colTransparentTop);
                    window.draw(fadeV);
                }

                // Right column line
                float rightX0 = rightColX;
                float rightXR = rightColX + colWidth;
                float rightCenterL = rightX0 + edgeFadeCol;
                float rightCenterR = rightXR + 7;
                if (rightCenterR > rightCenterL) {
                    sf::RectangleShape centerRectRight(sf::Vector2f(rightCenterR - rightCenterL, colSepThickness));
                    centerRectRight.setPosition(rightCenterL, colYTop);
                    centerRectRight.setFillColor(colOpaqueTop);
                    window.draw(centerRectRight);
                }
                if (rightCenterL > rightX0 + 1.f) {
                    sf::VertexArray leftGradColR(sf::Triangles, 6);
                    leftGradColR[0] = sf::Vertex(sf::Vector2f(rightX0, colYTop), colTransparentTop);
                    leftGradColR[1] = sf::Vertex(sf::Vector2f(rightCenterL, colYTop), colOpaqueTop);
                    leftGradColR[2] = sf::Vertex(sf::Vector2f(rightCenterL, colYBottom), colOpaqueTop);
                    leftGradColR[3] = sf::Vertex(sf::Vector2f(rightX0, colYTop), colTransparentTop);
                    leftGradColR[4] = sf::Vertex(sf::Vector2f(rightCenterL, colYBottom), colOpaqueTop);
                    leftGradColR[5] = sf::Vertex(sf::Vector2f(rightX0, colYBottom), colTransparentTop);
                    window.draw(leftGradColR);
                }

                // Draw a short vertical extension down the right side of the right column
                {
                    const float colDownLen = std::min(140.f, panel.getSize().y * 0.04f);
                    const float VERT_SHIFT = 8.f; // match left column shift
                    sf::RectangleShape vertR(sf::Vector2f(colSepThickness, colDownLen));
                    float vertRX = (rightColX + colWidth) - colSepThickness + VERT_SHIFT;
                    vertR.setPosition(vertRX, colYTop);
                    vertR.setFillColor(colOpaqueTop);
                    window.draw(vertR);

                    // stronger fade: increase height and top opacity for contrast
                    const float FADE_HR = 36.f;
                    sf::Color fadeOpaqueR = colOpaqueTop; fadeOpaqueR.a = 255;
                    sf::VertexArray fadeVR(sf::Triangles, 6);
                    float yTopFadeR = colYTop + colDownLen;
                    float yBottomFadeR = yTopFadeR + FADE_HR;
                    // triangle 1
                    fadeVR[0] = sf::Vertex(sf::Vector2f(vertRX, yTopFadeR), fadeOpaqueR);
                    fadeVR[1] = sf::Vertex(sf::Vector2f(vertRX + colSepThickness, yTopFadeR), fadeOpaqueR);
                    fadeVR[2] = sf::Vertex(sf::Vector2f(vertRX + colSepThickness, yBottomFadeR), colTransparentTop);
                    // triangle 2
                    fadeVR[3] = sf::Vertex(sf::Vector2f(vertRX, yTopFadeR), fadeOpaqueR);
                    fadeVR[4] = sf::Vertex(sf::Vector2f(vertRX + colSepThickness, yBottomFadeR), colTransparentTop);
                    fadeVR[5] = sf::Vertex(sf::Vector2f(vertRX, yBottomFadeR), colTransparentTop);
                    window.draw(fadeVR);
                }

                // draw left column bars
                for (size_t i = 0; i < leftLabels.size(); ++i) {
                    float y = startY + static_cast<float>(i) * (barHeight + spacing);
                    sf::RectangleShape bar(sf::Vector2f(colWidth, barHeight));
                    bar.setPosition(leftColX, y);
                    bar.setFillColor(sf::Color(36,36,36,220));
                    bar.setOutlineColor(sf::Color(100,100,100,200));
                    bar.setOutlineThickness(1.f);
                    window.draw(bar);

                    // draw icon placeholder on left side of bar
                    sf::RectangleShape iconSlot(sf::Vector2f(iconSlotW, barHeight - 12.f));
                    iconSlot.setPosition(leftColX + 8.f, y + 6.f);
                    iconSlot.setFillColor(sf::Color(24,24,24,200));
                    iconSlot.setOutlineThickness(1.f);
                    iconSlot.setOutlineColor(sf::Color(80,80,80,200));
                    size_t iconIndex = i;
                    if (iconIndex < iconSprites.size() && iconTextures[iconIndex].getSize().x > 0) {
                        sf::Vector2u tsize = iconTextures[iconIndex].getSize();
                        float slotH = barHeight - 12.f;
                        float slotW = iconSlotW;
                        float scale = std::min(slotH / static_cast<float>(tsize.y), slotW / static_cast<float>(tsize.x));
                        iconSprites[iconIndex].setScale(scale, scale);
                        float spriteW = tsize.x * scale;
                        float spriteX = leftColX + 8.f + (iconSlotW - spriteW) * 0.5f;
                        float spriteY = y + 6.f + (slotH - tsize.y * scale) * 0.5f;
                        iconSprites[iconIndex].setPosition(spriteX, spriteY);
                        window.draw(iconSprites[iconIndex]);
                    } else {
                        window.draw(iconSlot);
                    }

                    // draw the label right-aligned within the bar (leaving icon slot on the left)
                    sf::Text label;
                    label.setFont(font2);
                    label.setCharacterSize(24);
                    label.setFillColor(sf::Color::White);
                    std::string fullLeft = leftLabels[i];
                    label.setString(fullLeft);
                    sf::FloatRect lb = label.getLocalBounds();
                    float textRightPadding = 12.f;
                    float textX = leftColX + colWidth - textRightPadding - lb.width - lb.left;
                    float textY = y + (barHeight - lb.height) / 2.f - lb.top;
                    label.setPosition(textX, textY);
                    window.draw(label);
                }

                // draw right column bars
                for (size_t i = 0; i < rightLabels.size(); ++i) {
                    float y = startY + static_cast<float>(i) * (barHeight + spacing);
                    sf::RectangleShape bar(sf::Vector2f(colWidth, barHeight));
                    bar.setPosition(rightColX, y);
                    bar.setFillColor(sf::Color(36,36,36,220));
                    bar.setOutlineColor(sf::Color(100,100,100,200));
                    bar.setOutlineThickness(1.f);
                    window.draw(bar);

                    // icon slot on left of this bar too
                    sf::RectangleShape iconSlot(sf::Vector2f(iconSlotW, barHeight - 12.f));
                    iconSlot.setPosition(rightColX + 8.f, y + 6.f);
                    iconSlot.setFillColor(sf::Color(24,24,24,200));
                    iconSlot.setOutlineThickness(1.f);
                    iconSlot.setOutlineColor(sf::Color(80,80,80,200));
                    size_t iconIndexR = leftLabels.size() + i;
                    if (iconIndexR < iconSprites.size() && iconTextures[iconIndexR].getSize().x > 0) {
                        sf::Vector2u tsize = iconTextures[iconIndexR].getSize();
                        float slotH = barHeight - 12.f;
                        float slotW = iconSlotW;
                        float scale = std::min(slotH / static_cast<float>(tsize.y), slotW / static_cast<float>(tsize.x));
                        iconSprites[iconIndexR].setScale(scale, scale);
                        float spriteW = tsize.x * scale;
                        float spriteX = rightColX + 8.f + (slotW - spriteW) * 0.5f;
                        float spriteY = y + 6.f + (slotH - tsize.y * scale) * 0.5f;
                        iconSprites[iconIndexR].setPosition(spriteX, spriteY);
                        window.draw(iconSprites[iconIndexR]);
                    } else {
                        window.draw(iconSlot);
                    }

                    // draw the label right-aligned within the bar
                    sf::Text label;
                    label.setFont(font2);
                    label.setCharacterSize(24);
                    label.setFillColor(sf::Color::White);
                    std::string fullRight = rightLabels[i];
                    label.setString(fullRight);
                    sf::FloatRect rb = label.getLocalBounds();
                    float textRightPadding = 12.f;
                    float textX = rightColX + colWidth - textRightPadding - rb.width - rb.left;
                    float textY = y + (barHeight - rb.height) / 2.f - rb.top;
                    label.setPosition(textX, textY);
                    window.draw(label);
                }
             } // end if (!isSettings)
             else {
                // Draw settings sliders: Master, Music, SFX
                // use shared slider geometry declared above
                sf::Text lbl;
                lbl.setFont(font);
                lbl.setCharacterSize(static_cast<unsigned int>(28u * SETTINGS_UI_SCALE));
                lbl.setFillColor(sf::Color::White);

                auto drawSlider = [&](const std::string &text, float value, bool muted, float y) {
                    lbl.setString(text);
                    sf::FloatRect lb = lbl.getLocalBounds();
                    lbl.setOrigin(lb.left + 0.f, lb.top + lb.height/2.f);
                    lbl.setCharacterSize(static_cast<unsigned int>(28u * SETTINGS_UI_SCALE));
                    lbl.setPosition(panel.getPosition().x - panel.getSize().x/2.f + panelPaddingInner, y + 8.f * SETTINGS_UI_SCALE);
                    window.draw(lbl);

                    // Slider background
                    float bgHeight = 12.f * SETTINGS_UI_SCALE;
                    sf::RectangleShape bg(sf::Vector2f(sliderWidth, bgHeight));
                    bg.setPosition(sliderX, y);
                    bg.setFillColor(sf::Color(60,60,60,220));
                    bg.setOutlineThickness(1.f);
                    bg.setOutlineColor(sf::Color(100,100,100,180));
                    window.draw(bg);

                    // Filled portion
                    float frac = std::clamp(value / 100.f, 0.f, 1.f);
                    sf::RectangleShape fill(sf::Vector2f(sliderWidth * frac, bgHeight));
                    fill.setPosition(sliderX, y);
                    fill.setFillColor(sf::Color(200,200,200,220));
                    window.draw(fill);

                    // Knob
                    float kx = sliderX + sliderWidth * frac;
                    float knobR = 8.f * SETTINGS_UI_SCALE;
                    sf::CircleShape knob(knobR);
                    knob.setOrigin(knobR, knobR);
                    knob.setPosition(kx, y + bgHeight/2.f);
                    knob.setFillColor(sf::Color::White);
                    knob.setOutlineColor(sf::Color(120,120,120));
                    knob.setOutlineThickness(2.f);
                    window.draw(knob);

                    // Percent text
                    sf::Text pct;
                    pct.setFont(font2);
                    pct.setCharacterSize(static_cast<unsigned int>(18u * SETTINGS_UI_SCALE));
                    pct.setFillColor(sf::Color::White);
                    pct.setString(std::to_string(static_cast<int>(value)) + "%");
                    sf::FloatRect pb = pct.getLocalBounds();
                    pct.setPosition(sliderX + sliderWidth + 8.f * SETTINGS_UI_SCALE, y - pb.top);
                    window.draw(pct);

                    // Mute checkbox
                    float muteX = sliderX + sliderWidth + 60.f * SETTINGS_UI_SCALE; // slightly closer
                    sf::RectangleShape box(sf::Vector2f(22.f * SETTINGS_UI_SCALE, 22.f * SETTINGS_UI_SCALE));
                    box.setPosition(muteX, y - 6.f * SETTINGS_UI_SCALE);
                    box.setFillColor(sf::Color(30,30,30,220));
                    box.setOutlineColor(sf::Color::White);
                    box.setOutlineThickness(1.f);
                    window.draw(box);
                    if (muted) {
                        sf::VertexArray cross(sf::Lines, 4);
                        cross[0] = sf::Vertex(sf::Vector2f(muteX + 4.f * SETTINGS_UI_SCALE, y - 1.f * SETTINGS_UI_SCALE), sf::Color::Red);
                        cross[1] = sf::Vertex(sf::Vector2f(muteX + 22.f * SETTINGS_UI_SCALE, y + 15.f * SETTINGS_UI_SCALE), sf::Color::Red);
                        cross[2] = sf::Vertex(sf::Vector2f(muteX + 22.f * SETTINGS_UI_SCALE, y - 1.f * SETTINGS_UI_SCALE), sf::Color::Red);
                        cross[3] = sf::Vertex(sf::Vector2f(muteX + 4.f * SETTINGS_UI_SCALE, y + 15.f * SETTINGS_UI_SCALE), sf::Color::Red);
                        window.draw(cross);
                    }
                };

                drawSlider("Master", masterVolume, masterMuted, rowYStart);
                drawSlider("Music", musicVolume, musicMuted, rowYStart + rowSpacing * SETTINGS_UI_SCALE);
                drawSlider("SFX", sfxVolume, sfxMuted, rowYStart + rowSpacing * 2 * SETTINGS_UI_SCALE);
             }

            // Debug: draw hit rectangles for sliders and mute boxes so alignment can be tuned
            if (DRAW_HIT_DEBUG) {
                // slider hit rects (match the tightened hit rects used in handleEvents)
                sf::RectangleShape dbgMaster(sf::Vector2f(sliderWidth + 8.f, 20.f * SETTINGS_UI_SCALE));
                dbgMaster.setPosition(sliderX - 4.f * SETTINGS_UI_SCALE, rowYStart - 6.f * SETTINGS_UI_SCALE);
                 dbgMaster.setFillColor(sf::Color(255, 0, 0, 60));
                 dbgMaster.setOutlineColor(sf::Color::White);
                 dbgMaster.setOutlineThickness(1.f);
                 window.draw(dbgMaster);

                sf::RectangleShape dbgMusic(sf::Vector2f(sliderWidth + 8.f, 20.f * SETTINGS_UI_SCALE));
                dbgMusic.setPosition(sliderX - 4.f * SETTINGS_UI_SCALE, rowYStart + rowSpacing * SETTINGS_UI_SCALE - 6.f * SETTINGS_UI_SCALE);
                 dbgMusic.setFillColor(sf::Color(0, 255, 0, 60));
                 dbgMusic.setOutlineColor(sf::Color::White);
                 dbgMusic.setOutlineThickness(1.f);
                 window.draw(dbgMusic);

                sf::RectangleShape dbgSfx(sf::Vector2f(sliderWidth + 8.f, 20.f * SETTINGS_UI_SCALE));
                dbgSfx.setPosition(sliderX - 4.f * SETTINGS_UI_SCALE, rowYStart + rowSpacing * 2 * SETTINGS_UI_SCALE - 6.f * SETTINGS_UI_SCALE);
                 dbgSfx.setFillColor(sf::Color(0, 0, 255, 60));
                 dbgSfx.setOutlineColor(sf::Color::White);
                 dbgSfx.setOutlineThickness(1.f);
                 window.draw(dbgSfx);

                // mute boxes
                float muteX_dbg = sliderX + sliderWidth + 60.f * SETTINGS_UI_SCALE;
                sf::RectangleShape dbgMute(sf::Vector2f(22.f * SETTINGS_UI_SCALE, 22.f * SETTINGS_UI_SCALE));
                dbgMute.setFillColor(sf::Color(255, 255, 0, 60));
                dbgMute.setOutlineColor(sf::Color::White);
                dbgMute.setOutlineThickness(1.f);
                dbgMute.setPosition(muteX_dbg, rowYStart - 6.f * SETTINGS_UI_SCALE);
                 window.draw(dbgMute);
                 dbgMute.setPosition(muteX_dbg, rowYStart + rowSpacing * SETTINGS_UI_SCALE - 6.f * SETTINGS_UI_SCALE);
                 window.draw(dbgMute);
                 dbgMute.setPosition(muteX_dbg, rowYStart + rowSpacing * 2 * SETTINGS_UI_SCALE - 6.f * SETTINGS_UI_SCALE);
                 window.draw(dbgMute);

                // Draw mapped mouse position crosshair (default view) to help diagnose offsets
                sf::Vector2i mp = sf::Mouse::getPosition(window);
                sf::Vector2f mw = window.mapPixelToCoords(mp, window.getDefaultView());
                sf::CircleShape ch(4.f);
                ch.setOrigin(4.f, 4.f);
                ch.setPosition(mw);
                ch.setFillColor(sf::Color::Magenta);
                window.draw(ch);
                sf::VertexArray cross(sf::Lines, 4);
                cross[0] = sf::Vertex(sf::Vector2f(mw.x - 8.f, mw.y), sf::Color::Magenta);
                cross[1] = sf::Vertex(sf::Vector2f(mw.x + 8.f, mw.y), sf::Color::Magenta);
                cross[2] = sf::Vertex(sf::Vector2f(mw.x, mw.y - 8.f), sf::Color::Magenta);
                cross[3] = sf::Vertex(sf::Vector2f(mw.x, mw.y + 8.f), sf::Color::Magenta);
                window.draw(cross);

                // Show raw pixel coords and mapped world coords on screen
                sf::Text dbgText;
                dbgText.setFont(font);
                dbgText.setCharacterSize(14);
                dbgText.setFillColor(sf::Color::White);
                std::ostringstream ss; ss << "pixel: (" << mp.x << "," << mp.y << ")  world: (" << static_cast<int>(mw.x) << "," << static_cast<int>(mw.y) << ")";
                dbgText.setString(ss.str());
                dbgText.setPosition(mw.x + 12.f, mw.y - 18.f);
                window.draw(dbgText);
            }

            // Draw back button (icon + text) at top-right of the panel - explicit colors and hover state
            {
                const unsigned int BACK_CHAR_SIZE = static_cast<unsigned int>(36u);
                const float ICON_W = 52.f;
                const float ICON_PAD = 12.f;
                sf::Text tmpBack; tmpBack.setFont(font); tmpBack.setCharacterSize(BACK_CHAR_SIZE); tmpBack.setString("Back");
                sf::FloatRect backBounds = tmpBack.getLocalBounds();
                float backWidth = backBounds.width + backBounds.left + 28.f + ICON_W + ICON_PAD;
                float backHeight = backBounds.height + backBounds.top + 20.f;
                // Position at bottom-right inside the panel to match hit-tests and ensure it's fully inside
                sf::Vector2f backPos(panel.getPosition().x + panel.getSize().x/2.f - BACK_PANEL_MARGIN - backWidth,
                                     panel.getPosition().y + panel.getSize().y/2.f - BACK_PANEL_MARGIN - backHeight);

                // Draw a short separator line immediately above the back button (with fading edges)
                {
                    float lineThickness = 2.f;
                    float lineY = backPos.y - 12.f; // gap above the button
                    // Use the panel inner bounds computed earlier (xL/xR) so the line spans the whole panel
                    float lx_full = xL;
                    float rx_full = xR;
                    float edgeFadeLocal = std::min(32.f, (rx_full - lx_full) * 0.18f);
                    float centerL = lx_full + edgeFadeLocal;
                    float centerR = rx_full - edgeFadeLocal;
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

                // Outline color reflects hover state
                sf::Color outlineCol = backWasHovering ? sf::Color::Yellow : sf::Color::White;

                // Draw border background (transparent fill)
                backButtonBorder.setSize(sf::Vector2f(backWidth, backHeight));
                backButtonBorder.setFillColor(sf::Color::Transparent);
                backButtonBorder.setOutlineColor(outlineCol);
                backButtonBorder.setOutlineThickness(2.f);
                backButtonBorder.setOrigin(0.f, 0.f);
                backButtonBorder.setPosition(backPos);
                window.draw(backButtonBorder);

                // Icon slot
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
                    window.draw(backIconSprite);
                } else {
                    window.draw(iconSlot);
                }

                // Back label
                backButtonText.setFont(font);
                backButtonText.setCharacterSize(BACK_CHAR_SIZE);
                backButtonText.setString("Back");
                backButtonText.setFillColor(backWasHovering ? sf::Color::Yellow : sf::Color::White);
                // place text to the right of icon slot
                sf::FloatRect tb = backButtonText.getLocalBounds();
                float tx = backPos.x + 8.f + slotW + ICON_PAD;
                float ty = backPos.y + (backHeight - tb.height) / 2.f - tb.top;
                backButtonText.setPosition(tx, ty);
                window.draw(backButtonText);
            }
        }

        fadeRect.setPosition(0.f,0.f);
        fadeRect.setSize(sf::Vector2f(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)));
        window.draw(fadeRect);
    } else {
        // Cutscene playback: draw world using cameraView so background moves with the camera
        window.setView(cameraView);
        // Ensure the background texture covers the view. If the texture is smaller than the view,
        // scale it up to avoid uncovered (black) areas at the bottom/top.
        sf::Vector2u texSize = backgroundTexture.getSize();
        if (texSize.x > 0 && texSize.y > 0) {
            sf::Vector2f viewSize = cameraView.getSize();
            float sx = viewSize.x / static_cast<float>(texSize.x);
            float sy = viewSize.y / static_cast<float>(texSize.y);
            // choose the larger scale so the texture fully covers the view in both dimensions
            float useScale = std::max(1.0f, std::max(sx, sy));
            backgroundSprite.setScale(useScale, useScale);
        } else {
            backgroundSprite.setScale(1.f, 1.f);
        }
        backgroundSprite.setPosition(0.f, 0.f);
        window.draw(backgroundSprite);
        // draw world objects that move with the camera
        window.draw(planetSprite);
        window.draw(shipSprite);

        // Draw fade overlay in default view space (screen-aligned). First switch back to default view
        window.setView(window.getDefaultView());

        // Draw dialogue UI if active
        if (dialogueActive) {
            // Draw dialogue box
            window.draw(dialogueBox);

            // Compose visible substring for typewriter effect
            std::string visible;
            if (currentLineIndex < static_cast<int>(dialogueScript.size())) {
                const auto &dl = dialogueScript[currentLineIndex];
                int len = std::clamp(currentCharIndex, 0, static_cast<int>(dl.line.size()));
                visible = dl.line.substr(0, len);
            }

            // Wrap text to fit inside the dialog box (use small padding)
            const float padding = 12.0f;
            float maxTextWidth = dialogueBox.getSize().x - padding * 2.0f;
            std::string wrapped = wrapText(dialogueText, visible, maxTextWidth, dialogueDisplayFont, dialogueText.getCharacterSize());
            dialogueText.setFont(dialogueDisplayFont);
            dialogueText.setCharacterSize(24);
            dialogueText.setFillColor(sf::Color::White);
            dialogueText.setString(wrapped);

            // Position text at top-left inside the dialogue box
            sf::Vector2f boxPos = dialogueBox.getPosition();
            float textX = boxPos.x - dialogueBox.getSize().x / 2.0f + padding;
            float textY = boxPos.y - dialogueBox.getSize().y / 2.0f + padding;
            dialogueText.setPosition(textX, textY);
            window.draw(dialogueText);

            // Position skip prompt at bottom-right inside the dialog box
            sf::FloatRect spb = skipPromptText.getLocalBounds();
            float spX = boxPos.x + dialogueBox.getSize().x / 2.0f - padding - (spb.width + spb.left);
            float spY = boxPos.y + dialogueBox.getSize().y / 2.0f - padding - (spb.height + spb.top);
            skipPromptText.setPosition(spX, spY);
            window.draw(skipPromptText);
        }

        fadeRect.setPosition(0.f,0.f);
        fadeRect.setSize(sf::Vector2f(static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)));
        window.draw(fadeRect);
    }

    window.display();
}

} // namespace TDCod