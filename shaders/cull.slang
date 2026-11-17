struct CullConstants
{
    float4 planes[6];
    uint count;
};

[[vk::binding(0, 2)]] ConstantBuffer<CullConstants> cull;
[[vk::binding(1, 2)]] StructuredBuffer<float4> spheres;
[[vk::binding(2, 2)]] RWStructuredBuffer<uint> visible;

[shader("compute")]
[numthreads(64, 1, 1)]
void cullMain(uint3 thread : SV_DispatchThreadID)
{
    if (thread.x >= cull.count)
        return;
    float4 sphere = spheres[thread.x];
    uint accepted = 1;
    for (uint plane = 0; plane < 6; ++plane)
        if (dot(cull.planes[plane].xyz, sphere.xyz) + cull.planes[plane].w < -sphere.w)
            accepted = 0;
    visible[thread.x] = accepted;
}
