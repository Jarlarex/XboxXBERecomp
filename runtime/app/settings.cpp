// the settings file beside the exe
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string_view>

#include <toml++/toml.hpp>

#include "app/app.h"
#include "core/log.h"

namespace app {

namespace {

const char* kModes[] = {"fullscreen", "borderless", "windowed"};
const char* kGpus[] = {"high_performance", "minimum_power", "unspecified"};
// XC_LANGUAGE 1 to 9
const char* kLanguages[] = {"english", "japanese", "german",  "french",    "spanish",
                            "italian", "korean",   "chinese", "portuguese"};

std::filesystem::path File() { return ExeDir() / kGame.settings; }

// [pad] and [keyboard]: a console control's name = an input's name or a list of them, an empty list or "none" for
// nothing. Controls left out keep their default; a name camber does not know is reported and skipped.
template <class T>
void LoadBindings(const toml::table& t, const char* name, std::array<std::vector<T>, input::kControls>& out,
                  bool (*parse)(std::string_view, T*)) {
    const toml::table* table = t[name].as_table();
    if (!table) return;
    for (auto&& entry : *table) {
        std::string key(entry.first.str());
        input::Control control;
        if (!input::ParseControl(key, &control)) {
            LOGR("settings: [%s] %s is not a console control, left out", name, key.c_str());
            continue;
        }
        std::vector<T> list;
        auto add = [&](const toml::node& item) {
            std::optional<std::string> text = item.value<std::string>();
            T value{};
            if (text && (text->empty() || *text == "none")) return;
            if (text && parse(*text, &value)) list.push_back(value);
            else LOGR("settings: [%s] %s: %s is not a name camber knows, left out", name, key.c_str(),
                      text ? text->c_str() : "a value that is not a string");
        };
        if (const toml::array* items = entry.second.as_array()) {
            for (const toml::node& item : *items) add(item);
        } else {
            add(entry.second);
        }
        out[size_t(control)] = std::move(list);
    }
}

// one name as a string, else a list
template <class T, class Name>
toml::table SaveBindings(const std::array<std::vector<T>, input::kControls>& bindings, Name name) {
    toml::table table;
    for (size_t i = 0; i < input::kControls; i++) {
        const char* control = input::ControlName(input::Control(i));
        if (bindings[i].size() == 1) {
            table.insert(control, std::string(name(bindings[i][0])));
            continue;
        }
        toml::array list;
        for (const T& value : bindings[i]) list.push_back(std::string(name(value)));
        table.insert(control, std::move(list));
    }
    return table;
}

}  // namespace

Settings& Get() {
    static Settings s;
    return s;
}

void Load() {
    Settings& s = Get();
    toml::table t;
    try {
        t = toml::parse_file(File().string());
    } catch (...) {
        return;
    }
    std::string mode = t["display_mode"].value_or(std::string(kModes[uint32_t(s.display_mode)]));
    for (uint32_t i = 0; i < 3; i++)
        if (mode == kModes[i]) s.display_mode = DisplayMode(i);
    s.width = t["width"].value_or(s.width);
    s.height = t["height"].value_or(s.height);
    s.render_percent = std::clamp<uint32_t>(t["render_percent"].value_or(s.render_percent), 10, 400);
    std::string aspect = t["aspect_ratio"].value_or(std::string(display::AspectName(s.aspect)));
    if (!display::ParseAspect(aspect, &s.aspect)) LOGR("settings: aspect_ratio %s is not auto, 16:9 or 4:3", aspect.c_str());
    s.fps_cap = t["fps_cap"].value_or(s.fps_cap);
    s.vsync = t["vsync"].value_or(s.vsync);
    s.aniso = display::Anisotropy(t["anisotropic_filtering"].value_or(int64_t(s.aniso)));
    s.msaa = display::MsaaSamples(t["msaa"].value_or(int64_t(s.msaa)));
    display::Transfer transfer{t["display_gamma"].value_or(s.display_gamma), t["hdr"].value_or(s.hdr),
                               double(t["hdr_paper_white"].value_or(int64_t(s.hdr_paper_white))),
                               double(t["hdr_peak"].value_or(int64_t(s.hdr_peak)))};
    transfer = display::ClampTransfer(transfer);
    s.display_gamma = transfer.gamma;
    s.hdr = transfer.hdr;
    s.hdr_paper_white = uint32_t(transfer.paper_white);
    s.hdr_peak = uint32_t(transfer.peak);
    s.perf_overlay = t["perf_overlay"].value_or(s.perf_overlay);
    s.join_ip = t["join_ip"].value_or(s.join_ip);
    s.rumble_percent = std::min<uint32_t>(t["rumble_percent"].value_or(s.rumble_percent), 200);
    s.volume_percent = std::min<uint32_t>(t["volume_percent"].value_or(s.volume_percent), 100);
    s.language = t["language"].value_or(s.language);
    std::string gpu = t["gpu"].value_or(std::string(kGpus[uint32_t(s.gpu)]));
    for (uint32_t i = 0; i < 3; i++)
        if (gpu == kGpus[i]) s.gpu = GpuPreference(i);
    s.stick_dead_zone = std::min<uint32_t>(t["stick_dead_zone"].value_or(s.stick_dead_zone), 90);
    s.trigger_dead_zone = std::min<uint32_t>(t["trigger_dead_zone"].value_or(s.trigger_dead_zone), 90);
    LoadBindings(t, "pad", s.bindings.pad, input::ParsePadInput);
    LoadBindings(t, "keyboard", s.bindings.keys, input::ParseKey);
    for (const Option& o : kGame.options) {
        if (o.flag) *o.flag = t[o.key].value_or(*o.flag);
        else *o.value = t[o.key].value_or(*o.value);
    }
    if (kGame.load) kGame.load(&t);
}

namespace {

toml::table Table() {
    const Settings& s = Get();
    toml::table t{{"display_mode", kModes[uint32_t(s.display_mode)]},
                  {"width", int64_t(s.width)},
                  {"height", int64_t(s.height)},
                  {"render_percent", int64_t(s.render_percent)},
                  {"aspect_ratio", display::AspectName(s.aspect)},
                  {"fps_cap", int64_t(s.fps_cap)},
                  {"vsync", s.vsync},
                  {"anisotropic_filtering", int64_t(s.aniso)},
                  {"msaa", int64_t(s.msaa)},
                  {"display_gamma", s.display_gamma},
                  {"hdr", s.hdr},
                  {"hdr_paper_white", int64_t(s.hdr_paper_white)},
                  {"hdr_peak", int64_t(s.hdr_peak)},
                  {"perf_overlay", s.perf_overlay},
                  {"join_ip", s.join_ip},
                  {"rumble_percent", int64_t(s.rumble_percent)},
                  {"volume_percent", int64_t(s.volume_percent)},
                  {"language", s.language},
                  {"gpu", kGpus[uint32_t(s.gpu)]},
                  {"stick_dead_zone", int64_t(s.stick_dead_zone)},
                  {"trigger_dead_zone", int64_t(s.trigger_dead_zone)}};
    t.insert("pad", SaveBindings(s.bindings.pad, input::PadInputName));
    t.insert("keyboard", SaveBindings(s.bindings.keys, input::KeyName));
    for (const Option& o : kGame.options) {
        if (o.flag) t.insert(o.key, *o.flag);
        else t.insert(o.key, int64_t(*o.value));
    }
    if (kGame.save) kGame.save(&t);
    return t;
}

}  // namespace

void Save() {
    std::ofstream f(File());
    f << Table() << "\n";
}

void LogSettings() {
    std::ostringstream text;
    text << Table();
    std::istringstream lines(text.str());
    LOGR("Settings:");
    for (std::string line; std::getline(lines, line);)
        if (!line.empty()) LOGR("  %s", line.c_str());
}

uint32_t Language() {
    for (uint32_t i = 0; i < std::size(kLanguages); i++)
        if (Get().language == kLanguages[i]) return i + 1;
    return 1;
}

Mode Desktop() {
    HMONITOR mon = MonitorFromWindow(static_cast<HWND>(Window()), MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    if (GetMonitorInfoW(mon, &mi) && EnumDisplaySettingsW(mi.szDevice, ENUM_REGISTRY_SETTINGS, &dm))
        return {dm.dmPelsWidth, dm.dmPelsHeight};
    return {uint32_t(GetSystemMetrics(SM_CXSCREEN)), uint32_t(GetSystemMetrics(SM_CYSCREEN))};
}

Mode Output() {
    const Settings& s = Get();
    if (s.width && s.height) return {s.width, s.height};
    return Desktop();
}

double FrameAspect() {
    // asked per frame and per HUD quad: the desktop mode lookup only runs when the resolution or aspect setting changes
    static std::atomic<uint64_t> key{~0ull};
    static std::atomic<double> aspect{display::kClassic};
    const Settings& s = Get();
    uint64_t k = uint64_t(s.width) << 32 | uint64_t(s.height) << 8 | uint8_t(s.aspect);
    if (key.exchange(k) != k) {
        Mode m = Output();
        aspect = display::FrameAspect(s.aspect, m.width, m.height, kGame.any_aspect);
    }
    return aspect;
}

bool Widescreen() { return display::Widescreen(FrameAspect()); }

float RenderScale(uint32_t guest_w, uint32_t guest_h) {
    Mode m = Output();
    return display::RenderScale(Get().render_percent, display::FrameRect(FrameAspect(), m.width, m.height), guest_w,
                                guest_h);
}

}  // namespace app
