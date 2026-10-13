#include "Resources.h"

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(VulkanContext &ctx, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
{
	vk::BufferCreateInfo bufferInfo{
	    .size        = size,
	    .usage       = usage,
	    .sharingMode = vk::SharingMode::eExclusive};

	vk::raii::Buffer       buffer          = vk::raii::Buffer(ctx.device, bufferInfo);
	vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
	vk::MemoryAllocateInfo allocInfo{
	    .allocationSize  = memRequirements.size,
	    .memoryTypeIndex = ctx.findMemoryType(memRequirements.memoryTypeBits, properties)};
	vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(ctx.device, allocInfo);
	buffer.bindMemory(*bufferMemory, 0);
	return {std::move(buffer), std::move(bufferMemory)};
}

void copyBuffer(VulkanContext &ctx, vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size)
{
	vk::raii::CommandBuffer commandCopyBuffer = ctx.beginSingleTimeCommands();
	commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy{.size = size});
	ctx.endSingleTimeCommands(std::move(commandCopyBuffer));
}

std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(VulkanContext &ctx, uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples, vk::Format format,
                                                               vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties)
{
	vk::ImageCreateInfo imageInfo{
	    .imageType   = vk::ImageType::e2D,
	    .format      = format,
	    .extent      = {width, height, 1},
	    .mipLevels   = mipLevels,
	    .arrayLayers = 1,
	    .samples     = numSamples,
	    .tiling      = tiling,
	    .usage       = usage,
	    .sharingMode = vk::SharingMode::eExclusive,
	};
	vk::raii::Image image = vk::raii::Image(ctx.device, imageInfo);

	vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
	vk::MemoryAllocateInfo allocInfo{.allocationSize  = memRequirements.size,
	                                 .memoryTypeIndex = ctx.findMemoryType(memRequirements.memoryTypeBits, properties)};
	vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(ctx.device, allocInfo);
	image.bindMemory(imageMemory, 0);

	return {std::move(image), std::move(imageMemory)};
}

vk::raii::ImageView createImageView(VulkanContext &ctx, vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels)
{
	vk::ImageViewCreateInfo viewInfo{
	    .image            = image,
	    .viewType         = vk::ImageViewType::e2D,
	    .format           = format,
	    .subresourceRange = {.aspectMask = aspectFlags, .baseMipLevel = 0, .levelCount = mipLevels, .baseArrayLayer = 0, .layerCount = 1}};

	return vk::raii::ImageView(ctx.device, viewInfo);
}

void transitionImageLayout(vk::raii::CommandBuffer &commandBuffer,
                           vk::Image                image,
                           vk::ImageLayout          oldLayout,
                           vk::ImageLayout          newLayout,
                           vk::AccessFlags2         srcAccessMask,
                           vk::AccessFlags2         dstAccessMask,
                           vk::PipelineStageFlags2  srcStageMask,
                           vk::PipelineStageFlags2  dstStageMask,
                           vk::ImageAspectFlags     aspectFlags)
{
	vk::ImageMemoryBarrier2 barrier = {
	    .srcStageMask        = srcStageMask,
	    .srcAccessMask       = srcAccessMask,
	    .dstStageMask        = dstStageMask,
	    .dstAccessMask       = dstAccessMask,
	    .oldLayout           = oldLayout,
	    .newLayout           = newLayout,
	    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	    .image               = image,
	    .subresourceRange    = {
	           .aspectMask     = aspectFlags,
	           .baseMipLevel   = 0,
	           .levelCount     = 1,
	           .baseArrayLayer = 0,
	           .layerCount     = 1}};

	vk::DependencyInfo dependencyInfo{
	    .imageMemoryBarrierCount = 1,
	    .pImageMemoryBarriers    = &barrier};

	commandBuffer.pipelineBarrier2(dependencyInfo);
}
