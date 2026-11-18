// Both matrices map the same object-local vertex into each frame's clip space.
// The CPU forms them from poses expressed in one checked stationary basis.
struct MotionConstants
{
    float4x4 currentClipFromLocal;
    float4x4 previousClipFromLocal;
    float4 viewportAndHistory; // width, height in pixels, valid history (0/1), reserved
};
[[vk::binding(0, 0)]] ConstantBuffer<MotionConstants> history;

struct MotionVertex
{
    [[vk::location(0)]] float3 position : POSITION;
};
struct MotionVarying
{
    float4 position : SV_Position;
    float4 previousClip : TEXCOORD0;
};

[shader("vertex")]
MotionVarying motionVertex(MotionVertex input)
{
    MotionVarying output;
    float4 local = float4(input.position, 1);
    output.position = mul(history.currentClipFromLocal, local);
    output.previousClip = mul(history.previousClipFromLocal, local);
    return output;
}

[shader("fragment")]
float4 motionFragment(MotionVarying input) : SV_Target0
{
    const float4 previous = input.previousClip;
    if (history.viewportAndHistory.z != 1.0 || !all(isfinite(previous)) || previous.w <= 1e-6)
        return 0;
    const float3 ndc = previous.xyz / previous.w;
    if (any(abs(ndc.xy) > 1) || ndc.z < 0 || ndc.z > 1)
        return 0;
    // Positive-height Vulkan viewport. xy is current pixel to previous pixel;
    // invalid and cleared pixels have zero alpha, including history resets.
    const float2 pixel = (ndc.xy * 0.5 + 0.5) * history.viewportAndHistory.xy;
    return float4(pixel - input.position.xy, 0, 1);
}
