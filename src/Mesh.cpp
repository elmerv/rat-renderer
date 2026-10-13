#include "Mesh.h"

#include "Resources.h"

#include <cstring>
#include <stdexcept>
#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

namespace
{
const std::vector<Vertex> plane_vertices = {
    {{-0.5f, -0.5f, 0.0f}, {0.2f, 0.0f, 0.4f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 0.3f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.3f, 0.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}};

const std::vector<uint32_t> plane_indices = {
    0, 1, 2, 2, 3, 0};

void computeSmoothNormals(std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices)
{
	// 1. zero everything
	for (auto &v : vertices)
		v.normal = glm::vec3(0.0f);

	// 2 + 3. accumulate face normals into each triangle's vertices
	for (size_t i = 0; i < indices.size(); i += 3)
	{
		Vertex &a = vertices[indices[i]];
		Vertex &b = vertices[indices[i + 1]];
		Vertex &c = vertices[indices[i + 2]];

		// length of this vector is proportional to triangle area,
		// so bigger triangles contribute more
		glm::vec3 faceNormal = glm::cross(b.pos - a.pos, c.pos - a.pos);

		a.normal += faceNormal;
		b.normal += faceNormal;
		c.normal += faceNormal;
	}

	// 4. normalize the sums into unit-length directions
	for (auto &v : vertices)
	{
		if (glm::length(v.normal) > 1e-8f)
			v.normal = glm::normalize(v.normal);
		else
			v.normal = glm::vec3(0.0f, 0.0f, 1.0f);        // fallback for degenerate vertices
	}
}
}        // namespace

Mesh loadModel(const std::string &modelPath)
{
	std::vector<Vertex>   vertices;
	std::vector<uint32_t> indices;

	tinyobj::attrib_t                attrib;
	std::vector<tinyobj::shape_t>    shapes;
	std::vector<tinyobj::material_t> materials;
	std::string                      warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str()))
	{
		throw std::runtime_error(warn + err);
	}

	std::unordered_map<Vertex, uint32_t> uniqueVertices;

	for (const auto &shape : shapes)
	{
		for (const auto &index : shape.mesh.indices)
		{
			Vertex vertex{};

			vertex.pos = {
			    attrib.vertices[3 * index.vertex_index + 0],
			    attrib.vertices[3 * index.vertex_index + 1],
			    attrib.vertices[3 * index.vertex_index + 2]};

			vertex.texCoord = {
			    attrib.texcoords[2 * index.texcoord_index + 0],
			    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};

			vertex.color = {
			    1.0f, 1.0f, 1.0f};

			if (uniqueVertices.count(vertex) == 0)
			{
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}
			indices.push_back(uniqueVertices[vertex]);
		}
	}

	computeSmoothNormals(vertices, indices);

	Mesh obj{
	    .vertices    = vertices,
	    .indices     = indices,
	    .index_count = static_cast<uint32_t>(indices.size())};

	return obj;
}

Mesh makePlaneGeometry()
{
	Mesh mesh;
	mesh.vertices    = plane_vertices;
	mesh.indices     = plane_indices;
	mesh.index_count = static_cast<uint32_t>(plane_indices.size());
	return mesh;
}

void createVertexBuffer(VulkanContext &ctx, Mesh &mesh)
{
	vk::DeviceSize bufferSize                 = sizeof(mesh.vertices[0]) * mesh.vertices.size();
	auto [stagingBuffer, stagingBufferMemory] = createBuffer(ctx, bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	void *dataStaging                         = stagingBufferMemory.mapMemory(0, bufferSize);
	memcpy(dataStaging, mesh.vertices.data(), bufferSize);
	stagingBufferMemory.unmapMemory();

	std::tie(mesh.vertexBuffer, mesh.vertexBufferMemory) = createBuffer(ctx, bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
	copyBuffer(ctx, stagingBuffer, mesh.vertexBuffer, bufferSize);
}

void createIndexBuffer(VulkanContext &ctx, Mesh &mesh)
{
	vk::DeviceSize bufferSize                 = sizeof(mesh.indices[0]) * mesh.indices.size();
	auto [stagingBuffer, stagingBufferMemory] = createBuffer(ctx, bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	void *dataStaging                         = stagingBufferMemory.mapMemory(0, bufferSize);
	memcpy(dataStaging, mesh.indices.data(), (size_t) bufferSize);
	stagingBufferMemory.unmapMemory();

	std::tie(mesh.indexBuffer, mesh.indexBufferMemory) = createBuffer(ctx, bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
	copyBuffer(ctx, stagingBuffer, mesh.indexBuffer, bufferSize);
}
