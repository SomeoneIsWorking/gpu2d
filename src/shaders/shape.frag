#version 450
#extension GL_GOOGLE_include_directive : require
#include "shape.glsl"

layout(std430, set = 2, binding = 0) readonly buffer Shapes {
    Shape shapes[];
};
layout(location = 0) in vec2 local;
layout(location = 1) flat in int index;
layout(location = 0) out vec4 outColour;

float roundBox(vec2 p, vec2 half_, float radius, float stroke) {
    vec2 q = abs(p) - half_ + radius;
    float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
    return stroke > 0.0 ? max(d, -(d + stroke)) : d;
}

// An annulus cut to the wedge of half angle `halfSweep` about +x, with square ends.
float arc(vec2 p, float inner, float outer, float halfSweep) {
    float ring = abs(length(p) - (inner + outer) * 0.5) - (outer - inner) * 0.5;
    if (halfSweep >= 3.14159265) {
        return ring;
    }
    p.y = abs(p.y);
    vec2 edge = vec2(cos(halfSweep), sin(halfSweep));
    float toEdge = length(p - edge * clamp(dot(p, edge), 0.0, length(p)));
    float wedge = toEdge * sign(edge.x * p.y - edge.y * p.x);
    return max(ring, wedge);
}

void main() {
    Shape s = shapes[index];
    float d = s.axis.z < 0.5 ? roundBox(local, s.bounds.zw, s.params.x, s.params.y)
                             : arc(local, s.params.x, s.params.y, s.params.z);
    float coverage = clamp(0.5 - d, 0.0, 1.0);
    outColour = vec4(s.colour.rgb * s.colour.a, s.colour.a) * coverage;
}
