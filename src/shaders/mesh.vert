#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 colour;
layout(set = 1, binding = 0) uniform Target {
    vec2 size;
};
layout(location = 0) out vec2 outUv;
layout(location = 1) out vec4 outColour;

void main() {
    outUv = uv;
    outColour = colour;
    gl_Position = vec4(position / size * vec2(2.0, -2.0) + vec2(-1.0, 1.0), 0.0, 1.0);
}
