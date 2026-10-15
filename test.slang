import common;

struct LightInfo {
    float3 position;   // 12 bytes (+ 4B padding in std140/uniform)
    float intensity;   // 4 bytes
    float4 color;      // 16 bytes
};

struct MaterialData {
    float4 albedo;
    float roughness;
    float metallic;
    
    // (A) Single Struct
    LightInfo mainLight;
    
    // (B) Array of Struct
    LightInfo extraLights[2];
};

// Vulkan 1.4 BDA (Buffer Device Address) Test
struct RawVertex {
    float3 position;
    float2 uv;
};

// Push Constant
struct PushConstants {
    float4x4 modelMatrix;
    
    // BDA Pointer
    RawVertex* vertexBufferPtr; 
    
    uint materialCount;
    float globalAlpha;
};

// (1) Push Constant
[vk::push_constant] 
PushConstants gPushConstants;

// (2) Uniform Buffer - Set 0, Binding 0
[[vk::binding(0, 0)]] 
ConstantBuffer<MaterialData> gMaterialBuffer;

// (3) Sampled Texture & Sampler State - Set 0, Binding 1 & 2
[[vk::binding(1, 0)]] 
Texture2D gAlbedoMap;

[[vk::binding(2, 0)]] 
SamplerState gLinearSampler;

// (4) Storage Image (RWTexture) - Set 1, Binding 0 (MEMBER_KIND_STORAGE_TEXTURE)
[[vk::binding(0, 1)]] 
RWTexture2D<float4> gOutputImage;


[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{
    // BDA 포인터 디레퍼런싱 테스트
    if (gPushConstants.vertexBufferPtr != nullptr) {
        RawVertex v = gPushConstants.vertexBufferPtr[threadId.x];
    }

    // 텍스처 샘플링 & Storage Image 쓰기
    float2 uv = float2(threadId.xy) / 1024.0;
    float4 sampledColor = gAlbedoMap.Sample(gLinearSampler, uv);
    
    float4 finalColor = sampledColor * gMaterialBuffer.mainLight.color * gPushConstants.globalAlpha;
    
    gOutputImage[threadId.xy] = finalColor;
}
