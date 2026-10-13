#version 450
#extension GL_GOOGLE_include_directive : require
#include "gaussian.glsl"

layout(set = 2, binding = 0) uniform sampler2D image;
layout(set = 3, binding = 0) uniform Blur {
    vec2 texel;
    float sigma;
};
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColour;

void main() {
    outColour = gaussian(image, uv, texel, sigma);
}
