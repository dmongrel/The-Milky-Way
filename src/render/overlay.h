// The overlays: a film-credit title, and labels that ride on Andromeda and on the Sun.
//
// Text is set once, at start-up, by GDI in the embedded Michroma font, into a small texture. Every
// frame the targets are projected to the screen and the overlay is rebuilt as a list of quads:
// text quads sample that texture, line quads (brackets, underlines, leaders) are anti-aliased in
// the fragment shader. They are drawn over the finished, tonemapped image.
#pragma once

#include "sim/camera.h"

#include <cstdint>
#include <random>
#include <vector>

namespace render {

// Must match `Vertex` in shaders/overlay.vert (std430, three vec4s).
struct OverlayVertex {
    float pos[2];    // NDC
    float uv[2];     // text: texture coordinates
    float color[4];  // straight alpha
    float mode;      // 0 text, 1 line
    float halfWidth; // line: half its width, pixels
    float across;    // line: signed distance from its centre line, pixels
    float pad;
};

// One string as set in the atlas.
struct TextRun {
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    float width = 0, height = 0;  // the cell, atlas pixels
    float baseline = 0;           // from the top of the cell
};

struct TextAtlas {
    std::uint32_t             width = 0, height = 0;
    std::vector<std::uint8_t> rgba;  // coverage in every channel
    TextRun                   title, andromeda, sol;
};

// Empty `rgba` if the font or GDI fails; the overlays are then simply not drawn.
TextAtlas BuildTextAtlas();

// What one window needs to place the overlays this frame.
struct OverlayView {
    sim::Mat4 viewProj;
    float     width = 0, height = 0;  // pixels
    float     focal = 0;              // pixels per unit at unit distance
    sim::Vec3 camPos;
    sim::Vec3 sol;                    // the Sun, kpc
    sim::Vec3 m31Dir, m31Right, m31Up;
    float     m31HalfWidth = 0, m31Aspect = 0;
};

class OverlayDirector {
public:
    explicit OverlayDirector(const TextAtlas& atlas);

    // Once per frame: runs the schedule.
    void Advance(double seconds);
    // Per window: appends that window's quads to `out`.
    void Emit(const OverlayView& view, std::vector<OverlayVertex>& out);
    // Once per frame, after every Emit: lets labels fade early when their target leaves the screen.
    void EndFrame();

private:
    struct Show {
        double start = -1e9;  // when the current showing began
        double next  = 0.0;   // when the next one may begin
        float  alpha = 0.0f;  // this frame's envelope
        float  seen  = 0.0f;  // eased on-screen factor
        bool   onScreen = false;
    };

    float Envelope(const Show& s) const;
    void  Label(const OverlayView& view, const TextRun& run, float x0, float y0, float x1, float y1, float alpha,
                std::vector<OverlayVertex>& out) const;

    TextRun         titleRun_, andromedaRun_, solRun_;
    std::mt19937    rng_;
    bool            forced_   = false;
    bool            started_  = false;
    double          seconds_  = 0.0;
    double          lastSeconds_ = 0.0;
    Show            title_, andromeda_, sol_;
};

}  // namespace render
