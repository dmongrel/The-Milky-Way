#include "render/overlay.h"

#include "app/log.h"
#include "resource.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace render {
namespace {

// ---- timing --------------------------------------------------------------------------------

constexpr double kFadeIn  = 1.5;   // seconds
constexpr double kHold    = 10.0;  // fully on screen
constexpr double kFadeOut = 2.0;
constexpr double kShowLength = kFadeIn + kHold + kFadeOut;

// The title returns every three to four minutes, start to start; the first comes soon after the
// saver starts. Labels come and go on their own, independently of each other.
constexpr double kTitleFirst = 25.0, kTitleMin = 180.0, kTitleMax = 240.0;
constexpr double kLabelFirstMin = 12.0, kLabelFirstMax = 60.0;
constexpr double kLabelGapMin = 30.0, kLabelGapMax = 100.0;  // after one showing ends

// ---- look ----------------------------------------------------------------------------------

constexpr float kInk[3]    = {0.90f, 0.93f, 1.00f};  // a cool white, a little blue of the stars
constexpr float kTextAlpha = 0.92f;
constexpr float kLineAlpha = 0.70f;

// Font sizes in the atlas, in pixels of em height. Big enough to stay sharp on a 1440p screen.
constexpr int kTitleEm = 112;
constexpr int kLabelEm = 56;

float Smooth(double a, double b, double x) {
    const double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
    return static_cast<float>(t * t * (3.0 - 2.0 * t));
}

// ---- geometry ------------------------------------------------------------------------------

struct Px {
    float x, y;
};

struct Writer {
    float                       width, height;
    std::vector<OverlayVertex>& out;

    OverlayVertex V(Px p) const {
        OverlayVertex v{};
        v.pos[0] = p.x / width * 2.0f - 1.0f;  // Vulkan NDC: y down, like pixels
        v.pos[1] = p.y / height * 2.0f - 1.0f;
        return v;
    }

    // An anti-aliased line, `w` pixels wide. Ends are extended by half the width, so lines that
    // meet at a corner close it.
    void Line(Px a, Px b, float w, float alpha) {
        float dx = b.x - a.x, dy = b.y - a.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-3f) return;
        dx /= len;
        dy /= len;
        const float half = 0.5f * w, reach = half + 1.0f;  // one pixel of feather each side
        const Px    a2{a.x - dx * half, a.y - dy * half}, b2{b.x + dx * half, b.y + dy * half};
        const float nx = -dy * reach, ny = dx * reach;
        const Px    corners[4] = {{a2.x + nx, a2.y + ny}, {b2.x + nx, b2.y + ny}, {b2.x - nx, b2.y - ny}, {a2.x - nx, a2.y - ny}};
        const float across[4]  = {reach, reach, -reach, -reach};
        const int   order[6]   = {0, 1, 2, 0, 2, 3};
        for (int i : order) {
            OverlayVertex v = V(corners[i]);
            std::memcpy(v.color, kInk, sizeof(kInk));
            v.color[3]  = alpha * kLineAlpha;
            v.mode      = 1.0f;
            v.halfWidth = half;
            v.across    = across[i];
            out.push_back(v);
        }
    }

    // A string from the atlas with the top-left of its cell at `at`, scaled by `scale`.
    void Text(const TextRun& run, Px at, float scale, float alpha) {
        const float w = run.width * scale, h = run.height * scale;
        const Px    corners[4] = {{at.x, at.y}, {at.x + w, at.y}, {at.x + w, at.y + h}, {at.x, at.y + h}};
        const float uvs[4][2]  = {{run.u0, run.v0}, {run.u1, run.v0}, {run.u1, run.v1}, {run.u0, run.v1}};
        const int   order[6]   = {0, 1, 2, 0, 2, 3};
        for (int i : order) {
            OverlayVertex v = V(corners[i]);
            v.uv[0] = uvs[i][0];
            v.uv[1] = uvs[i][1];
            std::memcpy(v.color, kInk, sizeof(kInk));
            v.color[3] = alpha * kTextAlpha;
            out.push_back(v);
        }
    }
};

// Clip-space projection; w <= 0 is behind the camera.
void Project(const sim::Mat4& m, float x, float y, float z, float w, float out[4]) {
    for (int r = 0; r < 4; ++r) out[r] = m.m[r] * x + m.m[4 + r] * y + m.m[8 + r] * z + m.m[12 + r] * w;
}

