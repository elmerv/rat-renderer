struct Particle {
    float3 position;
    float3 velocity;
    float4 color;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float deltaTime;
};

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBuffer> ubo;

[[vk::binding(1, 0)]]
StructuredBuffer<Particle> particlesIn;

[[vk::binding(2, 0)]]
RWStructuredBuffer<Particle> particlesOut;

[shader("compute")]
[numthreads(256, 1, 1)]
void compMain(uint3 threadId : SV_DispatchThreadID)
{
    uint index = threadId.x;

    Particle p;
    p.position = particlesIn[index].position
               + particlesIn[index].velocity * ubo.deltaTime;
    p.velocity = particlesIn[index].velocity;
    p.color    = particlesIn[index].color;

    if (p.position.x <= -1.0 || p.position.x >= 1.0)
        p.velocity.x = -p.velocity.x;
    if (p.position.y <= -1.0 || p.position.y >= 1.0)
        p.velocity.y = -p.velocity.y;
     if (p.position.z <= -1.0 || p.position.z >= 1.0)
        p.velocity.z = -p.velocity.z;   
    particlesOut[index] = p;
}

struct VSOutput {
    float4 position : SV_Position;
    float4 color;
};

[shader("vertex")]
VSOutput particleVertMain(
    [[vk::location(0)]] float3 inPosition : POSITION,
    [[vk::location(1)]] float4 inColor    : COLOR)
{
    VSOutput output;
    output.position = float4(inPosition, 1.0);  // ? z=0, w=1


    // output.position = float4(0.0, 0.0, 0.0, 1.0);

    // output.position = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(inPosition, 1.0))));

    output.color    = inColor;
    return output;
}

[shader("fragment")]
float4 particleFragMain(VSOutput input) : SV_Target
{
    // return input.color;
    return float4(1.0, 0.0, 0.0, 1.0);  // ← force bright red

}