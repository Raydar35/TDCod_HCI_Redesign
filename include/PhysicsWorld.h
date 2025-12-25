#pragma once

#include <vector>
#include "PhysicsBody.h"
#include "Entity.h"

class PhysicsWorld {
public:
    std::vector<PhysicsBody*> dynamicBodies;
    std::vector<PhysicsBody*> staticBodies;

    void addBody(PhysicsBody* body, bool isStatic);
    void removeBody(PhysicsBody* body);
    void update(float dt);

    // Debug / diagnostics
    void setDebugLogging(bool enabled) { debugLogging = enabled; }
    int getDynamicBodyCount() const { return static_cast<int>(dynamicBodies.size()); }
    int getStaticBodyCount() const { return static_cast<int>(staticBodies.size()); }
    int getLastCollisionChecks() const { return lastCollisionChecks; }

private:
    void resolveCollisions();
    bool isColliding(PhysicsBody* a, PhysicsBody* b);
    bool isCircleCircle(PhysicsBody* a, PhysicsBody* b);
    bool isCircleAABB(PhysicsBody* circle, PhysicsBody* box);

    void resolveDynamicCollision(PhysicsBody& a, PhysicsBody& b);
    void resolveStaticCollision(PhysicsBody& a, PhysicsBody& b);
    void handleCollision(Entity* a, Entity* b);

    // Debugging fields
    bool debugLogging = false;
    int lastCollisionChecks = 0;
};