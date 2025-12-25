#include "ExplosionProvider.hpp"
#include "Explosion.hpp"
#include "Vec2.h"
#include "Guts.hpp"
#include <cmath>

void ExplosionProvider::initProvider()
{
    Props::Explosion::init();
    Guts::init();
}

Props::Explosion* ExplosionProvider::getBase(const Vec2& pos, bool isTrace)
{
    Props::Explosion* e = Props::Explosion::add(pos.x, pos.y, 360.0f, 0.0f, 3.0f, 25.0f, 15, true);
    e->setDecrease(0.05f);
    e->setTrace(isTrace);

    return e;
}

Props::Explosion* ExplosionProvider::getHit(const Vec2& pos, float angle, bool isTrace)
{
    Props::Explosion* e = Props::Explosion::add(pos.x, pos.y, 170.0f, angle, 2.0f, 5.0f, 15, true);
    e->setDecrease(0.05f);
    e->setTrace(isTrace);

    return e;
}

Props::Explosion* ExplosionProvider::getThrough(const Vec2& pos, float angle, bool isTrace)
{
    Props::Explosion* e = Props::Explosion::add(pos.x, pos.y, 1.0f, angle, 3.0f, 3.0f, 30, true);
    e->setDecrease(0.035f);
    e->setTrace(isTrace);

    return e;
}

Props::Explosion* ExplosionProvider::getBigThrough(const Vec2& pos, float angle, bool isTrace)
{
    Props::Explosion* e = Props::Explosion::add(pos.x, pos.y, 1.0f, angle, 5.0f, 30.0f, 10, true);
    e->setDecrease(0.25f);
    e->setTrace(isTrace);

    return e;
}

Props::Explosion* ExplosionProvider::getBig(const Vec2& pos, bool isTrace)
{
    Props::Explosion* e = Props::Explosion::add(pos.x, pos.y, 360.0f, 0.0f, 4.0f, 10.0f, 25, true);
    e->setDecrease(0.1f);
    e->setTrace(isTrace);

    return e;
}

Props::Explosion* ExplosionProvider::getBigFast(const Vec2& pos, bool isTrace)
{
    Props::Explosion* e = Props::Explosion::add(pos.x, pos.y, 360.0f, 0.0f, 6.0f, 10.0f, 25, true);
    e->setDecrease(0.07f);
    e->setTrace(isTrace);

    return e;
}

Props::Explosion* ExplosionProvider::getBigSlow(const Vec2& pos, bool isTrace)
{
    Props::Explosion* e = Props::Explosion::add(pos.x, pos.y, 360.0f, 0.0f, 2.0f, 70.0f, 15, true);
    e->setDecrease(0.025f);
    e->setTrace(isTrace);

    return e;
}

Props::Explosion* ExplosionProvider::getClose(const Vec2& pos, float angle, bool isTrace)
{
    Props::Explosion* e = Props::Explosion::add(pos.x, pos.y, 170.0f, angle+3.14159265f, 20.0f, 10.0f, 50, true);
    e->setDecrease(0.1f);
    e->setTrace(isTrace);

    return e;
}
