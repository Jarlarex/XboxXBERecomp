#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "core/bindings.h"
#include "core/display.h"

// the host side: window, settings
namespace app {

enum class DisplayMode : uint32_t { Fullscreen, Borderless, Windowed };
// the adapter DXGI is asked for: the discrete GPU on a hybrid laptop, the integrated one, or Windows' per-app choice
enum class GpuPreference : uint32_t { HighPerformance, MinimumPower, Unspecified };
inline constexpr uint32_t kFpsCapRefresh = 1;  // fps_cap: just under the monitor's refresh rate

struct Settings {
    DisplayMode display_mode = DisplayMode::Borderless;
    uint32_t width = 0, height = 0;  // 0 is the desktop mode
    uint32_t render_percent = 100;   // of what covers the picture's rectangle (display::RenderScale)
    display::Aspect aspect = display::Aspect::Auto;  // aspect_ratio: the picture's shape
    uint32_t fps_cap = 60;           // 0 unlimited, kFpsCapRefresh the monitor's refresh rate
    bool vsync = true;
    uint32_t aniso = 0;              // 0 as the game asks
    uint32_t msaa = 1;               // samples a pixel of the title's frame buffers: 1, 2, 4 or 8
    // the output transfer after the title's gamma ramp (display::Transfer): display_gamma 2.2 is the frame as drawn, 2.4
    // a CRT television's; hdr scRGB with paper white and peak in nits, when Windows runs the monitor in HDR
    double display_gamma = display::kPcGamma;
    bool hdr = false;
    uint32_t hdr_paper_white = 200;
    uint32_t hdr_peak = 400;
    // the textures the title uploads, named by a hash: dumped as DDS files, and replaced from the player's pack
    // (gpu/d3d12.cpp, core/texture_pack.h), under textures\dump and textures\replace in UserData()
    bool texture_dump = false;
    bool texture_pack = true;
    bool perf_overlay = false;       // the performance overlay at start; F2 toggles it
    std::string join_ip;                   // co-op: a host past the LAN the system link search also asks
    uint32_t rumble_percent = 100;   // of the title's motor speeds, up to 200; past full speed it saturates
    uint32_t volume_percent = 100;   // the whole output, up to 100
    std::string language = "english";  // the dashboard language the title reads (Language)
    GpuPreference gpu = GpuPreference::HighPerformance;
    uint32_t stick_dead_zone = 0;    // percent of a stick's travel read as rest, up to 90 (input::ApplyDeadZones)
    uint32_t trigger_dead_zone = 0;  // the same at the bottom of a trigger's
    input::Bindings bindings = input::DefaultBindings();  // [pad] and [keyboard]
};

// the dashboard language as the console's XC_LANGUAGE has it, 1 english to 9 portuguese; english for a name it does
// not know
uint32_t Language();

Settings& Get();
void Load();
void Save();
void LogSettings();  // every setting, as the settings file has them
void LogSystem();    // the machine and the settings, at a run's start
void ApplyDisplay();  // any thread
struct Mode {
    uint32_t width, height;
};
Mode Desktop();
Mode Output();
// the render scale for a title frame of guest_w x guest_h (display::RenderScale), from the output size the settings give
float RenderScale(uint32_t guest_w, uint32_t guest_h);
// the picture's width over its height (display::FrameAspect of aspect_ratio and the output size); the title's frame is
// shown at it, centred in the window
double FrameAspect();
// the picture is wider than 4:3: the console reports a widescreen TV, as on one a title draws anamorphic
bool Widescreen();

std::filesystem::path ExeDir();
// the player's own files besides the saves (texture dumps and packs): GameInfo::user_data, or the exe's folder
std::filesystem::path UserData();
// the game in assets (game_files.cpp): its folder and this executable's xbe, from the extracted files or a disc
// image (.iso), which it mounts as D:; false after telling the player what is missing
struct GameFiles {
    std::filesystem::path dir;
    std::vector<uint8_t> xbe;
};
bool FindGame(GameFiles& game);
std::string Sha1(const std::vector<uint8_t>& data);  // lower case hex
// the recompiled code, <exe>.dll, built from the game in assets when missing or out of date (game_code.cpp) and
// loaded; false after telling the player why the game cannot start, or when they chose not to start it yet
bool PrepareGameCode(const GameFiles& game);
int Instance();  // --instance N on the command line, 0 without: a second copy on this PC for system link tests
void* Window();  // HWND
// boot splash: GameInfo::splash .bmp or .png beside the exe, up before the game window
bool OpenSplash();        // window thread, at launch; false without one, and the game window opens at once
void SplashFirstFrame();  // the game's first Swap: waits out the splash's minimum, then opens the game window
void CloseSplash();       // window thread
void OpenGameWindow(bool apply = true);  // any thread but the window thread's; returns once the window is up
// relaunched: the previous instance keeps its window up until this one has shown its first frames (handover.cpp)
bool StartHandover();   // window thread, at launch; true: the game window starts hidden
bool HandingOver();
void FinishHandover();  // any thread but the window thread's: opens the game window, lets the old instance go

// held around ImGui: the window thread's messages and the renderer's frame
std::recursive_mutex& UiLock();

// what the game repo defines once
struct Option {  // a setting in the settings file, flag or value
    const char* key;
    bool* flag;
    uint32_t* value;
};
struct GameInfo {
    const wchar_t* title;        // window and splash
    const wchar_t* short_title;  // message boxes
    const wchar_t* name;         // window classes, crash dumps, handover event
    const char* log;             // log prefix
    const char* settings;        // settings file beside the exe
    const wchar_t* splash;       // .bmp or .png beside the exe
    const wchar_t* missing;      // message when neither the game files nor a disc image are found
    std::vector<std::pair<const char*, const char*>> images;  // xbe file name, executable name
    std::vector<Option> options;
    // the settings file's toml::table, for what options cannot hold
    void (*load)(const void* table);
    void (*save)(void* table);
    void (*init)();                // after the settings load, before the window and boot
    void (*loaded)();              // image in memory, before its entry runs
    void (*window)(void* hwnd);    // the game window is up, before boot
    // window thread, each message after ImGui's; true returns result instead of the host's handling
    bool (*message)(void* hwnd, uint32_t msg, uintptr_t wp, intptr_t lp, intptr_t* result);
    void (*ui)(double fps);        // each frame inside the ImGui frame, UiLock held
    void (*pad)(int port, input::Pad& pad);  // a polled pad before the game gets it
    void (*rumble)(int port, uint16_t& left, uint16_t& right);
    // the host folder of the console's hard disk (its partitions, the saves on E:), given the folder's name: hdd, or
    // hdd2 and on for a second copy (Instance). Once, before boot; null keeps it beside the exe
    std::filesystem::path (*hard_disk)(const std::wstring& folder);
    // the game's hooks give the title any picture aspect FrameAspect reports (a widescreen title's camera and 2D
    // layouts), so aspect_ratio auto follows the window; without, a title draws the console's 16:9 or 4:3
    bool any_aspect = false;
    // the folder of the player's own files besides the saves (UserData); null keeps them beside the exe
    std::filesystem::path (*user_data)() = nullptr;
};
extern const GameInfo kGame;

// sleeps out the rest of the frame: for the fps cap, and for the title's presentation interval (the vertical blanks a
// frame stays up, 0 or 1 for none); Forza asks for two, a 30 frames a second game that steps its simulation once a frame
void Pace(uint32_t interval = 1);
// the frames a second Pace is holding the title to, the cap without an interval, 0 for none
uint32_t TargetFps();
// when the title's last swap was due to end (Pace's schedule), nothing before the first swap or after the title stopped
// swapping for a while (a movie, a load); the vertical blank thread keeps half a frame from it
std::chrono::steady_clock::time_point LastSwapSlot();

}  // namespace app
