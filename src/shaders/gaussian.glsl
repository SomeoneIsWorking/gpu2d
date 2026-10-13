// A Gaussian of standard deviation `sigma` pixels along `texel`: whole-pixel taps out to three
// sigma, at most 96.
vec4 gaussian(sampler2D image, vec2 uv, vec2 texel, float sigma) {
    int radius = int(min(ceil(3.0 * sigma), 96.0));
    vec4 sum = texture(image, uv);
    float weight = 1.0;
    for (int i = 1; i <= radius; ++i) {
        float w = exp(-0.5 * float(i * i) / (sigma * sigma));
        sum += w * (texture(image, uv + texel * float(i)) + texture(image, uv - texel * float(i)));
        weight += 2.0 * w;
    }
    return sum / weight;
}
