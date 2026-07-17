// src/initializer/Initializer.h
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../Renderer.h"
#include "../Scene.h"
#include "../PhysicsInterface.h"
#include "../scf/SCFParser.h"
#include "objects/objects.h"

#include "physics/World.h"
#include "physics/2D_BODY.h"
#include "physics/COLLISION/collider.h"
#include "physics/COLLISION/collision_resolver.h"

#include <iostream>
#include <string>

class Initializer {
public:
    Initializer(GLFWwindow* window) : window(window) {}

    void setup(const std::string& scenePath) {
        // ── parse scene file (the .scf file parser and docs in src/SCF) ───────────────────
        SCFParser parser;
        auto root = parser.parse(scenePath);

        if (!root) {
            std::cerr << "SCF: root is null\n";
            return;
        }

        // debug — print what the parser actually sees/////////////////////
        std::cout << "SCF root children:\n";
        for (auto& c : root->children)
            std::cout << "  [" << c->name << "]\n";

        auto scene_node = root->get("scene");
        if (!scene_node) {
            std::cerr << "SCF: no 'scene' node found\n";
            return;
        }

        std::cout << "scene children:\n";
        for (auto& c : scene_node->children)
            std::cout << "  [" << c->name << "]\n";

        auto objects_node = scene_node->get("objects");
        if (!objects_node) {
            std::cerr << "SCF: no 'objects' node found\n";
            return;
        }

        std::cout << "objects children:\n";
        for (auto& c : objects_node->children)
            std::cout << "  [" << c->name << "]\n";

        /////////////////////////////////////////////////////////////

        scene_node = root->get("scene");
        objects_node = scene_node ? scene_node->get("objects") : nullptr;

        if (!objects_node) {
            std::cerr << "SCF: no objects found in scene\n";
            return;
        }

        // ── spawn every object in file ─────────
        for (auto& obj_node : objects_node->children) {
            Object obj;
            obj.name = obj_node->name;

            auto physics = obj_node->get("physics");
            auto visual = obj_node->get("visual");

            if (physics) {
                if (auto n = physics->get("mass"))
                    obj.mass = n->getFloat();
                if (auto n = physics->get("radius"))
                    obj.radius = n->getFloat();
                if (auto n = physics->get("pos")) {
                    auto v = n->getVec2();
                    obj.x = v.x;
                    obj.y = v.y;
                }
                if (auto n = physics->get("vel")) {
                    auto v = n->getVec2();
                    obj.vx = v.x;
                    obj.vy = v.y;
                }
                if (auto n = physics->get("static"))
                    obj.isStatic = (n->getFloat() > 0.0f);
            }

            if (visual) {
                if (auto n = visual->get("color")) {
                    auto c = n->getVec3();
                    obj.r = c.x;
                    obj.g = c.y;
                    obj.b = c.z;
                }
                if (auto n = visual->get("texture"))
                    obj.texture = n->getString();
            }

            spawnObject(obj);
        }

        // ── flush spawn events ─────────────────
        for (auto& e : iface.pullEvents()) {
            if (e.type == BodyEvent::Type::SPAWN)
                renderScene.addEntity(e.id);
        }
        iface.clearEvents();
        renderScene.rebuildBVH();

        // ── init renderer ──────────────────────
        renderer.init(window);
        std::cout << "Scene loaded: "
            << objects_node->children.size()
            << " objects\n";
    }

    void step() {
        float now = (float)glfwGetTime();
        float dt = now - lastTime;
        lastTime = now;

        // physics
        physicsWorld.update(dt);

        // collisions
        auto& bs = physicsWorld.getBodies();
        for (int i = 0; i < (int)bs.size(); i++)
            for (int j = i + 1; j < (int)bs.size(); j++)
                if (Collider::isColliding(bs[i], bs[j]))
                    collision_resolver::resolve_collision(bs[i], bs[j]);

        // push to interface
        std::vector<BodyState> states;
        for (int i = 0; i < (int)bs.size(); i++) {
            BodyState s;
            s.id = i;
            s.x = bs[i].position.x;
            s.y = bs[i].position.y;
            s.radius = bs[i].radius;
            s.trail = bs[i].trail;
            states.push_back(s);
        }
        iface.push(states);

        // sync scene
        for (auto& s : iface.pull())
            renderScene.updateEntity(s);
        renderScene.syncBVH();
    }

    void render() {
        renderer.drawFrame(renderScene);
    }

    void cleanup() {
        renderer.cleanup();
    }

private:
    GLFWwindow* window = nullptr;
    float            lastTime = 0.0f;

    World            physicsWorld;
    PhysicsInterface iface;
    Scene            renderScene;
    Renderer         renderer;

    void spawnObject(const Object& obj) {
        Body body;
        body.mass = obj.mass;
        body.radius = obj.radius;
        body.position = Vec2(obj.x, obj.y);
        body.velocity = Vec2(obj.vx, obj.vy);
        body.isStatic = obj.isStatic;
        physicsWorld.addBody(body);

        iface.spawn(obj.x, obj.y, obj.radius);
    }
};