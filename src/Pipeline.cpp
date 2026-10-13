#include "Pipeline.h"

#include <fstream>
#include <stdexcept>

vk::raii::Pipeline createPipeline(VulkanContext &ctx, PipelineConfig &config)
{
	vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
	    .stage = vk::ShaderStageFlagBits::eVertex, .module = **config.shaderModule, .pName = config.vertEntry.c_str()};
	vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
	    .stage = vk::ShaderStageFlagBits::eFragment, .module = **config.shaderModule, .pName = config.fragEntry.c_str()};
	vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	auto bindingDescription    = config.bindingDesc;
	auto attributeDescriptions = config.attributeDescs;

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
	    .vertexBindingDescriptionCount   = 1,
	    .pVertexBindingDescriptions      = &bindingDescription,
	    .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
	    .pVertexAttributeDescriptions    = attributeDescriptions.data()};

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = config.topology};

	std::vector<vk::DynamicState> dynamicState = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

	vk::PipelineDynamicStateCreateInfo dynamicstate{
	    .dynamicStateCount = static_cast<uint32_t>(dynamicState.size()),
	    .pDynamicStates    = dynamicState.data()};

	vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};

	vk::PipelineRasterizationStateCreateInfo rasterizer{
	    .depthClampEnable        = vk::False,
	    .rasterizerDiscardEnable = vk::False,
	    .polygonMode             = vk::PolygonMode::eFill,
	    .cullMode                = config.cullMode,
	    .frontFace               = vk::FrontFace::eCounterClockwise,
	    .depthBiasEnable         = vk::False,
	    .lineWidth               = 1.0f};

	vk::PipelineMultisampleStateCreateInfo multisampling{.rasterizationSamples = config.samples, .sampleShadingEnable = vk::False};

	vk::PipelineColorBlendAttachmentState colorBlendAttachment;

	if (config.additiveBlending)
	{
		colorBlendAttachment = {
		    .blendEnable         = vk::True,
		    .srcColorBlendFactor = vk::BlendFactor::eOne,
		    .dstColorBlendFactor = vk::BlendFactor::eOne,
		    .colorBlendOp        = vk::BlendOp::eAdd,
		    .srcAlphaBlendFactor = vk::BlendFactor::eOne,
		    .dstAlphaBlendFactor = vk::BlendFactor::eOne,
		    .alphaBlendOp        = vk::BlendOp::eAdd,
		    .colorWriteMask      = vk::ColorComponentFlagBits::eR |
		                      vk::ColorComponentFlagBits::eG |
		                      vk::ColorComponentFlagBits::eB |
		                      vk::ColorComponentFlagBits::eA};
	}
	else
	{
		colorBlendAttachment = {
		    .blendEnable    = vk::False,
		    .colorWriteMask = vk::ColorComponentFlagBits::eR |
		                      vk::ColorComponentFlagBits::eG |
		                      vk::ColorComponentFlagBits::eB |
		                      vk::ColorComponentFlagBits::eA};
	}

	vk::PipelineColorBlendStateCreateInfo colorBlending{
	    .logicOpEnable   = vk::False,
	    .logicOp         = vk::LogicOp::eCopy,
	    .attachmentCount = 1,
	    .pAttachments    = &colorBlendAttachment};

	vk::PipelineDepthStencilStateCreateInfo depthStencil{
	    .depthTestEnable       = config.depthTest,
	    .depthWriteEnable      = config.depthWrite,
	    .depthCompareOp        = vk::CompareOp::eLess,
	    .depthBoundsTestEnable = vk::False,
	    .stencilTestEnable     = vk::False};

	vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
	    {.stageCount          = 2,
	     .pStages             = shaderStages,
	     .pVertexInputState   = &vertexInputInfo,
	     .pInputAssemblyState = &inputAssembly,
	     .pViewportState      = &viewportState,
	     .pRasterizationState = &rasterizer,
	     .pMultisampleState   = &multisampling,
	     .pDepthStencilState  = &depthStencil,
	     .pColorBlendState    = &colorBlending,
	     .pDynamicState       = &dynamicstate,
	     .layout              = *config.layout,
	     .renderPass          = nullptr},
	    {.colorAttachmentCount    = 1,
	     .pColorAttachmentFormats = &config.colorFormat,
	     .depthAttachmentFormat   = config.depthFormat}};

	return vk::raii::Pipeline(ctx.device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

vk::raii::ShaderModule createShaderModule(VulkanContext &ctx, const std::vector<char> &code)
{
	vk::ShaderModuleCreateInfo createInfo{
	    .codeSize = code.size() * sizeof(char),
	    .pCode    = reinterpret_cast<const uint32_t *>(code.data())};

	return vk::raii::ShaderModule{ctx.device, createInfo};
}

std::vector<char> readFile(const std::string &filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		throw std::runtime_error("failed to open file");
	}

	std::vector<char> buffer(file.tellg());
	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
	file.close();

	return buffer;
}
