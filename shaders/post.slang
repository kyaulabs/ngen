struct PostConstants
{
    float4 exposureFxaaTexel; // Linear exposure multiplier, AA toggle, reciprocal source extent.
};
[[vk::push_constant]] ConstantBuffer<PostConstants> post;
[[vk::binding(0, 0)]] Texture2D<float4> hdrImage;
[[vk::binding(1, 0)]] SamplerState hdrSampler;
struct PostVarying
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};
[shader("vertex")]
PostVarying postVertex(uint id : SV_VulkanVertexID)
{
    PostVarying output;
    output.uv = float2(float((id << 1) & 2), float(id & 2));
    output.position = float4(output.uv * 2 - 1, 0, 1);
    return output;
}
float3 mapped(float2 uv)
{
    float3 x = max(hdrImage.SampleLevel(hdrSampler, uv, 0).rgb * post.exposureFxaaTexel.x, 0);
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}
float luminance(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}
[shader("fragment")]
float4 postFragment(PostVarying input) : SV_Target0
{
    float3 center = mapped(input.uv);
    if (post.exposureFxaaTexel.y < 0.5)
        return float4(center, 1);
    float2 texel = post.exposureFxaaTexel.zw;
    float nw = luminance(mapped(input.uv + float2(-1, -1) * texel));
    float ne = luminance(mapped(input.uv + float2(1, -1) * texel));
    float sw = luminance(mapped(input.uv + float2(-1, 1) * texel));
    float se = luminance(mapped(input.uv + float2(1, 1) * texel));
    float mid = luminance(center);
    float low = min(mid, min(min(nw, ne), min(sw, se)));
    float high = max(mid, max(max(nw, ne), max(sw, se)));
    if (high - low < max(0.0312, high * 0.125))
        return float4(center, 1);
    float2 direction = float2(-((nw + ne) - (sw + se)), (nw + sw) - (ne + se));
    float reduce = max((nw + ne + sw + se) * 0.03125, 0.0078125);
    direction = clamp(direction / (min(abs(direction.x), abs(direction.y)) + reduce), -8, 8) * texel;
    float3 a = 0.5 * (mapped(input.uv + direction * (-1.0 / 6.0)) + mapped(input.uv + direction * (1.0 / 6.0)));
    float3 b = a * 0.5 + 0.25 * (mapped(input.uv + direction * -0.5) + mapped(input.uv + direction * 0.5));
    float lb = luminance(b);
    return float4(lb < low || lb > high ? a : b, 1); // Linear LDR; sRGB attachment encodes once.
}
