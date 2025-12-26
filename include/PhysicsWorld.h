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

private:
    void resolveCollisions();
    bool isColliding(PhysicsBody* a, PhysicsBody* b);
    bool isCircleCircle(PhysicsBody* a, PhysicsBody* b);
    bool isCircleAABB(PhysicsBody* circle, PhysicsBody* box);

    void resolveDynamicCollision(PhysicsBody& a, PhysicsBody& b);
    void resolveStaticCollision(PhysicsBody& a, PhysicsBody& b);
    void handleCollision(Entity* a, Entity* b);
};