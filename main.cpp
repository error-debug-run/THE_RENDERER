#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "src/Renderer.h"
#include "src/Scene.h"
#include "src/PhysicsInterface.h"
#include <iostream>

// your physics engine
#include "physics/World.h"
#include "physics/2D_BODY.h"
#include "physics/COLLISION/collider.h"
#include "physics/COLLISION/collision_resolver.h"


const uint32_t WIDTH = 1280;
const uint32_t HEIGHT = 720;

int main() {

    // ─── GLFW Init ─────────────────────────────
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "CAP", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        return -1;
    }

    // ─── Physics World ─────────────────────────
    World physicsWorld;

    Body a;
    a.mass = 1.0f;
    a.radius = 0.1f;
    a.position = Vec2(-0.5f, 0.5f);
    a.velocity = Vec2(0.3f, 0.0f);

    Body b;
    b.mass = 1.0f;
    b.radius = 0.1f;
    b.position = Vec2(0.5f, 0.5f);
    b.velocity = Vec2(-0.3f, 0.0f);

    physicsWorld.addBody(a);
    physicsWorld.addBody(b);

    // ─── Interface + Scene Init ────────────────
    PhysicsInterface iface;
    Scene scene;

    // spawn one entity per body
    auto& bodies = physicsWorld.getBodies();
    for (int i = 0; i < (int)bodies.size(); i++) {
        uint32_t id = iface.spawn(
            bodies[i].position.x,
            bodies[i].position.y,
            bodies[i].radius
        );
    }

    // flush spawn events into scene
    for (auto& e : iface.pullEvents()) {
        if (e.type == BodyEvent::Type::SPAWN)
            scene.addEntity(e.id);
    }
    iface.clearEvents();
    scene.rebuildBVH();

    // ─── Renderer Init ─────────────────────────
    Renderer renderer;
    renderer.init(window);
    std::cout << "Init done\n";

    // ─── Main Loop ─────────────────────────────
    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        float now = (float)glfwGetTime();
        float dt = now - lastTime;
        lastTime = now;

        // ── 1. step physics ─────────────────────
        physicsWorld.update(dt);

        // ── 2. check collisions ─────────────────
        auto& bs = physicsWorld.getBodies();
        for (int i = 0; i < (int)bs.size(); i++) {
            for (int j = i + 1; j < (int)bs.size(); j++) {
                if (Collider::isColliding(bs[i], bs[j])) {
                    collision_resolver::resolve_collision(bs[i], bs[j]);
                }
            }
        }

        // ── 3. push physics → interface ─────────
        std::vector<BodyState> states;
        for (auto& body : bs) {
            BodyState s;
            s.id = &body - &bs[0];  // index as id for now
            s.x = body.position.x;
            s.y = body.position.y;
            s.radius = body.radius;
            states.push_back(s);
        }
        iface.push(states);

        // ── 4. sync scene ───────────────────────
        for (auto& s : iface.pull()) {
            scene.updateEntity(s);
        }
        scene.syncBVH();

        // ── 3. render ───────────────────────────
        renderer.drawFrame(scene);
    }

    // ─── Cleanup ───────────────────────────────
    renderer.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}