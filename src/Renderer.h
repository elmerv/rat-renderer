#pragma once

#include "Particles.h"
#include "Scene.h"
#include "Swapchain.h"
#include "VulkanContext.h"

// Owns the window and frame loop: per-frame uniforms, descriptors, render targets,
// synchronization and command recording. Members are destroyed in reverse order,
// so ctx (declared first) outlives everything that uses the device.
class RatRenderer
{
  public:
	void run();

  private:
	GLFWwindow   *window = nullptr;
	VulkanContext ctx;
	Swapchain     swapchain;

	// mesh pipeline
	vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
	vk::raii::PipelineLayout      pipelineLayout      = nullptr;
	vk::raii::Pipeline            graphicsPipeline    = nullptr;
	vk::raii::Sampler             textureSampler      = nullptr;

	// render targets (MSAA color resolves into the swapchain image)
	vk::raii::Image        colorImage       = nullptr;
	vk::raii::DeviceMemory colorImageMemory = nullptr;
	vk::raii::ImageView    colorImageView   = nullptr;
	vk::raii::Image        depthImage       = nullptr;
	vk::raii::DeviceMemory depthImageMemory = nullptr;
	vk::raii::ImageView    depthImageView   = nullptr;

	// per-frame uniforms
	std::vector<vk::raii::Buffer>       uniformBuffers;
	std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
	std::vector<void *>                 uniformBuffersMapped;

	// declared before scene/particles so their descriptor sets are freed first
	vk::raii::DescriptorPool descriptorPool = nullptr;

	Scene          scene;
	ParticleSystem particles;

	// commands and sync
	std::vector<vk::raii::CommandBuffer> commandBuffers;
	std::vector<vk::raii::CommandBuffer> computeCommandBuffers;
	std::vector<vk::raii::Semaphore>     presentCompleteSemaphores;
	std::vector<vk::raii::Semaphore>     renderFinishedSemaphores;
	std::vector<vk::raii::Fence>         inFlightFences;
	std::vector<vk::raii::Semaphore>     computeFinishedSemaphores;
	std::vector<vk::raii::Fence>         computeInFlightFences;

	uint32_t frameIndex         = 0;
	uint32_t semaphoreIndex     = 0;
	bool     framebufferResized = false;

	// setup
	void        initWindow();
	static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
	void        initVulkan();
	void        createDescriptorSetLayout();
	void        createGraphicsPipeline();
	void        createColorResources();
	void        createDepthResources();
	void        createTextureSampler();
	void        createUniformBuffers();
	void        createDescriptorPool();
	void        createMeshDescriptorSets(Mesh &mesh);
	void        createCommandBuffers();
	void        createSyncObjects();

	// per frame
	void mainLoop();
	void drawFrame();
	void updateUniformBuffer(uint32_t currentImage);
	void recordCommandBuffer(uint32_t imageIndex, vk::raii::CommandBuffer &commandBuffer);
	void recordMainPass(uint32_t imageIndex, vk::raii::CommandBuffer &commandBuffer);
	void recreateSwapChain();

	void cleanup();
};
