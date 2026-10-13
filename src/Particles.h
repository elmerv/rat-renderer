#pragma once

#include "VulkanContext.h"

#include <array>

struct Particle
{
	glm::vec3 position;
	float     _pad0 = 0;
	glm::vec3 velocity;
	float     _pad1 = 0;
	glm::vec4 color;

	static vk::VertexInputBindingDescription getBindingDescription()
	{
		return {
		    .binding   = 0,
		    .stride    = sizeof(Particle),
		    .inputRate = vk::VertexInputRate::eVertex};
	}

	static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
	{
		return {
		    {{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Particle, position)},
		     {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32A32Sfloat, .offset = offsetof(Particle, color)}}};
	}
};

// GPU particle simulation: a compute pass that ping-pongs between two storage
// buffers, plus a point-list pipeline that draws the result.
class ParticleSystem
{
  public:
	uint32_t particleCount = 100000;

	void createDescriptorSetLayout(VulkanContext &ctx);
	void createPipelines(VulkanContext &ctx, vk::Format colorFormat, vk::Format depthFormat, vk::SampleCountFlagBits samples);
	void createStorageBuffers(VulkanContext &ctx);
	void createDescriptorSets(VulkanContext &ctx, vk::raii::DescriptorPool &pool, std::vector<vk::raii::Buffer> &uniformBuffers);

	void recordCompute(vk::raii::CommandBuffer &commandBuffer, uint32_t frameIndex);
	// Records the particle draw into an active rendering pass.
	void draw(vk::raii::CommandBuffer &commandBuffer, uint32_t frameIndex);

  private:
	vk::raii::DescriptorSetLayout        descriptorSetLayout = nullptr;
	vk::raii::PipelineLayout             pipelineLayout      = nullptr;
	vk::raii::Pipeline                   computePipeline     = nullptr;
	vk::raii::Pipeline                   graphicsPipeline    = nullptr;
	std::vector<vk::raii::DescriptorSet> descriptorSets;

	std::vector<vk::raii::Buffer>       storageBuffers;
	std::vector<vk::raii::DeviceMemory> storageBuffersMemory;
};
