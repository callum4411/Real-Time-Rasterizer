//
// Created by smith on 2026-06-19.
//

#ifndef RENDERER_ENTITY_H
#define RENDERER_ENTITY_H

#include <functional>
#include "Transform.h"

class Mesh;   // we only store a Mesh*, so a forward declaration is enough here

// An Entity is "one thing in the scene": some geometry (mesh), where/how it sits
// (transform), and optionally how it moves over time (update). The scene is just
// a std::vector<Entity>; the render loop walks that list every frame.
//
// We deliberately store a Mesh* (a non-owning pointer) rather than a Mesh. A
// Mesh owns GPU buffers and forbids copying, but lots of entities may want to
// share ONE cube mesh. The pointer lets many entities reuse the same geometry
// while the Mesh objects themselves live elsewhere (in main) and outlive these.
struct Entity {
    Mesh* mesh = nullptr;     // which geometry to draw (shared, not owned)
    Transform transform;      // where/how big/which way it is, this frame

    // The animation hook. Given the current time (seconds since start), an
    // entity may rewrite its own transform. An empty std::function means "this
    // entity is static" -- the render loop simply skips the update for it.
    // Using std::function lets each entity carry its OWN little movement recipe
    // as a lambda, instead of the render loop hard-coding every object's motion.
    std::function<void(Transform&, float time)> update;
};

#endif //RENDERER_ENTITY_H
