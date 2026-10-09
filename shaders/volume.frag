#version 450
#extension GL_GOOGLE_include_directive : require

// The galaxy's unresolved light and its dust, ray-marched. The output is (light, transmittance):
// the blend multiplies whatever is behind (the sky) by the transmittance and adds the light.
//
// Most of the structure lives within a few hundred parsecs of the plane, so the march spends most
// of its samples in a thin slab there and only a few in the thicker slab that holds the bulge.

#include "common.glsl"

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

const float SLAB_THICK = 2.4;   // kpc: bulge and thick disk
const float SLAB_THIN  = 0.55;  // kpc: thin disk, arms, dust
const float RADIUS     = 19.0;  // kpc
const int   STEPS_THIN  = 40;
const int   STEPS_THICK = 12;

const vec3 OLD_COLOUR    = vec3(1.00, 0.80, 0.58);
const vec3 BULGE_COLOUR  = vec3(1.00, 0.74, 0.48);
const vec3 YOUNG_COLOUR  = vec3(0.40, 0.58, 1.00);
const vec3 HII_COLOUR    = vec3(1.00, 0.30, 0.45);
const vec3 DUST_ALBEDO   = vec3(0.55, 0.42, 0.32);  // light the dust scatters back: brownish lanes

vec3 viewRay(vec2 uv) {
    vec4 world = frame.invViewProj * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    return normalize(world.xyz / world.w - frame.camPos.xyz);
}

// [near, far] of the ray inside |z| < h and R < RADIUS; near > far when it misses.
vec2 slab(vec3 ro, vec3 rd, float h) {
    vec2 t = vec2(0.0, 1e5);
    if (abs(rd.z) > 1e-6) {
        float a = (-h - ro.z) / rd.z, b = (h - ro.z) / rd.z;
        t = vec2(max(min(a, b), 0.0), max(a, b));
    } else if (abs(ro.z) > h) {
        return vec2(1.0, 0.0);
    }
    // Cylinder |ro.xy + t rd.xy| < RADIUS.
    float A = dot(rd.xy, rd.xy), B = dot(ro.xy, rd.xy), C = dot(ro.xy, ro.xy) - RADIUS * RADIUS;
    float disc = B * B - A * C;
    if (A < 1e-9 || disc < 0.0) return C < 0.0 ? t : vec2(1.0, 0.0);
    float s = sqrt(disc);
    return vec2(max(t.x, (-B - s) / A), min(t.y, (-B + s) / A));
}

// Light emitted per kpc at p, and the dust there.
vec3 emissionAndDust(vec3 p, out float dust) {
    float r = length(p.xy);
    float theta = atan(p.y, p.x);
    ArmField arm = armField(r, theta);

    // Old disk: exponential, thin plus thick, a little brighter in the two major stellar arms,
    // and thinned out inside the bar's radius, which the bar has swept.
    float edge = 1.0 - smoothstep(13.0, 18.5, r);
    float hole = 0.45 + 0.55 * smoothstep(2.0, 4.5, r);
    float disk = exp(-r / 2.6) * (sech2(p.z / 0.30) + 0.10 * sech2(p.z / 0.9)) * edge * hole;
    vec3  light = OLD_COLOUR * disk * (1.0 + 0.9 * arm.old) * 0.9;

    // Bar and boxy bulge, in the bar's rotating frame.
    float c = cos(barAngle()), s = sin(barAngle());
    vec2  q = vec2(c * p.x + s * p.y, -s * p.x + c * p.y);
    vec3  b = abs(vec3(q, p.z)) / vec3(1.05, 0.48, 0.42);
    float boxy = pow(b.x * b.x * b.x + b.y * b.y * b.y + b.z * b.z * b.z, 1.0 / 3.0);  // boxy, not round
    float bulge = exp(-boxy * 1.6) * 1.9;
    float longBar = exp(-pow(abs(q.x) / 4.6, 6.0)) * exp(-pow(q.y / 0.42, 2.0)) * exp(-pow(p.z / 0.18, 2.0)) * 0.32;
    float nucleus = exp(-length(p) / 0.09) * 6.0;
    light += BULGE_COLOUR * (bulge + longBar + nucleus);

    // Young stars, glowing gas and dust, all clumped by one noise that moves with the gas. Away
    // from the thin layer none of them is left, and the noise is the expensive part.
    dust = 0.0;
    if (abs(p.z) > 0.45) return light;
    float clumps = flowNoise(p, r, theta, 2.0, 3);
    dust = dustDensity(p, r, arm, clumps);
    if (arm.young > 0.01) {
        float young = arm.young * sech2(p.z / 0.10) * exp(-r / 9.0);
        light += YOUNG_COLOUR * young * (0.25 + 1.6 * clumps * clumps) * 1.5;
        float hii = pow(clamp(clumps * 1.25 - 0.25, 0.0, 1.0), 6.0);
        light += HII_COLOUR * young * hii * 9.0;
    }
    return light;
}

void march(vec3 ro, vec3 rd, float t0, float t1, int steps, float jitter, inout vec3 light, inout float trans) {
    if (t1 <= t0) return;
    float dt = (t1 - t0) / float(steps);
    for (int i = 0; i < steps; ++i) {
        float t = t0 + (float(i) + jitter) * dt;
        float dust;
        vec3  e = emissionAndDust(ro + rd * t, dust);
        // Exact for constant emission and absorption across the step.
        float a = exp(-dust * dt);
        vec3  scattered = DUST_ALBEDO * dust * 0.12;  // dust lit faintly by the disk around it
        float through = dust > 1e-4 ? (1.0 - a) / dust : dt;
        light += trans * (e + scattered) * through;
        trans *= a;
        if (trans < 0.01) break;
    }
}

void main() {
    vec3 ro = frame.camPos.xyz;
    vec3 rd = viewRay(vUV);

    vec2 thick = slab(ro, rd, SLAB_THICK);
    if (thick.x >= thick.y) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    vec2 thin = slab(ro, rd, SLAB_THIN);
    thin = vec2(max(thin.x, thick.x), min(thin.y, thick.y));

    // Interleaved-gradient noise breaks up the banding from the fixed step count.
    float jitter = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))) + 0.618 * frame.render.z);

    vec3  light = vec3(0.0);
    float trans = 1.0;
    if (thin.x < thin.y) {
        // Thick slab in front of the thin one, the thin one, then the thick slab behind.
        march(ro, rd, thick.x, thin.x, STEPS_THICK / 2, jitter, light, trans);
        march(ro, rd, thin.x, thin.y, STEPS_THIN, jitter, light, trans);
        march(ro, rd, thin.y, thick.y, STEPS_THICK / 2, jitter, light, trans);
    } else {
        march(ro, rd, thick.x, thick.y, STEPS_THICK, jitter, light, trans);
    }
    outColor = vec4(light * 0.25, trans);
}
