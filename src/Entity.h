// Entity.h
#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>


struct Entity {

	uint32_t id = 0;

	glm::vec3 position = glm::vec3{ 0.0f };
	glm::quat rotation = glm::quat{ 1.0f, 0.0f, 0.0f, 0.0f };
	glm::vec3 scale    = glm::vec3{ 1.0f };
    glm::vec3 velocity = glm::vec3(0.0f);
    float     mass = 1.0f;

    // bounding size — BVH uses this
    float     half_extent = 0.5f;

    // renderer reads this to know where to draw
    glm::mat4 modelMatrix() const {
        glm::mat4 m = glm::mat4(1.0f);
        m           = glm::translate(m, position);
        m           = m * glm::mat4_cast(rotation);
        m           = glm::scale(m, scale);
        return m;
    }
};