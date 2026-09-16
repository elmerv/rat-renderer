struct Particle {
	float2 position;
	float2 velocity;
    float4 color;
};

struct UniformBuffer {
    float deltaTime;
};
ConstantBuffer<UniformBuffer> ubo;

struct ParticleSSBO {
    Particle particles;
};
StructuredBuffer<ParticleSSBO> particlesIn;
RWStructuredBuffer<ParticleSSBO> particlesOut;

struct VSOutput {
	float3 color;
	float4 pos: SV_Position;
};


[shader("vertex")]
VSOutput vertMain(VSInput input){
	VSOutput output;

	// output.pos = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 1.0))));

	output.pos = float4(input.inPosition, 0.0, 1.0);
	output.color = input.inColor;
	// output.fragTexCoord = input.inTexCoord;
	return output;
}

[shader("fragment")]

float4 fragMain(VSOutput vertIn) : SV_Target {
	// float3 color = vertIn.color;
	// return float4(vertIn.fragTexCoord, 0.0, 1.0);
	
	// return texture.Sample(vertIn.fragTexCoord * 2.0);
	
	return float4(vertIn.color * texture.Sample(vertIn.fragTexCoord).rgb, 1.0);
}
