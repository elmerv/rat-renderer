#include "Renderer.h"

#include "GpuTypes.h"
#include "Pipeline.h"
#include "Resources.h"

#include <cassert>
#include <chrono>
#include <cstring>
#include <stdexcept>

void RatRenderer::run()
{
	initWindow();
	initVulkan();
	mainLoop();
	cleanup();
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void RatRenderer::initWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void RatRenderer::framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
	auto app                = reinterpret_cast<RatRenderer *>(glfwGetWindowUserPointer(window));
	app->framebufferResized = true;
}

void RatRenderer::initVulkan()
{
	ctx.init(window);
	swapchain.create(ctx, window);

	createDescriptorSetLayout();
	createGraphicsPipeline();
	particles.createDescriptorSetLayout(ctx);
	particles.createPipelines(ctx, swapchain.surfaceFormat.format, ctx.findDepthFormat(), ctx.msaaSamples);

	createColorResources();
	createDepthResources();
	createTextureSampler();
	particles.createStorageBuffers(ctx);
	createUniformBuffers();
	createDescriptorPool();

	scene.load(ctx);
	for (auto &mesh : scene.meshes)
	{
		createMeshDescriptorSets(mesh);
	}
	particles.createDescriptorSets(ctx, descriptorPool, uniformBuffers);

	createCommandBuffers();
	createSyncObjects();
}

void RatRenderer::createDescriptorSetLayout()
{
	std::array<vk::DescriptorSetLayoutBinding, 2> bindings{
	    {{.binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment},
	     {.binding = 1, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment}}};

	vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data()};

	descriptorSetLayout = vk::raii::DescriptorSetLayout(ctx.device, layoutInfo);

	vk::PushConstantRange pushConstantRange{
	    .stageFlags = vk::ShaderStageFlagBits::eVertex,
	    .offset     = 0,
	    .size       = sizeof(MeshPushConstants)};

	vk::PipelineLayoutCreateInfo meshLayoutInfo{
	    .setLayoutCount         = 1,
	    .pSetLayouts            = &*descriptorSetLayout,
	    .pushConstantRangeCount = 1,
	    .pPushConstantRanges    = &pushConstantRange};
	pipelineLayout = vk::raii::PipelineLayout(ctx.device, meshLayoutInfo);
}

void RatRenderer::createGraphicsPipeline()
{
	vk::raii::ShaderModule shaderModule = createShaderModule(ctx, readFile("shaders/slang.spv"));

	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	PipelineConfig config = {
	    .vertEntry      = "vertMain",
	    .fragEntry      = "fragMain",
	    .shaderModule   = &shaderModule,
	    .layout         = &pipelineLayout,
	    .colorFormat    = swapchain.surfaceFormat.format,
	    .depthFormat    = ctx.findDepthFormat(),
	    .samples        = ctx.msaaSamples,
	    .bindingDesc    = Vertex::getBindingDescription(),
	    .attributeDescs = {attributeDescriptions.begin(), attributeDescriptions.end()}};

	graphicsPipeline = createPipeline(ctx, config);
}

void RatRenderer::createColorResources()
{
	vk::Format colorFormat                 = swapchain.surfaceFormat.format;
	std::tie(colorImage, colorImageMemory) = createImage(ctx, swapchain.extent.width, swapchain.extent.height, 1, ctx.msaaSamples, colorFormat, vk::ImageTiling::eOptimal,
	                                                     vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
	                                                     vk::MemoryPropertyFlagBits::eDeviceLocal);

	colorImageView = createImageView(ctx, colorImage, colorFormat, vk::ImageAspectFlagBits::eColor, 1);
}

