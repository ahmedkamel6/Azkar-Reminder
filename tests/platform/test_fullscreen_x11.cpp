#include "platform/IFullScreenDetector.h"
#include <xcb/xcb.h>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <iostream>

int main() {
    // This test mutates EWMH properties and must run in a disposable Xvfb server.
    if (!std::getenv("AZKAR_TEST_X11")) return 77;
    unsetenv("WAYLAND_DISPLAY");
    unsetenv("XDG_SESSION_TYPE");
    std::unique_ptr<xcb_connection_t, decltype(&xcb_disconnect)> connection(xcb_connect(nullptr, nullptr), xcb_disconnect);
    if (xcb_connection_has_error(connection.get())) return 1;
    auto* screen = xcb_setup_roots_iterator(xcb_get_setup(connection.get())).data;
    auto atom = [&](const char* name) {
        auto cookie = xcb_intern_atom(connection.get(), 0, std::strlen(name), name);
        std::unique_ptr<xcb_intern_atom_reply_t, decltype(&std::free)> reply(xcb_intern_atom_reply(connection.get(), cookie, nullptr), std::free);
        return reply ? reply->atom : XCB_ATOM_NONE;
    };
    const auto active = atom("_NET_ACTIVE_WINDOW");
    const auto state = atom("_NET_WM_STATE");
    const auto fullscreen = atom("_NET_WM_STATE_FULLSCREEN");
    if (!active || !state || !fullscreen) return 1;
    auto detector = dhikr::IFullScreenDetector::Create();
    auto synchronize = [&] {
        std::free(xcb_get_input_focus_reply(connection.get(), xcb_get_input_focus(connection.get()), nullptr));
    };
    const xcb_window_t stale = 0xdeadbeef;
    xcb_change_property(connection.get(), XCB_PROP_MODE_REPLACE, screen->root, active, XCB_ATOM_WINDOW, 32, 1, &stale);
    xcb_flush(connection.get());
    synchronize();
    if (detector->CheckFullScreen() != dhikr::FullScreenState::Unknown) return 1;
    const auto window = xcb_generate_id(connection.get());
    xcb_create_window(connection.get(), XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, 50, 50, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, 0, nullptr);
    xcb_change_property(connection.get(), XCB_PROP_MODE_REPLACE, screen->root, active, XCB_ATOM_WINDOW, 32, 1, &window);
    xcb_change_property(connection.get(), XCB_PROP_MODE_REPLACE, window, state, XCB_ATOM_ATOM, 32, 1, &fullscreen);
    xcb_flush(connection.get());
    synchronize();
    if (detector->CheckFullScreen() != dhikr::FullScreenState::FullScreen) return 1;
    xcb_delete_property(connection.get(), window, state);
    xcb_flush(connection.get());
    synchronize();
    if (detector->CheckFullScreen() != dhikr::FullScreenState::NotFullScreen) return 1;
    xcb_delete_property(connection.get(), screen->root, active);
    xcb_destroy_window(connection.get(), window);
    xcb_flush(connection.get());
    std::cout << "PASS: stale X11 window returns Unknown; fullscreen and normal states detected.\n";
}
