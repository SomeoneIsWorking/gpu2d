#version 450

layout(set = 2, binding = 0) uniform sampler2D image;
layout(set = 3, binding = 0) uniform Sampling {
    // 1 when the texture already holds premultiplied colour, as a render target does.
    float premultiplied;
};
layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 colour;
layout(location = 0) out vec4 outColour;

void main() {
    vec4 texel = texture(image, uv);
    if (premultiplied < 0.5) {
        texel.rgb *= texel.a;
    }
    outColour = texel * vec4(colour.rgb * colour.a, colour.a);
}
