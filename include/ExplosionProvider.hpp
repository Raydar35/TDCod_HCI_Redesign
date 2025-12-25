#pragma once
#include "Explosion.hpp"

struct Vec2;

class ExplosionProvider {
public:
    static void initProvider();
    static Props::Explosion* getHit(const Vec2& pos, float angle, bool isTrace = false);
    static Props::Explosion* getBase(const Vec2& pos, bool isTrace = false);
    static Props::Explosion* getThrough(const Vec2& pos, float angle, bool isTrace = false);
    static Props::Explosion* getBigThrough(const Vec2& pos, float angle, bool isTrace = false);
    static Props::Explosion* getBig(const Vec2& pos, bool isTrace = false);
    static Props::Explosion* getBigFast(const Vec2& pos, bool isTrace = false);
    static Props::Explosion* getBigSlow(const Vec2& pos, bool isTrace = false);
    static Props::Explosion* getClose(const Vec2& pos, float angle, bool isTrace = false);
};

// Backwards-compatible wrapper used by other parts of the codebase
// (some files call ExplosionProviderWrapper::getHit etc.). Forward to ExplosionProvider.
class ExplosionProviderWrapper {
public:
    static void initProvider() { ExplosionProvider::initProvider(); }
    static Props::Explosion* getHit(const Vec2& pos, float angle) { return ExplosionProvider::getHit(pos, angle); }
    static Props::Explosion* getBase(const Vec2& pos) { return ExplosionProvider::getBase(pos); }
    static Props::Explosion* getThrough(const Vec2& pos, float angle) { return ExplosionProvider::getThrough(pos, angle); }
    static Props::Explosion* getBigThrough(const Vec2& pos, float angle) { return ExplosionProvider::getBigThrough(pos, angle); }
    static Props::Explosion* getBig(const Vec2& pos) { return ExplosionProvider::getBig(pos); }
    static Props::Explosion* getBigFast(const Vec2& pos) { return ExplosionProvider::getBigFast(pos); }
    static Props::Explosion* getBigSlow(const Vec2& pos) { return ExplosionProvider::getBigSlow(pos); }
    static Props::Explosion* getClose(const Vec2& pos, float angle) { return ExplosionProvider::getClose(pos, angle); }
};
