// The camera's orbit around the galaxy, and the small vector maths the renderer needs.
#pragma once

namespace sim {

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

// Column-major, as GLSL expects.
struct Mat4 {
    float m[16] = {};
};

struct CameraPose {
    Vec3 eye;
    Vec3 target;
    Vec3 up;
};

// Where the camera is `seconds` into the run. The orbit is slow next to the galaxy's own spin so
// that the rotation reads clearly; distance and elevation drift so the view never repeats exactly.
CameraPose OrbitCamera(double seconds);

Mat4 LookAt(const CameraPose& pose);
Mat4 Perspective(float verticalFovRadians, float aspect, float zNear, float zFar);  // Vulkan clip space
Mat4 Multiply(const Mat4& a, const Mat4& b);
Mat4 Inverse(const Mat4& m);

}  // namespace sim
