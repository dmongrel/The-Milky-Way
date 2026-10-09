// The Milky Way model: its geometry, its rotation, and the stars that make it up.
//
// Frame: kiloparsecs, origin at the Galactic centre, the disk in the xy plane, +z toward the North
// Galactic Pole. Seen from +z the Galaxy turns clockwise, so every angle below *decreases* with
// time. The spiral arms trail: followed outward they wind counter-clockwise (angle increases with
// radius). Sources: The Milky Way References/physics/milky-way-structure-and-rotation.md.
//
// The arms and the bar are patterns, rotating rigidly at their own speeds, slower than most stars.
// Stars are positioned on the GPU from these parameters and the simulation time, so the CPU only
// builds the star catalogue once.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace sim {

constexpr float kPi = 3.14159265358979f;

// 1 km/s/kpc is 1.0227 rad/Gyr.
constexpr float kRadPerMyrPerKmsKpc = 1.0227e-3f;

// The simulation clock: two million years per second. The Sun's 215 Myr orbit then takes under
// two minutes, the bar turns once in about 80 s, the arms in about 130 s, and stars within a
// kiloparsec of the centre lap it in 10 to 30 s, so both the centre and the arms visibly move.
constexpr double kMyrPerSecond = 2.0;

// A spiral arm: a log-periodic spiral with one kink, after Reid et al. 2019, extended beyond the
// measured segment so the arms read as whole arms. Angles are in the pattern frame at t = 0.
struct Arm {
    float thetaKink;    // radians
    float rKink;        // kpc
    float tanPitchIn;   // tan(pitch) inside the kink radius
    float tanPitchOut;  // tan(pitch) outside it
    float rMin, rMax;   // where the arm starts and ends, kpc
    float width;        // Gaussian sigma at the kink, kpc; grows outward
    float young;        // weight in young stars, glowing gas and HII regions
    float old;          // weight in the old stellar population (only the two major stellar arms)
    float dust;         // weight in dust
};

constexpr int kArmCount = 5;

struct Galaxy {
    // Rotation curve: v(R) = v0 (1 - exp(-R / rCore)), flat at v0 beyond a few kpc.
    float v0    = 233.0f;  // km/s at the Sun
    float rCore = 0.6f;    // kpc

    float omegaBar  = 38.0f * kRadPerMyrPerKmsKpc;  // bar pattern speed, rad/Myr
    float omegaArms = 24.0f * kRadPerMyrPerKmsKpc;  // spiral pattern speed, rad/Myr

    float barAngle0     = 0.0f;  // radians at t = 0, set from the Sun's position below
    float barHalfLength = 5.0f;  // kpc

    float sunR     = 8.15f;  // kpc
    float sunTheta = -kPi / 2;  // the Sun on the -y axis at t = 0

    std::array<Arm, kArmCount> arms{};
};

const Galaxy& MilkyWay();

// Angular speed of a circular orbit at radius r, rad/Myr (positive; motion is clockwise).
float AngularSpeed(const Galaxy& g, float r);

// Pattern-frame angle of an arm at radius r.
float ArmAngle(const Arm& arm, float r);

enum class Population : std::uint32_t {
    Disk    = 0,  // old thin and thick disk, on circular orbits
    Bar     = 1,  // boxy bulge and long bar, on elongated orbits that turn with the bar
    Young   = 2,  // OB stars, born in an arm and fading as they drift out of it
    Nebula  = 3,  // HII regions: short-lived, large and pink
    Nucleus = 4,  // the dense star cluster around Sgr A*
};

// Laid out as three vec4s for the GPU (std430). Meaning of a/b depends on the population:
//   Disk, Nucleus: a = (R, theta0, z, pop)                     b = colour, luminosity
//   Bar:           a = (semi-major, phase0, z, pop)            b = colour, luminosity   c.x = semi-minor
//   Young, Nebula: a = (R, birth angle in pattern frame, z, pop)  c = (lifetime Myr, phase 0..1, size, 0)
struct Star {
    float a[4];
    float b[4];
    float c[4];
};
static_assert(sizeof(Star) == 48);

struct StarCounts {
    std::uint32_t disk    = 420000;
    std::uint32_t bar     = 160000;
    std::uint32_t young   = 180000;
    std::uint32_t nebula  = 6000;
    std::uint32_t nucleus = 24000;
};

std::vector<Star> GenerateStars(const Galaxy& g, const StarCounts& counts, std::uint32_t seed);

}  // namespace sim
