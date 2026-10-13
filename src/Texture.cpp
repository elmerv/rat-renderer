#include "Texture.h"

#include "Resources.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace
{
void copyBufferToImage(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Buffer &buffer, vk::raii::Image &image, uint32_t width, uint32_t height)
{
	vk::BufferImageCopy region{
	    .bufferOffset      = 0,
	    .bufferRowLength   = 0,
	    .bufferImageHeight = 0,
	    .imageSubresource  = {
	         .aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
	    .imageOffset = {0, 0, 0},
	    .imageExtent = {width, height, 1}};

	commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
}

// Older (non-synchronization2) transition used only during texture upload.
void transitionTextureLayout(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels)
{
	vk::ImageMemoryBarrier barrier{.oldLayout = oldLayout, .newLayout = newLayout, .srcQueueFamilyIndex = vk::QueueFamilyIgnored, .dstQueueFamilyIndex = vk::QueueFamilyIgnored, .image = image, .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = mipLevels, .layerCount = 1}};

	vk::PipelineStageFlags sourceStage;
	vk::PipelineStageFlags destinationStage;

	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
	{
		barrier.srcAccessMask = {};
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

		sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
		destinationStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		sourceStage      = vk::PipelineStageFlagBits::eTransfer;
		destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else
	{
		throw std::invalid_argument("unsupported layout transition!");
	}

	commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, barrier);
}

void generateMipMaps(VulkanContext &ctx, vk::raii::CommandBuffer &commandBuffer, vk::raii::Image &image, vk::Format imageFormat, uint32_t width, uint32_t height, uint32_t mipLevels)
{
	vk::FormatProperties formatProperties = ctx.physicalDevice.getFormatProperties(imageFormat);

	if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
	{
		throw std::runtime_error("texture image format does not support linear blitting!");
	}

	vk::ImageMemoryBarrier barrier = {
	    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
	    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
	    .image               = image,
	    .subresourceRange    = {
	           .aspectMask     = vk::ImageAspectFlagBits::eColor,
	           .levelCount     = 1,
	           .baseArrayLayer = 0,
	           .layerCount     = 1}};

	uint32_t mipWidth  = width;
	uint32_t mipHeight = height;

	for (uint32_t i = 1; i < mipLevels; i++)
	{
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout                     = vk::ImageLayout::eUndefined;
		barrier.newLayout                     = vk::ImageLayout::eTransferSrcOptimal;
		barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask                 = vk::AccessFlagBits::eTransferRead;

		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

		vk::ImageBlit blit = {
		    .srcSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = i - 1, .layerCount = 1},
		    .srcOffsets     = std::array<vk::Offset3D, 2>({{}, {static_cast<int32_t>(mipWidth), static_cast<int32_t>(mipHeight), 1}}),
		    .dstSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = i, .layerCount = 1},
		    .dstOffsets     = std::array<vk::Offset3D, 2>({{}, {static_cast<int32_t>(1 < mipWidth ? mipWidth / 2 : 1), static_cast<int32_t>(1 < mipHeight ? mipHeight / 2 : 1), 1}})};
		commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);

		if (1 < mipWidth)
		{
			mipWidth /= 2;
		}

		if (1 < mipHeight)
		{
			mipHeight /= 2;
		}
	}

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount   = mipLevels;
	barrier.oldLayout                     = vk::ImageLayout::eUndefined;
	barrier.newLayout                     = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask                 = vk::AccessFlagBits::eShaderRead;

	commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);
}
}        // namespace

Texture loadTexture(VulkanContext &ctx, const std::string &path)
{
	Texture texture;

	int            texWidth, texHeight, texChannels;
	stbi_uc       *pixels    = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	vk::DeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels)
	{
		throw std::runtime_error(
		    std::string("failed to load texture: ") +
		    stbi_failure_reason());
	}

	uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texHeight, texWidth)))) + 1;

	auto [stagingBuffer, stagingBufferMemory] = createBuffer(ctx, imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	void *data = stagingBufferMemory.mapMemory(0, imageSize);
	memcpy(data, pixels, imageSize);
	stagingBufferMemory.unmapMemory();

	stbi_image_free(pixels);

	std::tie(texture.image, texture.memory) = createImage(ctx, texWidth, texHeight, mipLevels, vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
	                                                      vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
	                                                      vk::MemoryPropertyFlagBits::eDeviceLocal);

	vk::raii::CommandBuffer commandBuffer = ctx.beginSingleTimeCommands();
	transitionTextureLayout(commandBuffer, texture.image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels);
	copyBufferToImage(commandBuffer, stagingBuffer, texture.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
	generateMipMaps(ctx, commandBuffer, texture.image, vk::Format::eR8G8B8A8Srgb, texWidth, texHeight, mipLevels);
	ctx.endSingleTimeCommands(std::move(commandBuffer));

	texture.mipLevels = mipLevels;
	texture.view      = createImageView(ctx, *texture.image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, mipLevels);

	return texture;
}
