#version 450
#extension GL_GOOGLE_include_directive : require
#include "gaussian.glsl"

layout(set = 2, binding = 0) uniform sampler2D image;
layout(set = 3, binding = 0) uniform Blur {
    vec2 texel;
    float sigma;
    float opacity;
    // The rounded rectangle the blur shows through: centre, half extent, then corner radius.
    vec4 shape;
    float radius;
};
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColour;

void main() {
    vec4 blurred = gaussian(image, uv, texel, sigma);
    vec2 q = abs(gl_FragCoord.xy - shape.xy) - shape.zw + radius;
    float outside = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
    float coverage = clamp(0.5 - outside, 0.0, 1.0) * opacity;
    outColour = vec4(blurred.rgb, 1.0) * coverage;
}
