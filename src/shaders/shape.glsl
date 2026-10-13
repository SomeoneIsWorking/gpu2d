// One SDF shape, as canvas.cpp writes it into the storage buffer (gpu2d::ShapeRecord).
struct Shape {
    // Centre in target pixels, then the half extent of the shape's frame.
    vec4 bounds;
    // Straight-alpha colour.
    vec4 colour;
    // The frame's x axis (unit), then the kind: 0 rounded box, 1 arc.
    vec4 axis;
    // Box: corner radius, stroke width (0 fills). Arc: inner radius, outer radius, half sweep.
    vec4 params;
};
