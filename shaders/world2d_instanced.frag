#version 450

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUv;
layout(location = 2) flat in float fragTextureIndex;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 textureColor = texture(uTexture, fragUv);
    outColor = fragColor * textureColor;
}
