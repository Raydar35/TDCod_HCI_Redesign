#include "PhysicsWorld.h"
#include <cmath>
#include <iostream>

// local debug logging timer
static float g_debugLogTimer = 0.0f;
static constexpr float g_debugLogInterval = 1.0f;

void PhysicsWorld::addBody(PhysicsBody* body, bool isStatic) {
    if (!body) return;
    auto &vec = (isStatic ? staticBodies : dynamicBodies);
    // Prevent duplicate registration
    if (std::find(vec.begin(), vec.end(), body) != vec.end()) {
        if (debugLogging) {
            std::cout << "[PhysicsWorld] addBody skipped duplicate registration" << std::endl;
        }
        return;
    }
    vec.push_back(body);
}

void PhysicsWorld::removeBody(PhysicsBody* body) {
    if (!body) return;
    // Remove from dynamic bodies (erase all instances)
    auto it_dynamic = std::remove(dynamicBodies.begin(), dynamicBodies.end(), body);
    if (it_dynamic != dynamicBodies.end()) {
        dynamicBodies.erase(it_dynamic, dynamicBodies.end());
        return;
    }

    // Remove from static bodies (erase all instances)
    auto it_static = std::remove(staticBodies.begin(), staticBodies.end(), body);
    if (it_static != staticBodies.end()) {
        staticBodies.erase(it_static, staticBodies.end());
        return;
    }
}

void PhysicsWorld::update(float dt) {
    for (auto* body : dynamicBodies) {
        body->applyDamping(0.1f * dt);
        body->update(dt);
    }
    resolveCollisions();

    // Cleanup: remove any bodies whose owner was destroyed to prevent vector growth
    auto deadPredicate = [](PhysicsBody* b) {
        return (b == nullptr) || (b->owner == nullptr) || (!b->owner->isAlive());
    };

    size_t beforeDynamic = dynamicBodies.size();
    auto itd = std::remove_if(dynamicBodies.begin(), dynamicBodies.end(), deadPredicate);
    if (itd != dynamicBodies.end()) dynamicBodies.erase(itd, dynamicBodies.end());
    size_t removedDynamic = beforeDynamic - dynamicBodies.size();

    size_t beforeStatic = staticBodies.size();
    auto its = std::remove_if(staticBodies.begin(), staticBodies.end(), deadPredicate);
    if (its != staticBodies.end()) staticBodies.erase(its, staticBodies.end());
    size_t removedStatic = beforeStatic - staticBodies.size();

    if (debugLogging) {
        g_debugLogTimer += dt;
        if (g_debugLogTimer >= g_debugLogInterval) {
            g_debugLogTimer = 0.0f;
            std::cout << "[PhysicsWorld] dynamicBodies=" << dynamicBodies.size()
                      << " staticBodies=" << staticBodies.size()
                      << " lastCollisionChecks=" << lastCollisionChecks
                      << " prunedDynamic=" << removedDynamic << " prunedStatic=" << removedStatic
                      << std::endl;
        }
    }
}

void PhysicsWorld::resolveCollisions() {
    // reset collision counter
    lastCollisionChecks = 0;

    for (auto* a : dynamicBodies) {
        // Skip dead entities
        if (!a->owner || !a->owner->isAlive()) continue;

        for (auto* b : dynamicBodies) {
            if (a == b || !b->owner || !b->owner->isAlive()) continue;

            // count this collision test
            ++lastCollisionChecks;
            if (isColliding(a, b)) {
                if (!a->isTrigger && !b->isTrigger) {
                    resolveDynamicCollision(*a, *b);
                }
                handleCollision(a->owner, b->owner);
            }
        }

        for (auto* s : staticBodies) {
            if (!s->owner || !s->owner->isAlive()) continue;

            ++lastCollisionChecks;
            if (isColliding(a, s)) {
                if (!a->isTrigger && !s->isTrigger) {
                    resolveStaticCollision(*a, *s);
                }
                handleCollision(a->owner, s->owner);
            }
        }
    }
}

bool PhysicsWorld::isColliding(PhysicsBody* a, PhysicsBody* b) {
    if (a->isCircle && b->isCircle)
        return isCircleCircle(a, b);
    else
        return isCircleAABB(a->isCircle ? a : b, a->isCircle ? b : a);
}

