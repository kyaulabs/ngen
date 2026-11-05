#include <SDL3/SDL.h>
#include <wayland-client.h>
#include <wayland-server.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
// A private, version-1 Wayland peer exercises SDL through its public API.
// Only the shell/tablet requests used by video initialization are implemented.
extern const wl_interface tablet_seat;
extern const wl_interface tablet;
extern const wl_interface pad;
extern const wl_interface group;
extern const wl_interface ring;
extern const wl_interface strip;
const wl_interface *seat_types[]{&tablet_seat, &wl_seat_interface};
const wl_message manager_requests[]{{"get_tablet_seat", "no", seat_types},
                                    {"destroy", "", nullptr}};
const wl_interface manager{"zwp_tablet_manager_v2", 1, 2, manager_requests, 0, nullptr};
const wl_message destroy_request[]{{"destroy", "", nullptr}};
const wl_interface *seat_event_types[]{&tablet, nullptr, &pad};
const wl_message seat_events[]{{"tablet_added", "n", seat_event_types},
                               {"tool_added", "n", seat_event_types + 1},
                               {"pad_added", "n", seat_event_types + 2}};
const wl_interface tablet_seat{"zwp_tablet_seat_v2", 1, 1, destroy_request, 3, seat_events};
const wl_message tablet_events[]{{"name", "s", nullptr},
                                 {"id", "uu", nullptr},
                                 {"path", "s", nullptr},
                                 {"done", "", nullptr},
                                 {"removed", "", nullptr}};
const wl_interface tablet{"zwp_tablet_v2", 1, 1, destroy_request, 5, tablet_events};
const wl_message pad_requests[]{{"set_feedback", "usu", nullptr}, {"destroy", "", nullptr}};
const wl_interface *group_type[]{&group};
const wl_interface *enter_types[]{nullptr, &tablet, &wl_surface_interface};
const wl_message pad_events[]{{"group", "n", group_type}, {"path", "s", nullptr},
                              {"buttons", "u", nullptr},  {"done", "", nullptr},
                              {"button", "uuu", nullptr}, {"enter", "uoo", enter_types},
                              {"leave", "uo", nullptr},   {"removed", "", nullptr}};
const wl_interface pad{"zwp_tablet_pad_v2", 1, 2, pad_requests, 8, pad_events};
const wl_interface *ring_type[]{&ring};
const wl_interface *strip_type[]{&strip};
const wl_message group_events[]{{"buttons", "a", nullptr},  {"ring", "n", ring_type},
                                {"strip", "n", strip_type}, {"modes", "u", nullptr},
                                {"done", "", nullptr},      {"mode_switch", "uuu", nullptr}};
const wl_interface group{"zwp_tablet_pad_group_v2", 1, 1, destroy_request, 6, group_events};
const wl_message control_requests[]{{"set_feedback", "su", nullptr}, {"destroy", "", nullptr}};
const wl_message ring_events[]{{"source", "u", nullptr},
                               {"angle", "f", nullptr},
                               {"stop", "", nullptr},
                               {"frame", "u", nullptr}};
const wl_interface ring{"zwp_tablet_pad_ring_v2", 1, 2, control_requests, 4, ring_events};
const wl_message strip_events[]{{"source", "u", nullptr},
                                {"position", "u", nullptr},
                                {"stop", "", nullptr},
                                {"frame", "u", nullptr}};
const wl_interface strip{"zwp_tablet_pad_strip_v2", 1, 2, control_requests, 4, strip_events};
const wl_message shell_requests[]{{"destroy", "", nullptr},
                                  {"create_positioner", "n", nullptr},
                                  {"get_xdg_surface", "no", nullptr},
                                  {"pong", "u", nullptr}};
const wl_message shell_events[]{{"ping", "u", nullptr}};
const wl_interface shell{"xdg_wm_base", 1, 4, shell_requests, 1, shell_events};

struct Server {
    wl_display *display{};
    wl_listener disconnected{};
    std::array<std::uint32_t, 5> destroyed{};
    bool remove{};
    int report{};
};

int checkpoint(int fd, std::uint32_t, void *data) {
    auto &state = *static_cast<Server *>(data);
    char command{};
    if (read(fd, &command, 1) != 1)
        return -1;
    return write(state.report, state.destroyed.data(), sizeof(state.destroyed)) ==
                   static_cast<ssize_t>(sizeof(state.destroyed))
               ? 0
               : -1;
}

