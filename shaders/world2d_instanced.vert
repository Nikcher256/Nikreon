#version 450

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
} pushConstants;

layout(location = 0) in vec3 inCorner0;
layout(location = 1) in vec3 inCorner1;
layout(location = 2) in vec3 inCorner2;
layout(location = 3) in vec3 inCorner3;
layout(location = 4) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

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

void main()
{
    vec3 worldPosition = cornerForVertex(uint(gl_VertexIndex));
    gl_Position = pushConstants.viewProjection * vec4(worldPosition, 1.0);
    fragColor = inColor;
}