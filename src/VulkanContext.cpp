#include "VulkanContext.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>

void VulkanContext::init(GLFWwindow *window)
{
	createInstance();
	setupDebugMessenger();
	createSurface(window);
	pickPhysicalDevice();
	createLogicalDevice();
	createCommandPool();
}

void VulkanContext::createInstance()
{
	constexpr vk::ApplicationInfo appInfo{
	    .pApplicationName   = "Rat Renderer",
	    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
	    .pEngineName        = "Rat Engine",
	    .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
	    .apiVersion         = vk::ApiVersion14};

	// Get the required layers
	std::vector<char const *> requiredLayers;
	if (enableValidationLayers)
	{
		requiredLayers.assign(validationLayers.begin(), validationLayers.end());
	}

	// Check if the required layers are supported by the Vulkan implementation
	auto layerProperties = context.enumerateInstanceLayerProperties();

	auto unsupportedLayerIt = std::ranges::find_if(requiredLayers, [&layerProperties](auto const &requiredLayer) {
		return std::ranges::none_of(layerProperties, [requiredLayer](auto const &layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
	});

	if (unsupportedLayerIt != requiredLayers.end())
	{
		throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
	}

	// Get the required extensions
	auto requiredExtensions = getRequiredInstanceExtensions();

	// Check if the required extensions are supported by the Vulkan implementation.
	auto extensionProperties = context.enumerateInstanceExtensionProperties();

	auto unsupportedPropertyIt = std::ranges::find_if(requiredExtensions, [&extensionProperties](auto const &requiredExtension) {
		return std::ranges::none_of(extensionProperties, [requiredExtension](auto const &extensionProperty) { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
	});

	if (unsupportedPropertyIt != requiredExtensions.end())
	{
		throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
	}

	vk::InstanceCreateInfo createInfo{.pApplicationInfo        = &appInfo,
	                                  .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
	                                  .ppEnabledLayerNames     = requiredLayers.data(),
	                                  .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
	                                  .ppEnabledExtensionNames = requiredExtensions.data()};
	instance = vk::raii::Instance(context, createInfo);
}

std::vector<const char *> VulkanContext::getRequiredInstanceExtensions()
{
	uint32_t glfwExtensionCount = 0;
	auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers)
	{
		extensions.push_back(vk::EXTDebugUtilsExtensionName);
	}

	return extensions;
}

void VulkanContext::setupDebugMessenger()
{
	if (!enableValidationLayers)
		return;

	vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
	vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
	vk::DebugUtilsMessengerCreateInfoEXT  debugUtilsMessengerCreateInfoEXT{.messageSeverity = severityFlags,
	                                                                       .messageType     = messageTypeFlags,
	                                                                       .pfnUserCallback = &debugCallback};
	debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL VulkanContext::debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT       severity,
                                                              vk::DebugUtilsMessageTypeFlagsEXT             type,
                                                              const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                              void                                         *pUserData)
{
	std::cerr << "Validation layer: type " << to_string(type) << "msg: " << pCallbackData->pMessage << std::endl;
	return vk::False;
}

void VulkanContext::createSurface(GLFWwindow *window)
{
	VkSurfaceKHR rawSurface;
	if (glfwCreateWindowSurface(*instance, window, nullptr, &rawSurface) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a window surface!");
	}
	surface = vk::raii::SurfaceKHR(instance, rawSurface);
}

void VulkanContext::pickPhysicalDevice()
{
	auto devices = instance.enumeratePhysicalDevices();

	if (devices.empty())
	{
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}

	for (auto &device : devices)
	{
		if (isDeviceSuitable(device))
		{
			physicalDevice = std::move(device);
			break;
		}
	}

	if (*physicalDevice == VK_NULL_HANDLE)
	{
		throw std::runtime_error("failed to find a suitable GPU!");
	}
	msaaSamples = getMaxUsableSampleCount();
}

bool VulkanContext::isDeviceSuitable(vk::raii::PhysicalDevice &device)
{
	QueueFamilyIndices indices             = findQueueFamilies(device);
	bool               extensionsSupported = checkDeviceExtensionSupport(device);
	bool               swapChainAdequate   = false;
	if (extensionsSupported)
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
		swapChainAdequate                        = !swapChainSupport.formats.empty() && !swapChainSupport.presentMode.empty();
	}

	auto features                 = device.template getFeatures2<vk::PhysicalDeviceFeatures2,
	                                                             vk::PhysicalDeviceVulkan13Features,
	                                                             vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
	bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
	                                features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
	                                features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
	                                features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

	// check Vulkan 1.3 support:
	auto props            = device.getProperties();
	bool supportsVulkan13 = VK_API_VERSION_MAJOR(props.apiVersion) >= 1 &&
	                        VK_API_VERSION_MINOR(props.apiVersion) >= 3;

	return indices.isComplete() &&
	       extensionsSupported &&
	       swapChainAdequate &&
	       supportsRequiredFeatures &&
	       supportsVulkan13;
}

bool VulkanContext::checkDeviceExtensionSupport(vk::raii::PhysicalDevice &device)
{
	auto availableExtensions = device.enumerateDeviceExtensionProperties();

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
	for (const auto &extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

vk::SampleCountFlagBits VulkanContext::getMaxUsableSampleCount()
{
	vk::PhysicalDeviceProperties physicalDeviceProperties = physicalDevice.getProperties();
	vk::SampleCountFlags         counts                   = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
	if (counts & vk::SampleCountFlagBits::e64)
	{
		return vk::SampleCountFlagBits::e64;
	}
	if (counts & vk::SampleCountFlagBits::e32)
	{
		return vk::SampleCountFlagBits::e32;
	}
	if (counts & vk::SampleCountFlagBits::e16)
	{
		return vk::SampleCountFlagBits::e16;
	}
	if (counts & vk::SampleCountFlagBits::e8)
	{
		return vk::SampleCountFlagBits::e8;
	}
	if (counts & vk::SampleCountFlagBits::e4)
	{
		return vk::SampleCountFlagBits::e4;
	}
	if (counts & vk::SampleCountFlagBits::e2)
	{
		return vk::SampleCountFlagBits::e2;
	}

	return vk::SampleCountFlagBits::e1;
}

void VulkanContext::createLogicalDevice()
{
	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
	graphicsQueueFamilyIndex   = indices.graphicsFamily.value();

	float queuePriority = 1.0f;

	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t>                     uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		vk::DeviceQueueCreateInfo queueCreateInfo{
		    .queueFamilyIndex = queueFamily,
		    .queueCount       = 1,
		    .pQueuePriorities = &queuePriority};
		queueCreateInfos.push_back(queueCreateInfo);
	}

	vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
	    {.features = {.samplerAnisotropy = true}},                   // vk::PhysicalDeviceFeatures2
	    {.synchronization2 = true, .dynamicRendering = true},        // vk::PhysicalDeviceVulkan13Features
	    {.extendedDynamicState = true}                               // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
	};

	vk::DeviceCreateInfo createInfo{
	    .pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
	    .queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size()),
	    .pQueueCreateInfos       = queueCreateInfos.data(),
	    .enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size()),
	    .ppEnabledExtensionNames = deviceExtensions.data(),
	};

	device        = vk::raii::Device(physicalDevice, createInfo);
	graphicsQueue = device.getQueue(indices.graphicsFamily.value(), 0);
	presentQueue  = device.getQueue(indices.presentFamily.value(), 0);
	computeQueue  = device.getQueue(indices.graphicsFamily.value(), 0);
}