bool ToScreen(const OverlayView& view, const float clip[4], Px& p) {
    if (clip[3] <= 1e-4f) return false;
    p = {(clip[0] / clip[3] * 0.5f + 0.5f) * view.width, (clip[1] / clip[3] * 0.5f + 0.5f) * view.height};
    return true;
}

bool OnScreen(const OverlayView& view, float x0, float y0, float x1, float y1) {
    // The whole box, with room around it for the label.
    const float m = 0.02f * view.height;
    return x0 > m && y0 > m && x1 < view.width - m && y1 < view.height - m;
}

// ---- the text atlas ------------------------------------------------------------------------

struct GdiText {
    HDC     dc   = nullptr;
    HFONT   font = nullptr;
    HGDIOBJ old  = nullptr;
};

}  // namespace

TextAtlas BuildTextAtlas() {
    TextAtlas atlas;

    // The font comes from the .scr itself, private to this process: never installed.
    HMODULE     module = GetModuleHandleW(nullptr);
    HRSRC       found  = FindResourceW(module, MAKEINTRESOURCEW(IDR_OVERLAY_FONT), MAKEINTRESOURCEW(10));
    HGLOBAL     loaded = found ? LoadResource(module, found) : nullptr;
    void*       data   = loaded ? LockResource(loaded) : nullptr;
    const DWORD size   = found ? SizeofResource(module, found) : 0;
    DWORD       fonts  = 0;
    HANDLE      added  = data ? AddFontMemResourceEx(data, size, nullptr, &fonts) : nullptr;
    if (!added) {
        app::Log("overlay: Michroma could not be loaded");
        return atlas;
    }

    HDC dc = CreateCompatibleDC(nullptr);
    auto makeFont = [](int em) {
        return CreateFontW(-em, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS,
                           CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Michroma");
    };
    HFONT big = makeFont(kTitleEm), small = makeFont(kLabelEm);

    struct Item {
        const wchar_t* text;
        HFONT          font;
        TextRun*       run;
        SIZE           size{};
        int            ascent = 0;
        int            x = 0, y = 0;
    };
    Item items[3] = {{L"The Milky Way Galaxy", big, &atlas.title}, {L"Andromeda", small, &atlas.andromeda},
                     {L"Sol", small, &atlas.sol}};

    // Measure, then stack the strings one under another with padding for the mips.
    const int pad = 8;
    int       w = 0, h = pad;
    wchar_t   face[64] = {};
    for (Item& it : items) {
        HGDIOBJ old = SelectObject(dc, it.font);
        GetTextExtentPoint32W(dc, it.text, static_cast<int>(wcslen(it.text)), &it.size);
        TEXTMETRICW tm{};
        GetTextMetricsW(dc, &tm);
        it.ascent = tm.tmAscent;
        GetTextFaceW(dc, 64, face);
        SelectObject(dc, old);
        it.x = pad;
        it.y = h;
        h += it.size.cy + pad * 2;
        w = std::max<int>(w, it.size.cx + pad * 2);
    }
    if (std::wcscmp(face, L"Michroma") != 0) app::Log("overlay: GDI chose %ls, not Michroma", face);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize        = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth       = w;
    bi.bmiHeader.biHeight      = -h;  // top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void*   bits = nullptr;
    HBITMAP bmp  = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp && bits) {
        HGDIOBJ oldBmp = SelectObject(dc, bmp);
        std::memset(bits, 0, std::size_t(w) * h * 4);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        for (Item& it : items) {
            HGDIOBJ old = SelectObject(dc, it.font);
            TextOutW(dc, it.x, it.y, it.text, static_cast<int>(wcslen(it.text)));
            SelectObject(dc, old);
            it.run->u0       = float(it.x) / w;
            it.run->v0       = float(it.y) / h;
            it.run->u1       = float(it.x + it.size.cx) / w;
            it.run->v1       = float(it.y + it.size.cy) / h;
            it.run->width    = float(it.size.cx);
            it.run->height   = float(it.size.cy);
            it.run->baseline = float(it.ascent);
        }
        GdiFlush();
        // White on black: the green channel is the coverage. Put it in every channel.
        const auto* src = static_cast<const std::uint8_t*>(bits);
        atlas.rgba.resize(std::size_t(w) * h * 4);
        for (std::size_t i = 0; i < std::size_t(w) * h; ++i) {
            std::memset(&atlas.rgba[i * 4], src[i * 4 + 1], 4);
        }
        atlas.width  = static_cast<std::uint32_t>(w);
        atlas.height = static_cast<std::uint32_t>(h);
        SelectObject(dc, oldBmp);
        DeleteObject(bmp);
    }
    DeleteObject(big);
    DeleteObject(small);
    DeleteDC(dc);
    RemoveFontMemResourceEx(added);
    app::Log("overlay: text atlas %ux%u", atlas.width, atlas.height);
    return atlas;
}

