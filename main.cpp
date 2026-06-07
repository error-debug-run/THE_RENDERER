#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "src/Renderer.h"
#include "src/BVH.h"
#include <iostream>
#include <chrono>

const uint32_t WIDTH = 1280;
const uint32_t HEIGHT = 720;

int main() {

    // ─── GLFW Init ─────────────────────────────
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window =
        glfwCreateWindow(
            WIDTH,
            HEIGHT,
            "THE_RENDERER",
            nullptr,
            nullptr
        );

    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        return -1;
    }

    // ─── Renderer Init ─────────────────────────
    Renderer renderer;
    renderer.init(window);

    std::cout << "Init done" << std::endl;
    std::cout.flush();

    try {
        // ─── BVH Test ─────────────────────────────────────────────────────────────────
        BVH bvh;
        std::vector<BVHObject> objects;

        // Spawn 5 test objects with simple AABBs
        for (int i = 0; i < 5; i++) {
            BVHObject obj;
            glm::vec3 center = glm::vec3(i * 1.0f, 0.0f, 0.0f);
            obj.bounds.min = center - glm::vec3(0.3f);
            obj.bounds.max = center + glm::vec3(0.3f);
            obj.centroid = center;
            obj.id = i;
            objects.push_back(obj);
        }

        bvh.build(objects);
        std::cout << "BVH built: " << bvh.nodes().size() << " nodes" << std::flush << std::endl;

        // Test raycast — fire a ray along X axis
        auto hit = bvh.raycast(
            glm::vec3(-1.0f, 0.0f, 0.0f),  // origin
            glm::vec3(1.0f, 0.0f, 0.0f)   // direction
        );

        if (hit.hit)
            std::cout << "Ray hit object: " << hit.objectID << " at t=" << hit.t << std::endl;
        else
            std::cout << "Ray missed" << std::endl;

        // Test sphere query
        std::vector<int> nearby;
        bvh.querySphere(glm::vec3(2.0f, 0.0f, 0.0f), 1.5f, nearby);
        std::cout << "Sphere query found " << nearby.size() << " objects near (2,0,0)" << std::endl;
        for (int id : nearby) std::cout << "  -> object " << id << std::endl;

        if (hit.hit)
            std::cout << "Ray hit object: " << hit.objectID << " at t=" << hit.t << std::endl;
        else
            std::cout << "Ray missed" << std::endl;

        std::cout << "Sphere query found " << nearby.size() << " objects" << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "BVH ERROR: " << e.what() << std::endl;
    }
    
    // ─── Main Loop ─────────────────────────────
    while (!glfwWindowShouldClose(window)) {

        glfwPollEvents();

        float time =
            static_cast<float>(glfwGetTime());

        renderer.drawFrame(time);
    }

    // ─── Cleanup ───────────────────────────────
    renderer.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}