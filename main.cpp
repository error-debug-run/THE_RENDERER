#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "src/Renderer.h"

#include <iostream>

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