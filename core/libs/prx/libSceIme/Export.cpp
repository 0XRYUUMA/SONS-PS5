#include <cstdint>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceKeyboard/include/KeyboardState.hpp"

namespace {

std::mutex g_keyboardMutex;
std::set<int32_t> g_openKeyboards;

bool imeKeyboardEnabled() {
    static const bool enabled = std::getenv("APS5_IME_KEYBOARD") != nullptr;
    return enabled;
}

using GuestHandler = void (APS5_VABI *)(void* arg, const ImeEvent* event);

constexpr std::uint32_t KeyboardResource = 1;

constexpr std::uint32_t EventKeycodeDown = 0x101;
constexpr std::uint32_t EventKeycodeUp = 0x102;
constexpr std::uint32_t EventKeycodeRepeat = 0x103;
constexpr std::uint32_t EventConnection = 0x104;

int32_t g_keyboardUser = 0;
EventHandler g_keyboardHandler = nullptr;
void* g_keyboardArg = nullptr;
bool g_openEventPending = false;

char16_t characterOf(std::uint16_t usage) {
    if (usage >= 0x04 && usage <= 0x1d) return static_cast<char16_t>(u'a' + (usage - 0x04));
    if (usage >= 0x1e && usage <= 0x26) return static_cast<char16_t>(u'1' + (usage - 0x1e));
    if (usage == 0x27) return u'0';
    if (usage == 0x2c) return u' ';
    return 0;
}

}

