#ifndef CORE_LIBS_PRX_LIBSCEKEYBOARD_KEYBOARDSTATE_HPP
#define CORE_LIBS_PRX_LIBSCEKEYBOARD_KEYBOARDSTATE_HPP

#include "prx/libSceKeyboard/include/keyboard_structs.h"

struct KeyboardInputEvent {
    std::uint16_t keyCode = 0;
    bool pressed = false;
    std::uint32_t led = 0;
    bool resetKeys = false;
    bool connectionChange = false;
    bool connected = true;
};

struct KeyboardImeKey {
    std::uint16_t usage;
    bool pressed;
    std::uint32_t led;
    std::uint64_t timestampUs;
};

namespace Keyboard {
void ImeSetActive(bool active);
bool ImeActive();
std::uint32_t ImeDrain(KeyboardImeKey* out, std::uint32_t max);
int Initialize();
int Open(int userId, std::int32_t type, std::int32_t index);
int Close(std::int32_t handle);
int Read(std::int32_t handle, KeyboardData* data, std::int32_t num);
int ReadState(std::int32_t handle, KeyboardData* data);
int GetKey2Char(std::int32_t handle, std::int32_t arrange, std::uint32_t led, std::uint32_t modifierKey, std::uint16_t keyCode, KeyboardCharData* charData);
void Publish(const KeyboardInputEvent& event);
bool IsOpen();
}

extern "C" void KeyboardPublishInput_nid_postfix(const KeyboardInputEvent& event);
extern "C" bool KeyboardIsOpen_nid_postfix();
extern "C" void KeyboardImeSetActive_nid_postfix(bool active);
extern "C" std::uint32_t KeyboardImeDrain_nid_postfix(KeyboardImeKey* out, std::uint32_t max);

#endif
