#version 450

const int TextureSlotCount = 16;

layout(set = 0, binding = 0) uniform sampler2D uTextures[TextureSlotCount];

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUv;
layout(location = 2) flat in float fragTextureIndex;

layout(location = 0) out vec4 outColor;

void main()
{
    int textureSlot = clamp(int(fragTextureIndex + 0.5), 0, TextureSlotCount - 1);
    vec4 textureColor = texture(uTextures[textureSlot], fragUv);
    outColor = fragColor * textureColor;
}