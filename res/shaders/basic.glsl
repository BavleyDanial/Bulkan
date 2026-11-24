#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : require

struct Vertex {
    vec3 position; float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout(push_constant) uniform PushConstants {
    mat4 modelMatrix;
    uint64_t vertexBufferAddress;
    uint textureIndex;   // bindless slot, 0 = white fallback
    uint cubmapIndex;
} pc;

layout(set = 1, binding = 0) uniform SceneData {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 ambientColor;
    vec4 camPosition;
    vec4 sunDirection;
    vec4 sunColor;      // w = intensity
} sceneData;

layout(set = 2, binding = 0) uniform sampler2D textures[];

#ShaderType Vertex

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outUV;

void main() {
    VertexBuffer vb = VertexBuffer(pc.vertexBufferAddress);
    Vertex v = vb.vertices[gl_VertexIndex];

    gl_Position = sceneData.viewProjection * pc.modelMatrix * vec4(v.position, 1.0f);
    outNormal = mat3(pc.modelMatrix) * v.normal;
    outUV = vec2(v.uv_x, v.uv_y);
}

#ShaderType Fragment

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec4 outFragColor;

void main() {
    vec3 N = normalize(inNormal);
    vec3 L = normalize(-sceneData.sunDirection.xyz);

    vec3 sunColor = sceneData.sunColor.xyz;
    float sunIntensity = sceneData.sunColor.w;

    vec4 albedo = texture(textures[nonuniformEXT(pc.textureIndex)], inUV);

    if (albedo.a < 0.99)
        discard;

    float diffuse = max(dot(N, L), 0.0f);
    vec3 ambient = 0.1f * albedo.rgb;

    vec3 lighting = (ambient + diffuse * sunIntensity * sunColor);
    vec3 color = lighting * albedo.rgb;
    vec3 finalColor = clamp(color, 0.0f, 1.0f);

    outFragColor = vec4(finalColor, 1.0);
}
