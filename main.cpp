#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "src/initializer/Initializer.h"
#include <iostream>
#include <string>

const uint32_t WIDTH = 1280;
const uint32_t HEIGHT = 720;

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "CAP", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        return -1;
    }

    Initializer init(window);
    init.setup(std::string(SCENES_PATH) + "/" + "default.scf");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        init.step();
        init.render();
    }

    init.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}