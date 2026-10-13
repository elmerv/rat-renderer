#pragma once

#include "Texture.h"
#include "VulkanContext.h"

#include <array>
#include <glm/gtx/hash.hpp>

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;
	glm::vec3 normal;

	static vk::VertexInputBindingDescription getBindingDescription()
	{
		return {
		    .binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
	}

	static std::array<vk::VertexInputAttributeDescription, 4> getAttributeDescriptions()
	{
		return {
		    {{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, pos)},
		     {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color)},
		     {.location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, texCoord)},
		     {.location = 3, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, normal)}}};
	}

	bool operator==(const Vertex &other) const
	{
		return pos == other.pos &&
		       color == other.color &&
		       texCoord == other.texCoord;
	}
};

namespace std
{
template <>
struct hash<Vertex>
{
	size_t operator()(Vertex const &vertex) const
	{
		return ((hash<glm::vec3>()(vertex.pos) ^
		         (hash<glm::vec3>()(vertex.color) << 1)) >>
		        1) ^
		       (hash<glm::vec2>()(vertex.texCoord) << 1);
	}
};
}        // namespace std

struct Mesh
{
	std::vector<Vertex>   vertices;
	std::vector<uint32_t> indices;

	vk::raii::Buffer       vertexBuffer       = nullptr;
	vk::raii::DeviceMemory vertexBufferMemory = nullptr;
	vk::raii::Buffer       indexBuffer        = nullptr;
	vk::raii::DeviceMemory indexBufferMemory  = nullptr;

	uint32_t index_count = 0;

	Texture texture;

	// one descriptor set per frame in flight (UBO + this mesh's texture)
	vk::raii::DescriptorSets descriptorSets = nullptr;

	glm::mat4 transform = glm::mat4(1.0f);
};

// CPU-side geometry
Mesh loadModel(const std::string &modelPath);
Mesh makePlaneGeometry();

// GPU upload of vertices/indices into device-local buffers
void createVertexBuffer(VulkanContext &ctx, Mesh &mesh);
void createIndexBuffer(VulkanContext &ctx, Mesh &mesh);
