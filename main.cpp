#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#define NOMINMAX
#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define TINYOBJLOADER_IMPLEMENTATION


//#include <GLFW/glfw3native.h>
#include <memory>
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <map>
#include <optional>
#include <set>
#include <limits>
#include <algorithm>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <stb_image.h>
#include <tiny_obj_loader.h>
#include <random>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;

const std::string MODEL_PATH   = "models/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";

const std::vector<char const *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr int MAX_FRAMES_IN_FLIGHT = 2;


struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	static vk::VertexInputBindingDescription getBindingDescription()
	{
		return {
			.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex };
	}

	static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
	{
		return {
			{
				{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, pos)},
				{.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color)},
				{.location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, texCoord)}
			} };
	}

	bool operator==(const Vertex& other) const {
		return pos == other.pos &&
			color == other.color &&
			texCoord == other.texCoord;
	}
};

namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^
				(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}


class HelloTriangleApplication
{
  public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

  private:
	vk::raii::Context                context;
	vk::raii::Instance               instance       = nullptr;
	vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
	vk::raii::PhysicalDevice         physicalDevice = nullptr;
	vk::raii::Device                 device         = nullptr;
	vk::raii::Queue                  graphicsQueue  = nullptr;
	vk::raii::Queue					 computeQueue = nullptr;
	vk::raii::SurfaceKHR             surface        = nullptr;
	vk::raii::Queue                  presentQueue   = nullptr;
	vk::raii::SwapchainKHR           swapChain      = nullptr;
	vk::Format                       swapChainImageFormat;
	vk::Extent2D                     swapChainExtent;
	vk::SurfaceFormatKHR             swapChainSurfaceFormat;
	vk::raii::Pipeline               graphicsPipeline = nullptr;
	vk::raii::Pipeline               particlePipeline    = nullptr;
	vk::raii::CommandPool            commandPool      = nullptr;
	//vk::raii::Buffer                 vertexBuffer     = nullptr;
	//vk::raii::DeviceMemory           vertexBufferMemory = nullptr;
	//vk::raii::Buffer				 indexBuffer = nullptr;
	//vk::raii::DeviceMemory           indexBufferMemory  = nullptr;
	vk::raii::DescriptorPool		 descriptorPool = nullptr;

	vk::PhysicalDeviceTimelineSemaphoreFeaturesKHR timelineSemaphoreFeatures{
	    .timelineSemaphore = vk::True};
	
	//uint32_t                         mipLevels          = 0;
	//vk::raii::Image                  textureImage       = nullptr;
	//vk::raii::DeviceMemory           textureImageMemory = nullptr;
	//vk::raii::ImageView              textureImageView   = nullptr;
	vk::raii::Sampler				 textureSampler   = nullptr;

	vk::raii::Image					 depthImage = nullptr;
	vk::raii::DeviceMemory			 depthImageMemory = nullptr;
	vk::raii::ImageView              depthImageView     = nullptr;

	vk::raii::Image    colorImage       = nullptr;
	vk::raii::DeviceMemory colorImageMemory = nullptr;
	vk::raii::ImageView    colorImageView   = nullptr;

	std::vector<vk::raii::DescriptorSet> descriptorSets;
	std::vector<vk::raii::Buffer>		uniformBuffers;
	std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
	std::vector<void *>                 uniformBuffersMapped;

	std::vector<vk::raii::CommandBuffer>          commandBuffers;
	std::vector<vk::raii::CommandBuffer>		  computeCommandBuffers;

	std::vector<vk::Image>           swapChainImages;
	std::vector<vk::raii::ImageView> swapChainImageViews;

	std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
	std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
	std::vector<vk::raii::Fence>     inFlightFences;

	std::vector<vk::raii::Semaphore> computeFinishedSemaphores;
	std::vector<vk::raii::Fence>     computeInFlightFences;

	std::vector<vk::raii::Buffer> shaderStorageBuffers;
	std::vector<vk::raii::DeviceMemory> shaderStorageBuffersMemory;

	vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
	vk::raii::PipelineLayout pipelineLayout = nullptr;
	vk::raii::DescriptorSetLayout computeDescriptorSetLayout = nullptr;
	vk::raii::PipelineLayout             computePipelineLayout      = nullptr;
	vk::raii::Pipeline                   computePipeline            = nullptr;


	std::vector<vk::raii::DescriptorSet> computeDescriptorSets;

	vk::SampleCountFlagBits msaaSamples = vk::SampleCountFlagBits::e1;

	GLFWwindow *window = nullptr;

	uint32_t semaphoreIndex = 0;

	const std::vector<const char *> deviceExtensions = {
	    vk::KHRSwapchainExtensionName};

	uint32_t graphicsQueueFamilyIndex;
	uint32_t frameIndex = 0;

	bool framebufferResized = false;

	uint32_t PARTICLE_COUNT = 100000;

	uint64_t timelineValue;

