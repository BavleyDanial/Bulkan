import common;

struct DrawPushConstants {
    float4x4 Transform;
    Vertex* Vertices;
    uint Texture2DIndex;
    uint TextureCubeIndex;
};

struct VSOutput {
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

[shader("vertex")]
VSOutput VertexMain(uint vertexId : SV_VertexID, uniform DrawPushConstants pc) {
    Vertex v = pc.Vertices[vertexId];

    VSOutput output;
    output.Position = mul(Scene.ViewProjection, mul(pc.Transform, float4(v.Position, 1.0f)));
    output.Normal = mul((float3x3)pc.Transform, v.Normal);
    output.UV = float2(v.UVx, v.UVy);
    return output;
}

[shader("fragment")]
float4 FragmentMain(VSOutput input, uniform DrawPushConstants pc) : SV_Target0 {
    float3 N = normalize(input.Normal);
    float3 L = normalize(-Scene.SunDirection.xyz);

    float3 sunColor = Scene.SunColor.xyz;
    float sunIntensity = Scene.SunColor.w;

    float4 albedo = Textures2D[NonUniformResourceIndex(pc.Texture2DIndex)].Sample(input.UV);

    if (albedo.a < 0.99f)
        discard;

    float diffuse = max(dot(N, L), 0.0f);
    float3 ambient = 0.02f * albedo.rgb;

    float3 lighting = ambient + diffuse * sunIntensity * sunColor;
    float3 color = lighting * albedo.rgb;
    return float4(color, 1.0f);
}