void RatRenderer::createDepthResources()
{
	vk::Format depthFormat                 = ctx.findDepthFormat();
	std::tie(depthImage, depthImageMemory) = createImage(ctx, swapchain.extent.width, swapchain.extent.height, 1, ctx.msaaSamples, depthFormat, vk::ImageTiling::eOptimal,
	                                                     vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
	depthImageView                         = createImageView(ctx, depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
}

void RatRenderer::createTextureSampler()
{
	vk::PhysicalDeviceProperties properties = ctx.physicalDevice.getProperties();
	vk::SamplerCreateInfo        samplerInfo{.magFilter               = vk::Filter::eLinear,
	                                         .minFilter               = vk::Filter::eLinear,
	                                         .mipmapMode              = vk::SamplerMipmapMode::eLinear,
	                                         .addressModeU            = vk::SamplerAddressMode::eRepeat,
	                                         .addressModeV            = vk::SamplerAddressMode::eRepeat,
	                                         .addressModeW            = vk::SamplerAddressMode::eRepeat,
	                                         .mipLodBias              = 0.0f,
	                                         .anisotropyEnable        = vk::True,
	                                         .maxAnisotropy           = properties.limits.maxSamplerAnisotropy,
	                                         .compareEnable           = vk::False,
	                                         .compareOp               = vk::CompareOp::eAlways,
	                                         .minLod                  = 0.0f,
	                                         .maxLod                  = vk::LodClampNone,
	                                         .borderColor             = vk::BorderColor::eIntOpaqueBlack,
	                                         .unnormalizedCoordinates = vk::False};

	textureSampler = vk::raii::Sampler(ctx.device, samplerInfo);
}

void RatRenderer::createUniformBuffers()
{
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
		auto [buffer, bufferMem]  = createBuffer(ctx, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		uniformBuffers.emplace_back(std::move(buffer));
		uniformBuffersMemory.emplace_back(std::move(bufferMem));
		uniformBuffersMapped.emplace_back(uniformBuffersMemory.back().mapMemory(0, bufferSize));
	}
}

void RatRenderer::createDescriptorPool()
{
	const uint32_t maxMeshes = 16;
	const uint32_t maxSets   = MAX_FRAMES_IN_FLIGHT * (maxMeshes + 1);

	std::array<vk::DescriptorPoolSize, 3> poolSize{
	    {{.type            = vk::DescriptorType::eUniformBuffer,
	      .descriptorCount = MAX_FRAMES_IN_FLIGHT * (maxMeshes + 1)},
	     {.type            = vk::DescriptorType::eCombinedImageSampler,
	      .descriptorCount = MAX_FRAMES_IN_FLIGHT * maxMeshes},
	     {.type            = vk::DescriptorType::eStorageBuffer,
	      .descriptorCount = MAX_FRAMES_IN_FLIGHT * 2}}};

	vk::DescriptorPoolCreateInfo poolInfo{
	    .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
	    .maxSets       = maxSets,
	    .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
	    .pPoolSizes    = poolSize.data()};

	descriptorPool = vk::raii::DescriptorPool(ctx.device, poolInfo);
}

void RatRenderer::createMeshDescriptorSets(Mesh &mesh)
{
	std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);

	vk::DescriptorSetAllocateInfo allocInfo{
	    .descriptorPool     = *descriptorPool,
	    .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
	    .pSetLayouts        = layouts.data()};

	mesh.descriptorSets = vk::raii::DescriptorSets(ctx.device, allocInfo);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vk::DescriptorBufferInfo bufferInfo{
		    .buffer = *uniformBuffers[i],
		    .offset = 0,
		    .range  = sizeof(UniformBufferObject)};

		vk::DescriptorImageInfo imageInfo{
		    .sampler     = *textureSampler,
		    .imageView   = *mesh.texture.view,
		    .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

		std::array descriptorWrites{
		    vk::WriteDescriptorSet{
		        .dstSet          = *mesh.descriptorSets[i],
		        .dstBinding      = 0,
		        .descriptorCount = 1,
		        .descriptorType  = vk::DescriptorType::eUniformBuffer,
		        .pBufferInfo     = &bufferInfo},
		    vk::WriteDescriptorSet{
		        .dstSet          = *mesh.descriptorSets[i],
		        .dstBinding      = 1,
		        .descriptorCount = 1,
		        .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
		        .pImageInfo      = &imageInfo}};

		ctx.device.updateDescriptorSets(descriptorWrites, {});
	}
}

void RatRenderer::createCommandBuffers()
{
	vk::CommandBufferAllocateInfo allocInfo{
	    .commandPool        = ctx.commandPool,
	    .level              = vk::CommandBufferLevel::ePrimary,
	    .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
	commandBuffers        = vk::raii::CommandBuffers(ctx.device, allocInfo);
	computeCommandBuffers = vk::raii::CommandBuffers(ctx.device, allocInfo);
}

void RatRenderer::createSyncObjects()
{
	assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());
	assert(computeInFlightFences.empty() && computeFinishedSemaphores.empty());

	for (size_t i = 0; i < swapchain.images.size(); i++)
	{
		presentCompleteSemaphores.emplace_back(ctx.device, vk::SemaphoreCreateInfo());
		renderFinishedSemaphores.emplace_back(ctx.device, vk::SemaphoreCreateInfo());
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		inFlightFences.emplace_back(ctx.device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});

		computeFinishedSemaphores.emplace_back(ctx.device, vk::SemaphoreCreateInfo());
		computeInFlightFences.emplace_back(ctx.device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
	}
}

// ---------------------------------------------------------------------------
// Per frame
// ---------------------------------------------------------------------------

void RatRenderer::mainLoop()
{
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		drawFrame();
	}
	ctx.device.waitIdle();
}

