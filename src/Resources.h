#pragma once

#include "VulkanContext.h"

#include <utility>

// Generic buffer/image helpers shared by meshes, textures, particles and render targets.

std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(VulkanContext &ctx, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

void copyBuffer(VulkanContext &ctx, vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size);

std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(VulkanContext &ctx, uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples, vk::Format format,
                                                               vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);

vk::raii::ImageView createImageView(VulkanContext &ctx, vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels);

// Records a synchronization2 layout transition for a single-mip, single-layer image.
void transitionImageLayout(vk::raii::CommandBuffer &commandBuffer,
                           vk::Image                image,
                           vk::ImageLayout          oldLayout,
                           vk::ImageLayout          newLayout,
                           vk::AccessFlags2         srcAccessMask,
                           vk::AccessFlags2         dstAccessMask,
                           vk::PipelineStageFlags2  srcStageMask,
                           vk::PipelineStageFlags2  dstStageMask,
                           vk::ImageAspectFlags     aspectFlags);
