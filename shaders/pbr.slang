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
struct InstanceTransform
{
    float4x4 model;
    float4x4 normalMatrix;
};
struct InstanceConstants
{
    InstanceTransform transforms[100];
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
    // w: emissive blend flow, normalized phase(low16), amount(high16); zero disables.
    uint4 flags; // x: alpha; y: UV1 bits; z: low2 normal/facing, high16 stable flow seed.
};
[[vk::binding(0, 0)]] ConstantBuffer<FrameConstants> frame;
[[vk::binding(1, 0)]] ConstantBuffer<LightConstants> localLights;
[[vk::binding(2, 0)]] Texture2D<float> shadowMap;
[[vk::binding(3, 0)]] SamplerComparisonState shadowSampler;
[[vk::binding(4, 0)]] ConstantBuffer<InstanceConstants> instances;
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
Varying vertexMain(Vertex input, uint instanceId : SV_VulkanInstanceID)
{
    Varying output;
    InstanceTransform instance = instances.transforms[instanceId];
    float4x4 model = mul(frame.model, instance.model);
    float4 world = mul(model, float4(input.position, 1));
    output.position = mul(frame.viewProjection, world);
    output.worldPosition = world.xyz;
    output.normal = mul((float3x3)frame.normalMatrix, mul((float3x3)instance.normalMatrix, input.normal));
    float handedness = determinant((float3x3)model) < 0 ? -1.0 : 1.0;
    output.tangent = float4(mul((float3x3)model, input.tangent.xyz), input.tangent.w * handedness);
    output.uv = input.uv;
    output.uv1 = input.uv1;
    output.shadowPosition = mul(frame.lightViewProjection, world);
    return output;
}

struct ShadowVarying
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};
[shader("vertex")]
ShadowVarying shadowVertex(Vertex input, uint instanceId : SV_VulkanInstanceID)
{
    ShadowVarying output;
    float4 world = mul(frame.model, mul(instances.transforms[instanceId].model, float4(input.position, 1)));
    output.position = mul(frame.lightViewProjection, world);
    output.uv = input.uv;
    output.uv1 = input.uv1;
    return output;
}
[shader("fragment")]
void shadowFragment(ShadowVarying input)
{
    if (material.flags.x == 1)
    {
        float2 uv = (material.flags.y & 1u) != 0 ? input.uv1 : input.uv;
        float alpha = baseColorMap.Sample(materialSamplers[0], uv).a * material.baseColor.a;
        if (alpha < material.emissiveAlphaCutoff.w)
            discard;
    }
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
float jetHash(int2 cell, uint mask)
{
    uint2 wrapped = uint2(cell) & uint2(mask, mask);
    uint bits = wrapped.x * 0x8da6b343u ^ wrapped.y * 0xd8163841u;
    bits ^= (material.flags.z >> 16u) * 0x9e3779b9u;
    bits ^= bits >> 13u;
    bits *= 0x85ebca6bu;
    bits ^= bits >> 16u;
    return float(bits >> 8u) / 16777216.0;
}
float jetNoise(float2 coordinate, uint mask)
{
    int2 cell = int2(floor(coordinate));
    float2 f = frac(coordinate);
    float2 blend = f * f * (3 - 2 * f);
    float lower = lerp(jetHash(cell, mask), jetHash(cell + int2(1, 0), mask), blend.x);
    float upper = lerp(jetHash(cell + int2(0, 1), mask), jetHash(cell + int2(1, 1), mask), blend.x);
    return lerp(lower, upper, blend.y);
}
float jetField(float2 coordinate)
{
    return 0.60 * jetNoise(coordinate, 511u)
         + 0.25 * jetNoise(coordinate * 2 + float2(11, 19), 511u)
         + 0.15 * jetNoise(coordinate * 0.125 + float2(2, 5), 63u);
}
// UV0.u guides aft flow. A 512-cell cycle gives a long, continuous loop rather
// than repeating transverse sine bands. Anisotropic density makes longitudinal
// filaments. Four trailing samples soften subframe motion without erasing it.
// The workload traverses this cycle in 16 seconds; the hot base stays anchored.
float2 emissiveFlow(float2 guide)
{
    if (material.flags.w == 0)
        return float2(0, 1);
    // Packages may contain finite tiled/large UVs. Bound only this normalized
    // guide before multiplying or converting lattice coordinates to integers.
    if (any(guide < 0) || any(guide > 1))
        guide = saturate(guide);
    float phase = float(material.flags.w & 65535u) / 65536.0;
    float amount = float(material.flags.w >> 16u) / 65535.0;
    float tail = smoothstep(0.12, 0.28, guide.x) * amount;
    float advected = guide.x * 3 - phase * 512;
    float field = 0;
    [unroll]
    for (uint sample = 0; sample < 4; ++sample)
        field += jetField(float2(advected + float(sample) * 0.2, guide.y * 24)) * 0.25;
    float density = 1 + tail * 0.9 * (field - 0.5);
    float skin = tail * smoothstep(0.5, 0.95, abs(guide.y * 2 - 1));
    float tip = amount * smoothstep(0.60, 0.93, guide.x);
    density *= lerp(1.0, smoothstep(0.15, 0.75, field), max(skin, tip));
    // Texture alpha alone cannot guarantee a soft end: coarse/oblique mips
    // average nonzero alpha across the card. Enforce the aft envelope analytically.
    density *= lerp(1.0, 1 - smoothstep(0.68, 0.995, guide.x), amount);
    float transverse = tail * 0.04 * (jetNoise(float2(advected * 0.125, guide.y * 3), 63u) - 0.5);
    return float2(transverse, density);
}
[shader("fragment")]
float4 fragmentMain(Varying input, bool frontFace : SV_IsFrontFace) : SV_Target0
{
    float2 flow = emissiveFlow(input.uv);
    float2 flowOffset = float2(0, flow.x);
    // Color/emissive views decode sRGB; all other texture inputs are linear.
    float4 albedo = baseColorMap.Sample(materialSamplers[0], textureUv(input, 0) + flowOffset) * material.baseColor;
    if (material.flags.w != 0)
        albedo.a = saturate(albedo.a * flow.y);
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
    color += emissiveMap.Sample(materialSamplers[4], textureUv(input, 4) + flowOffset).rgb * material.emissiveAlphaCutoff.rgb * flow.y;
    return float4(color, albedo.a); // Linear HDR; tone mapping belongs to the post pass.
}