bool PhysicsWorld::isCircleCircle(PhysicsBody* a, PhysicsBody* b) {
    float r1 = a->size.x / 2.f;
    float r2 = b->size.x / 2.f;
    Vec2 diff = a->position - b->position;
    float distSq = diff.x * diff.x + diff.y * diff.y;
    float radiusSum = r1 + r2;
    return distSq <= radiusSum * radiusSum;
}

bool PhysicsWorld::isCircleAABB(PhysicsBody* circle, PhysicsBody* box) {
    float radius = circle->size.x / 2.f;
    float cx = circle->position.x;
    float cy = circle->position.y;

    float halfW = box->size.x / 2.f;
    float halfH = box->size.y / 2.f;

    float left = box->position.x - halfW;
    float right = box->position.x + halfW;
    float top = box->position.y - halfH;
    float bottom = box->position.y + halfH;

    // Clamp circle center to the box boundaries
    float closestX = std::max(left, std::min(cx, right));
    float closestY = std::max(top, std::min(cy, bottom));

    float dx = cx - closestX;
    float dy = cy - closestY;

    return (dx * dx + dy * dy) <= radius * radius;
}

void PhysicsWorld::resolveDynamicCollision(PhysicsBody& a, PhysicsBody& b) {
    // Calculate the normal vector between the two bodies
    Vec2 normal = b.position - a.position;
    normal.normalize();

    // Calculate the relative velocity along the normal direction
    float relativeVelocity = (b.velocity - a.velocity).dot(normal);

    // If the relative velocity is positive, no collision resolution is needed
    if (relativeVelocity > 0) return;

    // Coefficient of restitution (e) for elastic collisions
    float e = 0.2f;

    // Calculate the inverse masses (mass scaling)
    float invMassA = a.isStatic ? 0.f : 1.f / a.mass;
    float invMassB = b.isStatic ? 0.f : 1.f / b.mass;

    // Calculate the impulse magnitude (scaled by inverse mass)
    float j = -(1 + e) * relativeVelocity / (invMassA + invMassB);

    // Calculate the impulse vector
    Vec2 impulse = normal * j;

    // Apply the impulse to update the velocities of both bodies
    if (!a.isStatic) a.velocity -= impulse * invMassA;
    if (!b.isStatic) b.velocity += impulse * invMassB;

    // Positional correction (optional)
    const float percent = 0.8f; // usually 20% to 80%
    const float slop = 0.01f;   // small value to prevent jitter

    Vec2 diff = b.position - a.position;
    float distance = diff.length();
    float penetration = (a.size.x / 2.f + b.size.x / 2.f) - distance;

    if (penetration > slop) {
        diff.normalize(); // Normalize in-place
        Vec2 correction = diff * (percent * penetration / (invMassA + invMassB));
        if (!a.isStatic) a.position -= correction * invMassA;
        if (!b.isStatic) b.position += correction * invMassB;
    }
}



void PhysicsWorld::resolveStaticCollision(PhysicsBody& a, PhysicsBody& b) {
    // Only resolve if 'a' is a circle and 'b' is a box (AABB)
    if (!a.isCircle || b.isCircle) return;

    float radius = a.size.x / 2.f;

    // AABB bounds
    float halfW = b.size.x / 2.f;
    float halfH = b.size.y / 2.f;

    float left = b.position.x - halfW;
    float right = b.position.x + halfW;
    float top = b.position.y - halfH;
    float bottom = b.position.y + halfH;

    // Clamp circle center to AABB
    float closestX = std::max(left, std::min(a.position.x, right));
    float closestY = std::max(top, std::min(a.position.y, bottom));

    // Compute vector from closest point to circle center
    Vec2 diff = a.position - Vec2(closestX, closestY);
    float distSq = diff.x * diff.x + diff.y * diff.y;

    if (distSq < radius * radius) {
        float distance = std::sqrt(distSq);
        float penetration = radius - distance;

        if (penetration > 0.01f) {
            if (distance != 0.f) {
                diff.normalize(); // normalize in-place
                a.position += diff * penetration;
            }
            else {
                // If perfectly overlapping, push upward
                a.position.y -= penetration;
            }
        }


        // Reflect velocity with some energy loss
        a.velocity = a.velocity * -0.3f;
    }
}


void PhysicsWorld::handleCollision(Entity* a, Entity* b) {
    if (a && b) {
        a->onCollision(b);
        b->onCollision(a);
    }
}