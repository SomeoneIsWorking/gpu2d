#version 450
// FidelityFX Super Resolution 1 EASU (AMD, MIT): an edge-following 12-tap Lanczos-like upscale.

layout(set = 2, binding = 0) uniform sampler2D image;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColour;

ivec2 lastTexel;

vec3 texelAt(ivec2 at) {
    return texelFetch(image, clamp(at, ivec2(0), lastTexel), 0).rgb;
}

float luma(vec3 colour) {
    return colour.b * 0.5 + (colour.r * 0.5 + colour.g);
}

void addEdge(inout vec2 direction, inout float length2, float weight, float up, float left,
             float centre, float right, float down) {
    float across = right - left;
    float spanX = max(abs(right - centre), abs(centre - left));
    direction.x += across * weight;
    float edgeX = clamp(abs(across) / max(spanX, 1.0e-5), 0.0, 1.0);
    length2 += edgeX * edgeX * weight;

    float along = down - up;
    float spanY = max(abs(down - centre), abs(centre - up));
    direction.y += along * weight;
    float edgeY = clamp(abs(along) / max(spanY, 1.0e-5), 0.0, 1.0);
    length2 += edgeY * edgeY * weight;
}

void tap(inout vec3 sum, inout float total, vec2 offset, vec2 direction, vec2 stretch, float lobe,
         float clipAt, vec3 colour) {
    vec2 turned = vec2(dot(offset, direction), dot(offset, vec2(-direction.y, direction.x)));
    turned *= stretch;
    float distance2 = min(dot(turned, turned), clipAt);
    float base = 0.4 * distance2 - 1.0;
    float window = lobe * distance2 - 1.0;
    base *= base;
    window *= window;
    base = 1.5625 * base - 0.5625;
    float weight = base * window;
    sum += colour * weight;
    total += weight;
}

void main() {
    ivec2 size = textureSize(image, 0);
    lastTexel = size - ivec2(1);
    vec2 position = uv * vec2(size) - 0.5;
    vec2 floored = floor(position);
    vec2 fraction = position - floored;
    ivec2 at = ivec2(floored);

    //    b c
    //  e f g h
    //  i j k l
    //    n o
    vec3 b = texelAt(at + ivec2(0, -1));
    vec3 c = texelAt(at + ivec2(1, -1));
    vec3 e = texelAt(at + ivec2(-1, 0));
    vec3 f = texelAt(at);
    vec3 g = texelAt(at + ivec2(1, 0));
    vec3 h = texelAt(at + ivec2(2, 0));
    vec3 i = texelAt(at + ivec2(-1, 1));
    vec3 j = texelAt(at + ivec2(0, 1));
    vec3 k = texelAt(at + ivec2(1, 1));
    vec3 l = texelAt(at + ivec2(2, 1));
    vec3 n = texelAt(at + ivec2(0, 2));
    vec3 o = texelAt(at + ivec2(1, 2));

    float bL = luma(b);
    float cL = luma(c);
    float eL = luma(e);
    float fL = luma(f);
    float gL = luma(g);
    float hL = luma(h);
    float iL = luma(i);
    float jL = luma(j);
    float kL = luma(k);
    float lL = luma(l);
    float nL = luma(n);
    float oL = luma(o);

    vec2 direction = vec2(0.0);
    float length2 = 0.0;
    addEdge(direction, length2, (1.0 - fraction.x) * (1.0 - fraction.y), bL, eL, fL, gL, jL);
    addEdge(direction, length2, fraction.x * (1.0 - fraction.y), cL, fL, gL, hL, kL);
    addEdge(direction, length2, (1.0 - fraction.x) * fraction.y, fL, iL, jL, kL, nL);
    addEdge(direction, length2, fraction.x * fraction.y, gL, jL, kL, lL, oL);

    float squared = dot(direction, direction);
    bool flat_ = squared < 1.0 / 32768.0;
    direction = flat_ ? vec2(1.0, 0.0) : direction * inversesqrt(squared);

    length2 *= 0.5;
    length2 *= length2;
    float widest = 1.0 / max(abs(direction.x), abs(direction.y));
    vec2 stretch = vec2(1.0 + (widest - 1.0) * length2, 1.0 - 0.5 * length2);
    float lobe = 0.5 + ((1.0 / 4.0 - 0.04) - 0.5) * length2;
    float clipAt = 1.0 / lobe;

    vec3 sum = vec3(0.0);
    float total = 0.0;
    tap(sum, total, vec2(0.0, -1.0) - fraction, direction, stretch, lobe, clipAt, b);
    tap(sum, total, vec2(1.0, -1.0) - fraction, direction, stretch, lobe, clipAt, c);
    tap(sum, total, vec2(-1.0, 1.0) - fraction, direction, stretch, lobe, clipAt, i);
    tap(sum, total, vec2(0.0, 1.0) - fraction, direction, stretch, lobe, clipAt, j);
    tap(sum, total, vec2(0.0, 0.0) - fraction, direction, stretch, lobe, clipAt, f);
    tap(sum, total, vec2(-1.0, 0.0) - fraction, direction, stretch, lobe, clipAt, e);
    tap(sum, total, vec2(1.0, 1.0) - fraction, direction, stretch, lobe, clipAt, k);
    tap(sum, total, vec2(2.0, 1.0) - fraction, direction, stretch, lobe, clipAt, l);
    tap(sum, total, vec2(2.0, 0.0) - fraction, direction, stretch, lobe, clipAt, h);
    tap(sum, total, vec2(1.0, 0.0) - fraction, direction, stretch, lobe, clipAt, g);
    tap(sum, total, vec2(1.0, 2.0) - fraction, direction, stretch, lobe, clipAt, o);
    tap(sum, total, vec2(0.0, 2.0) - fraction, direction, stretch, lobe, clipAt, n);

    vec3 lowest = min(min(f, g), min(j, k));
    vec3 highest = max(max(f, g), max(j, k));
    outColour = vec4(clamp(sum / total, lowest, highest), 1.0);
}
