#pragma once

#include "VulkanContext.h"

class Swapchain
{
  public:
	vk::raii::SwapchainKHR           swapChain = nullptr;
	std::vector<vk::Image>           images;
	std::vector<vk::raii::ImageView> imageViews;
	vk::SurfaceFormatKHR             surfaceFormat;
	vk::Extent2D                     extent;

	void create(VulkanContext &ctx, GLFWwindow *window);
	void cleanup();

  private:
	void createImageViews(VulkanContext &ctx);

	vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats);
	vk::PresentModeKHR   chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes);
	vk::Extent2D         chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities, GLFWwindow *window);
};
