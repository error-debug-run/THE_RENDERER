//PhysicsInterface.h

#pragma once

#include "IDRegistry.h"
#include <vector>
#include <atomic>
#include "physics/world.h"
#include "physics/2D_BODY.h"


struct BodyState{

	uint32_t id = 0;

	float x			= 0.0f;
	float y			= 0.0f;
	float angle		= 0.0f;
	float radius	= 0.0f;

	float r = 1.0f;
	float g = 1.0f;
	float b = 1.0f;

	std::vector<Body::TrailPoint> trail;

};

struct BodyEvent {
    enum class Type : uint8_t { SPAWN, DESTROY };
    Type     type;
    uint32_t id;
};

struct PhysicsInterface {

	IDRegistry				registry;
	std::vector<BodyState>	buffer[ 2 ];
	std::vector<BodyEvent>	events;
	std::atomic<int>		writeIndex{ 0 };


	uint32_t spawn(float x, float y, float radius){
	
		uint32_t id = registry.alloc();
		
		BodyState s;
		s.id		= id;
		s.x			= x;
		s.y			= y;
		s.radius	= radius;

		buffer[writeIndex.load()].push_back(s);
		events.push_back({ BodyEvent::Type::SPAWN, id });
        return id;
	}
	
	void destroy(uint32_t id) {
        events.push_back({ BodyEvent::Type::DESTROY, id });
        registry.free(id);
    }

	void push(const std::vector<BodyState>& states) {
        int w = writeIndex.load();
        buffer[w] = states;
        writeIndex.store(w ^ 1);
    }

    const std::vector<BodyState>& pull() const {
        return buffer[writeIndex.load() ^ 1];
    }

    const std::vector<BodyEvent>& pullEvents() const {
        return events;
    }

    void clearEvents() {
        events.clear();
    }
};