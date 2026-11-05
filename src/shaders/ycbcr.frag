#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 colour;

layout(set = 2, binding = 0) uniform sampler2D luma;
// Cb in red, Cr in green, at half the size each way.
layout(set = 2, binding = 1) uniform sampler2D chroma;
layout(set = 3, binding = 0) uniform Conversion {
    vec4 red;
    vec4 green;
    vec4 blue;
};

void main() {
    vec4 ycbcr = vec4(texture(luma, uv).r, texture(chroma, uv).rg, 1.0);
    colour = vec4(dot(red, ycbcr), dot(green, ycbcr), dot(blue, ycbcr), 1.0);
}
