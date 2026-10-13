#pragma once

#include "VulkanCommon.h"

#include <optional>

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool isComplete()
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails
{
	vk::SurfaceCapabilitiesKHR        capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR>   presentMode;
};

// Owns the core Vulkan objects that live for the whole program: instance, surface,
// physical/logical device, queues and the command pool. Everything else takes a
// VulkanContext& to reach the device.
class VulkanContext
{
  public:
	vk::raii::Context                context;
	vk::raii::Instance               instance       = nullptr;
	vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
	vk::raii::SurfaceKHR             surface        = nullptr;
	vk::raii::PhysicalDevice         physicalDevice = nullptr;
	vk::raii::Device                 device         = nullptr;
	vk::raii::Queue                  graphicsQueue  = nullptr;
	vk::raii::Queue                  presentQueue   = nullptr;
	vk::raii::Queue                  computeQueue   = nullptr;
	vk::raii::CommandPool            commandPool    = nullptr;

	uint32_t                graphicsQueueFamilyIndex = 0;
	vk::SampleCountFlagBits msaaSamples              = vk::SampleCountFlagBits::e1;

	void init(GLFWwindow *window);

	QueueFamilyIndices      findQueueFamilies(vk::raii::PhysicalDevice &device);
	SwapChainSupportDetails querySwapChainSupport(vk::raii::PhysicalDevice &device);

	uint32_t   findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
	vk::Format findSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);
	vk::Format findDepthFormat();

	// One-off command buffer for uploads; endSingleTimeCommands submits and waits.
	vk::raii::CommandBuffer beginSingleTimeCommands();
	void                    endSingleTimeCommands(vk::raii::CommandBuffer &&commandBuffer);

  private:
	const std::vector<const char *> validationLayers = {
	    "VK_LAYER_KHRONOS_validation"};

	const std::vector<const char *> deviceExtensions = {
	    vk::KHRSwapchainExtensionName};

	void createInstance();
	void setupDebugMessenger();
	void createSurface(GLFWwindow *window);
	void pickPhysicalDevice();
	void createLogicalDevice();
	void createCommandPool();

	bool                     isDeviceSuitable(vk::raii::PhysicalDevice &device);
	bool                     checkDeviceExtensionSupport(vk::raii::PhysicalDevice &device);
	vk::SampleCountFlagBits  getMaxUsableSampleCount();
	std::vector<const char *> getRequiredInstanceExtensions();

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT       severity,
	                                                      vk::DebugUtilsMessageTypeFlagsEXT             type,
	                                                      const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
	                                                      void                                         *pUserData);
};
