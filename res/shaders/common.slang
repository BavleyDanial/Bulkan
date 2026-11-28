public struct Vertex {
    public float3 Position;
    public float UVx;
    public float3 Normal;
    public float UVy;
    public float4 Color;
};

public struct SceneData {
    public float4x4 View;
    public float4x4 Projection;
    public float4x4 ViewProjection;
    public float4 AmbientColor;
    public float4 CamPosition;
    public float4 SunDirection;
    public float4 SunColor;               // w = intensity
};

// TODO: Eventually remove all vk::binding to be cross-platform and performant
[[vk::binding(0, 1)]] public ConstantBuffer<SceneData> Scene;
[[vk::binding(0, 2)]] public Sampler2D Textures2D[];
[[vk::binding(0, 3)]] public SamplerCube TexturesCube[];
