import common;

struct PushConstants {
    float4x4 ViewProjection;
    uint TextureCubeIndex;
};

struct VSOutput {
    float4 Position : SV_Position;
    float3 TexCoord : TEXCOORD0;
};

static const float3 Positions[8] = {
    float3(-1.0, -1.0, -1.0),
    float3( 1.0, -1.0, -1.0),
    float3( 1.0,  1.0, -1.0),
    float3(-1.0,  1.0, -1.0),
    float3(-1.0, -1.0,  1.0),
    float3( 1.0, -1.0,  1.0),
    float3( 1.0,  1.0,  1.0),
    float3(-1.0,  1.0,  1.0)
};

static const int Indices[36] = {
    0, 1, 2,  2, 3, 0,  // -Z
    5, 4, 7,  7, 6, 5,  // +Z
    4, 0, 3,  3, 7, 4,  // -X
    1, 5, 6,  6, 2, 1,  // +X
    3, 2, 6,  6, 7, 3,  // +Y
    4, 5, 1,  1, 0, 4   // -Y
};



[shader("vertex")]
VSOutput VertexMain(uint vertexId : SV_VertexID, uniform PushConstants pc) {
    float3 position = Positions[Indices[vertexId]];

    VSOutput output;
    output.Position = mul(pc.ViewProjection, float4(position, 1.0f)).xyww;
    output.TexCoord = position;
    return output;
}

[shader("fragment")]
float4 FragmentMain(VSOutput input, uniform PushConstants pc) : SV_Target0 {
    return TexturesCube[pc.TextureCubeIndex].Sample(input.TexCoord);
}
