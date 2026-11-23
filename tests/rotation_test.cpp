// Proves the one thing the brief insists on: the Galaxy turns the right way, on screen.
//
// Seen from the North Galactic Pole the Milky Way rotates clockwise and its arms trail. The model
// encodes that as decreasing angles and arms that wind the other way outward, but a mirrored view
// matrix would quietly show it backwards, so this pushes a star's real motion through the real
// camera and looks at which way it goes on screen.

#include "sim/camera.h"
#include "sim/galaxy.h"

#include <cmath>
#include <cstdio>

namespace {

int failures = 0;

void Check(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++failures;
}

// Screen position with +y up (Vulkan clip space has y down).
struct P2 {
    float x, y;
};
P2 Project(const sim::Mat4& vp, float x, float y, float z) {
    const float* m  = vp.m;
    const float  cx = m[0] * x + m[4] * y + m[8] * z + m[12];
    const float  cy = m[1] * x + m[5] * y + m[9] * z + m[13];
    const float  cw = m[3] * x + m[7] * y + m[11] * z + m[15];
    return {cx / cw, -cy / cw};
}

}  // namespace

int main() {
    const sim::Galaxy& g = sim::MilkyWay();

    // 1. A star on a circular orbit sweeps clockwise on screen, from every point of the camera's
    //    orbit (it always looks down from the north side of the disk).
    bool clockwise = true;
    for (double seconds = 0.0; seconds < 1200.0; seconds += 7.0) {
        const sim::CameraPose pose = sim::OrbitCamera(seconds);
        const sim::Mat4       vp   = sim::Multiply(sim::Perspective(0.8f, 16.0f / 9.0f, 0.05f, 4000.0f),
                                                   sim::LookAt(pose));
        for (float r : {1.0f, 4.0f, 8.15f, 14.0f}) {
            for (int k = 0; k < 8; ++k) {
                const float theta0 = k * sim::kPi / 4;
                const float dTheta = -sim::AngularSpeed(g, r) * 2.0f;  // two Myr later
                const P2    c      = Project(vp, 0, 0, 0);
                const P2    a      = Project(vp, r * std::cos(theta0), r * std::sin(theta0), 0);
                const P2    b = Project(vp, r * std::cos(theta0 + dTheta), r * std::sin(theta0 + dTheta), 0);
                const float cross = (a.x - c.x) * (b.y - c.y) - (a.y - c.y) * (b.x - c.x);
                clockwise &= cross < 0.0f;
            }
        }
    }
    Check(clockwise, "stars sweep clockwise on screen, seen from the north side");

    // 2. Every arm trails: followed outward it winds against the rotation (its angle increases).
    bool trailing = true;
    for (const sim::Arm& arm : g.arms) {
        for (float r = arm.rMin; r + 0.5f <= arm.rMax; r += 0.5f) {
            trailing &= sim::ArmAngle(arm, r + 0.5f) > sim::ArmAngle(arm, r);
        }
    }
    Check(trailing, "all five arms trail");

    // 3. The bar's near end is at positive Galactic longitude. From the Sun on -y, the centre is
    //    at l = 0 along +y and l = +90 is along -x, the way the Sun moves.
    const float ex = g.barHalfLength * std::cos(g.barAngle0), ey = g.barHalfLength * std::sin(g.barAngle0);
    const float sx = g.sunR * std::cos(g.sunTheta), sy = g.sunR * std::sin(g.sunTheta);
    const float l  = std::atan2(-(ex - sx), ey - sy) * 180.0f / sim::kPi;
    const bool  near = std::hypot(ex - sx, ey - sy) < g.sunR;
    std::printf("      bar end at l = %.1f deg, %s end\n", l, near ? "near" : "far");
    Check(near && l > 0.0f && l < 90.0f, "the bar's near end is at positive longitude");

    // 4. The Sun moves toward l = +90 (the Galaxy turns clockwise, seen from the north).
    const float vx = std::sin(g.sunTheta), vy = -std::cos(g.sunTheta);  // d/dt of (cos, sin) at -omega
    Check(vx < -0.99f && std::fabs(vy) < 0.01f, "the Sun heads toward l = +90");

    // 5. Patterns turn slower than the stars they sweep through near the Sun, and the same way.
    Check(g.omegaArms > 0 && g.omegaArms < sim::AngularSpeed(g, g.sunR) && g.omegaBar > g.omegaArms,
          "arms turn slower than the Sun's orbit, the bar faster than the arms");

    std::printf("%s\n", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
