struct PostProcessConstants {
    float Exposure;
};

[[vk::binding(0, 0)]] [format("rgba16f")] RWTexture2D<float4> DrawImage;

float Luminance(float3 c) {
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 TonemapReinhard(float3 c) {
    float l = Luminance(c);
    float3 tc = c / (1.0f + c);
    return lerp(c / (1.0f + l), tc, tc);
}

float3 LinearToSRGB(float3 c) {
    float3 lo = c * 12.92f;
    float3 hi = 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
    return select(c <= 0.0031308f, lo, hi);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 id : SV_DispatchThreadID, uniform PostProcessConstants pc) {
    uint width, height;
    DrawImage.GetDimensions(width, height);

    if (id.x >= width || id.y >= height)
        return;

    float3 hdr = DrawImage[id.xy].rgb * pc.Exposure;
    float3 ldr = TonemapReinhard(hdr);
    DrawImage[id.xy] = float4(LinearToSRGB(ldr), 1.0f);
}
