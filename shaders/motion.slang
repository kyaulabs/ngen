#include "motion_vector.slangh"

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
    return motionVector(input.previousClip, input.position.xy, history.viewportAndHistory);
}
