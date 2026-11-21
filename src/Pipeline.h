#pragma once

#include "VulkanContext.h"

struct PipelineConfig
{
	std::string               vertEntry;
	std::string               fragEntry;
	vk::raii::ShaderModule   *shaderModule;
	vk::raii::PipelineLayout *layout;

	// render target formats
	vk::Format              colorFormat;
	vk::Format              depthFormat;
	vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;

	// vertex input
	vk::VertexInputBindingDescription                bindingDesc;
	std::vector<vk::VertexInputAttributeDescription> attributeDescs;

	// topology
	vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;

	// depth
	bool depthTest  = true;
	bool depthWrite = true;

	// blending
	bool additiveBlending = false;

	// culling
	vk::CullModeFlags cullMode = vk::CullModeFlagBits::eNone;

	bool hasFragment = true;
	bool hasColor = true;
	bool depthBias = false;
	float depthBiasConst = 0.0f;
	float depthBiasSlope = 0.0f;


};

vk::raii::Pipeline createPipeline(VulkanContext &ctx, PipelineConfig &config);

[[nodiscard]] vk::raii::ShaderModule createShaderModule(VulkanContext &ctx, const std::vector<char> &code);

std::vector<char> readFile(const std::string &filename);
