//Camera.h

#pragma once 

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera {
	glm::vec3 position	= glm::vec3{ 0.0f, 0.0f, 3.0f };
	glm::vec3 forward   = glm::vec3{ 0.0f, 0.0f, -1.0f };
	glm::vec3 up		= glm::vec3{ 0.0f, 1.0f, 0.0f };
	
	float fov	= 45.0f;
	float near	= 0.1f;
	float far	= 1000.0f;

	glm::mat4 view(float aspect) const {
		return glm::lookAt(position, position + forward, up);
	}

	glm::mat4 projection(float aspect) const {
		return glm::perspective(glm::radians(fov), aspect, near, far);
	}
};