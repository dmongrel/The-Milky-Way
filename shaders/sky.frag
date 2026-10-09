#version 450
#extension GL_GOOGLE_include_directive : require

// The sky behind the galaxy. The camera is outside the Milky Way, so there is no Milky Way band:
// a sparse sprinkle of halo stars, distant galaxies taken from the Hubble eXtreme Deep Field, and
// Andromeda from Hubble's PHAT+PHAST mosaic.

#include "common.glsl"

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 2) uniform sampler2D deepField;
layout(set = 0, binding = 3) uniform sampler2D andromeda;

vec3 viewRay(vec2 uv) {
    vec4 world = frame.invViewProj * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    return normalize(world.xyz / world.w - frame.camPos.xyz);
}

// Direction to a cube face and that face's [0,1]^2 coordinates.
vec2 cubeFace(vec3 d, out float face) {
    vec3 a = abs(d);
    vec2 uv;
    if (a.x >= a.y && a.x >= a.z) {
        face = d.x > 0.0 ? 0.0 : 1.0;
        uv   = d.yz / a.x;
    } else if (a.y >= a.z) {
        face = d.y > 0.0 ? 2.0 : 3.0;
        uv   = d.xz / a.y;
    } else {
        face = d.z > 0.0 ? 4.0 : 5.0;
        uv   = d.xy / a.z;
    }
    return uv * 0.5 + 0.5;
}

vec2 hash22(vec2 p) {
    vec3 q = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    q += dot(q, q.yzx + 33.33);
    return fract((q.xx + q.yz) * q.zy);
}

// Point-like stars: one candidate per cell of a grid on each cube face.
vec3 haloStars(vec3 d, float cells, float density, float seed) {
    float face;
    vec2  uv   = cubeFace(d, face) * cells;
    vec2  cell = floor(uv);
    vec2  h    = hash22(cell + face * 131.0 + seed);
    if (h.x > density) return vec3(0.0);
    vec2  centre = cell + 0.15 + 0.7 * hash22(cell * 1.7 + seed + face);
    // Size of one pixel, in cell units, so the stars stay sharp at any resolution.
    float pixel  = cells * 2.0 / frame.render.y;
    float r      = length(uv - centre) / pixel;
    float bright = pow(h.y, 10.0) * 3.0 + 0.03;
    vec3  tint   = mix(vec3(1.0, 0.82, 0.62), vec3(0.75, 0.85, 1.0), hash22(cell + 7.0).x);
    return tint * bright * exp(-r * r * 1.6);
}

// Distant galaxies. Each cube face is tiled with pieces of the deep field, every tile turned and
// shifted differently so the single image never visibly repeats.
vec3 distantGalaxies(vec3 d) {
    float face;
    vec2  uv   = cubeFace(d, face) * 9.0;
    vec2  cell = floor(uv);
    vec2  f    = fract(uv) - 0.5;
    vec2  h    = hash22(cell + face * 17.0);
    float a    = h.x * TWO_PI;
    mat2  rot  = mat2(cos(a), sin(a), -sin(a), cos(a));
    vec2  t    = rot * f * 0.42 + 0.5 + (h - 0.5) * 0.4;
    vec3  c    = textureGrad(deepField, t, dFdx(uv) * 0.42, dFdy(uv) * 0.42).rgb;
    // The deep field's sky is not quite black; lift it out so only the galaxies remain. The tile
    // edges fade so seams never show.
    vec2  edge = smoothstep(0.5, 0.40, abs(f));
    return max(c - 0.035, 0.0) * 0.4 * edge.x * edge.y;
}

vec3 andromedaGalaxy(vec3 d) {
    float facing = dot(d, frame.m31Dir.xyz);
    if (facing <= 0.0) return vec3(0.0);
    vec2 plane = vec2(dot(d, frame.m31Right.xyz), dot(d, frame.m31Up.xyz)) / facing;
    vec2 uv    = plane / vec2(frame.m31Dir.w, frame.m31Dir.w * frame.m31Right.w) * 0.5 + 0.5;
    uv.y += 0.119;  // the nucleus is 62% of the way down the prepared image: centre on it
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return vec3(0.0);
    vec3 c = texture(andromeda, uv).rgb;
    // Soft edges: the mosaic's rectangle must not show against the black.
    vec2 edge = smoothstep(vec2(0.0), vec2(0.06, 0.12), uv) * smoothstep(vec2(0.0), vec2(0.06, 0.12), 1.0 - uv);
    return max(c - 0.02, 0.0) * 2.6 * edge.x * edge.y * frame.m31Up.w;
}

void main() {
    vec3 d = viewRay(vUV);
    vec3 sky = vec3(0.0);
    sky += haloStars(d, 70.0, 0.16, 0.0);
    sky += haloStars(d, 190.0, 0.22, 3.1) * 0.3;
    sky += distantGalaxies(d);
    sky += andromedaGalaxy(d);
    outColor = vec4(sky * 0.08, 1.0);
}
