#include "LinuxFullScreenDetector.h"
#include "../../core/Logger.h"
#include <cstdlib>
#include <cstring>
#include <memory>
#if defined(__linux__)
#include <xcb/xcb.h>

namespace dhikr {
std::unique_ptr<IFullScreenDetector> IFullScreenDetector::Create() {
    return std::make_unique<LinuxFullScreenDetector>();
}

LinuxFullScreenDetector::LinuxFullScreenDetector() {
    const auto* wayland = std::getenv("WAYLAND_DISPLAY");
    const auto* session = std::getenv("XDG_SESSION_TYPE");
    m_isWayland = (wayland && *wayland) || (session && std::strcmp(session, "wayland") == 0);
}
LinuxFullScreenDetector::~LinuxFullScreenDetector() = default;

FullScreenState LinuxFullScreenDetector::CheckFullScreen() {
    if (m_isWayland) return FullScreenState::Unsupported;
    int screenNumber = 0;
    std::unique_ptr<xcb_connection_t, decltype(&xcb_disconnect)> connection(xcb_connect(nullptr, &screenNumber), xcb_disconnect);
    if (!connection || xcb_connection_has_error(connection.get())) return FullScreenState::Unknown;
    auto screens = xcb_setup_roots_iterator(xcb_get_setup(connection.get()));
    for (int index = 0; index < screenNumber && screens.rem; ++index) xcb_screen_next(&screens);
    if (!screens.rem) return FullScreenState::Unknown;
    auto atom = [&](const char* name) {
        const auto cookie = xcb_intern_atom(connection.get(), 0, static_cast<uint16_t>(std::strlen(name)), name);
        std::unique_ptr<xcb_intern_atom_reply_t, decltype(&std::free)> reply(xcb_intern_atom_reply(connection.get(), cookie, nullptr), std::free);
        return reply ? reply->atom : XCB_ATOM_NONE;
    };
    const auto activeAtom = atom("_NET_ACTIVE_WINDOW");
    const auto stateAtom = atom("_NET_WM_STATE");
    const auto fullscreenAtom = atom("_NET_WM_STATE_FULLSCREEN");
    if (!activeAtom || !stateAtom || !fullscreenAtom) return FullScreenState::Unknown;
    auto cookie = xcb_get_property(connection.get(), 0, screens.data->root, activeAtom, XCB_ATOM_WINDOW, 0, 1);
    std::unique_ptr<xcb_get_property_reply_t, decltype(&std::free)> active(xcb_get_property_reply(connection.get(), cookie, nullptr), std::free);
    if (!active) return FullScreenState::Unknown;
    if (active->type == XCB_ATOM_NONE) return FullScreenState::NotFullScreen;
    if (active->type != XCB_ATOM_WINDOW || active->format != 32 || active->value_len != 1) return FullScreenState::Unknown;
    const auto window = *static_cast<xcb_window_t*>(xcb_get_property_value(active.get()));
    if (!window) return FullScreenState::NotFullScreen;
    cookie = xcb_get_property(connection.get(), 0, window, stateAtom, XCB_ATOM_ATOM, 0, 1024);
    std::unique_ptr<xcb_get_property_reply_t, decltype(&std::free)> state(xcb_get_property_reply(connection.get(), cookie, nullptr), std::free);
    // A foreground window can disappear between these requests; XCB reports that without terminating the process.
    if (!state) return FullScreenState::Unknown;
    if (state->type == XCB_ATOM_NONE) return FullScreenState::NotFullScreen;
    if (state->type != XCB_ATOM_ATOM || state->format != 32) return FullScreenState::Unknown;
    const auto* values = static_cast<xcb_atom_t*>(xcb_get_property_value(state.get()));
    for (uint32_t index = 0; index < state->value_len; ++index) {
        if (values[index] == fullscreenAtom) return FullScreenState::FullScreen;
    }
    return FullScreenState::NotFullScreen;
}
} // namespace dhikr
#endif
