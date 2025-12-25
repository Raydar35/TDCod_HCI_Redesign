#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <functional>
#include <cmath>

// Lightweight animator supporting either per-frame textures or a single spritesheet + rects.
class Animator {
public:
    Animator()
        : sprite(nullptr), texFrames(nullptr), sheetTexture(nullptr), frameTime(0.1f), timer(0.f), currentFrame(0), playing(false), loop(true) {}

    void setSprite(sf::Sprite* s) { sprite = s; if (sprite && hasFrames()) applyFrame(0); }

    // Per-frame textures (your current approach)
    void setFrames(const std::vector<sf::Texture>* textures, float frameTimeSecs, bool looping = true) {
        texFrames = textures;
        sheetTexture = nullptr;
        rects.clear();
        frameTime = frameTimeSecs;
        loop = looping;
        timer = 0.f;
        currentFrame = 0;
        if (sprite && texFrames && !texFrames->empty()) applyFrame(0);
    }

    // Spritesheet + rects
    void setFrames(const sf::Texture* sheet, const std::vector<sf::IntRect>& frames, float frameTimeSecs, bool looping = true) {
        sheetTexture = sheet;
        rects = frames;
        texFrames = nullptr;
        frameTime = frameTimeSecs;
        loop = looping;
        timer = 0.f;
        currentFrame = 0;
        if (sprite && sheetTexture && !rects.empty()) applyFrame(0);
    }

    void play(bool restart = true) {
        if (restart) { timer = 0.f; currentFrame = 0; }
        playing = true;
        if (sprite) applyFrame(currentFrame);
    }
    void stop() { playing = false; timer = 0.f; currentFrame = 0; }
    void pause() { playing = false; }
    bool isPlaying() const { return playing; }

    void update(float dt) {
        if (!playing || !hasFrames()) return;
        timer += dt;
        while (timer >= frameTime) {
            timer -= frameTime;
            advanceFrame();
            if (!playing) break; // stop if reached end and not looping
        }
    }

    void setOnComplete(std::function<void()> cb) { onComplete = std::move(cb); }
    void setOnFrame(std::function<void(size_t)> cb) { onFrame = std::move(cb); }

    size_t getCurrentFrameIndex() const { return currentFrame; }
    void setLoop(bool v) { loop = v; }

    // Allow runtime adjustment of frame time without resetting animation state
    void setFrameTime(float ft) { frameTime = ft; }

    // Set current frame index and apply it (resets timer)
    void setCurrentFrame(size_t idx) { if (!sprite) return; if (texFrames) currentFrame = (idx < texFrames->size()) ? idx : 0; else if (sheetTexture) currentFrame = (idx < rects.size()) ? idx : 0; timer = 0.f; applyFrame(currentFrame); }

private:
    sf::Sprite* sprite;
    const std::vector<sf::Texture>* texFrames = nullptr; // per-frame textures
    const sf::Texture* sheetTexture = nullptr; // single texture atlas
    std::vector<sf::IntRect> rects; // rects inside atlas
    float frameTime;
    float timer;
    size_t currentFrame;
    bool playing;
    bool loop;
    std::function<void()> onComplete;
    std::function<void(size_t)> onFrame;

    bool hasFrames() const {
        return (texFrames && !texFrames->empty()) || (sheetTexture && !rects.empty());
    }

    void applyFrame(size_t idx) {
        if (!sprite) return;
        if (texFrames && idx < texFrames->size()) {
            sprite->setTexture((*texFrames)[idx]);
        } else if (sheetTexture && idx < rects.size()) {
            sprite->setTexture(*sheetTexture);
            sprite->setTextureRect(rects[idx]);
        }
        if (onFrame) onFrame(idx);
    }

    void advanceFrame() {
        if (texFrames) {
            ++currentFrame;
            if (currentFrame >= texFrames->size()) {
                if (loop) currentFrame = 0;
                else { currentFrame = texFrames->empty() ? 0 : texFrames->size() - 1; playing = false; if (onComplete) onComplete(); }
            }
            applyFrame(currentFrame);
        } else if (sheetTexture) {
            ++currentFrame;
            if (currentFrame >= rects.size()) {
                if (loop) currentFrame = 0;
                else { currentFrame = rects.empty() ? 0 : rects.size() - 1; playing = false; if (onComplete) onComplete(); }
            }
            applyFrame(currentFrame);
        }
    }
};
