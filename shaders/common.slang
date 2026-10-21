// Shared by shader.slang and compute.slang.
// Must match UniformBufferObject in src/GpuTypes.h.

static const int MAX_LIGHTS = 20;

enum LightType : uint {
	Directional = 0,
	Point = 1,
	Spot = 2,
};


struct Light {
	float3 position;
	float innerCone;
	float3 color;
	float intensity;
	float3 direction;
	float outerCone;
	LightType type;
};	

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float3 cameraPosition;
    float deltaTime;
    int lightCount;
    Light lights[MAX_LIGHTS];
};
