// The performance overlay's window
#include "gpu/overlay.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <imgui.h>

namespace gpu {

uint64_t PerfOverlay::Add(const FrameSample& s) {
    samples_[frames_ % kHistory] = s;
    return frames_++;
}

void PerfOverlay::SetGpu(uint64_t frame, double ms) {
    if (frame < frames_ && frames_ - frame <= kHistory) samples_[frame % kHistory].gpu = ms;
}

FrameSample PerfOverlay::Mean(uint32_t n) const {
    n = uint32_t(std::min<uint64_t>({n, frames_, kHistory}));
    FrameSample m;
    m.gpu = 0;
    uint32_t gpus = 0;
    for (uint32_t i = 0; i < n; i++) {
        const FrameSample& s = samples_[(frames_ - 1 - i) % kHistory];
        m.gap += s.gap;
        m.title += s.title;
        m.recording += s.recording;
        m.waiting += s.waiting;
        m.host += s.host;
        if (s.gpu >= 0) {
            m.gpu += s.gpu;
            gpus++;
        }
    }
    if (n) {
        for (double* v : {&m.gap, &m.title, &m.recording, &m.waiting, &m.host}) *v /= n;
    }
    m.gpu = gpus ? m.gpu / gpus : -1;
    return m;
}

namespace {

struct Stat {
    double mean = 0, max = 0;
    uint32_t n = 0;
};

const char* Filtering(uint32_t aniso) {
    static char text[24];
    if (!aniso) return "the game's";
    std::snprintf(text, sizeof(text), "%ux anisotropic", aniso);
    return text;
}

}  // namespace

void PerfOverlay::Draw(const OverlayInfo& info, uint32_t window_h) const {
    uint32_t n = uint32_t(std::min<uint64_t>(frames_, kHistory));
    if (!n) return;
    auto sample = [&](uint32_t i) -> const FrameSample& { return samples_[(frames_ - n + i) % kHistory]; };
    double frame_ms = info.target_fps ? 1000.0 / info.target_fps : 0;
    // the newest second of frames (at least 30) for the averages
    uint32_t recent = std::min<uint32_t>(n, std::max<uint32_t>(info.target_fps ? info.target_fps : 60, 30));
    auto stat = [&](double FrameSample::*field) {
        Stat s;
        for (uint32_t i = n - recent; i < n; i++) {
            double v = sample(i).*field;
            if (v < 0) continue;
            s.mean += v;
            s.max = std::max(s.max, v);
            s.n++;
        }
        if (s.n) s.mean /= s.n;
        return s;
    };
    float gaps[kHistory];
    float longest = 0;
    uint32_t slow = 0, fast = 0;
    for (uint32_t i = 0; i < n; i++) {
        gaps[i] = float(sample(i).gap);
        longest = std::max(longest, gaps[i]);
        if (frame_ms && i) {
            slow += gaps[i] > frame_ms * 1.25;
            fast += gaps[i] < frame_ms * 0.75;
        }
    }
    Stat gap = stat(&FrameSample::gap), title = stat(&FrameSample::title), rec = stat(&FrameSample::recording),
         wait = stat(&FrameSample::waiting), host = stat(&FrameSample::host), gpu = stat(&FrameSample::gpu);

    // the default font is 13 pixels: kept readable from 720 lines up
    float ui = std::max(1.0f, float(window_h) / 720.0f);
    ImGui::SetNextWindowPos(ImVec2(8 * ui, 8 * ui), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
    if (!ImGui::Begin("performance", nullptr, flags)) {
        ImGui::End();
        return;
    }
    ImGui::SetWindowFontScale(ui);
    ImGui::Text("%.1f fps  %.2f ms a frame", info.fps, gap.mean);
    if (info.target_fps) {
        ImGui::SameLine();
        ImGui::TextDisabled("target %u fps", info.target_fps);
    }
    // frame times from 0 to two frames, the target as a line
    float top = frame_ms ? float(frame_ms * 2) : std::max(longest * 1.2f, 1.0f);
    ImGui::PlotLines("##gaps", gaps, int(n), 0, nullptr, 0.0f, top, ImVec2(260 * ui, 56 * ui));
    if (frame_ms) {
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        float y = b.y - (b.y - a.y) * float(frame_ms / top);
        ImGui::GetWindowDrawList()->AddLine(ImVec2(a.x, y), ImVec2(b.x, y), IM_COL32(255, 190, 40, 170), ui);
        ImGui::Text("over 1.25 frames %u, under 0.75 %u, longest %.1f ms", slow, fast, longest);
    }
    ImGui::Separator();
    ImGui::Text("title thread  %5.2f ms (recording %.2f, waiting %.2f)", title.mean, rec.mean, wait.mean);
    ImGui::Text("render thread %5.2f ms, longest %.2f", host.mean, host.max);
    if (gpu.n) ImGui::Text("GPU           %5.2f ms, longest %.2f", gpu.mean, gpu.max);
    else ImGui::Text("GPU           -");
    ImGui::Separator();
    ImGui::Text("%ux%u drawn at %.2fx: %ux%u", info.guest_w, info.guest_h, info.scale,
                unsigned(std::lround(info.guest_w * info.scale)), unsigned(std::lround(info.guest_h * info.scale)));
    char msaa[16] = "off";
    if (info.msaa > 1) std::snprintf(msaa, sizeof(msaa), "%ux", info.msaa);
    ImGui::Text("MSAA %s, filtering %s", msaa, Filtering(info.aniso));
    ImGui::Text("%s, window %ux%u, picture %ux%u", info.output, info.window_w, info.window_h, info.frame_w,
                info.frame_h);
    ImGui::End();
}

}  // namespace gpu
