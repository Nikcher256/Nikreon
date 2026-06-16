#version 450

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
} pushConstants;

layout(location = 0) in vec3 inCorner0;
layout(location = 1) in vec3 inCorner1;
layout(location = 2) in vec3 inCorner2;
layout(location = 3) in vec3 inCorner3;

layout(location = 4) in vec2 inUv0;
layout(location = 5) in vec2 inUv1;
layout(location = 6) in vec2 inUv2;
layout(location = 7) in vec2 inUv3;

layout(location = 8) in vec4 inColor;
layout(location = 9) in float inTextureIndex;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUv;
layout(location = 2) flat out float fragTextureIndex;

vec3 cornerForVertex(uint vertexIndex)
{
    uint cornerIndex = uint[6](0, 1, 2, 2, 3, 0)[vertexIndex];
    if (cornerIndex == 0) {
        return inCorner0;
    }
    if (cornerIndex == 1) {
        return inCorner1;
    }
    if (cornerIndex == 2) {
        return inCorner2;
    }
    return inCorner3;
}

vec2 uvForVertex(uint vertexIndex)
{
    uint cornerIndex = uint[6](0, 1, 2, 2, 3, 0)[vertexIndex];
    if (cornerIndex == 0) {
        return inUv0;
    }
    if (cornerIndex == 1) {
        return inUv1;
    }
    if (cornerIndex == 2) {
        return inUv2;
    }
    return inUv3;
}

void main()
{
    uint vertexIndex = uint(gl_VertexIndex);

    vec3 worldPosition = cornerForVertex(vertexIndex);
    gl_Position = pushConstants.viewProjection * vec4(worldPosition, 1.0);
    
    fragColor = inColor;
    fragUv = uvForVertex(vertexIndex);
    fragTextureIndex = inTextureIndex;
}