#version 450

// A star is a small Gaussian; an HII region a broader, softer one. Additive into the HDR scene.

layout(location = 0) in vec3 vColor;
layout(location = 1) in float vSoft;
layout(location = 0) out vec4 outColor;

void main() {
    vec2  d  = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(d, d);
    if (r2 > 1.0) discard;
    float falloff = vSoft > 0.5 ? 2.2 : 4.5;
    // Normalised so the sprite's total stays near vColor * (sprite area) whatever the falloff.
    float w = exp(-r2 * falloff) * falloff * 0.32;
    outColor = vec4(vColor * w, 0.0);
}
