
struct PushConstants
{
    float4x4 model;
    float4x4 normalMatrix;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> push;

static float3 lightColor = float3(0.56, 0.77, 0.88);
static float3 specColor = float3(0.56, 0.77, 0.88);
static const float PI = 3.14159265359;

struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
    float3 normal;
};

struct UniformBuffer {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float3 lightPosition;
    float deltaTime;
    float3 cameraPosition;
};
ConstantBuffer<UniformBuffer> ubo;

struct VSOutput
{
    float4 pos : SV_Position;
    float3 fragColor;
    float2 fragTexCoord;
    float3 fragNormal;
    float3 worldPos;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    float4 worldPos = mul(push.model, float4(input.inPosition, 1.0));
    output.worldPos = worldPos.xyz;
    output.pos = mul(ubo.proj, mul(ubo.view, worldPos));
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    output.fragNormal = normalize(mul(float3x3(push.normalMatrix), input.normal));
    return output;
}

Sampler2D texture;

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    
    int shininess = 32;
    float ambientStrength = 0.2;
    float specularStrength = 0.8;

    float3 texColor = texture.Sample(vertIn.fragTexCoord).rgb;

    float3 N = normalize(vertIn.fragNormal);

    float3 lightDir = normalize(ubo.lightPosition - vertIn.worldPos);
    float3 viewDir = normalize(ubo.cameraPosition - vertIn.worldPos);
    float3 halfwayDir = normalize(lightDir + viewDir);


    float distanceToLight = length(lightDir);
    float attenuation = 1.0 / (1.0 + 0.09 * distanceToLight + 0.032 * distanceToLight * distanceToLight);


    float3 reflectedViewDir = reflect(-lightDir, N);
    float nDotH = pow(max(dot(N, halfwayDir), 0.0), shininess);


    float nDotL = max(dot(N, lightDir), 0.0);

    // float3 diffuse = nDotL * lightColor;
    float3 diffuse = (texColor / PI) * nDotL;

    // float spec = pow(max(dot(viewDir, reflectedViewDir), 0.0), 32);
    // float3 specular = specularStrength * nDotH * lightColor; 
    float3 specular = specColor * ((8 + shininess) / (8.0 * PI)) * nDotH * nDotL;

    float3 ambient = ambientStrength * lightColor * texColor;



    float3 color    = ambient + attenuation * lightColor * (diffuse + specular);
    return float4(color, 1.0);

    // return float4((ambient + diffuse) * texColor + specular, 1.0);
    // return float4(vertIn.fragNormal, 1.0);
}