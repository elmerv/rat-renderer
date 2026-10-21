#pragma once

#include "VulkanCommon.h"
#include "Lights.h"

// Structs whose memory layout must match the shaders.
// UniformBufferObject mirrors the UniformBuffer struct in shaders/common.slang.

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec3 cameraPosition;
	float     deltaTime;
	int lightCount;
	uint32_t pad0[3];
	Light lights[MAX_LIGHTS];
};

struct MeshPushConstants
{
	glm::mat4 modelMatrix;
	glm::mat4 normalMatrix;
};