void RatRenderer::drawFrame()
{
	auto computeFenceResult = ctx.device.waitForFences(*computeInFlightFences[frameIndex], vk::True, UINT64_MAX);
	if (computeFenceResult != vk::Result::eSuccess)
	{
		throw std::runtime_error("failed to wait for fence!");
	}

	ctx.device.resetFences(*computeInFlightFences[frameIndex]);
	computeCommandBuffers[frameIndex].reset();
	particles.recordCompute(computeCommandBuffers[frameIndex], frameIndex);

	const vk::SubmitInfo computeSubmitInfo{
	    .waitSemaphoreCount   = 0,
	    .pWaitSemaphores      = nullptr,
	    .pWaitDstStageMask    = nullptr,
	    .commandBufferCount   = 1,
	    .pCommandBuffers      = &*computeCommandBuffers[frameIndex],
	    .signalSemaphoreCount = 1,
	    .pSignalSemaphores    = &*computeFinishedSemaphores[frameIndex]};

	ctx.computeQueue.submit(computeSubmitInfo, *computeInFlightFences[frameIndex]);

	auto fenceResult = ctx.device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
	if (fenceResult != vk::Result::eSuccess)
	{
		throw std::runtime_error("failed to wait for fence!");
	}

	auto [result, imageIndex] = swapchain.swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[semaphoreIndex], nullptr);

	if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized)
	{
		framebufferResized = false;
		recreateSwapChain();
		return;
	}
	else
	{
		assert(result == vk::Result::eSuccess);
	}

	if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
	{
		assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
		throw std::runtime_error("failed to acquire the swap chain image!");
	}

	ctx.device.resetFences(*inFlightFences[frameIndex]);

	recordCommandBuffer(imageIndex, commandBuffers[frameIndex]);

	updateUniformBuffer(frameIndex);

	std::array waitSemaphores{
	    *computeFinishedSemaphores[frameIndex],
	    *presentCompleteSemaphores[semaphoreIndex]};

	std::array<vk::PipelineStageFlags, 2> waitStages{vk::PipelineStageFlagBits::eVertexInput,
	                                                 vk::PipelineStageFlagBits::eColorAttachmentOutput};

	const vk::SubmitInfo submitInfo{
	    .waitSemaphoreCount   = 2,
	    .pWaitSemaphores      = waitSemaphores.data(),
	    .pWaitDstStageMask    = waitStages.data(),
	    .commandBufferCount   = 1,
	    .pCommandBuffers      = &*commandBuffers[frameIndex],
	    .signalSemaphoreCount = 1,
	    .pSignalSemaphores    = &*renderFinishedSemaphores[semaphoreIndex]};

	ctx.graphicsQueue.submit(submitInfo, *inFlightFences[frameIndex]);

	const vk::PresentInfoKHR presentInfoKHR{
	    .waitSemaphoreCount = 1,
	    .pWaitSemaphores    = &*renderFinishedSemaphores[semaphoreIndex],
	    .swapchainCount     = 1,
	    .pSwapchains        = &*swapchain.swapChain,
	    .pImageIndices      = &imageIndex};

	result = ctx.graphicsQueue.presentKHR(presentInfoKHR);

	if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR))
	{
		recreateSwapChain();
	}
	else
	{
		assert(result == vk::Result::eSuccess);
	}

	frameIndex     = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
	semaphoreIndex = (semaphoreIndex + 1) % swapchain.images.size();
}

void RatRenderer::updateUniformBuffer(uint32_t currentImage)
{
	static auto startTime = std::chrono::high_resolution_clock::now();
	static auto lastTime  = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();

	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
	lastTime        = currentTime;

	UniformBufferObject ubo{};
	ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	glm::vec3 cameraPos(2.0f, 2.0f, 2.0f);

	ubo.view = lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

	ubo.cameraPosition = cameraPos;

	ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(swapchain.extent.width) / static_cast<float>(swapchain.extent.height), 0.1f, 10.0f);

	ubo.proj[1][1] *= -1;

	ubo.deltaTime = deltaTime;

	float angle  = time * glm::radians(45.0f);        // speed: 45°/sec → full circle every 8 s
	float radius = 2.5f;                              // distance from the center
	float height = 1.0f;                              // above the room's floor

	ubo.lightPosition = glm::vec3(radius * cos(angle), radius * sin(angle), height);

	memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void RatRenderer::recordCommandBuffer(uint32_t imageIndex, vk::raii::CommandBuffer &commandBuffer)
{
	commandBuffer.begin({});

	// future passes (e.g. shadow map) go before the main pass
	recordMainPass(imageIndex, commandBuffer);

	// transition the swapchain image so it can be presented
	transitionImageLayout(
	    commandBuffer,
	    swapchain.images[imageIndex],
	    vk::ImageLayout::eColorAttachmentOptimal,
	    vk::ImageLayout::ePresentSrcKHR,
	    vk::AccessFlagBits2::eColorAttachmentWrite,
	    {},
	    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
	    vk::PipelineStageFlagBits2::eBottomOfPipe,
	    vk::ImageAspectFlagBits::eColor);

	commandBuffer.end();
}

