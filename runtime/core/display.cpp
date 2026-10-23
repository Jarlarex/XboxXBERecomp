#include "core/display.h"

#include <algorithm>
#include <cmath>

namespace display {

namespace {

constexpr const char* kAspectNames[] = {"auto", "16:9", "4:3"};
constexpr double kMaxTexels = 16384;  // D3D12's largest 2D texture side
constexpr float kMinScale = 0.1f;

double Unit(double v) { return std::clamp(v, 0.0, 1.0); }

}  // namespace

bool ParseAspect(std::string_view text, Aspect* out) {
    for (uint8_t i = 0; i < 3; i++) {
        if (text == kAspectNames[i]) {
            *out = Aspect(i);
            return true;
        }
    }
    return false;
}

const char* AspectName(Aspect a) { return kAspectNames[uint8_t(a)]; }

double FrameAspect(Aspect a, uint32_t w, uint32_t h, bool any_aspect) {
    switch (a) {
        case Aspect::Wide: return kWide;
        case Aspect::Classic: return kClassic;
        case Aspect::Auto: break;
    }
    if (!w || !h) return kClassic;
    double window = double(w) / double(h);
    if (!any_aspect) return window >= 1.5 ? kWide : kClassic;
    return std::max(window, kClassic);
}

// a hair over 4:3, so an output of 4:3 that rounds to a pixel more stays 4:3
bool Widescreen(double aspect) { return aspect > kClassic * 1.01; }

Rect FrameRect(double aspect, uint32_t w, uint32_t h) {
    Rect r{0, 0, w, h};
    if (!w || !h || aspect <= 0) return r;
    if (double(w) / double(h) > aspect) {
        r.w = std::min(w, uint32_t(std::lround(h * aspect)));
        r.x = (w - r.w) / 2;
    } else {
        r.h = std::min(h, uint32_t(std::lround(w / aspect)));
        r.y = (h - r.h) / 2;
    }
    return r;
}

float RenderScale(uint32_t percent, const Rect& frame, uint32_t guest_w, uint32_t guest_h) {
    if (!guest_w || !guest_h) return 1.0f;
    double cover = std::max(double(frame.w) / guest_w, double(frame.h) / guest_h);
    double cap = kMaxTexels / (2.0 * std::max(guest_w, guest_h));
    return float(std::clamp(percent / 100.0 * cover, double(kMinScale), cap));
}

uint32_t MsaaSamples(int64_t value) {
    if (value >= 8) return 8;
    if (value >= 4) return 4;
    if (value >= 2) return 2;
    return 1;
}

uint32_t Anisotropy(int64_t value) {
    if (value >= 16) return 16;
    for (uint32_t level : {8u, 4u, 2u})
        if (value >= level) return level;
    return 0;
}

double CameraAspectScale(double aspect) { return aspect / kWide; }

double LayoutScale(double aspect) { return std::max(1.0, aspect / kWide); }

double TransferSdr(double encoded, double gamma) { return std::pow(Unit(encoded), gamma / kPcGamma); }

double TransferNits(double encoded, const Transfer& t) {
    double x = std::pow(Unit(encoded), t.gamma);
    double nits = t.paper_white * x;
    if (t.peak > t.paper_white) {
        double k = Unit((x - kHdrKnee) / (1.0 - kHdrKnee));
        nits += (t.peak - t.paper_white) * k * k * (3.0 - 2.0 * k);
    }
    return nits;
}

double TransferScRgb(double encoded, const Transfer& t) { return TransferNits(encoded, t) / kScRgbNits; }

Transfer ClampTransfer(Transfer t) {
    t.gamma = std::clamp(t.gamma, 1.8, 2.8);
    t.paper_white = std::clamp(t.paper_white, 80.0, 500.0);
    t.peak = std::clamp(t.peak, t.paper_white, 10000.0);
    return t;
}

}  // namespace display
