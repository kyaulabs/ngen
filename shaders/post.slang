struct PostConstants
{
    float4 exposureFxaaTexel; // Linear exposure multiplier, AA toggle, reciprocal source extent.
    float4 bloom; // Restrained linear HDR strength; fixed threshold of one.
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
float3 bloomRadiance(float2 uv)
{
    if (post.bloom.x <= 0)
        return float3(0);
    // Normalized Gaussian weights with a 9x9 footprint and 25 bilinear taps.
    // Threshold reconstructed linear HDR; no scratch images or extra ownership.
    const float offsets[5] = {-3.2307692308, -1.3846153846, 0, 1.3846153846, 3.2307692308};
    const float weights[5] = {0.0702702703, 0.3162162162, 0.2270270270, 0.3162162162, 0.0702702703};
    float3 light = float3(0);
    [unroll] for (int y = 0; y < 5; ++y)
        [unroll] for (int x = 0; x < 5; ++x)
        {
            float2 at = uv + float2(offsets[x], offsets[y]) * post.exposureFxaaTexel.zw;
            light += max(hdrImage.SampleLevel(hdrSampler, at, 0).rgb - 1, 0) * weights[x] * weights[y];
        }
    return light * post.bloom.x;
}
float3 mapped(float2 uv, float3 bloom)
{
    float3 x = max((hdrImage.SampleLevel(hdrSampler, uv, 0).rgb + bloom) * post.exposureFxaaTexel.x, 0);
    return saturate((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14));
}
float luminance(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}
[shader("fragment")]
float4 postFragment(PostVarying input) : SV_Target0
{
    // Reuse the smooth local bloom estimate across FXAA's subpixel taps.
    // GPU post timing includes both fused operations; there is no separate bloom pass.
    float3 bloom = bloomRadiance(input.uv);
    float3 center = mapped(input.uv, bloom);
    if (post.exposureFxaaTexel.y < 0.5)
        return float4(center, 1);
    float2 texel = post.exposureFxaaTexel.zw;
    float nw = luminance(mapped(input.uv + float2(-1, -1) * texel, bloom));
    float ne = luminance(mapped(input.uv + float2(1, -1) * texel, bloom));
    float sw = luminance(mapped(input.uv + float2(-1, 1) * texel, bloom));
    float se = luminance(mapped(input.uv + float2(1, 1) * texel, bloom));
    float mid = luminance(center);
    float low = min(mid, min(min(nw, ne), min(sw, se)));
    float high = max(mid, max(max(nw, ne), max(sw, se)));
    if (high - low < max(0.0312, high * 0.125))
        return float4(center, 1);
    float2 direction = float2(-((nw + ne) - (sw + se)), (nw + sw) - (ne + se));
    float reduce = max((nw + ne + sw + se) * 0.03125, 0.0078125);
    direction = clamp(direction / (min(abs(direction.x), abs(direction.y)) + reduce), -8, 8) * texel;
    float3 a = 0.5 * (mapped(input.uv + direction * (-1.0 / 6.0), bloom) + mapped(input.uv + direction * (1.0 / 6.0), bloom));
    float3 b = a * 0.5 + 0.25 * (mapped(input.uv + direction * -0.5, bloom) + mapped(input.uv + direction * 0.5, bloom));
    float lb = luminance(b);
    return float4(lb < low || lb > high ? a : b, 1); // Linear LDR; sRGB attachment encodes once.
}
