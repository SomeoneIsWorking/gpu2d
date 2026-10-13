#version 450
#extension GL_GOOGLE_include_directive : require
#include "shape.glsl"

layout(std430, set = 0, binding = 0) readonly buffer Shapes {
    Shape shapes[];
};
layout(set = 1, binding = 0) uniform Target {
    vec2 size;
};
layout(location = 0) out vec2 local;
layout(location = 1) flat out int index;

// A quad over the shape's frame, a pixel larger so the antialiased edge is covered.
void main() {
    Shape s = shapes[gl_InstanceIndex];
    vec2 corner = vec2(gl_VertexIndex & 1, (gl_VertexIndex >> 1) & 1) * 2.0 - 1.0;
    local = corner * (s.bounds.zw + 1.0);
    vec2 x = s.axis.xy;
    vec2 y = vec2(-x.y, x.x);
    vec2 pixel = s.bounds.xy + x * local.x + y * local.y;
    index = gl_InstanceIndex;
    gl_Position = vec4(pixel / size * vec2(2.0, -2.0) + vec2(-1.0, 1.0), 0.0, 1.0);
}