void discard(wl_client *, wl_resource *resource) { wl_resource_destroy(resource); }
void counted_destroy(wl_client *, wl_resource *resource) {
    ++*static_cast<std::uint32_t *>(wl_resource_get_user_data(resource));
    wl_resource_destroy(resource);
}
struct Destroy {
    void (*destroy)(wl_client *, wl_resource *);
};
const Destroy destroy{discard};
const Destroy counted{counted_destroy};
struct Control {
    void (*feedback)(wl_client *, wl_resource *, const char *, std::uint32_t);
    void (*destroy)(wl_client *, wl_resource *);
};
const Control control{nullptr, counted_destroy};
struct Pad {
    void (*feedback)(wl_client *, wl_resource *, std::uint32_t, const char *, std::uint32_t);
    void (*destroy)(wl_client *, wl_resource *);
};
const Pad pad_impl{nullptr, counted_destroy};

void get_tablet_seat(wl_client *client, wl_resource *resource, std::uint32_t id, wl_resource *) {
    auto &state = *static_cast<Server *>(wl_resource_get_user_data(resource));
    auto *seat = wl_resource_create(client, &tablet_seat, 1, id);
    wl_resource_set_implementation(seat, &destroy, nullptr, nullptr);
    auto *t = wl_resource_create(client, &tablet, 1, 0);
    auto *p = wl_resource_create(client, &pad, 1, 0);
    auto *g = wl_resource_create(client, &group, 1, 0);
    auto *r = wl_resource_create(client, &ring, 1, 0);
    auto *s = wl_resource_create(client, &strip, 1, 0);
    wl_resource_set_implementation(t, &counted, &state.destroyed[0], nullptr);
    wl_resource_set_implementation(p, &pad_impl, &state.destroyed[1], nullptr);
    wl_resource_set_implementation(g, &counted, &state.destroyed[2], nullptr);
    wl_resource_set_implementation(r, &control, &state.destroyed[3], nullptr);
    wl_resource_set_implementation(s, &control, &state.destroyed[4], nullptr);
    wl_resource_post_event(seat, 0, t);
    wl_resource_post_event(t, 0, "NGEN test tablet");
    wl_resource_post_event(t, 3);
    wl_resource_post_event(seat, 2, p);
    wl_resource_post_event(p, 0, g);
    wl_resource_post_event(g, 1, r);
    wl_resource_post_event(r, 0, 1U);
    wl_resource_post_event(r, 1, wl_fixed_from_int(90));
    wl_resource_post_event(r, 3, 1U);
    wl_resource_post_event(g, 2, s);
    wl_resource_post_event(s, 0, 1U);
    wl_resource_post_event(s, 1, 0U);
    wl_resource_post_event(s, 3, 1U);
    wl_resource_post_event(g, 4);
    wl_resource_post_event(p, 3);
    if (state.remove) {
        wl_resource_post_event(p, 7);
        wl_resource_post_event(t, 4);
    }
}
struct Manager {
    void (*seat)(wl_client *, wl_resource *, std::uint32_t, wl_resource *);
    void (*destroy)(wl_client *, wl_resource *);
};
const Manager manager_impl{get_tablet_seat, discard};

void bind_manager(wl_client *client, void *data, std::uint32_t, std::uint32_t id) {
    auto *resource = wl_resource_create(client, &manager, 1, id);
    wl_resource_set_implementation(resource, &manager_impl, data, nullptr);
}
void bind_seat(wl_client *client, void *, std::uint32_t, std::uint32_t id) {
    static const struct wl_seat_interface impl{nullptr, nullptr, nullptr, discard};
    auto *resource = wl_resource_create(client, &wl_seat_interface, 5, id);
    wl_resource_set_implementation(resource, &impl, nullptr, nullptr);
    wl_seat_send_capabilities(resource, 0);
    wl_seat_send_name(resource, "test-seat");
}
void bind_output(wl_client *client, void *, std::uint32_t, std::uint32_t id) {
    auto *resource = wl_resource_create(client, &wl_output_interface, 2, id);
    wl_output_send_geometry(resource, 0, 0, 320, 240, WL_OUTPUT_SUBPIXEL_UNKNOWN, "NGEN", "test",
                            WL_OUTPUT_TRANSFORM_NORMAL);
    wl_output_send_mode(resource, WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED, 800, 600,
                        60000);
    wl_output_send_scale(resource, 1);
    wl_output_send_done(resource);
}
void bind_shell(wl_client *client, void *, std::uint32_t, std::uint32_t id) {
    struct Shell {
        void (*destroy)(wl_client *, wl_resource *);
        void (*positioner)(wl_client *, wl_resource *, std::uint32_t);
        void (*surface)(wl_client *, wl_resource *, std::uint32_t, wl_resource *);
        void (*pong)(wl_client *, wl_resource *, std::uint32_t);
    };
    static const Shell impl{discard, nullptr, nullptr, nullptr};
    auto *resource = wl_resource_create(client, &shell, 1, id);
    wl_resource_set_implementation(resource, &impl, nullptr, nullptr);
}
void bind_compositor(wl_client *client, void *, std::uint32_t, std::uint32_t id) {
    static const struct wl_compositor_interface impl{};
    auto *resource = wl_resource_create(client, &wl_compositor_interface, 4, id);
    wl_resource_set_implementation(resource, &impl, nullptr, nullptr);
}
void disconnected(wl_listener *listener, void *) {
    Server *state = nullptr;
    state = wl_container_of(listener, state, disconnected);
    wl_display_terminate(state->display);
}

