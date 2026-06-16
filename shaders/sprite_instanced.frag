#version 450

layout(set = 0, binding = 0) uniform sampler2D uTextures[16];

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUv;
layout(location = 2) flat in float fragTextureIndex;

layout(location = 0) out vec4 outColor;

vec4 sampleTexture(uint slot, vec2 uv)
{
    switch (slot) {
    case 0: return texture(uTextures[0], uv);
    case 1: return texture(uTextures[1], uv);
    case 2: return texture(uTextures[2], uv);
    case 3: return texture(uTextures[3], uv);
    case 4: return texture(uTextures[4], uv);
    case 5: return texture(uTextures[5], uv);
    case 6: return texture(uTextures[6], uv);
    case 7: return texture(uTextures[7], uv);
    case 8: return texture(uTextures[8], uv);
    case 9: return texture(uTextures[9], uv);
    case 10: return texture(uTextures[10], uv);
    case 11: return texture(uTextures[11], uv);
    case 12: return texture(uTextures[12], uv);
    case 13: return texture(uTextures[13], uv);
    case 14: return texture(uTextures[14], uv);
    case 15: return texture(uTextures[15], uv);
    default: return texture(uTextures[0], uv);
    }
}

void main()
{
    uint textureSlot = uint(clamp(fragTextureIndex, 0.0, 15.0));
    vec4 textureColor = sampleTexture(textureSlot, fragUv);
    outColor = fragColor * textureColor;
}
