struct FrameConstants
{
    float4x4 viewProjection;
    float4x4 model;
    float4x4 normalMatrix;
    float4x4 lightViewProjection;
    float4 cameraPosition;
    float4 keyDirectionIntensity;
    float4 keyColor;
};
struct LocalLight
{
    float4 positionRange;
    float4 colorIntensity;
};
struct LightConstants
{
    LocalLight lights[16];
    uint count;
};
struct MaterialConstants
{
    float4 baseColor;
    float4 emissiveAlphaCutoff;
    float4 metallicRoughnessNormalOcclusion;
    uint4 flags; // x: alpha mode; y: per-map UV1 bits; z bits: no normal texture, double sided; w reserved.
};
[[vk::binding(0, 0)]] ConstantBuffer<FrameConstants> frame;
[[vk::binding(1, 0)]] ConstantBuffer<LightConstants> localLights;
[[vk::binding(2, 0)]] Texture2D<float> shadowMap;
[[vk::binding(3, 0)]] SamplerComparisonState shadowSampler;
[[vk::binding(0, 1)]] ConstantBuffer<MaterialConstants> material;
[[vk::binding(1, 1)]] Texture2D<float4> baseColorMap;
[[vk::binding(2, 1)]] Texture2D<float4> normalMap;
[[vk::binding(3, 1)]] Texture2D<float4> metallicRoughnessMap;
[[vk::binding(4, 1)]] Texture2D<float4> occlusionMap;
[[vk::binding(5, 1)]] Texture2D<float4> emissiveMap;
[[vk::binding(6, 1)]] SamplerState materialSamplers[5];
[[vk::binding(7, 1)]] TextureCube<float4> irradianceMap;
[[vk::binding(8, 1)]] TextureCube<float4> prefilteredMap;
[[vk::binding(9, 1)]] Texture2D<float2> brdfMap;
[[vk::binding(10, 1)]] SamplerState environmentSampler;

struct Vertex
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
    [[vk::location(4)]] float2 uv1 : TEXCOORD1;
    [[vk::location(3)]] float4 tangent : TANGENT;
};
struct Varying
{
    float4 position : SV_Position;
    float3 normal : TEXCOORD0;
    float3 worldPosition : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float4 tangent : TEXCOORD3;
    float4 shadowPosition : TEXCOORD4;
    float2 uv1 : TEXCOORD5;
};
[shader("vertex")]
Varying vertexMain(Vertex input)
{
    Varying output;
    float4 world = mul(frame.model, float4(input.position, 1));
    output.position = mul(frame.viewProjection, world);
    output.worldPosition = world.xyz;
    output.normal = mul((float3x3)frame.normalMatrix, input.normal);
    float handedness = determinant((float3x3)frame.model) < 0 ? -1.0 : 1.0;
    output.tangent = float4(mul((float3x3)frame.model, input.tangent.xyz), input.tangent.w * handedness);
    output.uv = input.uv;
    output.uv1 = input.uv1;
    output.shadowPosition = mul(frame.lightViewProjection, world);
    return output;
}