struct PeerDescriptors {
    int connection;
    int report;
    int commands;
};

[[noreturn]] void serve(PeerDescriptors descriptors, bool remove) {
    Server state;
    state.remove = remove;
    state.report = descriptors.report;
    state.display = wl_display_create();
    if (!state.display || wl_display_init_shm(state.display) != 0)
        _exit(2);
    if (!wl_global_create(state.display, &wl_compositor_interface, 4, nullptr, bind_compositor) ||
        !wl_global_create(state.display, &wl_output_interface, 2, nullptr, bind_output) ||
        !wl_global_create(state.display, &wl_seat_interface, 5, nullptr, bind_seat) ||
        !wl_global_create(state.display, &shell, 1, nullptr, bind_shell) ||
        !wl_global_create(state.display, &manager, 1, &state, bind_manager))
        _exit(2);
    auto *client = wl_client_create(state.display, descriptors.connection);
    if (!client)
        _exit(2);
    state.disconnected.notify = disconnected;
    wl_client_add_destroy_listener(client, &state.disconnected);
    auto *source =
        wl_event_loop_add_fd(wl_display_get_event_loop(state.display), descriptors.commands,
                             WL_EVENT_READABLE, checkpoint, &state);
    if (!source)
        _exit(2);
    wl_display_run(state.display);
    wl_event_source_remove(source);
    close(descriptors.commands);
    wl_display_destroy(state.display);
    const bool sent = write(descriptors.report, state.destroyed.data(), sizeof(state.destroyed)) ==
                      static_cast<ssize_t>(sizeof(state.destroyed));
    close(descriptors.report);
    std::exit(sent ? 0 : 2);
}
} // namespace

int main(int argc, char **) {
    std::array<int, 2> sockets{}, reports{}, commands{};
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets.data()) != 0 ||
        pipe(reports.data()) != 0 || pipe(commands.data()) != 0)
        return 2;
    const auto child = fork();
    if (child < 0)
        return 2;
    if (child == 0) {
        close(sockets[0]);
        close(reports[0]);
        close(commands[1]);
        serve({.connection = sockets[1], .report = reports[1], .commands = commands[0]}, argc > 1);
    }
    close(sockets[1]);
    close(reports[1]);
    close(commands[0]);
    auto *display = wl_display_connect_to_fd(sockets[0]);
    if (!display || setenv("WAYLAND_DISPLAY", "ngen-private-test", 1) != 0 ||
        !SDL_SetPointerProperty(SDL_GetGlobalProperties(),
                                SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER, display))
        return 2;
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR, "0");
    const bool initialized = SDL_Init(SDL_INIT_VIDEO);
    if (!initialized)
        std::fprintf(stderr, "SDL initialization: %s\n", SDL_GetError());
    std::array<std::uint32_t, 5> before_shutdown{};
    const bool synchronized = initialized && display && wl_display_roundtrip(display) >= 0;
    const bool snapshot = synchronized && write(commands[1], "?", 1) == 1 &&
                          read(reports[0], before_shutdown.data(), sizeof(before_shutdown)) ==
                              static_cast<ssize_t>(sizeof(before_shutdown));
    const bool removal_observed =
        argc > 1 ? before_shutdown == std::array<std::uint32_t, 5>{1, 1, 1, 1, 1}
                 : before_shutdown[0] == 0 && before_shutdown[1] == 0;
    SDL_Quit();
    const bool flushed = wl_display_roundtrip(display) >= 0;
    wl_display_disconnect(display);
    std::array<std::uint32_t, 5> destroyed{};
    const auto received = read(reports[0], destroyed.data(), sizeof(destroyed));
    close(reports[0]);
    int status{};
    const auto waited = waitpid(child, &status, 0);
    close(commands[1]);
    std::printf("tablet=%u pad=%u group=%u ring=%u strip=%u\n", destroyed[0], destroyed[1],
                destroyed[2], destroyed[3], destroyed[4]);
    return initialized && snapshot && removal_observed && flushed &&
                   received == static_cast<ssize_t>(sizeof(destroyed)) && waited == child &&
                   WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
                   destroyed == std::array<std::uint32_t, 5>{1, 1, 1, 1, 1}
               ? 0
               : 1;
}
