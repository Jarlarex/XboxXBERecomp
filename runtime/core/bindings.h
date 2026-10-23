#pragma once
// What the player's controllers and keyboard drive on the console controller: one binding list per console control,
// the dead zones, and the mapping from a host controller's state to the console's report. Portable: SDL and the
// keyboard are read in input.cpp, the settings file in settings.cpp.
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "core/input.h"

namespace input {

// the console controller's controls in Pad order: the buttons bits 0 to 7, the analog buttons 0 to 7, then each
// stick's four directions
enum class Control : uint8_t {
    DpadUp, DpadDown, DpadLeft, DpadRight, Start, Back, LeftThumb, RightThumb,
    A, B, X, Y, Black, White, LeftTrigger, RightTrigger,
    LeftStickLeft, LeftStickRight, LeftStickUp, LeftStickDown,
    RightStickLeft, RightStickRight, RightStickUp, RightStickDown,
};
inline constexpr size_t kControls = 24;

// a host controller's inputs, named by their place on an Xbox controller (a is the bottom face button on any pad);
// the buttons come first, None aside
enum class PadInput : uint8_t {
    None,
    A, B, X, Y, Back, Guide, Start, LeftThumb, RightThumb, LeftShoulder, RightShoulder,
    DpadUp, DpadDown, DpadLeft, DpadRight, Misc, Paddle1, Paddle2, Paddle3, Paddle4, Touchpad,
    LeftTrigger, RightTrigger,
    LeftStickLeft, LeftStickRight, LeftStickUp, LeftStickDown,
    RightStickLeft, RightStickRight, RightStickUp, RightStickDown,
};
inline constexpr size_t kPadInputs = 32;
inline constexpr PadInput kLastPadButton = PadInput::Touchpad;

// a host controller as read: bit n of buttons for PadInput n, sticks from -32768 to 32767 with y up, triggers 0 to
// 32767
struct HostPad {
    uint32_t buttons = 0;
    int16_t lx = 0, ly = 0, rx = 0, ry = 0;
    int16_t lt = 0, rt = 0;
};

// the settings file's [pad] and [keyboard] tables: for each console control the inputs that drive it, none to many
struct Bindings {
    std::array<std::vector<PadInput>, kControls> pad;
    std::array<std::vector<uint16_t>, kControls> keys;  // Windows virtual-key codes
};

// the console's own layout on an Xbox controller (black on the right shoulder, white on the left) and the keyboard
// camber has always had: W and S the right and left triggers, A and D the left stick across, the arrows the D-pad,
// I J K L the right stick, Space A, Escape or Backspace B, X X, Y Y, Q white, E black, Enter start, Tab back
const Bindings& DefaultBindings();

// the names the settings file uses, lower case; parsing ignores case. Keys are named as on a US keyboard ("space",
// "left_shift", "semicolon"), any other virtual-key code "vk_" and two hex digits
const char* ControlName(Control control);
bool ParseControl(std::string_view name, Control* control);
const char* PadInputName(PadInput input);
bool ParsePadInput(std::string_view name, PadInput* input);
std::string KeyName(uint16_t vk);
bool ParseKey(std::string_view name, uint16_t* vk);

// The share of travel, in percent, that reads as rest: a stick's dead zone is round (both axes together), a
// trigger's at the bottom, and what is left is stretched over the whole range so full travel still reaches full. 0
// leaves the values exactly as read.
void ApplyDeadZones(HostPad& pad, uint32_t stick_percent, uint32_t trigger_percent);

// The console controller's report from a host controller through the bindings. A control driven by several inputs
// takes the strongest; a button drives an analog control fully, a trigger or stick direction drives a button once
// past half. With the default bindings every value is passed on unchanged.
Pad MapPad(const HostPad& host, const Bindings& bindings);

// The keyboard on top of a report: a held key presses its button fully, and pushes its stick direction fully unless
// the opposite direction's key is held too, which leaves the stick as the controller has it.
void AddKeys(Pad& pad, const Bindings& bindings, const std::function<bool(uint16_t vk)>& down);

}  // namespace input