void RatRenderer::recordMainPass(uint32_t imageIndex, vk::raii::CommandBuffer &commandBuffer)
{
	// Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
	transitionImageLayout(
	    commandBuffer,
	    swapchain.images[imageIndex],
	    vk::ImageLayout::eUndefined,
	    vk::ImageLayout::eColorAttachmentOptimal,
	    {},
	    vk::AccessFlagBits2::eColorAttachmentWrite,
	    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
	    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
	    vk::ImageAspectFlagBits::eColor);

	transitionImageLayout(
	    commandBuffer,
	    *colorImage,
	    vk::ImageLayout::eUndefined,
	    vk::ImageLayout::eColorAttachmentOptimal,
	    vk::AccessFlagBits2::eColorAttachmentWrite,
	    vk::AccessFlagBits2::eColorAttachmentWrite,
	    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
	    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
	    vk::ImageAspectFlagBits::eColor);

	transitionImageLayout(
	    commandBuffer,
	    *depthImage,
	    vk::ImageLayout::eUndefined,
	    vk::ImageLayout::eDepthAttachmentOptimal,
	    {},
	    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
	    vk::PipelineStageFlagBits2::eEarlyFragmentTests |
	        vk::PipelineStageFlagBits2::eLateFragmentTests,
	    vk::PipelineStageFlagBits2::eEarlyFragmentTests |
	        vk::PipelineStageFlagBits2::eLateFragmentTests,
	    vk::ImageAspectFlagBits::eDepth);

	vk::ClearValue              clearColor      = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
	vk::ClearValue              clearDepth      = vk::ClearDepthStencilValue(1.0f, 0);
	vk::RenderingAttachmentInfo colorAttachment = {
	    .imageView          = colorImageView,
	    .imageLayout        = vk::ImageLayout::eColorAttachmentOptimal,
	    .resolveMode        = vk::ResolveModeFlagBits::eAverage,
	    .resolveImageView   = swapchain.imageViews[imageIndex],
	    .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
	    .loadOp             = vk::AttachmentLoadOp::eClear,
	    .storeOp            = vk::AttachmentStoreOp::eDontCare,
	    .clearValue         = clearColor};

	vk::RenderingAttachmentInfo depthAttachmentInfo = {
	    .imageView   = depthImageView,
	    .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
	    .loadOp      = vk::AttachmentLoadOp::eClear,
	    .storeOp     = vk::AttachmentStoreOp::eDontCare,
	    .clearValue  = clearDepth};

	vk::RenderingInfo renderingInfo = {
	    .renderArea           = {.offset = {0, 0}, .extent = swapchain.extent},
	    .layerCount           = 1,
	    .colorAttachmentCount = 1,
	    .pColorAttachments    = &colorAttachment,
	    .pDepthAttachment     = &depthAttachmentInfo};
	commandBuffer.beginRendering(renderingInfo);

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
	commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapchain.extent.width), static_cast<float>(swapchain.extent.height), 0.0f, 1.0f));
	commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchain.extent));

	scene.draw(commandBuffer, *pipelineLayout, frameIndex);

	// particles are disabled for now
	// particles.draw(commandBuffer, frameIndex);

	commandBuffer.endRendering();
}

void RatRenderer::recreateSwapChain()
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	ctx.device.waitIdle();
	swapchain.cleanup();
	swapchain.create(ctx, window);

	semaphoreIndex = 0;
	presentCompleteSemaphores.clear();
	for (size_t i = 0; i < swapchain.images.size(); i++)
	{
		presentCompleteSemaphores.emplace_back(ctx.device, vk::SemaphoreCreateInfo());
	}
}

void RatRenderer::cleanup()
{
	swapchain.cleanup();
	glfwDestroyWindow(window);
	glfwTerminate();
}
