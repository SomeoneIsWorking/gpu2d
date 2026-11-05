#version 450

layout(location = 0) out vec2 uv;

// A strip of four corners over the whole target.
void main() {
    vec2 corner = vec2(gl_VertexIndex & 1, (gl_VertexIndex >> 1) & 1);
    uv = corner;
    gl_Position = vec4(corner * vec2(2.0, -2.0) + vec2(-1.0, 1.0), 0.0, 1.0);
}
