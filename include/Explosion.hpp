#pragma once
#include <SFML/Graphics.hpp>
#include <vector>

namespace Props {

    struct Particle
    {
        Particle();
        void update();
        float _x, _y;
        float _vx, _vy;
        float _vax, _vay;
        float _size;
        sf::Color _color;
    };

    class Explosion
    {
    public:
        Explosion();
        Explosion(float x, float y, float openAngle, float angle, float speed, float size, size_t n, bool blood = false);
        ~Explosion();

        void initPhysics(void* world) {}
        void update(void* world);
        void render(sf::RenderWindow& window);
        void setTrace(bool isTrace) { _isTrace = isTrace; }
        void setDecrease(float d) { _decrease = d; }
        void setSpeed(int32_t d) { _max_speed = d; }
        void kill(void* world) {}

        static void init();

        // Static management API
        static Explosion* add(float x, float y, float openAngle, float angle, float speed, float size, size_t n, bool blood = false);
        static void updateAll(float dt);
        static void renderAll(sf::RenderWindow& window);

        // Draw only the persistent ground canvas (call before rendering entities so stains appear under them)
        static void renderGround(sf::RenderWindow& window);
        // Texture support for blood particles (optional)
        static void setTexture(const sf::Texture& tex);
        // Set ground canvas size (should be called by Game with map pixel size so decals align with world coords)
        static void setGroundCanvasSize(unsigned int width, unsigned int height);
        // Commit remaining particles into ground canvas immediately (called on explosion end)
        void commitToGround();

    private:
        size_t _n;
        float  _ratio;
        float  _decrease;
        float  _openAngle;
        float  _angle;
        int32_t    _max_speed;
        float  _size;

        bool   _isTrace;
        bool   _traceOnEnd;
        bool   _isBlood;

        std::vector<Particle>  _particles;
        sf::VertexArray _vertexArray;

        // Explosion origin (used to detect and discard outlier particles)
        float _cx = 0.f;
        float _cy = 0.f;

        // maximum allowed distance (pixels) a particle may travel from origin before being ignored
        // tuned at construction time to scale with particle size
        float _maxAllowedDrawDistance = 256.f;

        static size_t _textureID;
        static std::vector<Explosion*> _active;
        static std::vector<float> _preCalculatedVx;
        static std::vector<float> _preCalculatedVy;

        // optional texture used for blood particles
        static sf::Texture _texture;

        static float getRandVx(int i);
        static float getRandVy(int i);
        bool _committedToGround = false; // true once this explosion's final particles were stamped to ground
    };

} // namespace Props