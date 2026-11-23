// Shared by every shader: the frame's uniforms and the galaxy model they describe.
//
// Frame: kpc, Galactic centre at the origin, disk in xy, +z toward the North Galactic Pole.
// The galaxy turns clockwise seen from +z, so angles decrease with time. The layout of Frame must
// match FrameUniforms in src/render/renderer.cpp.

#define ARM_COUNT 5

layout(set = 0, binding = 0, std140) uniform Frame {
    mat4 viewProj;
    mat4 invViewProj;
    vec4 camPos;      // xyz: camera; w: simulation time, Myr
    vec4 viewport;    // width, height, 1/width, 1/height (pixels)
    vec4 render;      // x: exposure, y: focal length in pixels, z: frame number, w: star brightness
    vec4 kinematics;  // x: v0 (km/s), y: core radius (kpc), z: bar pattern speed, w: arm pattern speed (rad/Myr)
    vec4 bar;         // x: bar angle at t = 0, y: half-length (kpc)
    vec4 arms[ARM_COUNT * 3];  // per arm: (thetaKink, rKink, tanPitchIn, tanPitchOut),
                               //          (rMin, rMax, width, young), (old, dust, 0, 0)
    vec4 m31Dir;      // xyz: direction to Andromeda; w: half-width (radians) of its image
    vec4 m31Right;    // xyz: image x axis on the sky; w: half-height / half-width
    vec4 m31Up;       // xyz: image y axis on the sky; w: brightness
} frame;

const float PI     = 3.14159265358979;
const float TWO_PI = 6.28318530717959;
const float KMS_PER_KPC_TO_RAD_PER_MYR = 1.0227e-3;

float simTime() { return frame.camPos.w; }

float wrapPi(float a) { return a - TWO_PI * floor((a + PI) / TWO_PI); }

float sech2(float x) {
    float c = cosh(clamp(x, -15.0, 15.0));
    return 1.0 / (c * c);
}

// Angular speed of a circular orbit, rad/Myr, from v(R) = v0 (1 - exp(-R / rCore)).
float angularSpeed(float r) {
    r = max(r, 1e-3);
    return frame.kinematics.x * (1.0 - exp(-r / frame.kinematics.y)) / r * KMS_PER_KPC_TO_RAD_PER_MYR;
}

float barAngle() { return frame.bar.x - frame.kinematics.z * simTime(); }
float armPatternAngle() { return -frame.kinematics.w * simTime(); }

// ---- noise ---------------------------------------------------------------------------------

float hash13(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

float valueNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash13(i);
    float n100 = hash13(i + vec3(1, 0, 0));
    float n010 = hash13(i + vec3(0, 1, 0));
    float n110 = hash13(i + vec3(1, 1, 0));
    float n001 = hash13(i + vec3(0, 0, 1));
    float n101 = hash13(i + vec3(1, 0, 1));
    float n011 = hash13(i + vec3(0, 1, 1));
    float n111 = hash13(i + vec3(1, 1, 1));
    return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}

float fbm(vec3 p, int octaves) {
    float sum = 0.0, amp = 0.5;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * valueNoise(p);
        p = p * 2.03 + vec3(17.1, 3.7, 9.2);
        amp *= 0.5;
    }
    return sum;
}

// Noise that moves with the gas. Gas orbits at angularSpeed(r), so sampling noise in co-rotating
// coordinates makes its clumps stream round the galaxy, shearing as the inner disk laps the outer.
// Left alone the shear would wind every clump into a thin ring; two layers that each reset every
// FLOW_PERIOD, half a period apart and cross-faded, keep the motion without the winding.
const float FLOW_PERIOD = 60.0;  // Myr

float flowLayer(vec3 p, float r, float theta, float age, float seed, float freq, int octaves) {
    float a = theta + angularSpeed(r) * age;  // back to where this gas was `age` Myr ago
    vec3  q = vec3(r * cos(a), r * sin(a), p.z * 3.0);
    return fbm(q * freq + seed, octaves);
}

