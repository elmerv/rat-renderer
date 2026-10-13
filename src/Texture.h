#pragma once

#include "VulkanContext.h"

struct Texture
{
	vk::raii::Image        image     = nullptr;
	vk::raii::DeviceMemory memory    = nullptr;
	vk::raii::ImageView    view      = nullptr;
	uint32_t               mipLevels = 1;
};

// Loads an image file into a mipmapped, sampled RGBA8 sRGB texture.
Texture loadTexture(VulkanContext &ctx, const std::string &path);
