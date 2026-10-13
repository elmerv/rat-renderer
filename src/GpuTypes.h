#pragma once

#include "VulkanCommon.h"

// Structs whose memory layout must match the shaders.
// UniformBufferObject mirrors the UniformBuffer struct in shaders/common.slang.

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec3 lightPosition;
	float     deltaTime;
	glm::vec3 cameraPosition;
};

struct MeshPushConstants
{
	glm::mat4 modelMatrix;
	glm::mat4 normalMatrix;
};
