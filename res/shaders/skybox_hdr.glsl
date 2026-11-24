#version 460
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : require

layout(push_constant) uniform PushConstants {
    mat4 viewProjMatrix;
    uint textureIndex;   // bindless slot, 0 = black fallback
} pc;

layout(set = 3, binding = 0) uniform samplerCube cubeMaps[];

#ShaderType Vertex

layout(location = 0) out vec3 outTexCoord;

const vec3 positions[] = vec3[](
    vec3(-1.0, -1.0, -1.0),
    vec3( 1.0, -1.0, -1.0),
    vec3( 1.0,  1.0, -1.0),
    vec3(-1.0,  1.0, -1.0),
    vec3(-1.0, -1.0,  1.0),
    vec3( 1.0, -1.0,  1.0),
    vec3( 1.0,  1.0,  1.0),
    vec3(-1.0,  1.0,  1.0)
);

const int indices[36] = int[](
    0, 1, 2,  2, 3, 0,  // -Z
    5, 4, 7,  7, 6, 5,  // +Z
    4, 0, 3,  3, 7, 4,  // -X
    1, 5, 6,  6, 2, 1,  // +X
    3, 2, 6,  6, 7, 3,  // +Y
    4, 5, 1,  1, 0, 4   // -Y
);

void main() {
    vec3 pos = positions[indices[gl_VertexIndex]];
    outTexCoord = pos;
    gl_Position = (pc.viewProjMatrix * vec4(pos, 1.0f)).xyww;
}

#ShaderType Fragment

layout(location = 0) in vec3 inTexCoord;
layout(location = 0) out vec4 outFragColor;

float luminance(vec3 v) {
    return dot(v, vec3(0.2126f, 0.7152f, 0.0722f));
}

vec3 reinhardToneMapping(vec3 v) {
    float l = luminance(v);
    vec3 tv = v / (1.0f + v);
    return mix(v / (1.0f + l), tv, tv);
}

void main() {
    outFragColor = vec4(reinhardToneMapping(texture(cubeMaps[pc.textureIndex], inTexCoord).rgb), 1.0f);
}
