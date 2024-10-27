#version 450
#pragma shader_stage(fragment)

layout (location = 0) in vec2 fragCoord;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D sampleTexture;

void main() {
    //outColor = texture(sampleTexture, fragCoord) * fragColor;
    outColor = fragColor;
}