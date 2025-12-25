#ifndef GAME_H
#define GAME_H

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include "Player.h"
#include "LevelManager.h"
#include "PhysicsWorld.h"
#include <array>
#include <vector>

class Game {
public:
    Game();
    void run();
    sf::RenderWindow& getWindow();
    
    bool isCollision(const sf::FloatRect& rect1, const sf::FloatRect& rect2);
    void addPoints(int amount);
    int getPoints() const;
    void reset();
    
    sf::Sprite& getMapSprite(int level);
    sf::Vector2u getMapSize(int level); // Added to get map size dynamically

    // Pause control
    bool isPaused() const { return paused; }
    void setPaused(bool p) { paused = p; }
    void togglePaused() { paused = !paused; }

private:
    std::vector<std::unique_ptr<Bullet>> bullets;

    sf::RenderWindow window;
    Player player;
    LevelManager levelManager;
    PhysicsWorld physics;
    
    sf::Sprite mapSprite1;
    sf::Sprite mapSprite2;
    sf::Sprite mapSprite3;
    sf::Texture mapTexture1;
    sf::Texture mapTexture2;
    sf::Texture mapTexture3;
    sf::RectangleShape background;
    
    // HUD tally textures for rounds (0..4)
    std::array<sf::Texture,5> tallyTextures;

    int points;
    int zombiesToNextLevel;
    sf::Text pointsText;
    sf::Font font;
	sf::Font font2;
    sf::Clock clock;
    sf::View gameView;
    // Deferred shooting request filled in processInput and handled in update
    bool shootRequested = false;
    sf::Vector2f shootTarget;

	void drawVictoryScreen();
    void processInput();
    void update(float deltaTime);
    void render();
    
    void checkZombiePlayerCollisions();
    void checkPlayerBoundaries();
    
    void drawHUD();

    sf::Music cutsceneMusic;
    sf::Music backgroundMusic;
    sf::Music waveStartSound;
    sf::SoundBuffer zombieBiteBuffer;
    // pool of sounds to allow overlapping bite SFX
    std::vector<sf::Sound> zombieBiteSounds;
    size_t nextBiteSoundIndex = 0;
    void playZombieBite();

    // UI sounds for menu hover and click
    sf::SoundBuffer uiHoverBuffer;
    sf::SoundBuffer uiClickBuffer;
    sf::Sound uiHoverSound;
    sf::Sound uiClickSound;
    // last hover state to avoid replaying hover sound every frame
    int lastHoveredMenuIndex = -1;
    bool lastBackHovered = false;
    // Settings panel audio state
    float masterVolume = 100.f;
    float musicVolume = 100.f;
    float sfxVolume = 100.f;
    bool masterMuted = false;
    bool musicMuted = false;
    bool sfxMuted = false;
    int draggingSlider = -1; // -1 none, 0 master,1 music,2 sfx
    void applyAudioSettings();

    // Player sprite-sheet textures (optional). Game will try to load them and pass to Player.
    // For feet we support one texture per feet state (idle, walk, run, strafe left, strafe right)
    std::array<sf::Texture,5> playerFeetStateTextures;

    // Icons used in the pause controls panel (load from Scene assets to match menu)
    std::vector<sf::Texture> pauseIconTextures;
    std::vector<sf::Sprite> pauseIconSprites;

    // Back button icon (escape icon used in scene)
    sf::Texture backIconTexture;
    sf::Sprite backIconSprite;

    // Weapon icon textures used by LevelManager HUD (must outlive LevelManager usage)
    sf::Texture pistolIconTexture;
    sf::Texture rifleIconTexture;
    // Key icons for HUD equip buttons
    sf::Texture keyIcon1Texture;
    sf::Texture keyIcon2Texture;
    sf::Texture KeyIconEscTexture;
    // Reload key icon used by on-screen reload prompt (optional)
    sf::Texture reloadKeyTexture;
    sf::Sprite reloadKeySprite;
    // Optional Esc key icon used in tutorial dialog
    sf::Texture escKeyTexture;

    // Blood overlay texture shown as screen-space HUD when player is damaged
    sf::Texture bloodTexture;
    sf::Sprite bloodSprite;
    // Maximum alpha when health is zero (0..255)
    float bloodMaxAlpha = 200.f;
    // Exponent controlling how quickly the blood intensifies as health falls (1.0 linear, 2.0 squared)
    float bloodIntensityPow = 2.0f;

    // Guts texture for particle sprites (optional)
    sf::Texture gutsTexture;
    // Blood particle texture (used by Explosion for textured blood particles)
    sf::Texture bloodParticleTexture;

    // Render-to-texture for world rendering so we can apply a desaturation shader
    sf::RenderTexture worldRenderTexture;
    sf::Sprite worldTextureSprite;
    sf::Shader desaturateShader;
    bool desaturateShaderLoaded = false;
    // Exponent controlling desaturation curve: higher -> slower ramp for mid-health values
    float desaturatePow = 2.5f;

    // Muzzle flash texture (placeholder path will be used in Game.cpp)
    sf::Texture muzzleFlashTexture;
    // Shared shadow texture used by player and zombies
    sf::Texture shadowTexture;

    // Upper sheets per weapon (idle, move, shoot, reload)
    std::array<sf::Texture,4> playerPistolSheets;
    std::array<sf::Texture,4> playerRifleSheets;
    std::array<sf::Texture,4> playerShotgunSheets;

    // Interpolation alpha between physics steps
    float renderAlpha = 1.0f;
    // Debug visuals
    bool debugDrawHitboxes = false;
    // Track whether player's physics body has been removed from the PhysicsWorld after death
    bool playerBodyRemoved = false;
    // Ensure we notify zombies once when player dies
    bool playerDeathNotified = false;

    // Aiming camera zoom and offset
    sf::Vector2f baseViewSize; // recorded after initial setup
    float currentZoomMul = 1.0f;
    float targetZoomMul = 1.0f;
    float aimZoomMul = 0.98f; // zoom factor when aiming (<1 => zoom in)
    float zoomLerpSpeed = 8.0f; // smoothing speed
    float aimOffsetDistance = 2000.0f; // how far to shift camera toward aim direction
    float centerLerpSpeed = 10.0f; // smoothing for center movement

    // Pause state
    bool paused = false;
    // Whether the pause overlay should show the controls panel
    bool showingControls = false;
    // Whether the pause overlay should show the settings panel
    bool showingSettings = false;
    // Game over fade state
    bool gameOverTriggered = false;
    float gameOverAlpha = 0.0f; // 0..1
    float gameOverFadeDuration = 2.25f; // seconds to reach full alpha
    // Game over menu: hover and delayed fade-in
    int gameOverHoveredIndex = -1; // -1 none, 0 retry, 1 exit
    int lastGameOverHovered = -1; // for hover sound dedupe
    float gameOverMenuTimer = 0.0f;
    float gameOverMenuDelay = 0.45f; // seconds after title before menu fades
    float gameOverMenuFadeDuration = 0.6f;
    float gameOverMenuAlpha = 0.0f; // 0..1
};

#endif // GAME_H