static const float PI = 3.141592653589793;
float3 fresnel(float cosine, float3 f0)
{
    return f0 + (1 - f0) * pow(1 - saturate(cosine), 5.0);
}
float3 directLight(float3 n, float3 v, float3 l, float3 albedo, float metal, float rough, float3 radiance)
{
    float3 h = normalize(v + l);
    float nv = max(dot(n, v), 0.0001);
    float nl = saturate(dot(n, l));
    float nh = saturate(dot(n, h));
    float a = rough * rough;
    float a2 = a * a;
    float d = nh * nh * (a2 - 1) + 1;
    float distribution = a2 / max(PI * d * d, 0.000001);
    float k = (rough + 1) * (rough + 1) / 8;
    float geometry = nv / (nv * (1 - k) + k) * nl / (nl * (1 - k) + k);
    float3 f = fresnel(dot(h, v), lerp(float3(0.04), albedo, metal));
    float3 specular = distribution * geometry * f / max(4 * nv * nl, 0.0001);
    return ((1 - f) * (1 - metal) * albedo / PI + specular) * radiance * nl;
}
float shadowVisibility(float4 position)
{
    if (position.w <= 0)
        return 1;
    float3 ndc = position.xyz / position.w;
    float2 uv = ndc.xy * 0.5 + 0.5;
    if (any(uv < 0) || any(uv > 1) || ndc.z < 0 || ndc.z > 1)
        return 1;
    return shadowMap.SampleCmpLevelZero(shadowSampler, uv, ndc.z - 0.001);
}
float2 textureUv(Varying input, uint slot)
{
    return (material.flags.y & (1u << slot)) != 0 ? input.uv1 : input.uv;
}
[shader("fragment")]
float4 fragmentMain(Varying input, bool frontFace : SV_IsFrontFace) : SV_Target0
{
    // Color/emissive views decode sRGB; all other texture inputs are linear.
    float4 albedo = baseColorMap.Sample(materialSamplers[0], textureUv(input, 0)) * material.baseColor;
    if (material.flags.x == 1 && albedo.a < material.emissiveAlphaCutoff.w)
        discard;
    float3 n = normalize(input.normal);
    if ((material.flags.z & 1u) == 0)
    {
        float3 t = normalize(input.tangent.xyz - n * dot(n, input.tangent.xyz));
        float3 b = cross(n, t) * input.tangent.w;
        float3 sampled = normalMap.Sample(materialSamplers[2], textureUv(input, 2)).xyz * 2 - 1;
        sampled.xy *= material.metallicRoughnessNormalOcclusion.z;
        n = normalize(t * sampled.x + b * sampled.y + n * sampled.z);
    }
    if ((material.flags.z & 2u) != 0 && !frontFace)
        n = -n;
    float3 v = normalize(frame.cameraPosition.xyz - input.worldPosition);
    float4 mr = metallicRoughnessMap.Sample(materialSamplers[1], textureUv(input, 1));
    float metal = saturate(mr.b * material.metallicRoughnessNormalOcclusion.x);
    float rough = clamp(mr.g * material.metallicRoughnessNormalOcclusion.y, 0.045, 1.0);
    float3 key = normalize(-frame.keyDirectionIntensity.xyz);
    float3 color = directLight(n, v, key, albedo.rgb, metal, rough,
                              frame.keyColor.rgb * frame.keyDirectionIntensity.w) * shadowVisibility(input.shadowPosition);
    for (uint i = 0; i < min(localLights.count, 16u); ++i)
    {
        LocalLight light = localLights.lights[i];
        float3 delta = light.positionRange.xyz - input.worldPosition;
        float distanceSquared = max(dot(delta, delta), 0.0001);
        float attenuation = 1.0 / distanceSquared;
        if (light.positionRange.w > 0)
        {
            float ratio = sqrt(distanceSquared) / light.positionRange.w;
            float fade = saturate(1 - ratio * ratio * ratio * ratio);
            attenuation *= fade * fade;
        }
        color += directLight(n, v, delta * rsqrt(distanceSquared), albedo.rgb, metal, rough,
                             light.colorIntensity.rgb * light.colorIntensity.w * attenuation);
    }
    float nv = saturate(dot(n, v));
    float3 f = fresnel(nv, lerp(float3(0.04), albedo.rgb, metal));
    float3 diffuse = irradianceMap.SampleLevel(environmentSampler, n, 0).rgb * albedo.rgb / PI;
    float3 reflected = reflect(-v, n);
    float3 prefiltered = prefilteredMap.SampleLevel(environmentSampler, reflected, rough * 4).rgb;
    float2 brdf = brdfMap.SampleLevel(environmentSampler, float2(nv, rough), 0);
    float occlusion = lerp(1.0, occlusionMap.Sample(materialSamplers[3], textureUv(input, 3)).r,
                           material.metallicRoughnessNormalOcclusion.w);
    color += ((1 - f) * (1 - metal) * diffuse + prefiltered * (f * brdf.x + brdf.y)) * occlusion;
    color += emissiveMap.Sample(materialSamplers[4], textureUv(input, 4)).rgb * material.emissiveAlphaCutoff.rgb;
    return float4(color, albedo.a); // Linear HDR; tone mapping belongs to the post pass.
}
