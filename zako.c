#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "input-method-unstable-v2.h"
#include "libzako/libzako.h"
#include "virtual-keyboard-unstable-v1.h"

struct zako_wayland {
  struct zako                             zako;
  struct wl_display                      *display;
  struct wl_registry                     *registry;
  struct wl_list                          seats;
  struct zwp_input_method_manager_v2     *input_method_manager;
  struct zwp_virtual_keyboard_manager_v1 *virtual_keyboard_manager;
  bool                                    active;
};

struct zako_seat {
  struct wl_list                            link;
  struct wl_seat                           *seat;
  struct zwp_input_method_v2               *input_method;
  struct zwp_input_method_keyboard_grab_v2 *keyboard_grab;
  struct zwp_virtual_keyboard_v1           *virtual_keyboard;
  struct xkb_context                       *context;
  struct xkb_keymap                        *keymap;
  struct xkb_state                         *state;
  struct zako                              *zako;
  struct zako_wayland                      *wayland;
  bool                                      active, activate, deactivate;
  uint32_t                                  name, serial;
  xkb_keycode_t                             record[32];
};

static bool zako_pressed_dispatch (struct zako_seat *seat,
                                   xkb_keycode_t     keycode) {
  xkb_keysym_t keysym = xkb_state_key_get_one_sym (seat->state, keycode);

  char *preedit, *commit = NULL;

  if (keysym == XKB_KEY_backslash &&
      xkb_state_mod_name_is_active (seat->state, XKB_MOD_NAME_CTRL,
                                    XKB_STATE_MODS_EFFECTIVE)) {
    if (seat->wayland->active && (commit = zako_get_commit (seat->zako))) {
      commit = strdup (commit);
      zako_reset (seat->zako);
      preedit = strdup (zako_get_preedit (seat->zako));

      zwp_input_method_v2_commit_string (seat->input_method, commit);
      zwp_input_method_v2_set_preedit_string (seat->input_method, preedit, 0,
                                              strlen (preedit));
      zwp_input_method_v2_commit (seat->input_method, seat->serial);

      free (commit);
      free (preedit);
    }

    seat->wayland->active ^= true;
    return true;
  }

  if (!seat->wayland->active ||
      xkb_state_mod_names_are_active (
        seat->state, XKB_STATE_MODS_EFFECTIVE,
        XKB_STATE_MATCH_ANY | XKB_STATE_MATCH_NON_EXCLUSIVE, XKB_MOD_NAME_CTRL,
        XKB_MOD_NAME_ALT, NULL))
    return false;

  bool handled = false;

  uint32_t key = xkb_keysym_to_utf32 (keysym);
  if (key >= 'a' && key <= 'z') {
    zako_forward (seat->zako, (char) key);
    handled = true;
  }

  if (zako_should_commit (seat->zako)) {
    commit  = strdup (zako_get_commit (seat->zako));
    handled = true;
  }

  switch (keysym) {
  case XKB_KEY_Left:
    handled = zako_select_previous (seat->zako);
    break;
  case XKB_KEY_Right:
    handled = zako_select_next (seat->zako);
    break;
  case XKB_KEY_Return:
    commit  = strdup (zako_get_commit (seat->zako));
    handled = commit[0] != '\0';
    break;
  case XKB_KEY_BackSpace:
    handled = zako_backward (seat->zako);
  }

  if (!handled && !commit && (commit = zako_get_commit (seat->zako)))
    commit = strdup (commit);

  if (commit) {
    zako_reset (seat->zako);
    zwp_input_method_v2_commit_string (seat->input_method, commit);
    free (commit);
  }

  if ((preedit = zako_get_preedit (seat->zako)))
    zwp_input_method_v2_set_preedit_string (seat->input_method, preedit, 0,
                                            strlen (preedit));

  zwp_input_method_v2_commit (seat->input_method, seat->serial);

  if (handled)
    for (size_t i = 0; i < sizeof (seat->record) / sizeof (seat->record[0]);
         i++)
      if (seat->record[i] == 0) {
        seat->record[i] = keycode;
        break;
      }

  return handled;
}

static bool zako_released_dispatch (struct zako_seat *seat,
                                    xkb_keycode_t     keycode) {
  bool handled = false;

  for (size_t i = 0; i < sizeof (seat->record) / sizeof (seat->record[0]); i++)
    if (seat->record[i] == keycode) {
      seat->record[i] = 0;
      handled         = true;
      break;
    }

  return handled;
}

