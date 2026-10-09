#version 450
#extension GL_GOOGLE_include_directive : require

// Places every star at the current simulation time and decides how bright it looks.
//
// Old disk stars ride circular orbits at their own angular speed, so the inner disk laps the
// outer. Bar stars circulate on elongated orbits in the bar's frame while the bar itself turns.
// Young stars are born in an arm at the arm's position at their birth, then orbit at their own
// speed: faster than the arms inside corotation, so they drift ahead (clockwise) of the arm
// that made them and fade within tens of millions of years. That is the density wave: the arms
// rotate as a pattern, while the stars stream through them.

#include "common.glsl"

layout(set = 0, binding = 1, std430) readonly buffer Stars {
    vec4 stars[];  // three vec4s per star; see sim::Star
};

layout(location = 0) out vec3 vColor;
layout(location = 1) out float vSoft;

const uint POP_DISK = 0u, POP_BAR = 1u, POP_YOUNG = 2u, POP_NEBULA = 3u, POP_NUCLEUS = 4u;

void cull() {
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);  // outside the clip volume
    gl_PointSize = 1.0;
    vColor = vec3(0.0);
    vSoft = 0.0;
}

// Optical depth of dust between a star and the camera. Only the thin dusty layer is sampled.
float extinction(vec3 star) {
    vec3  cam = frame.camPos.xyz;
    vec3  d = cam - star;
    float h = 0.35;
    float t0 = 0.0, t1 = 1.0;
    if (abs(d.z) > 1e-5) {
        float a = (-h - star.z) / d.z, b = (h - star.z) / d.z;
        t0 = max(min(a, b), 0.0);
        t1 = min(max(a, b), 1.0);
    } else if (abs(star.z) > h) {
        return 0.0;
    }
    if (t1 <= t0) return 0.0;
    const int N = 6;
    float len = length(d) * (t1 - t0);
    float tau = 0.0;
    for (int i = 0; i < N; ++i) {
        float t = t0 + (float(i) + 0.5) / float(N) * (t1 - t0);
        tau += dustDensity(star + d * t);
    }
    return tau * len / float(N);
}

void main() {
    uint i = uint(gl_VertexIndex) * 3u;
    vec4 a = stars[i], b = stars[i + 1u], c = stars[i + 2u];
    uint pop = uint(a.w + 0.5);
    float t = simTime();

    vec3  pos;
    float luminosity = b.w;
    float size = 1.0;
    float soft = 0.0;

    if (pop == POP_BAR) {
        float phase = a.y - (angularSpeed(a.x) - frame.kinematics.z) * t;
        vec2  local = vec2(a.x * cos(phase), c.x * sin(phase));
        float ba = barAngle();
        pos = vec3(cos(ba) * local.x - sin(ba) * local.y, sin(ba) * local.x + cos(ba) * local.y, a.z);
    } else if (pop == POP_YOUNG || pop == POP_NEBULA) {
        float life = c.x;
        float age = fract(t / life + c.y) * life;
        float born = t - age;
        float thetaBorn = a.y - frame.kinematics.w * born;      // the arm's angle at birth
        float theta = thetaBorn - angularSpeed(a.x) * age;      // then the star's own orbit
        pos = vec3(a.x * cos(theta), a.x * sin(theta), a.z);
        float x = age / life;
        if (pop == POP_YOUNG) {
            luminosity *= smoothstep(0.0, 0.05, x) * (1.0 - smoothstep(0.45, 1.0, x));
            size = c.z;
        } else {
            // HII regions shine only while their stars are newborn.
            luminosity *= smoothstep(0.0, 0.04, x) * (1.0 - smoothstep(0.12, 0.35, x));
            soft = 1.0;
        }
        if (luminosity <= 0.0) {
            cull();
            return;
        }
    } else {
        float theta = a.y - angularSpeed(a.x) * t;
        pos = vec3(a.x * cos(theta), a.x * sin(theta), a.z);
        if (pop == POP_DISK) {
            float r = a.x;
            ArmField arm = armField(r, theta);
            luminosity *= 0.65 + 1.0 * arm.old + 0.5 * arm.young;
        }
    }

    vec4 clip = frame.viewProj * vec4(pos, 1.0);
    if (clip.w <= 0.05) {
        cull();
        return;
    }

    float dist = length(pos - frame.camPos.xyz);
    float flux = luminosity * exp(-extinction(pos)) / (dist * dist);

    // Stars are unresolved: about two pixels at 1080p, scaled with the window. HII regions are
    // resolved clouds roughly 60-120 pc across.
    float scale = frame.viewport.y / 1080.0;
    float px;
    if (soft > 0.0) {
        px = max(frame.render.y * 0.012 * c.z / dist, 2.0 * scale);
    } else {
        px = max(1.8 * scale * size, 1.0);
    }
    px = min(px, frame.render.w);
    gl_PointSize = px;
    gl_Position = clip;

    // The sprite's energy is fixed whatever its size, so a big sprite is a dim one.
    float energy = flux * 220.0 / max(px * px, 1.0);
    vColor = b.rgb * energy;
    vSoft = soft;
}
