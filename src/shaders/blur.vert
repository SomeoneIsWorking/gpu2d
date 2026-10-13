#version 450

layout(set = 1, binding = 0) uniform Placement {
    // Where the quad lands, in target pixels: x, y, width, height.
    vec4 destination;
    // The texture coordinates it reads: left, top, right, bottom.
    vec4 source;
    vec2 size;
};
layout(location = 0) out vec2 uv;

void main() {
    vec2 corner = vec2(gl_VertexIndex & 1, (gl_VertexIndex >> 1) & 1);
    uv = mix(source.xy, source.zw, corner);
    vec2 pixel = destination.xy + corner * destination.zw;
    gl_Position = vec4(pixel / size * vec2(2.0, -2.0) + vec2(-1.0, 1.0), 0.0, 1.0);
}
