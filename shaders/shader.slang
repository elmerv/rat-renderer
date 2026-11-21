
#include "common.slang"
[[vk::binding(0,0)]] ConstantBuffer<UniformBuffer> ubo;

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
static const float shininess = 32.0;
static const float ambientStrength = 0.2;
static const float specularStrength = 0.8;


float3 shadeLight(Light l, float3 worldPos, float3 N, float3 V, float3 albedo){
    float3 lightDir;
    float attenuation = 1.0;


    if (l.type == LightType.Directional){
       lightDir = normalize(-l.direction);
    } else {
        float3 toLight = l.position - worldPos;
        float d = length(toLight);

        lightDir = toLight / d;
        attenuation = 1.0 / (1.0 + 0.09 * d + 0.032 * d * d);

        if(l.type == LightType.Spot) {
            float theta = dot(-lightDir, normalize(l.direction));
            attenuation *= smoothstep(l.outerCone, l.innerCone, theta);
        }
    }

    float3 halfwayDir = normalize(lightDir + V);


    float3 reflectedViewDir = reflect(-lightDir, N);
    float nDotH = pow(max(dot(N, halfwayDir), 0.0), shininess);

    float nDotL = max(dot(N, lightDir), 0.0);

    float3 diffuse = (albedo / PI) * nDotL;

    float3 specular = specColor * ((8 + shininess) / (8.0 * PI)) * nDotH * nDotL;

    return (diffuse + specular) * l.color * l.intensity * attenuation;
}


struct VSInput {
    float3 inPosition;
    float3 inColor;
    float2 inTexCoord;
    float3 normal;
};


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

[[vk::binding(1,0)]] Sampler2D texture;
[[vk::binding(2,0)]] Sampler2DShadow shadowMap;


float shadowFactor(float3 worldPos){
    float4 position = mul(ubo.lightViewProj, float4(worldPos, 1.0));
    if(position.w <= 0) {
        return 1.0;
    }
    float3 ndc = position.xyz / position.w;
    if(ndc.z > 1){
        return 1.0;
    }

    float2 normalized_ndc = (ndc.xy * 0.5) + 0.5; 
    
    return shadowMap.SampleCmp( normalized_ndc, ndc.z);
    
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    

    float3 texColor = texture.Sample(vertIn.fragTexCoord).rgb;

    float3 N = normalize(vertIn.fragNormal);


    // float3 lightDir = normalize(ubo.lightPosition - vertIn.worldPos);
    float3 viewDir = normalize(ubo.cameraPosition - vertIn.worldPos);


    float3 color = ambientStrength * texColor;

    for(int i = 0; i < ubo.lightCount; i++){
        float shadow = 1.0f;
        if( i == ubo.shadowLightIndex){
            shadow = shadowFactor(vertIn.worldPos);
        }
        color += shadeLight(ubo.lights[i], vertIn.worldPos, N, viewDir, texColor) * shadow;
        
    }

    
    return float4(color, 1.0);
}


[shader("vertex")] VSOutput shadowVertMain(VSInput input){
    VSOutput output;
    float4 worldPos = mul(push.model, float4(input.inPosition, 1.0));
    output.pos = mul(ubo.lightViewProj, worldPos);
    return output;
}