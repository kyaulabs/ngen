struct OverlayConstants
{
    float4 scale_translate;
};
[[vk::push_constant]] ConstantBuffer<OverlayConstants> overlay;
[[vk::binding(0, 0)]] Texture2D<float4> fontAtlas;
[[vk::binding(1, 0)]] SamplerState fontSampler;
struct OverlayInput
{
    [[vk::location(0)]] float2 position : POSITION;
    [[vk::location(1)]] float2 uv : TEXCOORD0;
    [[vk::location(2)]] uint color : COLOR0;
};
struct OverlayVarying
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};
float decodeColor(float value)
{
    return value <= 0.04045 ? value / 12.92 : pow((value + 0.055) / 1.055, 2.4);
}
[shader("vertex")]
OverlayVarying overlayVertex(OverlayInput input)
{
    OverlayVarying output;
    output.position = float4(input.position * overlay.scale_translate.xy + overlay.scale_translate.zw, 0, 1);
    output.uv = input.uv;
    float4 color = float4(input.color & 255u, (input.color >> 8) & 255u,
                          (input.color >> 16) & 255u, (input.color >> 24) & 255u) / 255.0;
    output.color = float4(decodeColor(color.r), decodeColor(color.g), decodeColor(color.b), color.a);
    return output;
}
[shader("fragment")]
float4 overlayFragment(OverlayVarying input) : SV_Target0
{
    float4 texel = fontAtlas.Sample(fontSampler, input.uv);
    float3 image = float3(decodeColor(texel.r), decodeColor(texel.g), decodeColor(texel.b));
    return float4(input.color.rgb * image, input.color.a * texel.a);
}