extern "C" {

int APS5_VABI sceImeClose_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeGetPanelSize(const Param* param, uint32_t* width, uint32_t* height) {
 (void)param;
 (void)width;
 (void)height;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeKeyboardClose(int32_t user_id) {
 std::lock_guard lock(g_keyboardMutex);
 if (g_openKeyboards.erase(user_id) == 0) throw std::logic_error("sceImeKeyboardClose: keyboard not open for user " + std::to_string(user_id));
 if (imeKeyboardEnabled()) {
  g_keyboardHandler = nullptr;
  KeyboardImeSetActive_nid_postfix(false);
 }
 return 0;
}

int APS5_VABI sceImeKeyboardGetInfo(uint32_t resource_id, KeyboardInfo* info) {
 if (imeKeyboardEnabled() && info != nullptr) {
  std::lock_guard lock(g_keyboardMutex);
  *info = KeyboardInfo{};
  info->user_id = g_keyboardUser;
  info->device = resource_id == KeyboardResource ? 1u : 0u;
  info->repeat_delay = 500;
  info->repeat_rate = 33;
  info->status = resource_id == KeyboardResource ? 1u : 0u;
  return 0;
 }

 if (info != nullptr) *info = KeyboardInfo{};
 return 0;
}

int APS5_VABI sceImeKeyboardGetResourceId(int32_t user_id, KeyboardResourceIdArray* resource_ids) {
 if (imeKeyboardEnabled() && resource_ids != nullptr) {
  std::lock_guard lock(g_keyboardMutex);
  g_keyboardUser = user_id;
  *resource_ids = KeyboardResourceIdArray{};
  resource_ids->user_id = user_id;
  resource_ids->resource_id[0] = KeyboardResource;
  return 0;
 }

 if (resource_ids != nullptr) {
  *resource_ids = KeyboardResourceIdArray{};
  resource_ids->user_id = user_id;
 }
 return 0;
}

int APS5_VABI sceImeKeyboardOpen(int32_t user_id, const KeyboardParam* param) {
 if (!param) APS5_INVALID_ARG_EX;
 std::lock_guard lock(g_keyboardMutex);
 if (!g_openKeyboards.insert(user_id).second) throw std::logic_error("sceImeKeyboardOpen: keyboard already open for user " + std::to_string(user_id));
 if (imeKeyboardEnabled()) {
  g_keyboardUser = user_id;
  g_keyboardHandler = param->handler;
  g_keyboardArg = param->arg;
  g_openEventPending = true;
  KeyboardImeSetActive_nid_postfix(true);
  std::fprintf(stderr, "[ime] keyboard opened for user %d (handler %p)\n", user_id, reinterpret_cast<void*>(param->handler));
 }
 return 0;
}

int APS5_VABI sceImeKeyboardSetMode(int32_t user_id, uint32_t mode) {
 (void)user_id;
 (void)mode;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeOpen_nid_postfix(const Param* param, const ExtendedParam* extended) {
 (void)param;
 (void)extended;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void APS5_VABI sceImeParamInit(Param* param) {
 (void)param;
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceImeSetCaret(const Caret* caret) {
 (void)caret;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeSetText(const char16_t* text, uint32_t length) {
 (void)text;
 (void)length;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeSetTextGeometry(TextAreaMode mode, const TextGeometry* geometry) {
 (void)mode;
 (void)geometry;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceImeUpdate(EventHandler handler) {
 if (!handler) APS5_INVALID_ARG_EX;
 if (!imeKeyboardEnabled()) return 0;
 void* arg = nullptr;
 bool open = false;
 int32_t user = 0;
 {
  std::lock_guard lock(g_keyboardMutex);
  if (g_keyboardHandler == nullptr) return 0;
  arg = g_keyboardArg;
  open = g_openEventPending;
  g_openEventPending = false;
  user = g_keyboardUser;
 }

 const auto guest = reinterpret_cast<GuestHandler>(handler);
 if (open) {
  ImeEvent event{};
  event.id = EventConnection;
  event.param.resource_id_array.user_id = user;
  event.param.resource_id_array.resource_id[0] = KeyboardResource;
  guest(arg, &event);
 }

 static const bool selfTest = std::getenv("APS5_IME_SELFTEST") != nullptr;
 if (selfTest) {
  static const auto start = std::chrono::steady_clock::now();
  static const double moveAfter = std::getenv("APS5_IME_SELFTEST_MOVE") != nullptr ? std::atof(std::getenv("APS5_IME_SELFTEST_MOVE")) : 110.0;
  static std::uint16_t held = 0;
  static auto last = start;
  const auto now = std::chrono::steady_clock::now();
  const double elapsed = std::chrono::duration<double>(now - start).count();
  std::uint16_t want = 0;
  bool decided = false;
  if (elapsed < moveAfter) {
   if (now - last > std::chrono::seconds(3)) {
    last = now;
    static bool down = false;
    static int round = 0;
    down = !down;
    if (down) ++round;
    want = down ? ((round % 2) != 0 ? std::uint16_t(0x2c) : std::uint16_t(0x28)) : std::uint16_t(0);
    decided = true;
   }
  } else {
   const double t = elapsed - moveAfter;
   static const std::uint16_t moves[4] = {0x07, 0x04, 0x4f, 0x50};
   const int slot = static_cast<int>(t / 8.0) % 4;
   want = std::fmod(t, 8.0) < 6.0 ? moves[slot] : std::uint16_t(0);
   decided = true;
  }
  if (decided && want != held) {
   const auto stamp = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count());
   const auto send = [&](std::uint16_t usage, bool down) {
    ImeEvent event{};
    event.id = down ? EventKeycodeDown : EventKeycodeUp;
    event.param.keycode.keycode = usage;
    event.param.keycode.character = (down && usage == 0x2c) ? u' ' : char16_t(0);
    event.param.keycode.user_id = user;
    event.param.keycode.resource_id = KeyboardResource;
    event.param.keycode.timestamp = stamp;
    guest(arg, &event);
    std::fprintf(stderr, "[ime] self-test key 0x%x %s\n", usage, down ? "down" : "up");
   };
   if (held != 0) send(held, false);
   if (want != 0) send(want, true);
   held = want;
  }
 }
 KeyboardImeKey keys[32];
 std::uint32_t count;
 while ((count = KeyboardImeDrain_nid_postfix(keys, 32)) != 0) {
  for (std::uint32_t i = 0; i < count; ++i) {
   ImeEvent event{};
   event.id = keys[i].pressed ? EventKeycodeDown : EventKeycodeUp;
   event.param.keycode.keycode = keys[i].usage;
   event.param.keycode.character = keys[i].pressed ? characterOf(keys[i].usage) : char16_t(0);
   event.param.keycode.status = 0;
   event.param.keycode.type = 0;
   event.param.keycode.user_id = user;
   event.param.keycode.resource_id = KeyboardResource;
   event.param.keycode.timestamp = keys[i].timestampUs;
   guest(arg, &event);
  }
 }
 return 0;
}

}