void VulkanContext::createCommandPool()
{
	vk::CommandPoolCreateInfo poolInfo{
	    .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
	    .queueFamilyIndex = graphicsQueueFamilyIndex};
	commandPool = vk::raii::CommandPool(device, poolInfo);
}

QueueFamilyIndices VulkanContext::findQueueFamilies(vk::raii::PhysicalDevice &device)
{
	QueueFamilyIndices indices;

	auto queueFamilies = device.getQueueFamilyProperties();

	int i = 0;

	for (const auto &queueFamily : queueFamilies)
	{
		if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
		{
			indices.graphicsFamily = i;
		}

		if (device.getSurfaceSupportKHR(i, *surface))
		{
			indices.presentFamily = i;
		}

		if (indices.isComplete())
		{
			break;
		}

		i++;
	}

	return indices;
}

SwapChainSupportDetails VulkanContext::querySwapChainSupport(vk::raii::PhysicalDevice &device)
{
	SwapChainSupportDetails details;

	details.capabilities = device.getSurfaceCapabilitiesKHR(*surface);
	details.formats      = device.getSurfaceFormatsKHR(*surface);
	details.presentMode  = device.getSurfacePresentModesKHR(*surface);

	return details;
}

uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
	vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}
	throw std::runtime_error("failed to find suitable memory type!");
}

vk::Format VulkanContext::findSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
{
	for (const auto format : candidates)
	{
		vk::FormatProperties props = physicalDevice.getFormatProperties(format);

		if (
		    ((tiling == vk::ImageTiling::eLinear) && ((props.linearTilingFeatures & features) == features)) ||
		    (((tiling == vk::ImageTiling::eOptimal)) && ((props.optimalTilingFeatures & features) == features)))
		{
			return format;
		}
	}

	throw std::runtime_error("failed to support format!");
}

vk::Format VulkanContext::findDepthFormat()
{
	return findSupportedFormat({vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
	                           vk::ImageTiling::eOptimal,
	                           vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

vk::raii::CommandBuffer VulkanContext::beginSingleTimeCommands()
{
	vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};

	vk::raii::CommandBuffer    commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());
	vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
	commandBuffer.begin(beginInfo);
	return commandBuffer;
}

void VulkanContext::endSingleTimeCommands(vk::raii::CommandBuffer &&commandBuffer)
{
	commandBuffer.end();

	vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffer};

	graphicsQueue.submit(submitInfo, nullptr);
	graphicsQueue.waitIdle();
}
