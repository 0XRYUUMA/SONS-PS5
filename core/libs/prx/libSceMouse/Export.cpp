#include <cstdio>
#include "prx/libSceMouse/include/MouseState.hpp"

extern "C" {

int APS5_VABI sceMouseInit() {
    return Mouse::Initialize();
}

int APS5_VABI sceMouseOpen(int userId, std::int32_t type, std::int32_t index, const void* param) {
    const auto handle = Mouse::Open(userId, type, index, static_cast<const MouseOpenParam*>(param));
    std::fprintf(stderr, "[mouse] sceMouseOpen user=%d type=%d index=%d -> %d\n", userId, type, index, handle);
    return handle;
}

int APS5_VABI sceMouseClose(std::int32_t handle) {
    return Mouse::Close(handle);
}

int APS5_VABI sceMouseRead(std::int32_t handle, MouseData* data, std::int32_t num) {
    return Mouse::Read(handle, data, num);
}

void MousePublishInput_nid_postfix(const MouseInputEvent& event) {
    Mouse::Publish(event);
}

}
