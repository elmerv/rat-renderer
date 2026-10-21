#pragma once
#include <cstdint>
#include <glm/glm.hpp>

enum class LightType : uint32_t {
	Directional = 0,
	Point = 1,
	Spot = 2,
};

struct Light {
	glm::vec3 position;
	float innerCone;
	glm::vec3 color;
	float intensity;
	glm::vec3 direction;
	float outerCone;
	LightType type;
	uint32_t pad[3];
};

