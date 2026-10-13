#include "Swapchain.h"

#include "Resources.h"

#include <algorithm>
#include <cassert>
#include <limits>

void Swapchain::create(VulkanContext &ctx, GLFWwindow *window)
{
	SwapChainSupportDetails swapChainSupport = ctx.querySwapChainSupport(ctx.physicalDevice);

	// what color format should pixels use?
	vk::SurfaceFormatKHR chosenFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	// when frames should be swapped?
	vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentMode);
	// how many pxels should the swapchain images use?
	vk::Extent2D chosenExtent = chooseSwapExtent(swapChainSupport.capabilities, window);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	vk::SwapchainCreateInfoKHR createInfo{
	    .surface          = *ctx.surface,
	    .minImageCount    = imageCount,
	    .imageFormat      = chosenFormat.format,
	    .imageColorSpace  = chosenFormat.colorSpace,
	    .imageExtent      = chosenExtent,
	    .imageArrayLayers = 1,
	    .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment};

	QueueFamilyIndices indices              = ctx.findQueueFamilies(ctx.physicalDevice);
	uint32_t           queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

	if (indices.graphicsFamily != indices.presentFamily)
	{
		createInfo.imageSharingMode      = vk::SharingMode::eConcurrent;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices   = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode      = vk::SharingMode::eExclusive;
		createInfo.queueFamilyIndexCount = 0;              // optional
		createInfo.pQueueFamilyIndices   = nullptr;        // optional
	}

	createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	createInfo.presentMode    = presentMode;
	createInfo.clipped        = vk::True;
	createInfo.oldSwapchain   = nullptr;

	swapChain = vk::raii::SwapchainKHR(ctx.device, createInfo);
	images    = swapChain.getImages();

	surfaceFormat = chosenFormat;
	extent        = chosenExtent;

	createImageViews(ctx);
}

void Swapchain::cleanup()
{
	imageViews.clear();
	swapChain = nullptr;
}

void Swapchain::createImageViews(VulkanContext &ctx)
{
	assert(imageViews.empty());
	imageViews.reserve(images.size());
	for (auto &image : images)
	{
		imageViews.emplace_back(createImageView(ctx, image, surfaceFormat.format, vk::ImageAspectFlagBits::eColor, 1));
	}
}

vk::SurfaceFormatKHR Swapchain::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
{
	for (const auto &availableFormat : availableFormats)
	{
		if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
		{
			return availableFormat;
		}
	}
	return availableFormats[0];
}

vk::PresentModeKHR Swapchain::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes)
{
	for (const auto &availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == vk::PresentModeKHR::eMailbox)
		{
			return availablePresentMode;
		}
	}

	return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Swapchain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities, GLFWwindow *window)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	vk::Extent2D actualExtent = {
	    static_cast<uint32_t>(width),
	    static_cast<uint32_t>(height)};

	actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	return actualExtent;
}
