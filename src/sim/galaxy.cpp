#include "sim/galaxy.h"

#include <algorithm>
#include <cmath>

namespace sim {
namespace {

constexpr float Deg(float degrees) { return degrees * kPi / 180.0f; }

// Reid et al. 2019 measure Galactocentric azimuth beta from the Sun, increasing in the direction of
// rotation, i.e. clockwise from the NGP. In this frame angles increase counter-clockwise.
float FromReidAzimuth(const Galaxy& g, float betaDegrees) { return g.sunTheta - Deg(betaDegrees); }

Galaxy Build() {
    Galaxy g;

    // The bar's near end leads the Sun-centre line by about 28 degrees (Shen & Zheng 2020).
    g.barAngle0 = FromReidAzimuth(g, 28.0f);

    // Kink azimuths and radii are Reid et al. 2019, Table 2. Pitch angles follow it where the arm
    // was measured; the changes, all to make whole arms out of measured segments, are:
    //  - Norma-Outer: outside the kink, 9.1 deg instead of Norma's -1 deg, which carries the arm
    //    one turn round to Reid's Outer-arm kink (12.24 kpc), joining the two as the paper suggests.
    //  - Sagittarius-Carina: 6 deg inside the kink instead of 1 deg, so it starts near the bar's
    //    far end rather than circling the centre.
    // Widths are about 1.3 times Reid's, which reads better at galactic scale.
    g.arms[0] = {FromReidAzimuth(g, 18.0f), 4.46f, std::tan(Deg(19.5f)), std::tan(Deg(9.1f)),
                 3.4f, 16.0f, 0.30f, 0.8f, 0.0f, 0.8f};  // Norma-Outer
    g.arms[1] = {FromReidAzimuth(g, 23.0f), 4.91f, std::tan(Deg(12.1f)), std::tan(Deg(14.1f)),
                 3.6f, 17.5f, 0.35f, 1.0f, 1.0f, 1.0f};  // Scutum-Centaurus-OSC (major stellar arm)
    g.arms[2] = {FromReidAzimuth(g, 24.0f), 6.04f, std::tan(Deg(6.0f)), std::tan(Deg(17.1f)),
                 4.3f, 14.0f, 0.35f, 0.9f, 0.0f, 0.9f};  // Sagittarius-Carina
    g.arms[3] = {FromReidAzimuth(g, 40.0f), 8.87f, std::tan(Deg(8.7f)), std::tan(Deg(10.3f)),
                 5.6f, 17.0f, 0.40f, 0.9f, 1.0f, 0.9f};  // Perseus (major stellar arm)
    g.arms[4] = {FromReidAzimuth(g, 9.0f), 8.26f, std::tan(Deg(11.4f)), std::tan(Deg(11.4f)),
                 7.0f, 10.5f, 0.35f, 0.55f, 0.0f, 0.4f};  // Local arm, a segment
    return g;
}

// xoshiro128+ : small, fast, and the same catalogue every run for a given seed.
class Rng {
public:
    explicit Rng(std::uint32_t seed) {
        std::uint64_t z = seed + 0x9E3779B97F4A7C15ull;
        for (auto& word : s_) {
            z            = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z            = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            word         = static_cast<std::uint32_t>(z ^ (z >> 31));
            z           += 0x9E3779B97F4A7C15ull;
        }
    }

    float Uniform() {  // [0, 1)
        const std::uint32_t result = s_[0] + s_[3];
        const std::uint32_t t      = s_[1] << 9;
        s_[2] ^= s_[0];
        s_[3] ^= s_[1];
        s_[1] ^= s_[2];
        s_[0] ^= s_[3];
        s_[2] ^= t;
        s_[3] = (s_[3] << 11) | (s_[3] >> 21);
        return static_cast<float>(result >> 8) * (1.0f / 16777216.0f);
    }

    float Range(float lo, float hi) { return lo + (hi - lo) * Uniform(); }

    float Gaussian() {
        const float u1 = std::fmax(Uniform(), 1e-7f);
        const float u2 = Uniform();
        return std::sqrt(-2.0f * std::log(u1)) * std::cos(2.0f * kPi * u2);
    }