	struct UniformBufferObject
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
		float     deltaTime;
	};


	//struct Plane {
	//	glm::vec3 position;
	//	glm::vec3 normal;
	//	glm::vec3 forward;
	//};

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool isComplete()
		{
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	struct SwapChainSupportDetails
	{
		vk::SurfaceCapabilitiesKHR        capabilities;
		std::vector<vk::SurfaceFormatKHR> formats;
		std::vector<vk::PresentModeKHR>   presentMode;
	};


	struct Particle {
		glm::vec3 position;
		float     _pad0 = 0;
		glm::vec3 velocity;
		float     _pad1 = 0;
		glm::vec4 color;

		static vk::VertexInputBindingDescription getBindingDescription()
		{
			return
			{
				.binding = 0,
				.stride  = sizeof(Particle),
				.inputRate = vk::VertexInputRate::eVertex
			};
		}

		static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
		{
			return {
			    {
					{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Particle, position)},
					{.location = 1, .binding = 0, .format = vk::Format::eR32G32B32A32Sfloat, .offset = offsetof(Particle, color)}
			    }
			};
		}
	};

	struct Mesh {
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		vk::raii::Buffer                 vertexBuffer = nullptr;
		vk::raii::DeviceMemory           vertexBufferMemory = nullptr;
		vk::raii::Buffer				 indexBuffer = nullptr;
		vk::raii::DeviceMemory           indexBufferMemory = nullptr;

		uint32_t index_count = 0;

		// texture
		vk::raii::Image textureImage = nullptr;
		vk::raii::DeviceMemory textureMemory = nullptr;
		vk::raii::ImageView textureView = nullptr;
		uint32_t mipLevels = 1;

		// descriptor sets
		vk::raii::DescriptorSets descriptorSets = nullptr;

		glm::mat4 transform = glm::mat4(1.0f);
	};


	struct PipelineConfig {
		std::string vertEntry;
		std::string fragEntry;
		vk::raii::ShaderModule* shaderModule;
		vk::raii::PipelineLayout* layout;

		// vertex input
		vk::VertexInputBindingDescription bindingDesc;
		std::vector<vk::VertexInputAttributeDescription> attributeDescs;

		//topology
		vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;

		//depth
		bool depthTest = true;
		bool depthWrite = true;

		//blending
		bool additiveBlending = false;

		//culling
		vk::CullModeFlags cullMode = vk::CullModeFlagBits::eNone;

	};


	// planes
	// 
	const std::vector<Vertex> plane_vertices = {
	    {{-0.5f, -0.5f, 0.0f}, {0.2f, 0.0f, 0.4f}, {1.0f, 0.0f}},
	    {{0.5f, -0.5f, 0.0f}, {0.0f, 0.3f, 0.0f}, {0.0f, 0.0f}},
	    {{0.5f, 0.5f, 0.0f}, {0.3f, 0.0f, 1.0f}, {0.0f, 1.0f}},
	    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}

	};
		
	//indices for planes
	// 
	const std::vector<uint32_t> plane_indices = {
	    0, 1, 2, 2, 3, 0
	}; 


	std::vector<Mesh> meshList;

	//std::vector<Vertex>   vertices;
	//std::vector<uint16_t> indices;


	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
	{
		auto app = reinterpret_cast<HelloTriangleApplication *>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}


	void initVulkan()
	{
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createDescriptorSetLayout();
		createComputeDescriptorSetLayout();
		createPipelineLayouts();
		createComputePipeline();
		createParticlePipeline();
		createGraphicsPipeline();
		createCommandPool();
		createColorResources();
		createDepthResources();
		//createTextureImage();
		//createTextureImageView();
		createTextureSampler();
		//createVertexBuffer();
		//createIndexBuffer();
		createShaderStorageBuffers();
		createUniformBuffers();
		createDescriptorPool();
		//createDescriptorSets();
		loadScene();
		createComputeDescriptorSets();
		createCommandBuffers();
		createSyncObjects();
	}



	void loadScene() {
		std::string viking_room_model_path = "models/viking_room.obj";
		std::string viking_room_texture_path = "textures/viking_room.png";
		glm::vec3 viking_position = {0.0, 0.0, 0.0};
		glm::vec3 viking_scale = { 1.0, 1.0, 1.0 };
		glm::vec3 viking_rotation = { 0.0, 0.0, 0.0 };
		Mesh vikingMesh = createMesh(viking_room_model_path, viking_room_texture_path, viking_position, viking_rotation, viking_scale);
		meshList.push_back(std::move(vikingMesh));
		glm::vec3 plane_position = { 0.0, 0.0, -3.0};
		glm::vec3 plane_scale = { 10.0, 10.0, 1.0 };
		glm::vec3 plane_forward = { 0.0, 0.0, 0.0 };
		glm::vec3 plane_normal = { 0.0, 0.0, 1.0 };
		std::string plane_texture_path = "textures/cat.png";
		Mesh planeMesh = createPlane(plane_texture_path, plane_position, plane_scale, plane_normal, plane_forward);
		meshList.push_back(std::move(planeMesh));
	}

	Mesh createMesh(std::string& mesh_path, const std::string& texture_path, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale){

		Mesh meshObj = loadModel(mesh_path);
		glm::mat4 transform = createTransform(position, rotation, scale);
		meshObj.transform = transform;
		createVertexBuffer(meshObj);
		createIndexBuffer(meshObj);
		createTextureImage(meshObj, texture_path);
		createTextureImageView(meshObj);
		createMeshDescriptorSets(meshObj);
		return meshObj;
	}

	Mesh createPlane(const std::string& texture_path, glm::vec3 position, glm::vec3 scale, glm::vec3 normal, glm::vec3 forward) {

		//glm::mat4 transform = createTransform(position);
		Mesh meshObj;
		meshObj.vertices = plane_vertices;
		meshObj.indices = plane_indices;
		meshObj.index_count = static_cast<uint32_t>(plane_indices.size());
		meshObj.transform = createTransform(position, glm::vec3(0.0f), scale);

		createVertexBuffer(meshObj);
		createIndexBuffer(meshObj);
		std::string path = texture_path.empty() ? "textures/white.jpg" : texture_path;
		createTextureImage(meshObj, path);
		createTextureImageView(meshObj);
		createMeshDescriptorSets(meshObj);

		return meshObj;
	}


	glm::mat4 createTransform(glm::vec3 position = glm::vec3(0.0f), glm::vec3 rotation = glm::vec3(0.0f), glm::vec3 scalar = glm::vec3(1.0f)) {

		glm::mat4 translate = glm::translate(glm::mat4(1.0f), position);
		glm::mat4 rotateX = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.x), glm::vec3(1.0, 0.0, 0.0));
		glm::mat4 rotateY = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.y), glm::vec3(0.0, 1.0, 0.0));
		glm::mat4 rotateZ = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.z), glm::vec3(0.0, 0.0, 1.0));
		glm::mat4 scale = glm::scale(glm::mat4(1.0f), scalar);
	
		return translate * rotateZ * rotateY * rotateX * scale;
	}


	void createPipelineLayouts()
	{

		vk::PushConstantRange pushConstantRange{
			.stageFlags = vk::ShaderStageFlagBits::eVertex,
			.offset = 0,
			.size = sizeof(glm::mat4)
		};

		// mesh layout:
		vk::PipelineLayoutCreateInfo meshLayoutInfo{
		    .setLayoutCount = 1,
		    .pSetLayouts    = &*descriptorSetLayout,
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pushConstantRange
		};
		pipelineLayout = vk::raii::PipelineLayout(device, meshLayoutInfo);

		// compute/particle layout:
		vk::PipelineLayoutCreateInfo computeLayoutInfo{
		    .setLayoutCount = 1,
		    .pSetLayouts    = &*computeDescriptorSetLayout};
		computePipelineLayout = vk::raii::PipelineLayout(device, computeLayoutInfo);
	}


	vk::raii::Pipeline createPipeline(PipelineConfig &config)
	{
		vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
		    .stage = vk::ShaderStageFlagBits::eVertex, .module = **config.shaderModule, .pName = config.vertEntry.c_str()};
		vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
		    .stage = vk::ShaderStageFlagBits::eFragment, .module = **config.shaderModule, .pName = config.fragEntry.c_str()};
		vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

		auto bindingDescription    = config.bindingDesc;
		auto attributeDescriptions = config.attributeDescs;

		vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
		    .vertexBindingDescriptionCount   = 1,
		    .pVertexBindingDescriptions      = &bindingDescription,
		    .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
		    .pVertexAttributeDescriptions    = attributeDescriptions.data()};

		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = config.topology};

		//vk::Viewport viewport{
		//    0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f};

		std::vector<vk::DynamicState> dynamicState = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

		vk::PipelineDynamicStateCreateInfo dynamicstate{
		    .dynamicStateCount = static_cast<uint32_t>(dynamicState.size()),
		    .pDynamicStates    = dynamicState.data()
		};

		vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1, .scissorCount = 1};

		vk::PipelineRasterizationStateCreateInfo rasterizer{
		    .depthClampEnable        = vk::False,
		    .rasterizerDiscardEnable = vk::False,
		    .polygonMode             = vk::PolygonMode::eFill,
		    .cullMode                = config.cullMode,
		    .frontFace               = vk::FrontFace::eCounterClockwise,
		    .depthBiasEnable         = vk::False,
		    .lineWidth               = 1.0f};

		vk::PipelineMultisampleStateCreateInfo multisampling{.rasterizationSamples = msaaSamples, .sampleShadingEnable = vk::False};


		vk::PipelineColorBlendAttachmentState colorBlendAttachment;

		if (config.additiveBlending)
		{
			colorBlendAttachment = {
				.blendEnable = vk::True,
				.srcColorBlendFactor = vk::BlendFactor::eOne,
				.dstColorBlendFactor = vk::BlendFactor::eOne,
				.colorBlendOp = vk::BlendOp::eAdd,
				.srcAlphaBlendFactor = vk::BlendFactor::eOne,
				.dstAlphaBlendFactor = vk::BlendFactor::eOne,
				.alphaBlendOp = vk::BlendOp::eAdd,
			    .colorWriteMask      = vk::ColorComponentFlagBits::eR |
			                      vk::ColorComponentFlagBits::eG |
			                      vk::ColorComponentFlagBits::eB |
			                      vk::ColorComponentFlagBits::eA};
		}
		else
		{
			colorBlendAttachment = {
			    .blendEnable    = vk::False,
			    .colorWriteMask = vk::ColorComponentFlagBits::eR |
			                      vk::ColorComponentFlagBits::eG |
			                      vk::ColorComponentFlagBits::eB |
			                      vk::ColorComponentFlagBits::eA};
		}

		vk::PipelineColorBlendStateCreateInfo colorBlending{
		    .logicOpEnable   = vk::False,
		    .logicOp         = vk::LogicOp::eCopy,
		    .attachmentCount = 1,
		    .pAttachments    = &colorBlendAttachment};

		vk::PipelineDepthStencilStateCreateInfo depthStencil{
		    .depthTestEnable       = config.depthTest,
		    .depthWriteEnable      = config.depthWrite,
		    .depthCompareOp        = vk::CompareOp::eLess,
		    .depthBoundsTestEnable = vk::False,
		    .stencilTestEnable     = vk::False};

		//vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
		//    .colorAttachmentCount        = 1,
		//    .pColorAttachmentFormats = &swapChainSurfaceFormat.format};

		vk::Format depthFormat = findDepthFormat();

		vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
		    
		        {.stageCount          = 2,
		         .pStages             = shaderStages,
		         .pVertexInputState   = &vertexInputInfo,
		         .pInputAssemblyState = &inputAssembly,
		         .pViewportState      = &viewportState,
				 .pRasterizationState = &rasterizer,
				 .pMultisampleState = &multisampling,
				 .pDepthStencilState = &depthStencil,
				 .pColorBlendState = &colorBlending,
				 .pDynamicState = &dynamicstate,
				 .layout = *config.layout,
				 .renderPass = nullptr
				},
				{
					.colorAttachmentCount = 1,
					.pColorAttachmentFormats = &swapChainSurfaceFormat.format,
					.depthAttachmentFormat = depthFormat
				}
		};

		vk::raii::Pipeline pipeline  = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
		return pipeline;
	}
	

	void createComputePipeline()
	{

		vk::raii::ShaderModule            computeShaderModule = createShaderModule(readFile("shaders/compute.spv"));
		vk::PipelineShaderStageCreateInfo computeShaderStageInfo{.stage = vk::ShaderStageFlagBits::eCompute, .module = computeShaderModule, .pName = "compMain"};

		vk::ComputePipelineCreateInfo pipelineInfo{
			.stage = computeShaderStageInfo,
			.layout = *computePipelineLayout
		};

		computePipeline = device.createComputePipeline(nullptr, pipelineInfo);
	}


	void createComputeDescriptorSetLayout()
	{
		std::array layoutBindings{
		    vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex, nullptr),
		    vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute, nullptr),
		    vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute, nullptr)};

		vk::DescriptorSetLayoutCreateInfo layoutInfo{
		    .bindingCount = static_cast<uint32_t>(layoutBindings.size()),
		    .pBindings    = layoutBindings.data()};
		computeDescriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
	}

	void createComputeDescriptorSets()
	{

		std::vector<vk::DescriptorSetLayout> layouts(
		    MAX_FRAMES_IN_FLIGHT, *computeDescriptorSetLayout);

		vk::DescriptorSetAllocateInfo allocInfo{
		    .descriptorPool     = descriptorPool,
		    .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
		    .pSetLayouts        = layouts.data()};

		computeDescriptorSets =
		    vk::raii::DescriptorSets(device, allocInfo);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vk::DescriptorBufferInfo bufferInfo{
			    .buffer = uniformBuffers[i],
			    .offset = 0,
			    .range  = sizeof(UniformBufferObject)};


			vk::DescriptorBufferInfo storageBufferInfoLastFrame(shaderStorageBuffers[(i - 1 + MAX_FRAMES_IN_FLIGHT) % MAX_FRAMES_IN_FLIGHT], 0, sizeof(Particle) * PARTICLE_COUNT);
			vk::DescriptorBufferInfo storageBufferInfoCurrentFrame(shaderStorageBuffers[i], 0, sizeof(Particle) * PARTICLE_COUNT);

			std::array descriptorWrites{
			    vk::WriteDescriptorSet{
			        .dstSet          = computeDescriptorSets[i],
			        .dstBinding      = 0,
			        .dstArrayElement = 0,
			        .descriptorCount = 1,
			        .descriptorType  = vk::DescriptorType::eUniformBuffer,
			        .pBufferInfo     = &bufferInfo},
			    vk::WriteDescriptorSet{
			        .dstSet          = computeDescriptorSets[i],
			        .dstBinding      = 1,
			        .dstArrayElement = 0,
			        .descriptorCount = 1,
			        .descriptorType  = vk::DescriptorType::eStorageBuffer,
			        .pBufferInfo     = &storageBufferInfoLastFrame},
			    vk::WriteDescriptorSet{
			        .dstSet          = computeDescriptorSets[i],
			        .dstBinding      = 2,
			        .dstArrayElement = 0,
			        .descriptorCount = 1,
			        .descriptorType  = vk::DescriptorType::eStorageBuffer,
			        .pBufferInfo     = &storageBufferInfoCurrentFrame}};

			device.updateDescriptorSets(descriptorWrites, {});
		}
	}


	void createShaderStorageBuffers()
	{
		shaderStorageBuffers.clear();
		shaderStorageBuffersMemory.clear();

		std::default_random_engine rndEngine((unsigned) time(nullptr));
		std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);


		std::vector<Particle> particles(PARTICLE_COUNT);

		for (auto &particle : particles)
		{
			float r = 0.8 * sqrtf(rndDist(rndEngine));

			float theta = rndDist(rndEngine) * 2.0f * 3.14159265358979323846f;

			//float x = r * cosf(theta) * HEIGHT / WIDTH;
			//float y = r * sinf(theta);
			//float z = r * sinf(theta);

			float phi = rndDist(rndEngine) * 3.14159265f;
			float x   = r * sinf(phi) * cosf(theta);
			float y   = r * sinf(phi) * sinf(theta);
			float z   = r * cosf(phi);

			particle.position = glm::vec3(x,y,z);
			particle.velocity = normalize(glm::vec3(x, y, z)) * 0.00025f;
			particle.color    = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
		}

		vk::DeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;

		// Created a staging buffer used to upload data to the gpu

		auto [stagingBuffer, stagingMemory] = createBuffer(
		   bufferSize,
		   vk::BufferUsageFlagBits::eTransferSrc,
		   vk::MemoryPropertyFlagBits::eHostVisible |
		   vk::MemoryPropertyFlagBits::eHostCoherent);

		void *data = stagingMemory.mapMemory(0, bufferSize);
		memcpy(data, particles.data(), bufferSize);
		stagingMemory.unmapMemory();

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			auto [buffer, bufferMemory] = createBuffer(bufferSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
			copyBuffer(stagingBuffer, buffer, bufferSize);
			shaderStorageBuffers.emplace_back(std::move(buffer));
			shaderStorageBuffersMemory.emplace_back(std::move(bufferMemory));
		}
	}


	void createColorResources()
	{
		vk::Format colorFormat = swapChainSurfaceFormat.format;
		std::tie(colorImage, colorImageMemory) = createImage(swapChainExtent.width,
		                                                     swapChainExtent.height,
		                                                     1,
		                                                     msaaSamples,
		                                                     colorFormat,
		                                                     vk::ImageTiling::eOptimal,
		                                                     vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
		                                                     vk::MemoryPropertyFlagBits::eDeviceLocal);

		  colorImageView = createImageView(colorImage, colorFormat, vk::ImageAspectFlagBits::eColor, 1);
	}

	vk::SampleCountFlagBits getMaxUsableSampleCount()
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

	//std::vector<Vertex>   vertices;
	//std::vector<uint16_t> indices;

	//Mesh loadPlane() {

	//}

	Mesh loadModel(std::string& modelPath)
	{

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string                      warn, err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str()))
		{
			throw std::runtime_error(warn + err);
		}

		std::unordered_map<Vertex, uint32_t> uniqueVertices;

		for (const auto& shape : shapes)
		{
			for (const auto& index : shape.mesh.indices)
			{
				Vertex vertex{};



				vertex.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};

				vertex.texCoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1] };

				vertex.color = {
					1.0f, 1.0f, 1.0f };


				if (uniqueVertices.count(vertex) == 0) {
					uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
					vertices.push_back(vertex);
				}
				indices.push_back(uniqueVertices[vertex]);

			}


		}
		Mesh obj{
			.vertices = vertices,
			.indices = indices,
			.index_count = static_cast<uint32_t>(indices.size())
		};

		return obj;
	}


	vk::Format findSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
	{
		for (const auto format : candidates)
		{
			vk::FormatProperties props = physicalDevice.getFormatProperties(format);

			if (
				((tiling == vk::ImageTiling::eLinear) && ((props.linearTilingFeatures & features) == features)) || 
				(((tiling == vk::ImageTiling::eOptimal)) && ((props.optimalTilingFeatures & features) == features))
				)
			{
				return format;
			}

		}

		throw std::runtime_error("failed to support format!");
	}

	vk::Format findDepthFormat()
	{
		return findSupportedFormat({vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
		                           vk::ImageTiling::eOptimal,
		                           vk::FormatFeatureFlagBits::eDepthStencilAttachment);
	}


	void createDepthResources()
	{
		vk::Format depthFormat = findDepthFormat();
		std::tie(depthImage, depthImageMemory) = createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
		depthImageView                         = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);

	}

	void createTextureSampler()
	{
		vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
		vk::SamplerCreateInfo        samplerInfo{.magFilter        = vk::Filter::eLinear,
		                                         .minFilter        = vk::Filter::eLinear,
		                                         .mipmapMode       = vk::SamplerMipmapMode::eLinear,
		                                         .addressModeU     = vk::SamplerAddressMode::eRepeat,
		                                         .addressModeV     = vk::SamplerAddressMode::eRepeat,
		                                         .addressModeW     = vk::SamplerAddressMode::eRepeat,
		                                         .mipLodBias   = 0.0f,
		                                         .anisotropyEnable = vk::True,
		                                         .maxAnisotropy    = properties.limits.maxSamplerAnisotropy,
		                                         .compareEnable    = vk::False,
		                                         .compareOp        = vk::CompareOp::eAlways,
		                                         .minLod                    = 0.0f,
		                                         .maxLod                    = vk::LodClampNone,
												.borderColor      = vk::BorderColor::eIntOpaqueBlack,
												.unnormalizedCoordinates = vk::False
		};

		textureSampler = vk::raii::Sampler(device, samplerInfo);

	}

	void createTextureImageView(Mesh& meshObj)
	{
		meshObj.textureView = createImageView(*(meshObj.textureImage), vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, meshObj.mipLevels);
	}


	vk::raii::ImageView createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels)
	{
		vk::ImageViewCreateInfo viewInfo{
		    .image            = image,
		    .viewType         = vk::ImageViewType::e2D,
		    .format           = format,
		    .subresourceRange = {.aspectMask = aspectFlags, .baseMipLevel = 0, .levelCount = mipLevels, .baseArrayLayer = 0, .layerCount = 1}};

		return vk::raii::ImageView(device, viewInfo);
	}
	

	void copyBufferToImage(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Buffer &buffer, vk::raii::Image &image, uint32_t width, uint32_t height) {
		vk::BufferImageCopy region
		{
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {
				.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1
			},
			.imageOffset = {0,0,0}, .imageExtent = { width, height, 1}
		};

		commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
	}

	void transitionImageLayout(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels)
	{
		vk::ImageMemoryBarrier barrier{.oldLayout = oldLayout, .newLayout = newLayout, .srcQueueFamilyIndex = vk::QueueFamilyIgnored, .dstQueueFamilyIndex = vk::QueueFamilyIgnored, .image = image, .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = mipLevels, .layerCount = 1}};

		vk::PipelineStageFlags sourceStage;
		vk::PipelineStageFlags destinationStage;

		if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
		{
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
			destinationStage = vk::PipelineStageFlagBits::eTransfer;
		}
		else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			sourceStage = vk::PipelineStageFlagBits::eTransfer;
			destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else
		{
			throw std::invalid_argument("unsupported layout transition!");
		}

		commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, barrier);
	}

	vk::raii::CommandBuffer beginSingleTimeCommands()
	{
		vk::CommandBufferAllocateInfo allocInfo
		{.commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};

		vk::raii::CommandBuffer commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());
		vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
		commandBuffer.begin(beginInfo);
		return std::move(commandBuffer);
	}


	void endSingleTimeCommands(vk::raii::CommandBuffer &&commandBuffer)
	{
	    commandBuffer.end();

		vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffer};

		graphicsQueue.submit(submitInfo, nullptr);
		graphicsQueue.waitIdle();
	}



	std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(
		uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties
	)
	{
		vk::ImageCreateInfo imageInfo{
		    .imageType   = vk::ImageType::e2D,
		    .format      = format,
		    .extent                  = {width,
		                                height,
		                                1},
		    .mipLevels   = mipLevels,
		    .arrayLayers = 1,
		    .samples     = numSamples,
		    .tiling      = tiling,
		    .usage       = usage,
		    .sharingMode = vk::SharingMode::eExclusive,
		};
		vk::raii::Image image = vk::raii::Image(device, imageInfo);

		vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo{.allocationSize  = memRequirements.size,
		                                 .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};
		vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(device, allocInfo);
		image.bindMemory(imageMemory, 0);

		return {std::move(image), std::move(imageMemory)};
	}

	void createTextureImage(Mesh& meshObj, std::string texture_path)
	{
		int texWidth, texHeight, texChannels;
		stbi_uc       *pixels    = stbi_load(texture_path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		vk::DeviceSize imageSize = texWidth * texHeight * 4;
		int mipLevels                = static_cast<uint32_t>(std::floor(std::log2(std::max(texHeight, texWidth)))) + 1;;

		if (!pixels)
		{
			throw std::runtime_error(
			    std::string("failed to load texture: ") +
			    stbi_failure_reason());
		}

		auto [stagingBuffer, stagingBufferMemory] = createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		void *data = stagingBufferMemory.mapMemory(0, imageSize);
		memcpy(data, pixels, imageSize);

		stagingBufferMemory.unmapMemory();

		stbi_image_free(pixels);

		std::tie(meshObj.textureImage, meshObj.textureMemory) = createImage(texWidth, texHeight, mipLevels, vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);

		vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
		transitionImageLayout(commandBuffer, meshObj.textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels);
		copyBufferToImage(commandBuffer, stagingBuffer, meshObj.textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		//transitionImageLayout(commandBuffer, textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, mipLevels);
		generateMipMaps(commandBuffer, meshObj.textureImage, vk::Format::eR8G8B8A8Srgb,  texWidth, texHeight, mipLevels);
		endSingleTimeCommands(std::move(commandBuffer));
		meshObj.mipLevels = mipLevels;
	}


	void generateMipMaps(vk::raii::CommandBuffer &commandBuffer, vk::raii::Image &image, vk::Format imageFormat, uint32_t width, uint32_t height, uint32_t mipLevels)
	{

		vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(imageFormat);

		if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear))
		{
			throw std::runtime_error("texture image format does not support linear blitting!");
		}

		vk::ImageMemoryBarrier barrier = {
		    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		    .image               = image,
			.subresourceRange = {
				.aspectMask = vk::ImageAspectFlagBits::eColor,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		
		};

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
			    .srcOffsets     = std::array<vk::Offset3D, 2>({{}, {mipWidth, mipHeight, 1}}),
			    .dstSubresource = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = i, .layerCount = 1},
			    .dstOffsets     = std::array<vk::Offset3D, 2>({{}, {1 < mipWidth ? mipWidth / 2 : 1, 1 < mipHeight ? mipHeight / 2 : 1, 1}})};
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
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.oldLayout                     = vk::ImageLayout::eUndefined;
		barrier.newLayout                     = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask                 = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask                 = vk::AccessFlagBits::eShaderRead;

		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);
	}

	void createMeshDescriptorSets(Mesh& mesh)
	{
		// allocate descriptor sets:
		std::vector<vk::DescriptorSetLayout> layouts(
			MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);

		vk::DescriptorSetAllocateInfo allocInfo{
			.descriptorPool = *descriptorPool,
			.descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
			.pSetLayouts = layouts.data()
		};

		mesh.descriptorSets = vk::raii::DescriptorSets(
			device, allocInfo);

		// update each descriptor set:
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vk::DescriptorBufferInfo bufferInfo{
				.buffer = *uniformBuffers[i],
				.offset = 0,
				.range = sizeof(UniformBufferObject)
			};

			vk::DescriptorImageInfo imageInfo{
				.sampler = *textureSampler,
				.imageView = *mesh.textureView,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
			};

			std::array descriptorWrites{
				vk::WriteDescriptorSet{
					.dstSet = *mesh.descriptorSets[i],
					.dstBinding = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eUniformBuffer,
					.pBufferInfo = &bufferInfo
				},
				vk::WriteDescriptorSet{
					.dstSet = *mesh.descriptorSets[i],
					.dstBinding = 1,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eCombinedImageSampler,
					.pImageInfo = &imageInfo
				}
			};

			device.updateDescriptorSets(descriptorWrites, {});
		}
	}

	//void createDescriptorSets()
	//{
	//	std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
	//	vk::DescriptorSetAllocateInfo        allocInfo{
	//	           .descriptorPool     = descriptorPool,
	//	           .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
	//	           .pSetLayouts        = layouts.data()};

	//	descriptorSets = vk::raii::DescriptorSets(device, allocInfo);
	//	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	//	{
	//		vk::DescriptorBufferInfo bufferInfo{
	//		    .buffer = uniformBuffers[i],
	//			.offset = 0,
	//		    .range  = sizeof(UniformBufferObject)
	//		};

	//		vk::DescriptorImageInfo imageInfo{
	//		    .sampler     = textureSampler,
	//		    .imageView   = textureImageView,
	//		    .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};


	//		std::array<vk::WriteDescriptorSet, 2> descriptorWrites{
	//		    {
	//				{
	//					.dstSet = descriptorSets[i],
	//					.dstBinding = 0,
	//					.dstArrayElement = 0,
	//					.descriptorCount = 1,
	//					.descriptorType = vk::DescriptorType::eUniformBuffer,
	//					.pBufferInfo = &bufferInfo
	//				},
	//				{
	//					.dstSet = descriptorSets[i],
	//					.dstBinding = 1,
	//					.dstArrayElement = 0,
	//					.descriptorCount = 1,
	//					.descriptorType = vk::DescriptorType::eCombinedImageSampler,
	//					.pImageInfo = &imageInfo
	//				}
	//			}};


	//		device.updateDescriptorSets(descriptorWrites, {});
	//	}
	//}

	void createDescriptorPool(){

		const uint32_t maxMeshes = 16;
		const uint32_t maxSets = MAX_FRAMES_IN_FLIGHT * (maxMeshes + 1);

		std::array<vk::DescriptorPoolSize, 3> poolSize{
		    {{.type            = vk::DescriptorType::eUniformBuffer,
		      .descriptorCount = MAX_FRAMES_IN_FLIGHT * (maxMeshes + 1)},
		     {.type            = vk::DescriptorType::eCombinedImageSampler,
		      .descriptorCount = MAX_FRAMES_IN_FLIGHT * maxMeshes},
			 {.type            = vk::DescriptorType::eStorageBuffer,
		      .descriptorCount = MAX_FRAMES_IN_FLIGHT * 2}}
		};


		vk::DescriptorPoolCreateInfo poolInfo
		{
			.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			.maxSets       = maxSets,
			.poolSizeCount = static_cast<uint32_t>(poolSize.size()),
			.pPoolSizes    = poolSize.data()
		};

		descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
	}
	

	void updateUniformBuffer(uint32_t currentImage)
	{
		static auto startTime = std::chrono::high_resolution_clock::now();
		static auto lastTime  = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();

		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		float deltaTime = std::chrono::duration<float,std::chrono::seconds::period>(currentTime - lastTime).count();
		lastTime = currentTime;

		UniformBufferObject ubo{};
		ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

		ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

		ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(swapChainExtent.width)/static_cast<float>(swapChainExtent.height), 0.1f, 10.0f);

		ubo.proj[1][1] *= -1;

		ubo.deltaTime = deltaTime;

		memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));

	}


	void createUniformBuffers()
	{
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
			//buffer is a description of the buffer
			auto [buffer, bufferMem]  = createBuffer(
                bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
			uniformBuffers.emplace_back(std::move(buffer));
			uniformBuffersMemory.emplace_back(std::move(bufferMem));
			uniformBuffersMapped.emplace_back(uniformBuffersMemory.back().mapMemory(0, bufferSize));
		}
	}

	void createDescriptorSetLayout()
	{
		vk::DescriptorSetLayoutBinding uboLayoutBinding{
		    .binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eVertex};


		std::array<vk::DescriptorSetLayoutBinding, 2> bindings{
			{
				{.binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eVertex},
				{.binding = 1, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment}
			}
		};

		vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data()};



		descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
	}


	std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties){
		vk::BufferCreateInfo bufferInfo{
		    .size        = size,
		    .usage       = usage,
		    .sharingMode = vk::SharingMode::eExclusive};

		vk::raii::Buffer buffer = vk::raii::Buffer(device, bufferInfo);
		vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo{
		    .allocationSize  = memRequirements.size,
		    .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};
		vk::raii::DeviceMemory bufferMemory  = vk::raii::DeviceMemory(device, allocInfo);
		buffer.bindMemory(*bufferMemory, 0);
		return {std::move(buffer), std::move(bufferMemory)};
	}

	void createIndexBuffer(Mesh& meshObj)
	{
		vk::DeviceSize bufferSize                 = sizeof(meshObj.indices[0]) * meshObj.indices.size();
		auto [stagingBuffer, stagingBufferMemory] = createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		void *dataStaging                         = stagingBufferMemory.mapMemory(0, bufferSize);
		memcpy(dataStaging, meshObj.indices.data(), (size_t) bufferSize);
		stagingBufferMemory.unmapMemory();

		std::tie(meshObj.indexBuffer, meshObj.indexBufferMemory) = createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
		copyBuffer(stagingBuffer, meshObj.indexBuffer, bufferSize);
	}
	void createVertexBuffer(Mesh& meshObj)
	{
		vk::DeviceSize bufferSize = sizeof(meshObj.vertices[0]) * meshObj.vertices.size();
		auto [stagingBuffer, stagingBufferMemory] = createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
		memcpy(dataStaging, meshObj.vertices.data(), bufferSize);
		stagingBufferMemory.unmapMemory();

		std::tie(meshObj.vertexBuffer, meshObj.vertexBufferMemory) = createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
		copyBuffer(stagingBuffer, meshObj.vertexBuffer, bufferSize);
	}

	void copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size)
	{

		vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();
		commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy{.size = size});
		endSingleTimeCommands(std::move(commandCopyBuffer));
	}



	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
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


	void cleanupSwapChain()
	{
		swapChainImageViews.clear();
		swapChain = nullptr;
	}

	void recreateSwapChain()
	{
		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		device.waitIdle();
		cleanupSwapChain();
		createSwapChain();
		createImageViews();

		semaphoreIndex = 0;
		presentCompleteSemaphores.clear();
		for (size_t i = 0; i < swapChainImages.size(); i++) {
			presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
		}

	}


	void createSyncObjects()
	{

		assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());

		assert(computeInFlightFences.empty() && computeFinishedSemaphores.empty());

		for (size_t i = 0; i < swapChainImages.size(); i++)
		{
			presentCompleteSemaphores.emplace_back(
				device, vk::SemaphoreCreateInfo());
			renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
		
			inFlightFences.emplace_back(device, vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled });

			computeFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
			computeInFlightFences.emplace_back(device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});

		}
	}

	void recordComputeCommandBuffers(vk::raii::CommandBuffer &computeCommandBuffer){
		computeCommandBuffer.begin({});
		computeCommandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);
		computeCommandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, computePipelineLayout, 0, *computeDescriptorSets[frameIndex], {});
		computeCommandBuffer.dispatch(PARTICLE_COUNT/256, 1, 1);
		computeCommandBuffer.end();
	}

	void renderMesh(Mesh& mesh, vk::raii::CommandBuffer& commandBuffer) {

		commandBuffer.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			*pipelineLayout,
			0,
			*mesh.descriptorSets[frameIndex],
			nullptr);

		commandBuffer.pushConstants(
			*pipelineLayout,
			vk::ShaderStageFlagBits::eVertex,
			0, sizeof(glm::mat4),
			&mesh.transform);
		commandBuffer.bindVertexBuffers(0, *(mesh.vertexBuffer), { 0 });
		commandBuffer.bindIndexBuffer(*(mesh.indexBuffer), 0, vk::IndexType::eUint32);
		commandBuffer.drawIndexed(static_cast<uint32_t>(mesh.index_count), 1, 0, 0, 0);
	}

	void recordCommandBuffer(uint32_t imageIndex, vk::raii::CommandBuffer &commandBuffer)
	{
		commandBuffer.begin({});

		// Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
		transition_image_layout(
		    imageIndex,
		    vk::ImageLayout::eUndefined,
		    vk::ImageLayout::eColorAttachmentOptimal,
		    {},
		    vk::AccessFlagBits2::eColorAttachmentWrite,
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		    vk::ImageAspectFlagBits::eColor);

		transition_image_layout(
		    *colorImage,
		    vk::ImageLayout::eUndefined,
		    vk::ImageLayout::eColorAttachmentOptimal,
		    vk::AccessFlagBits2::eColorAttachmentWrite,
		    vk::AccessFlagBits2::eColorAttachmentWrite,
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		    vk::ImageAspectFlagBits::eColor);

		// add this after the colorImage transition:
		transition_image_layout(
		    *depthImage,
		    vk::ImageLayout::eUndefined,
		    vk::ImageLayout::eDepthAttachmentOptimal,
		    {},
		    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		    vk::PipelineStageFlagBits2::eEarlyFragmentTests |
		        vk::PipelineStageFlagBits2::eLateFragmentTests,
		    vk::PipelineStageFlagBits2::eEarlyFragmentTests |
		        vk::PipelineStageFlagBits2::eLateFragmentTests,
		    vk::ImageAspectFlagBits::eDepth);

		vk::ClearValue              clearColor      = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::ClearValue              clearDepth      = vk::ClearDepthStencilValue(1.0f, 0);
		vk::RenderingAttachmentInfo colorAttachment = {
		    .imageView          = colorImageView,
		    .imageLayout        = vk::ImageLayout::eColorAttachmentOptimal,
		    .resolveMode        = vk::ResolveModeFlagBits::eAverage,
		    .resolveImageView   = swapChainImageViews[imageIndex],
		    .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		    .loadOp             = vk::AttachmentLoadOp::eClear,
		    .storeOp            = vk::AttachmentStoreOp::eDontCare,
		    .clearValue         = clearColor};

		vk::RenderingAttachmentInfo depthAttachmentInfo = {
		    .imageView   = depthImageView,
		    .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
		    .loadOp      = vk::AttachmentLoadOp::eClear,
		    .storeOp     = vk::AttachmentStoreOp::eDontCare,
		    .clearValue  = clearDepth};

		vk::RenderingInfo renderingInfo = {
		    .renderArea           = {.offset = {0, 0}, .extent = swapChainExtent},
		    .layerCount           = 1,
		    .colorAttachmentCount = 1,
		    .pColorAttachments    = &colorAttachment,
		    .pDepthAttachment     = &depthAttachmentInfo};
		commandBuffer.beginRendering(renderingInfo);

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
		commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f));
		commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

		//commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *descriptorSets[frameIndex], nullptr);

		for (auto& meshItem : meshList) {
			renderMesh(meshItem, commandBuffer);
		}

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *particlePipeline);
		commandBuffer.bindVertexBuffers(0, *shaderStorageBuffers[frameIndex], {0});
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, computePipelineLayout, 0, *computeDescriptorSets[frameIndex], nullptr);
		commandBuffer.draw(PARTICLE_COUNT, 1, 0, 0);


		commandBuffer.endRendering();
		transition_image_layout(
		    imageIndex,
		    vk::ImageLayout::eColorAttachmentOptimal,
		    vk::ImageLayout::ePresentSrcKHR,
		    vk::AccessFlagBits2::eColorAttachmentWrite,
		    {},
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
		    vk::PipelineStageFlagBits2::eBottomOfPipe,
			vk::ImageAspectFlagBits::eColor);

		commandBuffer.end();
	}

	void createCommandBuffers()
	{
		vk::CommandBufferAllocateInfo allocInfo{
		    .commandPool        = commandPool,
		    .level              = vk::CommandBufferLevel::ePrimary,
		    .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
		commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
		computeCommandBuffers = vk::raii::CommandBuffers(device, allocInfo);

	}

	void createCommandPool()
	{
		vk::CommandPoolCreateInfo poolInfo{
		    .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		    .queueFamilyIndex = graphicsQueueFamilyIndex};
		commandPool = vk::raii::CommandPool(device, poolInfo);
	}


	
	void transition_image_layout(
	    uint32_t                imageIndex,
	    vk::ImageLayout         old_layout,
	    vk::ImageLayout         new_layout,
	    vk::AccessFlags2        src_access_mask,
	    vk::AccessFlags2        dst_access_mask,
	    vk::PipelineStageFlags2 src_stage_mask,
	    vk::PipelineStageFlags2 dst_stage_mask,
		vk::ImageAspectFlags image_aspect_flags)
	{
		vk::ImageMemoryBarrier2 barrier = {
		    .srcStageMask        = src_stage_mask,
		    .srcAccessMask       = src_access_mask,
		    .dstStageMask        = dst_stage_mask,
		    .dstAccessMask       = dst_access_mask,
		    .oldLayout           = old_layout,
		    .newLayout           = new_layout,
		    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .image               = swapChainImages[imageIndex],
		    .subresourceRange    = {
		           .aspectMask     = image_aspect_flags,
		           .baseMipLevel   = 0,
		           .levelCount     = 1,
		           .baseArrayLayer = 0,
		           .layerCount     = 1}};

		vk::DependencyInfo dependency_info = {
		    .dependencyFlags         = {},
		    .imageMemoryBarrierCount = 1,
		    .pImageMemoryBarriers    = &barrier};

		commandBuffers[frameIndex].pipelineBarrier2(dependency_info);
	}

	void transition_image_layout(
	    vk::Image               image,        // ← takes image directly
	    vk::ImageLayout         old_layout,
	    vk::ImageLayout         new_layout,
	    vk::AccessFlags2        src_access_mask,
	    vk::AccessFlags2        dst_access_mask,
	    vk::PipelineStageFlags2 src_stage_mask,
	    vk::PipelineStageFlags2 dst_stage_mask,
	    vk::ImageAspectFlags    image_aspect_flags)
	{
		vk::ImageMemoryBarrier2 barrier = {
		    .srcStageMask        = src_stage_mask,
		    .srcAccessMask       = src_access_mask,
		    .dstStageMask        = dst_stage_mask,
		    .dstAccessMask       = dst_access_mask,
		    .oldLayout           = old_layout,
		    .newLayout           = new_layout,
		    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .image               = image,        // ← use passed image
		    .subresourceRange    = {
		           .aspectMask     = image_aspect_flags,
		           .baseMipLevel   = 0,
		           .levelCount     = 1,
		           .baseArrayLayer = 0,
		           .layerCount     = 1}};

		vk::DependencyInfo dependency_info{
		    .imageMemoryBarrierCount = 1,
		    .pImageMemoryBarriers    = &barrier};

		commandBuffers[frameIndex].pipelineBarrier2(dependency_info);
	}



	static std::vector<char> readFile(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			throw std::runtime_error("failed to open file");
		}

		std::vector<char> buffer(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		file.close();

		return buffer;
	}

	void createGraphicsPipeline()
	{
		vk::raii::ShaderModule shaderModule = createShaderModule(readFile("shaders/slang.spv"));

		auto                              bindingDescription    = Vertex::getBindingDescription();
		auto                              attributeDescriptions = Vertex::getAttributeDescriptions();
		
		std::vector<vk::VertexInputAttributeDescription> attributeDescs(attributeDescriptions.begin(), attributeDescriptions.end());

		PipelineConfig config = {
			.vertEntry    = "vertMain",
			.fragEntry    = "fragMain",
			.shaderModule = &shaderModule,
			.layout       = &pipelineLayout,
			.bindingDesc  = bindingDescription,
			.attributeDescs = attributeDescs
		};

		graphicsPipeline = createPipeline(config);
	}


	void createParticlePipeline()
	{
		vk::raii::ShaderModule shaderModule = createShaderModule(readFile("shaders/compute.spv"));

		auto bindingDescription    = Particle::getBindingDescription();
		auto attributeDescriptions = Particle::getAttributeDescriptions();
		
		std::vector<vk::VertexInputAttributeDescription> attributeDescs(attributeDescriptions.begin(), attributeDescriptions.end());

		PipelineConfig config = {
		    .vertEntry      = "particleVertMain",
		    .fragEntry      = "particleFragMain",
		    .shaderModule   = &shaderModule,
		    .layout         = &computePipelineLayout,
		    .bindingDesc    = bindingDescription,
		    .attributeDescs = attributeDescs,
		    .topology       = vk::PrimitiveTopology::ePointList,
			.depthTest = true,
			.depthWrite = false
		};

		particlePipeline = createPipeline(config);
	}


	[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) const
	{
		vk::ShaderModuleCreateInfo createInfo{
		    .codeSize = code.size() * sizeof(char),
		    .pCode    = reinterpret_cast<const uint32_t *>(code.data())};

		vk::raii::ShaderModule shaderModule{device, createInfo};
		return shaderModule;
	}


	void createImageViews()
	{
		assert(swapChainImageViews.empty());
		swapChainImageViews.reserve(swapChainImages.size());
		for (auto &image : swapChainImages)
		{
			swapChainImageViews.emplace_back(createImageView(image, swapChainSurfaceFormat.format, vk::ImageAspectFlagBits::eColor, 1));
		}
	}

	void createSwapChain()
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

		// what color format should pixels use?
		vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		// when frames should be swapped?
		vk::PresentModeKHR   presentMode   = chooseSwapPresentMode(swapChainSupport.presentMode);
		// how many pxels should the swapchain images use?
		vk::Extent2D         extent        = chooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;


		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}
		 
		vk::SwapchainCreateInfoKHR createInfo
		{
			.surface          = *surface,
			.minImageCount    = imageCount,
			.imageFormat      = surfaceFormat.format,
			.imageColorSpace  = surfaceFormat.colorSpace,
			.imageExtent      = extent,
			.imageArrayLayers = 1,
			.imageUsage = vk::ImageUsageFlagBits::eColorAttachment
		};

		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t           queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices   = queueFamilyIndices;

		}
		else
		{
			createInfo.imageSharingMode = vk::SharingMode::eExclusive;
			createInfo.queueFamilyIndexCount = 0; // optional
			createInfo.pQueueFamilyIndices   = nullptr; // optional  
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
		createInfo.presentMode    = presentMode;
		createInfo.clipped        = vk::True;
		createInfo.oldSwapchain   = nullptr;

		swapChain = vk::raii::SwapchainKHR(device, createInfo);
		swapChainImages = swapChain.getImages();

		swapChainSurfaceFormat = surfaceFormat;
		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent      = extent;


	}

	bool isDeviceSuitable(vk::raii::PhysicalDevice &device)
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

	bool checkDeviceExtensionSupport(vk::raii ::PhysicalDevice& device)
	{
		auto availableExtensions = device.enumerateDeviceExtensionProperties();

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
		for (const auto &extension : availableExtensions)
		{
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	SwapChainSupportDetails querySwapChainSupport(vk::raii::PhysicalDevice &device)
	{
		SwapChainSupportDetails details;

		details.capabilities = device.getSurfaceCapabilitiesKHR(*surface);
		details.formats      = device.getSurfaceFormatsKHR(*surface);
		details.presentMode  = device.getSurfacePresentModesKHR(*surface);

		return details;
	}

	vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
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

	vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
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

	vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);


			vk::Extent2D actualExtent = {
			    static_cast<uint32_t>(width),
			    static_cast<uint32_t>(height)};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}


	void createSurface()
	{
		VkSurfaceKHR rawSurface;
		if (glfwCreateWindowSurface(*instance, window, nullptr, &rawSurface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a window surface!");
		}
		surface = vk::raii::SurfaceKHR(instance, rawSurface);
	}

	void createLogicalDevice()
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

		vk::PhysicalDeviceFeatures deviceFeatures{};


		vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
		    {.features = {.samplerAnisotropy = true}},                   // vk::PhysicalDeviceFeatures2
		    {.synchronization2 = true, .dynamicRendering = true},        // vk::PhysicalDeviceVulkan13Features
		    {.extendedDynamicState = true}                               // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
		};


		vk::DeviceCreateInfo createInfo
		{
			.pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
			.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
			.ppEnabledExtensionNames = deviceExtensions.data(),
		};


		device        = vk::raii::Device(physicalDevice, createInfo);
		graphicsQueue = device.getQueue(indices.graphicsFamily.value(), 0);
		presentQueue = device.getQueue(indices.presentFamily.value(), 0);
		computeQueue  = device.getQueue(indices.graphicsFamily.value(), 0);

	}


	QueueFamilyIndices findQueueFamilies(vk::raii::PhysicalDevice& device)
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




	void pickPhysicalDevice()
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



	void setupDebugMessenger()
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

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			drawFrame();
		}
		device.waitIdle();
	}

	void drawFrame()
	{

		auto computeFenceResult = device.waitForFences(*computeInFlightFences[frameIndex], vk::True, UINT64_MAX);
		if (computeFenceResult  != vk::Result::eSuccess) {
			throw std::runtime_error("failed to wait for fence!");
		}

		device.resetFences(*computeInFlightFences[frameIndex]);
		computeCommandBuffers[frameIndex].reset();
		recordComputeCommandBuffers(computeCommandBuffers[frameIndex]);

		const vk::SubmitInfo computeSubmitInfo{
			.waitSemaphoreCount = 0,
			.pWaitSemaphores = nullptr,
			.pWaitDstStageMask = nullptr,
			.commandBufferCount = 1,
			.pCommandBuffers = &*computeCommandBuffers[frameIndex],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &*computeFinishedSemaphores[frameIndex]
		};

		computeQueue.submit(computeSubmitInfo, *computeInFlightFences[frameIndex]);

		auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
		if (fenceResult != vk::Result::eSuccess)
		{
			throw std::runtime_error("failed to wait for fence!");
		}

		auto [result, imageIndex] = swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[semaphoreIndex], nullptr);

		if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized)
		{
			framebufferResized = false;
			recreateSwapChain();
			return;
		}
		else
		{
			assert(result == vk::Result::eSuccess);
		}

		if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
		{
			assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
			throw std::runtime_error("failed to acquire the swap chain image!");
		}

		device.resetFences(*inFlightFences[frameIndex]);

		recordCommandBuffer(imageIndex, commandBuffers[frameIndex]);

		updateUniformBuffer(frameIndex);

		//vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);

		std::array waitSemaphores{
		    *computeFinishedSemaphores[frameIndex],
		    *presentCompleteSemaphores[semaphoreIndex]
		};

		std::array<vk::PipelineStageFlags, 2> waitStages{vk::PipelineStageFlagBits::eVertexInput,
		                      vk::PipelineStageFlagBits::eColorAttachmentOutput};


		const vk::SubmitInfo submitInfo{
		    .waitSemaphoreCount   = 2,
		    .pWaitSemaphores      = waitSemaphores.data(),
		    .pWaitDstStageMask    = waitStages.data(),
		    .commandBufferCount   = 1,
		    .pCommandBuffers      = &*commandBuffers[frameIndex],
		    .signalSemaphoreCount = 1,
		    .pSignalSemaphores    = &*renderFinishedSemaphores[semaphoreIndex]};

		graphicsQueue.submit(submitInfo, *inFlightFences[frameIndex]);

		const vk::PresentInfoKHR presentInfoKHR{
		    .waitSemaphoreCount = 1,
		    .pWaitSemaphores    = &*renderFinishedSemaphores[semaphoreIndex],
		    .swapchainCount     = 1,
		    .pSwapchains        = &*swapChain,
		    .pImageIndices      = &imageIndex};

		result               = graphicsQueue.presentKHR(presentInfoKHR);

		if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR))
		{
			recreateSwapChain();
		}
		else
		{
			assert(result == vk::Result::eSuccess);
		}


		frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
		semaphoreIndex = (semaphoreIndex + 1) % swapChainImages.size();

	}


	void cleanup()
	{
		cleanupSwapChain();
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void createInstance()
	{
		constexpr vk::ApplicationInfo appInfo{
		    .pApplicationName   = "Hello Triangle",
		    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		    .pEngineName        = "No Engine",
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


	std::vector<const char*> getRequiredInstanceExtensions()
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
	
	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
		vk::DebugUtilsMessageTypeFlagsEXT type,
		const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		std::cerr << "Validation layer: type " << to_string(type) << "msg: " << pCallbackData->pMessage << std::endl;
		return vk::False;
	}
};

int main()
{
	try
	{
		HelloTriangleApplication app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