float flowNoise(vec3 p, float r, float theta, float freq, int octaves) {
    float f = fract(simTime() / FLOW_PERIOD);
    float a = flowLayer(p, r, theta, f * FLOW_PERIOD, 0.0, freq, octaves);
    float b = flowLayer(p, r, theta, fract(f + 0.5) * FLOW_PERIOD, 41.7, freq, octaves);
    return mix(a, b, abs(2.0 * f - 1.0));
}

// ---- spiral arms ---------------------------------------------------------------------------

// Signed distance (kpc) from an arm's centre line, measured across the arm. Positive is upstream:
// the side the gas comes from, counter-clockwise of the arm and toward its inner edge.
float armOffset(int i, float r, float patternTheta, out float envelope, out float width) {
    vec4  geo  = frame.arms[i * 3 + 0];
    vec4  span = frame.arms[i * 3 + 1];
    float tanPitch = r > geo.y ? geo.w : geo.z;
    float sinPitch = tanPitch * inversesqrt(1.0 + tanPitch * tanPitch);
    float centre   = geo.x + log(max(r, 0.05) / geo.y) / tanPitch;
    envelope = smoothstep(span.x, span.x + 1.2, r) * (1.0 - smoothstep(span.y - 2.5, span.y, r));
    width    = max(span.z + 0.042 * (r - geo.y), 0.12);
    return r * sinPitch * wrapPi(patternTheta - centre);
}

struct ArmField {
    float young;  // where stars form and gas glows, just downstream of the dust
    float dust;   // the dust lane on the upstream edge
    float old;    // the gentle enhancement of old stars in the two major stellar arms
};

ArmField armField(float r, float theta) {
    ArmField field = ArmField(0.0, 0.0, 0.0);
    float patternTheta = theta - armPatternAngle();
    for (int i = 0; i < ARM_COUNT; ++i) {
        float envelope, width;
        float d = armOffset(i, r, patternTheta, envelope, width);
        if (envelope <= 0.0) continue;
        vec4  span = frame.arms[i * 3 + 1];
        vec4  mix3 = frame.arms[i * 3 + 2];
        float dy = (d + 0.10) / width;          // young: centred slightly downstream
        float dd = (d - 0.35 * width) / (0.55 * width);  // dust: a narrower lane on the upstream edge
        float dO = d / (2.2 * width);           // old stars: broad
        field.young += envelope * span.w * exp(-0.5 * dy * dy);
        field.dust  += envelope * mix3.y * exp(-0.5 * dd * dd);
        field.old   += envelope * mix3.x * exp(-0.5 * dO * dO);
    }
    return field;
}

// ---- dust ---------------------------------------------------------------------------------

// Dust density, in units of optical depth per kpc. Used both by the volume and by each star's
// extinction, so a star behind a lane is dimmed by the lane you can see.
// `clumps` is flowNoise at about 2 cycles per kpc; callers that already have it pass it in.
float dustDensity(vec3 p, float r, ArmField field, float clumps) {
    float disk = 0.25 * exp(-r / 5.0) * smoothstep(2.5, 4.5, r) * (1.0 - smoothstep(14.0, 18.0, r));
    float vertical = sech2(p.z / 0.09);
    float lane = field.dust * (0.6 + 2.6 * smoothstep(0.35, 0.75, clumps));
    float nuclear = 1.4 * exp(-r / 0.18);  // the Central Molecular Zone
    return 9.0 * (disk * (0.5 + clumps) + lane + nuclear) * vertical;
}

float dustDensity(vec3 p) {
    float r = length(p.xy);
    float theta = atan(p.y, p.x);
    if (abs(p.z) > 0.45) return 0.0;  // sech2 of 5 scale heights: nothing left
    return dustDensity(p, r, armField(r, theta), flowNoise(p, r, theta, 2.0, 3));
}
