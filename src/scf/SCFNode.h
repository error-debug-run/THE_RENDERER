// src/SCF/SCFNode.h

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <glm/glm.hpp>

struct SCFValue {
	
	std::vector<float>	fixedPass;
	std::string			freePass;

	bool isNumeric() const {
		return !fixedPass.empty();
	}

	int dimension() const {
		return (int)fixedPass.size();
	}

	float asFloat() const {
		if (dimension() == 1) {
			return fixedPass[0];
		}
		throw std::runtime_error("SCFValue is not a single float");
	}

	glm::vec2 asVec2() const {
		if (dimension() == 2) {
			return glm::vec2(fixedPass[0], fixedPass[1]);
		}
		return glm::vec2(fixedPass[0], 0.0f);
		throw std::runtime_error("SCFValue is not a vec2");
	}

	glm::vec3 asVec3() const {
		if (dimension() == 3) {
			return glm::vec3(fixedPass[0], fixedPass[1], fixedPass[2]);
		}if (dimension() == 2) {
			return glm::vec3(fixedPass[0], fixedPass[1], 0.0f);
		} 
		return glm::vec3(fixedPass[0], 0.0f, 0.0f);
		throw std::runtime_error("SCFValue is not a vec3");
	}

	glm::vec2 asBroadcastVec2() const {
		return glm::vec2(fixedPass[0]);
	}

	glm::vec3 asBroadcastVec3() const {
		return glm::vec3(fixedPass[0]) ;
	}

};


struct SCFNode {
	
	std::string name;
	SCFValue value;
	std::vector<std::shared_ptr<SCFNode>> children;
	
	SCFNode* get(const std::string& childName) const {
		for (const auto& child : children) {
			if (child->name == childName) {
				return child.get();
			}
		}
		return nullptr;
	}

	float		getFloat()	const {  return value.asFloat(); }
	glm::vec2	getVec2()	const {  return value.asVec2();  }
	glm::vec3	getVec3()	const {  return value.asVec3();  }
	std::string getString() const {  return value.freePass;  }
};