static void input_method_keyboard_grab_listener_keymap (
  void *data, struct zwp_input_method_keyboard_grab_v2 *keyboard_grab,
  uint32_t format, int32_t fd, uint32_t size) {
  struct zako_seat *seat = data;

  zwp_virtual_keyboard_v1_keymap (seat->virtual_keyboard, format, fd, size);

  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
    return;

  xkb_state_unref (seat->state);
  xkb_keymap_unref (seat->keymap);

  char *shm = mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);

  seat->keymap = xkb_keymap_new_from_string (
    seat->context, shm, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
  seat->state = xkb_state_new (seat->keymap);

  munmap (shm, size);
  close (fd);
}

static void input_method_keyboard_grab_listener_key (
  void *data, struct zwp_input_method_keyboard_grab_v2 *keyboard_grab,
  uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
  struct zako_seat *seat = data;

  bool          handled = false;
  xkb_keycode_t keycode = key + 8;

  switch (state) {
  case WL_KEYBOARD_KEY_STATE_PRESSED:
    xkb_state_update_key (seat->state, keycode, XKB_KEY_DOWN);
    handled = zako_pressed_dispatch (seat, keycode);
    break;
  case WL_KEYBOARD_KEY_STATE_RELEASED:
    xkb_state_update_key (seat->state, keycode, XKB_KEY_UP);
    handled = zako_released_dispatch (seat, keycode);
    break;
  }

  if (!handled)
    zwp_virtual_keyboard_v1_key (seat->virtual_keyboard, time, key, state);
}

static void input_method_keyboard_grab_listener_modifiers (
  void *data, struct zwp_input_method_keyboard_grab_v2 *keyboard_grab,
  uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
  uint32_t mods_locked, uint32_t group) {
  struct zako_seat *seat = data;

  xkb_state_update_mask (seat->state, mods_depressed, mods_latched, mods_locked,
                         0, 0, group);
  zwp_virtual_keyboard_v1_modifiers (seat->virtual_keyboard, mods_depressed,
                                     mods_latched, mods_locked, group);
}

static void input_method_keyboard_grab_listener_repeat_info (
  void *data, struct zwp_input_method_keyboard_grab_v2 *keyboard_grab,
  int32_t rate, int32_t delay) {}

static const struct zwp_input_method_keyboard_grab_v2_listener
  input_method_keyboard_grab_listener = {
    .keymap      = input_method_keyboard_grab_listener_keymap,
    .key         = input_method_keyboard_grab_listener_key,
    .modifiers   = input_method_keyboard_grab_listener_modifiers,
    .repeat_info = input_method_keyboard_grab_listener_repeat_info,
};

static void
input_method_listener_activate (void                       *data,
                                struct zwp_input_method_v2 *input_method) {
  struct zako_seat *seat = data;
  seat->activate         = true;
}

static void
input_method_listener_deactivate (void                       *data,
                                  struct zwp_input_method_v2 *input_method) {
  struct zako_seat *seat = data;
  seat->deactivate       = true;
}

static void input_method_listener_surrounding_text (
  void *data, struct zwp_input_method_v2 *input_method, const char *text,
  uint32_t cursor, uint32_t anchor) {}

static void input_method_listener_text_change_cause (
  void *data, struct zwp_input_method_v2 *input_method, uint32_t cause) {}

static void
input_method_listener_content_type (void                       *data,
                                    struct zwp_input_method_v2 *input_method,
                                    uint32_t hint, uint32_t purpose) {}

static void
input_method_listener_done (void                       *data,
                            struct zwp_input_method_v2 *input_method) {
  struct zako_seat *seat = data;
  seat->serial++;

  if (seat->activate && !seat->active) {
    seat->keyboard_grab = zwp_input_method_v2_grab_keyboard (input_method);
    zwp_input_method_keyboard_grab_v2_add_listener (
      seat->keyboard_grab, &input_method_keyboard_grab_listener, seat);
    seat->active = true;
  } else if (seat->deactivate && seat->active) {
    zwp_input_method_keyboard_grab_v2_release (seat->keyboard_grab);
    seat->keyboard_grab = NULL;
    seat->active        = false;
    memset (seat->record, 0, sizeof (seat->record));
  }

  seat->activate   = false;
  seat->deactivate = false;
}

static void
input_method_listener_unavailable (void                       *data,
                                   struct zwp_input_method_v2 *input_method) {}

static const struct zwp_input_method_v2_listener input_method_listener = {
  .activate          = input_method_listener_activate,
  .deactivate        = input_method_listener_deactivate,
  .surrounding_text  = input_method_listener_surrounding_text,
  .text_change_cause = input_method_listener_text_change_cause,
  .content_type      = input_method_listener_content_type,
  .done              = input_method_listener_done,
  .unavailable       = input_method_listener_unavailable,
};

