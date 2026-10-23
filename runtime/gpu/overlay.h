#pragma once
// The performance overlay (F2, perf_overlay): what each presented frame cost, drawn with ImGui over the game
#include <array>
#include <atomic>
#include <cstdint>

namespace gpu {

// shown while set; F2 flips it, perf_overlay sets it at start
inline std::atomic<bool> g_perf_overlay{false};

// one presented frame as the renderer measured it, in milliseconds
struct FrameSample {
    double gap = 0;        // since the previous present
    double title = 0;      // the title's thread from its last swap to this one, its pacing sleep left out
    double recording = 0;  // of which recording calls for the render thread
    double waiting = 0;    // and waiting on it
    double host = 0;       // the render thread drawing and presenting, its waits for the GPU and display left out
    double gpu = -1;       // the GPU busy on the frame's command lists; -1 until the timestamps are read back
};

// what the overlay names besides the times
struct OverlayInfo {
    double fps = 0;
    uint32_t target_fps = 0;               // what the pacing holds the title to, 0 for none
    uint32_t guest_w = 0, guest_h = 0;     // the title's back buffer
    float scale = 1;                       // host pixels per console pixel of every target
    uint32_t msaa = 1, aniso = 0;
    const char* output = "";               // the swap chain: "SDR", "SDR 10-bit", "HDR scRGB"
    uint32_t window_w = 0, window_h = 0;
    uint32_t frame_x = 0, frame_y = 0, frame_w = 0, frame_h = 0;  // where the picture lands in the window
};

class PerfOverlay {
public:
    // each present, in order; returns the frame's number for SetGpu
    uint64_t Add(const FrameSample& s);
    // a frame's GPU time once its timestamps are read back, a few frames later
    void SetGpu(uint64_t frame, double ms);
    // inside an ImGui frame, the window's height in pixels for the text size
    void Draw(const OverlayInfo& info, uint32_t window_h) const;
    // the mean of each time over the newest n frames that have it, for the perf log
    FrameSample Mean(uint32_t n) const;

private:
    static constexpr uint32_t kHistory = 240;  // four seconds of frames at 60, eight at 30
    std::array<FrameSample, kHistory> samples_{};
    uint64_t frames_ = 0;
};

}  // namespace gpu
