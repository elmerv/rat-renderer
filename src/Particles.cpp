#include "Particles.h"

#include "GpuTypes.h"
#include "Pipeline.h"
#include "Resources.h"

#include <cmath>
#include <cstring>
#include <ctime>
#include <random>

void ParticleSystem::createDescriptorSetLayout(VulkanContext &ctx)
{
	std::array layoutBindings{
	    vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex, nullptr),
	    vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute, nullptr),
	    vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute, nullptr)};

	vk::DescriptorSetLayoutCreateInfo layoutInfo{
	    .bindingCount = static_cast<uint32_t>(layoutBindings.size()),
	    .pBindings    = layoutBindings.data()};
	descriptorSetLayout = vk::raii::DescriptorSetLayout(ctx.device, layoutInfo);

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
	    .setLayoutCount = 1,
	    .pSetLayouts    = &*descriptorSetLayout};
	pipelineLayout = vk::raii::PipelineLayout(ctx.device, pipelineLayoutInfo);
}

void ParticleSystem::createPipelines(VulkanContext &ctx, vk::Format colorFormat, vk::Format depthFormat, vk::SampleCountFlagBits samples)
{
	vk::raii::ShaderModule shaderModule = createShaderModule(ctx, readFile("shaders/compute.spv"));

	// compute
	vk::PipelineShaderStageCreateInfo computeShaderStageInfo{.stage = vk::ShaderStageFlagBits::eCompute, .module = shaderModule, .pName = "compMain"};

	vk::ComputePipelineCreateInfo computeInfo{
	    .stage  = computeShaderStageInfo,
	    .layout = *pipelineLayout};

	computePipeline = ctx.device.createComputePipeline(nullptr, computeInfo);

	// graphics (points)
	auto attributeDescriptions = Particle::getAttributeDescriptions();

	PipelineConfig config = {
	    .vertEntry      = "particleVertMain",
	    .fragEntry      = "particleFragMain",
	    .shaderModule   = &shaderModule,
	    .layout         = &pipelineLayout,
	    .colorFormat    = colorFormat,
	    .depthFormat    = depthFormat,
	    .samples        = samples,
	    .bindingDesc    = Particle::getBindingDescription(),
	    .attributeDescs = {attributeDescriptions.begin(), attributeDescriptions.end()},
	    .topology       = vk::PrimitiveTopology::ePointList,
	    .depthTest      = true,
	    .depthWrite     = false};

	graphicsPipeline = createPipeline(ctx, config);
}

void ParticleSystem::createStorageBuffers(VulkanContext &ctx)
{
	storageBuffers.clear();
	storageBuffersMemory.clear();

	std::default_random_engine            rndEngine((unsigned) time(nullptr));
	std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

	std::vector<Particle> particles(particleCount);

	for (auto &particle : particles)
	{
		float r     = 0.8 * sqrtf(rndDist(rndEngine));
		float theta = rndDist(rndEngine) * 2.0f * 3.14159265358979323846f;
		float phi   = rndDist(rndEngine) * 3.14159265f;
		float x     = r * sinf(phi) * cosf(theta);
		float y     = r * sinf(phi) * sinf(theta);
		float z     = r * cosf(phi);

		particle.position = glm::vec3(x, y, z);
		particle.velocity = normalize(glm::vec3(x, y, z)) * 0.00025f;
		particle.color    = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
	}

	vk::DeviceSize bufferSize = sizeof(Particle) * particleCount;

	// Created a staging buffer used to upload data to the gpu
	auto [stagingBuffer, stagingMemory] = createBuffer(
	    ctx,
	    bufferSize,
	    vk::BufferUsageFlagBits::eTransferSrc,
	    vk::MemoryPropertyFlagBits::eHostVisible |
	        vk::MemoryPropertyFlagBits::eHostCoherent);

	void *data = stagingMemory.mapMemory(0, bufferSize);
	memcpy(data, particles.data(), bufferSize);
	stagingMemory.unmapMemory();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		auto [buffer, bufferMemory] = createBuffer(ctx, bufferSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
		copyBuffer(ctx, stagingBuffer, buffer, bufferSize);
		storageBuffers.emplace_back(std::move(buffer));
		storageBuffersMemory.emplace_back(std::move(bufferMemory));
	}
}

void ParticleSystem::createDescriptorSets(VulkanContext &ctx, vk::raii::DescriptorPool &pool, std::vector<vk::raii::Buffer> &uniformBuffers)
{
	std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);

	vk::DescriptorSetAllocateInfo allocInfo{
	    .descriptorPool     = pool,
	    .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
	    .pSetLayouts        = layouts.data()};

	descriptorSets = vk::raii::DescriptorSets(ctx.device, allocInfo);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vk::DescriptorBufferInfo bufferInfo{
		    .buffer = uniformBuffers[i],
		    .offset = 0,
		    .range  = sizeof(UniformBufferObject)};

		vk::DescriptorBufferInfo storageBufferInfoLastFrame(storageBuffers[(i - 1 + MAX_FRAMES_IN_FLIGHT) % MAX_FRAMES_IN_FLIGHT], 0, sizeof(Particle) * particleCount);
		vk::DescriptorBufferInfo storageBufferInfoCurrentFrame(storageBuffers[i], 0, sizeof(Particle) * particleCount);

		std::array descriptorWrites{
		    vk::WriteDescriptorSet{
		        .dstSet          = descriptorSets[i],
		        .dstBinding      = 0,
		        .dstArrayElement = 0,
		        .descriptorCount = 1,
		        .descriptorType  = vk::DescriptorType::eUniformBuffer,
		        .pBufferInfo     = &bufferInfo},
		    vk::WriteDescriptorSet{
		        .dstSet          = descriptorSets[i],
		        .dstBinding      = 1,
		        .dstArrayElement = 0,
		        .descriptorCount = 1,
		        .descriptorType  = vk::DescriptorType::eStorageBuffer,
		        .pBufferInfo     = &storageBufferInfoLastFrame},
		    vk::WriteDescriptorSet{
		        .dstSet          = descriptorSets[i],
		        .dstBinding      = 2,
		        .dstArrayElement = 0,
		        .descriptorCount = 1,
		        .descriptorType  = vk::DescriptorType::eStorageBuffer,
		        .pBufferInfo     = &storageBufferInfoCurrentFrame}};

		ctx.device.updateDescriptorSets(descriptorWrites, {});
	}
}

void ParticleSystem::recordCompute(vk::raii::CommandBuffer &commandBuffer, uint32_t frameIndex)
{
	commandBuffer.begin({});
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, *descriptorSets[frameIndex], {});
	commandBuffer.dispatch(particleCount / 256, 1, 1);
	commandBuffer.end();
}

void ParticleSystem::draw(vk::raii::CommandBuffer &commandBuffer, uint32_t frameIndex)
{
	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
	commandBuffer.bindVertexBuffers(0, *storageBuffers[frameIndex], {0});
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[frameIndex], nullptr);
	commandBuffer.draw(particleCount, 1, 0, 0);
}
