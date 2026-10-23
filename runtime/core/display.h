#pragma once
// The display maths: the shape and place of the title's picture in the window, the render scale, the values a
// widescreen title is given, and the output transfer at present time (gamma, HDR). Portable: the settings file is read in
// settings.cpp, the renderer applies these in gpu/d3d12.cpp and the same transfer in its present shader.
#include <cstdint>
#include <string_view>

namespace display {

inline constexpr double kClassic = 4.0 / 3.0;  // the console's 4:3
inline constexpr double kWide = 16.0 / 9.0;    // the console's widescreen

// aspect_ratio: the window's, or the console's 16:9 or 4:3
enum class Aspect : uint8_t { Auto, Wide, Classic };
bool ParseAspect(std::string_view text, Aspect* out);
const char* AspectName(Aspect a);

// the picture's width over its height for a window or output of w x h. Auto: for a title whose hooks follow any aspect
// (GameInfo::any_aspect) the window's own, never narrower than 4:3; for any other title 16:9 in a window 3:2 or wider,
// else 4:3, the two shapes a console title draws
double FrameAspect(Aspect a, uint32_t w, uint32_t h, bool any_aspect);
// the console reports a widescreen television: the picture is wider than 4:3
bool Widescreen(double aspect);

struct Rect {
    uint32_t x = 0, y = 0, w = 0, h = 0;
};
// the largest rectangle of the aspect in a window of w x h, centred; the rest stays black
Rect FrameRect(double aspect, uint32_t w, uint32_t h);
// the same inside another rectangle (a movie of its own shape inside the picture)
Rect FitRect(double aspect, const Rect& outer);
// whether a display mode of w x h has the screen's shape (within 1%): a display fills the screen with any mode it is
// given, so only such a mode shows the picture unstretched
bool SameShape(uint32_t w, uint32_t h, uint32_t screen_w, uint32_t screen_h);

// host pixels per console pixel of every render target: render_percent of what covers the picture's rectangle with one
// host pixel per window pixel on both axes, for a title frame of guest_w x guest_h; capped so a target of up to twice the
// frame each way (a multisampled back buffer) stays within D3D12's 16384 texels, and no lower than 0.1
float RenderScale(uint32_t percent, const Rect& frame, uint32_t guest_w, uint32_t guest_h);

// msaa: 1, 2, 4 or 8 samples, a value in between taken down to the one below
uint32_t MsaaSamples(int64_t value);
// anisotropic_filtering: 0 (the title's own), 2, 4, 8 or 16, a value in between taken down
uint32_t Anisotropy(int64_t value);

// A widescreen title draws anamorphic 16:9 in its 4:3 frame (Forza Motorsport: docs/re/forza-motorsport.md, "Widescreen");
// for a picture of another aspect: what its 3D views' aspect is multiplied by (wider: more seen to the sides at the
// title's vertical field of view), and what the width of a 2D projection it lays out for 16:9 is multiplied by. A 2D
// layout for 16:9 has pieces at fixed places out to 16:9's edges, so a narrower picture shows it whole and a little
// narrower (factor 1), a wider one at its own proportions, centred
double CameraAspectScale(double aspect);
double LayoutScale(double aspect);

// The output transfer of one channel after the title's gamma ramp, encoded 0 to 1. SDR: what goes to a display of gamma
// 2.2 so it shows the channel as one of display_gamma would. HDR: the channel decoded with display_gamma to linear light,
// at paper white below the knee, rising smoothly to the peak at full white, in scRGB (1 = 80 nits).
struct Transfer {
    double gamma = 2.2;
    bool hdr = false;
    double paper_white = 200;  // nits
    double peak = 400;         // nits, at or above paper white
};
inline constexpr double kPcGamma = 2.2;
inline constexpr double kHdrKnee = 0.6;      // linear light below which HDR keeps the SDR picture
inline constexpr double kScRgbNits = 80.0;   // scRGB 1.0
double TransferSdr(double encoded, double gamma);
double TransferNits(double encoded, const Transfer& t);
double TransferScRgb(double encoded, const Transfer& t);
// the settings' values in their ranges: display_gamma 1.8 to 2.8, paper white 80 to 500 nits, peak paper white to 10000
Transfer ClampTransfer(Transfer t);

}  // namespace display
