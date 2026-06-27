//IDRegistry.h

#pragma once

#include <cstdint>
#include <queue>

class IDRegistry {

	uint32_t next = 0;

	std::queue<uint32_t> freed;
	   
public:
	uint32_t alloc() {
		if (!freed.empty()) {
			uint32_t id = freed.front();
			freed.pop();
			return id;
		}
		return next++;
	}
	void free(uint32_t id) {
		freed.push(id);
	}

};