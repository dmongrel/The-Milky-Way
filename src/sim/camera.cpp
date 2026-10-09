#include "sim/camera.h"

#include <cmath>

namespace sim {
namespace {

constexpr double kTwoPi = 6.283185307179586;

Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
Vec3 Normalize(Vec3 v) {
    const float n = std::sqrt(Dot(v, v));
    return {v.x / n, v.y / n, v.z / n};
}

}  // namespace

CameraPose OrbitCamera(double seconds) {
    // Counter-clockwise, against the galaxy's clockwise turn, once every seven minutes. Seen from
    // above the stars then sweep clockwise past the viewer a little faster than they really turn,
    // never backwards.
    const double azimuth   = 0.6 + kTwoPi * seconds / 420.0;
    const double elevation = (38.0 + 22.0 * std::sin(kTwoPi * seconds / 170.0)) * kTwoPi / 360.0;
    const double distance  = 29.0 + 8.0 * std::sin(kTwoPi * seconds / 230.0 + 1.0);

    CameraPose pose;
    pose.eye = {static_cast<float>(distance * std::cos(elevation) * std::cos(azimuth)),
                static_cast<float>(distance * std::cos(elevation) * std::sin(azimuth)),
                static_cast<float>(distance * std::sin(elevation))};
    // A slow wander of the aim point keeps the centre from being pinned to the middle of the screen.
    pose.target = {static_cast<float>(1.5 * std::sin(kTwoPi * seconds / 140.0)),
                   static_cast<float>(1.5 * std::cos(kTwoPi * seconds / 190.0)), 0.0f};
    pose.up     = {0.0f, 0.0f, 1.0f};
    return pose;
}

Mat4 LookAt(const CameraPose& pose) {
    const Vec3 f = Normalize(Sub(pose.target, pose.eye));
    const Vec3 s = Normalize(Cross(f, pose.up));
    const Vec3 u = Cross(s, f);

    Mat4 r;
    r.m[0]  = s.x;
    r.m[4]  = s.y;
    r.m[8]  = s.z;
    r.m[1]  = u.x;
    r.m[5]  = u.y;
    r.m[9]  = u.z;
    r.m[2]  = -f.x;
    r.m[6]  = -f.y;
    r.m[10] = -f.z;
    r.m[12] = -Dot(s, pose.eye);
    r.m[13] = -Dot(u, pose.eye);
    r.m[14] = Dot(f, pose.eye);
    r.m[15] = 1.0f;
    return r;
}

Mat4 Perspective(float fovY, float aspect, float zNear, float zFar) {
    // Right-handed view space looking down -z, Vulkan clip space: y down, depth 0..1.
    const float f = 1.0f / std::tan(fovY * 0.5f);
    Mat4        p;
    p.m[0]  = f / aspect;
    p.m[5]  = -f;
    p.m[10] = zFar / (zNear - zFar);
    p.m[11] = -1.0f;
    p.m[14] = zNear * zFar / (zNear - zFar);
    return p;
}

Mat4 Multiply(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

Mat4 Inverse(const Mat4& in) {
    // Cofactor expansion; the matrices here are well conditioned.
    const float* m = in.m;
    float        inv[16];
    inv[0]  = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4]  = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8]  = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1]  = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5]  = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9]  = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2]  = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6]  = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3]  = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7]  = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    Mat4        r;
    if (det == 0.0f) return r;
    for (int i = 0; i < 16; ++i) r.m[i] = inv[i] / det;
    return r;
}

}  // namespace sim
