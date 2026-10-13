// Shared by shader.slang and compute.slang.
// Must match UniformBufferObject in src/GpuTypes.h.

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float3 lightPosition;
    float deltaTime;
    float3 cameraPosition;
};