    // Vertical profile proportional to sech^2(z / z0).
    float Sech2(float z0) {
        const float u = std::fmin(std::fmax(Uniform(), 1e-6f), 1.0f - 1e-6f);
        return z0 * std::atanh(2.0f * u - 1.0f);
    }

private:
    std::uint32_t s_[4];
};

void SetColour(Star& s, float r, float g, float b, float luminosity) {
    s.b[0] = r;
    s.b[1] = g;
    s.b[2] = b;
    s.b[3] = luminosity;
}

// Old stars: mostly K and G giants, some cooler M giants and a few hotter dwarfs.
void OldColour(Rng& rng, Star& s, float luminosity) {
    const float pick = rng.Uniform();
    if (pick < 0.45f) {
        SetColour(s, 1.00f, 0.78f, 0.55f, luminosity);  // K
    } else if (pick < 0.80f) {
        SetColour(s, 1.00f, 0.90f, 0.76f, luminosity);  // G
    } else if (pick < 0.92f) {
        SetColour(s, 1.00f, 0.62f, 0.42f, luminosity);  // M
    } else {
        SetColour(s, 0.88f, 0.92f, 1.00f, luminosity);  // F/A
    }
}

float Lognormal(Rng& rng, float sigma) { return std::exp(sigma * rng.Gaussian() - 0.5f * sigma * sigma); }

void AddDisk(const Galaxy&, Rng& rng, std::uint32_t count, std::vector<Star>& out) {
    for (std::uint32_t i = 0; i < count; ++i) {
        const bool thick  = rng.Uniform() < 0.12f;
        const float scale = thick ? 2.0f : 2.6f;  // Bland-Hawthorn & Gerhard 2016
        float r;
        do {
            // An exponential disk has surface density e^{-R/h}; R itself then follows Gamma(2, h).
            r = -scale * std::log(std::fmax(rng.Uniform() * rng.Uniform(), 1e-12f));
        } while (r < 0.9f || r > 18.0f);

        Star s{};
        s.a[0] = r;
        s.a[1] = rng.Range(-kPi, kPi);
        s.a[2] = std::fmax(std::fmin(rng.Sech2(thick ? 0.9f : 0.3f), 3.0f), -3.0f);
        s.a[3] = static_cast<float>(Population::Disk);
        OldColour(rng, s, Lognormal(rng, 0.9f));
        out.push_back(s);
    }
}

void AddBar(const Galaxy& g, Rng& rng, std::uint32_t count, std::vector<Star>& out) {
    for (std::uint32_t i = 0; i < count; ++i) {
        Star        s{};
        float       a, q, zSigma;
        const float pick = rng.Uniform();
        if (pick < 0.62f) {
            // The boxy/peanut bulge: the inner bar seen end-on from the Sun (Wegg et al. 2015).
            do {
                a = -0.7f * std::log(std::fmax(rng.Uniform(), 1e-6f));
            } while (a > 2.4f || a < 0.05f);
            q      = 0.43f;
            zSigma = 0.40f * (1.0f - 0.3f * a / 2.4f);
        } else {
            // The long thin bar out to about 5 kpc, thinning toward its ends.
            a      = rng.Range(1.2f, g.barHalfLength) * std::sqrt(rng.Range(0.3f, 1.0f));
            q      = 0.22f;
            zSigma = 0.15f;
        }
        s.a[0] = a;
        s.a[1] = rng.Range(-kPi, kPi);
        s.a[2] = zSigma * rng.Gaussian();
        s.a[3] = static_cast<float>(Population::Bar);
        s.c[0] = a * q * rng.Range(0.7f, 1.3f);
        SetColour(s, 1.00f, 0.76f, 0.50f, Lognormal(rng, 0.8f) * 1.2f);
        out.push_back(s);
    }
}

void AddNucleus(const Galaxy&, Rng& rng, std::uint32_t count, std::vector<Star>& out) {
    for (std::uint32_t i = 0; i < count; ++i) {
        Star  s{};
        float r;
        do {
            r = -0.07f * std::log(std::fmax(rng.Uniform() * rng.Uniform(), 1e-9f));
        } while (r < 0.004f || r > 0.5f);
        s.a[0] = r;
        s.a[1] = rng.Range(-kPi, kPi);
        s.a[2] = 0.5f * r * rng.Gaussian();
        s.a[3] = static_cast<float>(Population::Nucleus);
        SetColour(s, 1.00f, 0.84f, 0.66f, Lognormal(rng, 0.7f) * 1.5f);
        out.push_back(s);
    }
}

// Picks an arm in proportion to its young-star weight times its length.
int PickArm(const Galaxy& g, Rng& rng) {
    float total = 0.0f;
    for (const Arm& arm : g.arms) total += arm.young * (arm.rMax - arm.rMin);
    float x = rng.Uniform() * total;
    for (int i = 0; i < kArmCount; ++i) {
        x -= g.arms[i].young * (g.arms[i].rMax - g.arms[i].rMin);
        if (x <= 0.0f) return i;
    }
    return kArmCount - 1;
}

// Young stars are born in clusters along the arms. Each cluster is coeval: its stars share a
// lifetime and a phase, so they light up and fade together as they drift out of the arm.
void AddYoung(const Galaxy& g, Rng& rng, std::uint32_t stars, std::uint32_t nebulae, std::vector<Star>& out) {
    constexpr std::uint32_t kPerCluster = 24;
    const std::uint32_t     clusters    = (stars + kPerCluster - 1) / kPerCluster;
    const std::uint32_t     nebulaEvery = nebulae ? std::max<std::uint32_t>(1, clusters / nebulae) : 0;

    for (std::uint32_t c = 0; c < clusters; ++c) {
        const Arm& arm = g.arms[PickArm(g, rng)];
        float      r;
        do {
            r = rng.Range(arm.rMin, arm.rMax);
        } while (rng.Uniform() > std::exp(-(r - arm.rMin) / 6.0f));

        const float tanPitch = r > arm.rKink ? arm.tanPitchOut : arm.tanPitchIn;
        const float sinPitch = tanPitch / std::sqrt(1.0f + tanPitch * tanPitch);
        const float width    = arm.width + 0.042f * (r - arm.rKink);

        // Stars form where the gas is compressed, just downstream of the dust lane: a small
        // negative (clockwise) offset across the arm.
        const float across = (rng.Gaussian() * 0.8f * std::fmax(width, 0.15f) - 0.10f) / (r * sinPitch);
        const float theta  = ArmAngle(arm, r) + across;
        const float z      = 0.05f * rng.Gaussian();

        const float lifetime = rng.Range(25.0f, 60.0f);
        const float phase    = rng.Uniform();
        const float spread   = rng.Range(0.03f, 0.12f);  // cluster radius, kpc

        for (std::uint32_t k = 0; k < kPerCluster; ++k) {
            Star        s{};
            const float dr = spread * rng.Gaussian();
            s.a[0]         = std::fmax(r + dr, 0.5f);
            s.a[1]         = theta + spread * rng.Gaussian() / r;
            s.a[2]         = z + 0.3f * spread * rng.Gaussian();
            s.a[3]         = static_cast<float>(Population::Young);
            const float hot = rng.Uniform();
            if (hot < 0.55f) {
                SetColour(s, 0.62f, 0.75f, 1.00f, Lognormal(rng, 1.0f) * 3.0f);  // O/B
            } else if (hot < 0.85f) {
                SetColour(s, 0.82f, 0.88f, 1.00f, Lognormal(rng, 0.9f) * 1.8f);  // A
            } else {
                SetColour(s, 1.00f, 0.80f, 0.60f, Lognormal(rng, 0.8f) * 2.0f);  // young red supergiants
            }
            s.c[0] = lifetime;
            s.c[1] = phase;
            s.c[2] = rng.Range(1.0f, 1.8f);
            out.push_back(s);
        }

        if (nebulaEvery && c % nebulaEvery == 0) {
            Star s{};
            s.a[0] = r;
            s.a[1] = theta;
            s.a[2] = z;
            s.a[3] = static_cast<float>(Population::Nebula);
            SetColour(s, 1.00f, 0.38f, 0.52f, Lognormal(rng, 0.6f) * 4.0f);
            s.c[0] = lifetime;
            s.c[1] = phase;
            s.c[2] = rng.Range(3.0f, 7.0f) * (0.6f + spread * 6.0f);
            out.push_back(s);
        }
    }
}

}  // namespace

const Galaxy& MilkyWay() {
    static const Galaxy galaxy = Build();
    return galaxy;
}

float AngularSpeed(const Galaxy& g, float r) {
    r = std::fmax(r, 1e-3f);
    return g.v0 * (1.0f - std::exp(-r / g.rCore)) / r * kRadPerMyrPerKmsKpc;
}

float ArmAngle(const Arm& arm, float r) {
    const float tanPitch = r > arm.rKink ? arm.tanPitchOut : arm.tanPitchIn;
    return arm.thetaKink + std::log(r / arm.rKink) / tanPitch;
}

std::vector<Star> GenerateStars(const Galaxy& g, const StarCounts& counts, std::uint32_t seed) {
    Rng               rng(seed);
    std::vector<Star> stars;
    stars.reserve(counts.disk + counts.bar + counts.young + counts.nebula + counts.nucleus + 64);
    AddDisk(g, rng, counts.disk, stars);
    AddBar(g, rng, counts.bar, stars);
    AddNucleus(g, rng, counts.nucleus, stars);
    AddYoung(g, rng, counts.young, counts.nebula, stars);
    return stars;
}

}  // namespace sim
