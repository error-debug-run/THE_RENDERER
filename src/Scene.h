// Scene.h

#pragma once

#include "Camera.h"
#include "Entity.h"
#include "BVH.h"
#include "PhysicsInterface.h"
#include <vector>
#include <algorithm>


struct Scene {

    Camera              camera;
    std::vector<Entity> entities;
    BVH                 bvh;


    void rebuildBVH() {
        std::vector<BVHObject> objects = buildObjectList();
        bvh.build(objects);
    }

    void syncBVH() {
        std::vector<BVHObject> objects = buildObjectList();
        bvh.refit(objects);
    }

    // called when interface sees SPAWN event
    void addEntity(uint32_t id) {
        Entity e;
        e.id = id;
        entities.push_back(e);
    }

    // called when interface sees DESTROY event
    void removeEntity(uint32_t id) {
        entities.erase(
            std::remove_if(entities.begin(), entities.end(),
                [id](const Entity& e) { return e.id == id; }),
            entities.end()
        );
    }

    // called every frame — syncs position/size from physics
    void updateEntity(const BodyState& s) {
        for (auto& e : entities) {
            if (e.id != s.id) continue;
            e.position.x = s.x;
            e.position.y = s.y;
            e.half_extent = s.radius;
            break;
        }
    }

private:
    std::vector<BVHObject> buildObjectList() {
        std::vector<BVHObject> objects;
        objects.reserve(entities.size());

        for (auto& e : entities) {
            BVHObject obj;
            obj.id = e.id;
            obj.centroid = e.position;
            obj.bounds.min = e.position - glm::vec3(e.half_extent);
            obj.bounds.max = e.position + glm::vec3(e.half_extent);
            objects.push_back(obj);
        }

        return objects;
    }
};