static void registry_listener_global (void *data, struct wl_registry *registry,
                                      uint32_t name, const char *interface,
                                      uint32_t version) {
  struct zako_wayland *wayland = data;

  if (strcmp (interface, wl_seat_interface.name) == 0) {
    struct zako_seat *seat = calloc (1, sizeof (*seat));

    seat->seat    = wl_registry_bind (registry, name, &wl_seat_interface, 7);
    seat->context = xkb_context_new (XKB_CONTEXT_NO_FLAGS);
    seat->wayland = wayland;
    seat->name    = name;

    wl_list_insert (&wayland->seats, &seat->link);
  } else if (strcmp (interface, zwp_input_method_manager_v2_interface.name) ==
             0)
    wayland->input_method_manager = wl_registry_bind (
      registry, name, &zwp_input_method_manager_v2_interface, 1);
  else if (strcmp (interface, zwp_virtual_keyboard_manager_v1_interface.name) ==
           0)
    wayland->virtual_keyboard_manager = wl_registry_bind (
      registry, name, &zwp_virtual_keyboard_manager_v1_interface, 1);
}

static void registry_listener_global_remove (void               *data,
                                             struct wl_registry *registry,
                                             uint32_t            name) {
  struct zako_wayland *wayland = data;
  struct zako_seat    *seat, *tmp;

  wl_list_for_each_safe (seat, tmp, &wayland->seats,
                         link) if (seat->name == name) {
    wl_list_remove (&seat->link);

    wl_seat_release (seat->seat);
    zwp_input_method_v2_destroy (seat->input_method);
    zwp_virtual_keyboard_v1_destroy (seat->virtual_keyboard);

    if (seat->keyboard_grab)
      zwp_input_method_keyboard_grab_v2_release (seat->keyboard_grab);

    xkb_state_unref (seat->state);
    xkb_keymap_unref (seat->keymap);
    xkb_context_unref (seat->context);

    free (seat);
  }
}

static const struct wl_registry_listener registry_listener = {
  .global        = registry_listener_global,
  .global_remove = registry_listener_global_remove,
};

static void usage (FILE *out, const char *name) {
  fprintf (out,
           "Usage: %s [options...]\n"
           "\n"
           " -h         Show the help message and quit.\n"
           " -d <file>  Load in appropriate dictionary.\n"
           "\n"
           "Copyright (C) 2026 Jing Huang.\n",
           name);
}

int main (int argc, char *argv[]) {
  struct zako_wayland wayland = {0};
  wl_list_init (&wayland.seats);

  int32_t opt;
  char   *dictionary = NULL;

  while ((opt = getopt (argc, argv, "hd:")) != -1) {
    switch (opt) {
    case 'h':
      usage (stdout, *argv);
      return 0;
    case 'd':
      dictionary = strdup (optarg);
      break;
    default:
      usage (stderr, *argv);
      return 1;
    }
  }

  if (!dictionary)
    return 1;
  zako_init (&wayland.zako, dictionary);
  free (dictionary);

  wayland.display  = wl_display_connect (NULL);
  wayland.registry = wl_display_get_registry (wayland.display);

  wl_registry_add_listener (wayland.registry, &registry_listener, &wayland);
  wl_display_roundtrip (wayland.display);

  struct zako_seat *seat;
  wl_list_for_each (seat, &wayland.seats, link) {
    seat->input_method = zwp_input_method_manager_v2_get_input_method (
      wayland.input_method_manager, seat->seat);
    zwp_input_method_v2_add_listener (seat->input_method,
                                      &input_method_listener, seat);

    seat->virtual_keyboard =
      zwp_virtual_keyboard_manager_v1_create_virtual_keyboard (
        wayland.virtual_keyboard_manager, seat->seat);
    seat->zako = &wayland.zako;
  }

  while (wl_display_dispatch (wayland.display) != -1)
    ;

  struct zako_seat *tmp;
  wl_list_for_each_safe (seat, tmp, &wayland.seats, link) {
    wl_list_remove (&seat->link);

    wl_seat_release (seat->seat);
    zwp_input_method_v2_destroy (seat->input_method);
    zwp_virtual_keyboard_v1_destroy (seat->virtual_keyboard);

    if (seat->keyboard_grab)
      zwp_input_method_keyboard_grab_v2_release (seat->keyboard_grab);

    xkb_state_unref (seat->state);
    xkb_keymap_unref (seat->keymap);
    xkb_context_unref (seat->context);

    free (seat);
  }

  zwp_input_method_manager_v2_destroy (wayland.input_method_manager);
  zwp_virtual_keyboard_manager_v1_destroy (wayland.virtual_keyboard_manager);

  wl_registry_destroy (wayland.registry);
  wl_display_disconnect (wayland.display);

  zako_dispose (&wayland.zako);
}
