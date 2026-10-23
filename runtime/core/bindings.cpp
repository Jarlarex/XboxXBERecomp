#include "core/bindings.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <utility>

namespace input {

namespace {

const char* const kControlNames[kControls] = {
    "dpad_up",         "dpad_down",        "dpad_left",      "dpad_right",      "start",           "back",
    "left_thumb",      "right_thumb",      "a",              "b",               "x",               "y",
    "black",           "white",            "left_trigger",   "right_trigger",   "left_stick_left", "left_stick_right",
    "left_stick_up",   "left_stick_down",  "right_stick_left", "right_stick_right", "right_stick_up", "right_stick_down"};

const char* const kPadInputNames[kPadInputs] = {
    "none",          "a",              "b",                "x",              "y",               "back",
    "guide",         "start",          "left_thumb",       "right_thumb",    "left_shoulder",   "right_shoulder",
    "dpad_up",       "dpad_down",      "dpad_left",        "dpad_right",     "misc",            "paddle1",
    "paddle2",       "paddle3",        "paddle4",          "touchpad",       "left_trigger",    "right_trigger",
    "left_stick_left", "left_stick_right", "left_stick_up", "left_stick_down", "right_stick_left", "right_stick_right",
    "right_stick_up", "right_stick_down"};

// named keys beside the letters, digits, F keys and numpad digits, which KeyName and ParseKey work out
constexpr std::pair<const char*, uint16_t> kKeys[] = {
    {"backspace", 0x08},  {"tab", 0x09},          {"enter", 0x0D},        {"shift", 0x10},
    {"ctrl", 0x11},       {"alt", 0x12},          {"pause", 0x13},        {"caps_lock", 0x14},
    {"escape", 0x1B},     {"space", 0x20},        {"page_up", 0x21},      {"page_down", 0x22},
    {"end", 0x23},        {"home", 0x24},         {"left", 0x25},         {"up", 0x26},
    {"right", 0x27},      {"down", 0x28},         {"insert", 0x2D},       {"delete", 0x2E},
    {"numpad_multiply", 0x6A}, {"numpad_add", 0x6B}, {"numpad_subtract", 0x6D}, {"numpad_decimal", 0x6E},
    {"numpad_divide", 0x6F}, {"left_shift", 0xA0},  {"right_shift", 0xA1},  {"left_ctrl", 0xA2},
    {"right_ctrl", 0xA3}, {"left_alt", 0xA4},     {"right_alt", 0xA5},    {"semicolon", 0xBA},
    {"equals", 0xBB},     {"comma", 0xBC},        {"minus", 0xBD},        {"period", 0xBE},
    {"slash", 0xBF},      {"grave", 0xC0},        {"left_bracket", 0xDB}, {"backslash", 0xDC},
    {"right_bracket", 0xDD}, {"apostrophe", 0xDE}};

std::string Lower(std::string_view text) {
    std::string s(text);
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

template <size_t N>
int Find(const char* const (&names)[N], std::string_view name) {
    std::string lower = Lower(name);
    for (size_t i = 0; i < N; i++)
        if (lower == names[i]) return int(i);
    return -1;
}

Bindings MakeDefaults() {
    Bindings b;
    using C = Control;
    using P = PadInput;
    auto pad = [&b](C c, P p) { b.pad[size_t(c)] = {p}; };
    pad(C::DpadUp, P::DpadUp);
    pad(C::DpadDown, P::DpadDown);
    pad(C::DpadLeft, P::DpadLeft);
    pad(C::DpadRight, P::DpadRight);
    pad(C::Start, P::Start);
    pad(C::Back, P::Back);
    pad(C::LeftThumb, P::LeftThumb);
    pad(C::RightThumb, P::RightThumb);
    pad(C::A, P::A);
    pad(C::B, P::B);
    pad(C::X, P::X);
    pad(C::Y, P::Y);
    pad(C::Black, P::RightShoulder);
    pad(C::White, P::LeftShoulder);
    pad(C::LeftTrigger, P::LeftTrigger);
    pad(C::RightTrigger, P::RightTrigger);
    pad(C::LeftStickLeft, P::LeftStickLeft);
    pad(C::LeftStickRight, P::LeftStickRight);
    pad(C::LeftStickUp, P::LeftStickUp);
    pad(C::LeftStickDown, P::LeftStickDown);
    pad(C::RightStickLeft, P::RightStickLeft);
    pad(C::RightStickRight, P::RightStickRight);
    pad(C::RightStickUp, P::RightStickUp);
    pad(C::RightStickDown, P::RightStickDown);
    auto keys = [&b](C c, std::vector<uint16_t> vk) { b.keys[size_t(c)] = std::move(vk); };
    keys(C::DpadUp, {0x26});
    keys(C::DpadDown, {0x28});
    keys(C::DpadLeft, {0x25});
    keys(C::DpadRight, {0x27});
    keys(C::Start, {0x0D});
    keys(C::Back, {0x09});
    keys(C::A, {0x20});
    keys(C::B, {0x1B, 0x08});
    keys(C::X, {'X'});
    keys(C::Y, {'Y'});
    keys(C::Black, {'E'});
    keys(C::White, {'Q'});
    keys(C::LeftTrigger, {'S'});
    keys(C::RightTrigger, {'W'});
    keys(C::LeftStickLeft, {'A'});
    keys(C::LeftStickRight, {'D'});
    keys(C::RightStickLeft, {'J'});
    keys(C::RightStickRight, {'L'});
    keys(C::RightStickUp, {'I'});
    keys(C::RightStickDown, {'K'});
    return b;
}

// An input's level: 0 to 32767 for a trigger or a stick pushed right or up, 0 to 32768 for a stick pushed left or
// down, kFull for a held button. Kept apart this way, a stick's two directions add back up to the value read.
constexpr int32_t kFull = 32768;

int32_t Level(const HostPad& host, PadInput input) {
    if (input == PadInput::None) return 0;
    if (input <= kLastPadButton) return (host.buttons >> unsigned(input)) & 1 ? kFull : 0;
    auto plus = [](int16_t v) { return std::max<int32_t>(v, 0); };
    auto minus = [](int16_t v) { return std::max<int32_t>(-int32_t(v), 0); };
    switch (input) {
        case PadInput::LeftTrigger: return plus(host.lt);
        case PadInput::RightTrigger: return plus(host.rt);
        case PadInput::LeftStickLeft: return minus(host.lx);
        case PadInput::LeftStickRight: return plus(host.lx);
        case PadInput::LeftStickUp: return plus(host.ly);
        case PadInput::LeftStickDown: return minus(host.ly);
        case PadInput::RightStickLeft: return minus(host.rx);
        case PadInput::RightStickRight: return plus(host.rx);
        case PadInput::RightStickUp: return plus(host.ry);
        case PadInput::RightStickDown: return minus(host.ry);
        default: return 0;
    }
}

int32_t Strongest(const HostPad& host, const std::vector<PadInput>& inputs) {
    int32_t level = 0;
    for (PadInput input : inputs) level = std::max(level, Level(host, input));
    return level;
}

// a value from -1 to 1 back to a stick's range
int16_t ToAxis(double v) { return int16_t(std::lround(std::clamp(v, -1.0, 1.0) * 32767.0)); }

void RoundDeadZone(int16_t& x, int16_t& y, double zone) {
    double fx = std::clamp(x / 32767.0, -1.0, 1.0), fy = std::clamp(y / 32767.0, -1.0, 1.0);
    double m = std::hypot(fx, fy);
    if (m <= zone) {
        x = y = 0;
        return;
    }
    // the edge of the round gate stays at full; a square gate's corners, past 1, clamp per axis as before
    double scale = (m - zone) / (1.0 - zone) / m;
    x = ToAxis(fx * scale);
    y = ToAxis(fy * scale);
}

void TriggerDeadZone(int16_t& t, double zone) {
    double f = std::clamp(t / 32767.0, 0.0, 1.0);
    t = f <= zone ? 0 : int16_t(std::lround((f - zone) / (1.0 - zone) * 32767.0));
}

}  // namespace

const Bindings& DefaultBindings() {
    static const Bindings b = MakeDefaults();
    return b;
}

const char* ControlName(Control control) {
    return size_t(control) < kControls ? kControlNames[size_t(control)] : "";
}

bool ParseControl(std::string_view name, Control* control) {
    int i = Find(kControlNames, name);
    if (i < 0) return false;
    *control = Control(i);
    return true;
}

const char* PadInputName(PadInput input) {
    return size_t(input) < kPadInputs ? kPadInputNames[size_t(input)] : "";
}

bool ParsePadInput(std::string_view name, PadInput* input) {
    int i = Find(kPadInputNames, name);
    if (i < 0) return false;
    *input = PadInput(i);
    return true;
}

std::string KeyName(uint16_t vk) {
    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) return std::string(1, char(std::tolower(vk)));
    if (vk >= 0x60 && vk <= 0x69) return "numpad_" + std::to_string(vk - 0x60);
    if (vk >= 0x70 && vk <= 0x87) return "f" + std::to_string(vk - 0x70 + 1);
    for (auto [name, code] : kKeys)
        if (code == vk) return name;
    char text[8];
    std::snprintf(text, sizeof text, "vk_%02x", unsigned(vk & 0xFF));
    return text;
}

bool ParseKey(std::string_view name, uint16_t* vk) {
    std::string s = Lower(name);
    if (s.empty()) return false;
    if (s.size() == 1 && (std::isdigit(static_cast<unsigned char>(s[0])) || (s[0] >= 'a' && s[0] <= 'z'))) {
        *vk = uint16_t(std::toupper(static_cast<unsigned char>(s[0])));
        return true;
    }
    auto number = [&s](size_t from, int base, int* out) {
        if (from >= s.size()) return false;
        char* end = nullptr;
        long v = std::strtol(s.c_str() + from, &end, base);
        if (*end || v < 0) return false;
        *out = int(v);
        return true;
    };
    int n = 0;
    if (s.rfind("numpad_", 0) == 0 && s.size() == 8 && number(7, 10, &n) && n <= 9) {
        *vk = uint16_t(0x60 + n);
        return true;
    }
    if (s[0] == 'f' && s.size() <= 3 && number(1, 10, &n) && n >= 1 && n <= 24) {
        *vk = uint16_t(0x70 + n - 1);
        return true;
    }
    if (s.rfind("vk_", 0) == 0 && s.size() == 5 && number(3, 16, &n) && n > 0) {
        *vk = uint16_t(n);
        return true;
    }
    for (auto [key, code] : kKeys) {
        if (s != key) continue;
        *vk = code;
        return true;
    }
    return false;
}

void ApplyDeadZones(HostPad& pad, uint32_t stick_percent, uint32_t trigger_percent) {
    if (stick_percent) {
        double zone = std::min(stick_percent, 90u) / 100.0;
        RoundDeadZone(pad.lx, pad.ly, zone);
        RoundDeadZone(pad.rx, pad.ry, zone);
    }
    if (trigger_percent) {
        double zone = std::min(trigger_percent, 90u) / 100.0;
        TriggerDeadZone(pad.lt, zone);
        TriggerDeadZone(pad.rt, zone);
    }
}

Pad MapPad(const HostPad& host, const Bindings& bindings) {
    Pad p;
    p.connected = true;
    auto level = [&](Control c) { return Strongest(host, bindings.pad[size_t(c)]); };
    for (int i = 0; i < 8; i++)
        if (level(Control(i)) >= kFull / 2) p.buttons |= uint16_t(1 << i);
    // 0 to 32767 into 0 to 255, a button's full level to 255
    for (int i = 0; i < 8; i++) p.analog[i] = uint8_t(std::min(level(Control(8 + i)) >> 7, 255));
    auto axis = [&](Control negative, Control positive) {
        return int16_t(std::min(level(positive), 32767) - std::min(level(negative), 32768));
    };
    p.lx = axis(Control::LeftStickLeft, Control::LeftStickRight);
    p.ly = axis(Control::LeftStickDown, Control::LeftStickUp);
    p.rx = axis(Control::RightStickLeft, Control::RightStickRight);
    p.ry = axis(Control::RightStickDown, Control::RightStickUp);
    return p;
}

void AddKeys(Pad& pad, const Bindings& bindings, const std::function<bool(uint16_t vk)>& down) {
    auto held = [&](Control c) {
        const std::vector<uint16_t>& keys = bindings.keys[size_t(c)];
        return std::any_of(keys.begin(), keys.end(), [&](uint16_t vk) { return down(vk); });
    };
    pad.connected = true;
    for (int i = 0; i < 8; i++)
        if (held(Control(i))) pad.buttons |= uint16_t(1 << i);
    for (int i = 0; i < 8; i++)
        if (held(Control(8 + i))) pad.analog[i] = 0xFF;
    auto axis = [&](int16_t& value, Control negative, Control positive) {
        bool minus = held(negative), plus = held(positive);
        if (minus != plus) value = plus ? 32767 : -32768;
    };
    axis(pad.lx, Control::LeftStickLeft, Control::LeftStickRight);
    axis(pad.ly, Control::LeftStickDown, Control::LeftStickUp);
    axis(pad.rx, Control::RightStickLeft, Control::RightStickRight);
    axis(pad.ry, Control::RightStickDown, Control::RightStickUp);
}

}  // namespace input