// ---- the director --------------------------------------------------------------------------

OverlayDirector::OverlayDirector(const TextAtlas& atlas)
    : titleRun_(atlas.title), andromedaRun_(atlas.andromeda), solRun_(atlas.sol) {
    LARGE_INTEGER seed{};
    QueryPerformanceCounter(&seed);
    rng_.seed(static_cast<std::uint32_t>(seed.QuadPart));
    // MILKY_WAY_OVERLAYS: everything on, whenever its target is on screen, for captures.
    forced_ = GetEnvironmentVariableW(L"MILKY_WAY_OVERLAYS", nullptr, 0) > 0;
}

float OverlayDirector::Envelope(const Show& s) const {
    if (forced_) return 1.0f;
    const double t = seconds_ - s.start;
    if (t < 0.0 || t > kShowLength) return 0.0f;
    return Smooth(0.0, kFadeIn, t) * (1.0f - Smooth(kFadeIn + kHold, kShowLength, t));
}

void OverlayDirector::Advance(double seconds) {
    auto uniform = [this](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng_); };
    if (!started_) {
        started_       = true;
        lastSeconds_   = seconds;
        title_.next    = seconds + kTitleFirst;
        andromeda_.next = seconds + uniform(kLabelFirstMin, kLabelFirstMax);
        sol_.next      = seconds + uniform(kLabelFirstMin, kLabelFirstMax);
    }
    seconds_ = seconds;

    if (seconds >= title_.next) {
        title_.start = seconds;
        title_.next  = seconds + uniform(kTitleMin, kTitleMax);
    }
    for (Show* label : {&andromeda_, &sol_}) {
        if (seconds < label->next) continue;
        if (label->onScreen) {
            label->start = seconds;
            label->next  = seconds + kShowLength + uniform(kLabelGapMin, kLabelGapMax);
        } else {
            label->next = seconds + 3.0;  // its target is off screen: look again shortly
        }
    }

    // Labels follow their target's visibility with a short ease, so leaving the screen fades them.
    const float dt = static_cast<float>(std::clamp(seconds - lastSeconds_, 0.0, 0.25));
    lastSeconds_   = seconds;
    for (Show* label : {&andromeda_, &sol_}) {
        const float target = label->onScreen ? 1.0f : 0.0f;
        label->seen += (target - label->seen) * std::min(1.0f, dt * 3.0f);
        label->onScreen = false;  // Emit sets it again for this frame
    }
    title_.alpha     = Envelope(title_);
    andromeda_.alpha = Envelope(andromeda_);
    sol_.alpha       = Envelope(sol_);
}

void OverlayDirector::EndFrame() {
    // A label whose target has gone stops its showing once it has faded out.
    for (Show* label : {&andromeda_, &sol_}) {
        if (!label->onScreen && label->seen < 0.01f && seconds_ - label->start < kShowLength) label->start = -1e9;
    }
}

