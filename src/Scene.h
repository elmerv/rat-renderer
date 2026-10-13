#pragma once

#include "Mesh.h"
#include "VulkanContext.h"

glm::mat4 createTransform(glm::vec3 position = glm::vec3(0.0f), glm::vec3 rotation = glm::vec3(0.0f), glm::vec3 scalar = glm::vec3(1.0f));

// The objects being rendered. Meshes get their GPU buffers and textures here;
// the renderer allocates their descriptor sets afterwards.
class Scene
{
  public:
	std::vector<Mesh> meshes;

	void load(VulkanContext &ctx);

	// Binds each mesh's descriptor set, push constants and buffers and draws it.
	// The caller binds the pipeline, so the same call works for any pass that
	// uses the mesh pipeline layout.
	void draw(vk::raii::CommandBuffer &commandBuffer, vk::PipelineLayout layout, uint32_t frameIndex);

  private:
	Mesh createMesh(VulkanContext &ctx, const std::string &meshPath, const std::string &texturePath, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale);
	Mesh createPlane(VulkanContext &ctx, const std::string &texturePath, glm::vec3 position, glm::vec3 scale, glm::vec3 normal, glm::vec3 forward);
};
