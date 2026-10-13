#include "Scene.h"

#include "GpuTypes.h"

glm::mat4 createTransform(glm::vec3 position, glm::vec3 rotation, glm::vec3 scalar)
{
	glm::mat4 translate = glm::translate(glm::mat4(1.0f), position);
	glm::mat4 rotateX   = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.x), glm::vec3(1.0, 0.0, 0.0));
	glm::mat4 rotateY   = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.y), glm::vec3(0.0, 1.0, 0.0));
	glm::mat4 rotateZ   = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.z), glm::vec3(0.0, 0.0, 1.0));
	glm::mat4 scale     = glm::scale(glm::mat4(1.0f), scalar);

	return translate * rotateZ * rotateY * rotateX * scale;
}

void Scene::load(VulkanContext &ctx)
{
	std::string viking_room_model_path   = "models/viking_room.obj";
	std::string viking_room_texture_path = "textures/viking_room.png";
	glm::vec3   viking_position          = {0.0, 0.0, 0.0};
	glm::vec3   viking_scale             = {1.0, 1.0, 1.0};
	glm::vec3   viking_rotation          = {0.0, 0.0, 0.0};
	meshes.push_back(createMesh(ctx, viking_room_model_path, viking_room_texture_path, viking_position, viking_rotation, viking_scale));

	glm::vec3   plane_position     = {0.0, 0.0, -3.0};
	glm::vec3   plane_scale        = {10.0, 10.0, 1.0};
	glm::vec3   plane_forward      = {0.0, 0.0, 0.0};
	glm::vec3   plane_normal       = {0.0, 0.0, 1.0};
	std::string plane_texture_path = "textures/cat.png";
	meshes.push_back(createPlane(ctx, plane_texture_path, plane_position, plane_scale, plane_normal, plane_forward));
}

Mesh Scene::createMesh(VulkanContext &ctx, const std::string &meshPath, const std::string &texturePath, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale)
{
	Mesh mesh      = loadModel(meshPath);
	mesh.transform = createTransform(position, rotation, scale);
	createVertexBuffer(ctx, mesh);
	createIndexBuffer(ctx, mesh);
	mesh.texture = loadTexture(ctx, texturePath);
	return mesh;
}

Mesh Scene::createPlane(VulkanContext &ctx, const std::string &texturePath, glm::vec3 position, glm::vec3 scale, glm::vec3 normal, glm::vec3 forward)
{
	Mesh mesh      = makePlaneGeometry();
	mesh.transform = createTransform(position, glm::vec3(0.0f), scale);

	createVertexBuffer(ctx, mesh);
	createIndexBuffer(ctx, mesh);
	mesh.texture = loadTexture(ctx, texturePath.empty() ? "textures/white.jpg" : texturePath);
	return mesh;
}

void Scene::draw(vk::raii::CommandBuffer &commandBuffer, vk::PipelineLayout layout, uint32_t frameIndex)
{
	for (auto &mesh : meshes)
	{
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, *mesh.descriptorSets[frameIndex], nullptr);

		MeshPushConstants pc{};
		pc.modelMatrix  = mesh.transform;
		pc.normalMatrix = glm::transpose(glm::inverse(glm::mat3(mesh.transform)));

		commandBuffer.pushConstants(layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(pc), &pc);
		commandBuffer.bindVertexBuffers(0, *mesh.vertexBuffer, {0});
		commandBuffer.bindIndexBuffer(*mesh.indexBuffer, 0, vk::IndexType::eUint32);
		commandBuffer.drawIndexed(mesh.index_count, 1, 0, 0, 0);
	}
}