void OverlayDirector::Label(const OverlayView& view, const TextRun& run, float x0, float y0, float x1, float y1,
                            float alpha, std::vector<OverlayVertex>& out) const {
    Writer      w{view.width, view.height, out};
    const float k     = view.height / 1080.0f;
    const float lineW = std::max(1.3f * k, 1.0f);

    // Brackets: a vertical stroke with short serifs pointing in, one each side of the target.
    const float serif = std::min(0.22f * (y1 - y0), 0.30f * (x1 - x0));
    w.Line({x0 + serif, y0}, {x0, y0}, lineW, alpha);
    w.Line({x0, y0}, {x0, y1}, lineW, alpha);
    w.Line({x0, y1}, {x0 + serif, y1}, lineW, alpha);
    w.Line({x1 - serif, y0}, {x1, y0}, lineW, alpha);
    w.Line({x1, y0}, {x1, y1}, lineW, alpha);
    w.Line({x1, y1}, {x1 - serif, y1}, lineW, alpha);

    // The label sits above and outside the brackets, on the side with more room. Its underline
    // runs along the text, and a leader joins the underline's inner end to the nearest corner.
    const float cell  = 0.026f * view.height;  // label cell height on screen
    const float scale = cell / run.height;
    const float textW = run.width * scale;
    const float gap   = 0.6f * cell;
    const float ulY   = std::max(y0 - gap, cell * 1.6f);
    const float top   = ulY - 0.18f * cell - run.baseline * scale;
    const bool  right = 0.5f * (x0 + x1) < 0.6f * view.width;
    const float pad   = 0.25f * cell;
    if (right) {
        const float tx = x1 + gap;
        w.Text(run, {tx, top}, scale, alpha);
        w.Line({tx, ulY}, {tx + textW + pad, ulY}, lineW, alpha);
        w.Line({tx, ulY}, {x1, y0}, lineW, alpha);
    } else {
        const float tx = x0 - gap - textW;
        w.Text(run, {tx, top}, scale, alpha);
        w.Line({tx - pad, ulY}, {tx + textW, ulY}, lineW, alpha);
        w.Line({tx + textW, ulY}, {x0, y0}, lineW, alpha);
    }
}

void OverlayDirector::Emit(const OverlayView& view, std::vector<OverlayVertex>& out) {
    if (view.height < 300.0f || titleRun_.height <= 0.0f) return;  // the preview pane: too small to read
    const float k = view.height / 1080.0f;

    // ---- Sol: a box about a kiloparsec across, at the Sun, scaled by perspective.
    {
        float clip[4];
        Project(view.viewProj, view.sol.x, view.sol.y, view.sol.z, 1.0f, clip);
        Px c{};
        if (ToScreen(view, clip, c)) {
            const float half = std::max(0.5f * view.focal / clip[3], 9.0f * k);
            const float x0 = c.x - half, x1 = c.x + half, y0 = c.y - half, y1 = c.y + half;
            if (OnScreen(view, x0, y0, x1, y1)) {
                sol_.onScreen = true;
                const float a = sol_.alpha * sol_.seen;
                if (a > 0.003f) Label(view, solRun_, x0, y0, x1, y1, a, out);
            }
        }
    }

    // ---- Andromeda: the bright part of its billboard, as the sky shader draws it.
    {
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        bool  ok = true;
        const float hw = view.m31HalfWidth, hh = view.m31HalfWidth * view.m31Aspect;
        // In the billboard's plane: across its width, and down from the nucleus (the image's
        // nucleus is the centre). Only the inner, unfaded part is boxed.
        const float us[2] = {-0.62f * hw, 0.62f * hw};
        const float vs[2] = {-0.55f * hh, 0.45f * hh};
        for (float u : us) {
            for (float v : vs) {
                const sim::Vec3 d{view.m31Dir.x + view.m31Right.x * u + view.m31Up.x * v,
                                  view.m31Dir.y + view.m31Right.y * u + view.m31Up.y * v,
                                  view.m31Dir.z + view.m31Right.z * u + view.m31Up.z * v};
                float clip[4];
                Project(view.viewProj, d.x, d.y, d.z, 0.0f, clip);  // a direction: at infinity
                Px p{};
                ok &= ToScreen(view, clip, p);
                x0 = std::min(x0, p.x);
                y0 = std::min(y0, p.y);
                x1 = std::max(x1, p.x);
                y1 = std::max(y1, p.y);
            }
        }
        if (ok && OnScreen(view, x0, y0, x1, y1)) {
            andromeda_.onScreen = true;
            const float a = andromeda_.alpha * andromeda_.seen;
            if (a > 0.003f) Label(view, andromedaRun_, x0, y0, x1, y1, a, out);
        }
    }

    // ---- The title: a film credit, fixed to the lower right.
    if (title_.alpha > 0.003f) {
        Writer      w{view.width, view.height, out};
        const float cell   = 0.060f * view.height;
        const float scale  = cell / titleRun_.height;
        const float margin = 0.055f * view.height;
        const float right  = view.width - margin * 1.2f;
        const float textW  = titleRun_.width * scale;
        const float ulY    = view.height - margin;
        const float top    = ulY - 0.16f * cell - titleRun_.baseline * scale;
        w.Text(titleRun_, {right - textW, top}, scale, title_.alpha);
        w.Line({right - textW, ulY}, {right, ulY}, std::max(2.0f * k, 1.0f), title_.alpha);
    }
}

}  // namespace render
