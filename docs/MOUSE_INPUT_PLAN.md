> **Status (Oct 2026): implemented** on `play` (tiers 1 and 2; the OS
> cursor is used, hidden after 3 s still). Differences from this plan:
> mouse events are queued by the backend (`lotr_get_mouse()`) and turned
> into keys in `gui_frame()`, so the backend never calls the GUI; the
> click map is emptied by `gui_clear()`, not per frame; click-to-move went
> straight to a breadth-first search (uniform step cost). See CLAUDE.md.

> **Note (Oct 2026):** written against `main` before the `play` branch work.
> Line numbers have drifted; re-check them. `SCREEN_FACT` is 4 only with libhqx
> (otherwise 1), so the 1280x800 logical-size warning applies to hqx builds only.
> Always divide mouse coordinates by `SCREEN_FACT`.

# Mouse input — implementation plan

Handoff doc for adding mouse support to the LOTR engine (`jflemer/lotr`).
Audited at commit `258fcb7` (main, 2025-03-12) on Ubuntu 24.04 / SDL2 2.30 / gcc 13.3.

## Build status (verified, not assumed)

`autoreconf -fi && ./configure && make` succeeds with **zero errors and zero
warnings** across ~24.5k lines of C. The binary runs and correctly reports
missing game data. The SDL1→SDL2 migration is already done — do not redo it.

- `liblotr/lotr_sdl2.c` is the backend that actually compiles (see `liblotr/Makefile.am:27`).
- `liblotr/lotr_sdl.c` is the **legacy SDL1 backend, dead code**. It still contains
  `SDL_SetVideoMode` / `SDL_Flip`. Do not edit it. Do not use it as a reference for
  current behaviour. Consider deleting it in a separate commit.
- `libhqx` was not present at configure time, so `HAVE_LIBHQX` is undefined and HQX
  scaling is compiled out. Install it if you want that path exercised.

## Why this is tractable

All SDL event handling in the engine lives in exactly one function:
`lotr_poll_events()` at `liblotr/lotr_sdl2.c:280`. There is no `SDL_Event`
anywhere else in the tree.

The other ~24k lines consume input only through this contract in `liblotr/lotr_sdl.h`:

```c
int  lotr_get_key(void);        /* pops from lotr_keybuffer[] */
int  lotr_kb_hit(void);
void lotr_reset_keyboard(void);
int  lotr_key_left(), lotr_key_right(), lotr_key_up(), lotr_key_down();
int  lotr_key_esc(), lotr_key_shift(), lotr_key_ctrl();
```

Backing state is `static int lotr_keybuffer[KEYBOARDBUFFERESIZE]` and
`lotr_keybufferpos` (`lotr_sdl2.c:54-55`), plus the boolean `lotr_key_*_pressed` flags.

**Consequence: mouse support requires no changes to game logic.** Synthesize into
these primitives and every existing dialog and movement path runs unmodified.

---

## THE COORDINATE TRAP — read this before writing any code

`lotr_system_init()` (`lotr_sdl2.c:144`) does:

```c
SDL_RenderSetLogicalSize(main_window_renderer,
                         SCREEN_WIDTH * win_fact, SCREEN_HEIGHT * win_fact);
```

where `win_fact = SCREEN_FACT`. From `lotr.h:63-71`:

```
SCREEN_FACT  = 4  (or 1 in the other branch — check which is active in your build)
SCREEN_WIDTH  = 320
SCREEN_HEIGHT = 200
```

SDL2 auto-translates mouse coordinates into **logical** space. Logical space here is
`1280x800`, **not** the native `320x200` framebuffer the game code reasons about.

**Every mouse coordinate must be divided by `SCREEN_FACT` before game code sees it.**

```c
static void mouse_to_framebuffer(int wx, int wy, int *fx, int *fy) {
    *fx = wx / SCREEN_FACT;
    *fy = wy / SCREEN_FACT;
}
```

Get this wrong and clicks land at a quarter-scale offset that looks almost correct
near the top-left corner and increasingly wrong toward the bottom-right — which reads
as "my hit-testing rects are subtly off" and sends you debugging the wrong file.

**First thing to build and test:** a `SDL_MOUSEBUTTONDOWN` handler that does nothing
but `fprintf(stderr, ...)` the framebuffer coords. Confirm a click on a known screen
feature reports plausible 320x200 coords before proceeding.

The window is `SDL_WINDOW_RESIZABLE` (`lotr_sdl2.c:126`), so SDL letterboxes on
non-4:3 windows. It handles that inside the logical-size translation — but only if you
read `event.button.x/y`, never raw window geometry.

---

## Tier 1 — menus and dialogs

Do this first. Self-contained, highest felt value, and it validates the coordinate
math on something forgiving.

### How dialogs actually work

They are **keystroke-choice driven**, not index-selection driven. There is no
"currently highlighted item" state to drive with arrow keys or hover.

`gui_frame()` at `liblotr/gui.c:3044`:

```c
key = lotr_get_key();
if (key == 0 && !lotr_key_esc()) return dialog_mode;
if (dialog_mode == DIALOG_NONE) {
    if (lotr_key_esc()) { lotr_reset_keyboard(); dialog_options_show(); }
    switch (key) {
        case ' ': main_menu_show();      break;
        case 'a': dialog_attack_show();  break;
        case 'c': dialog_choose_show();  break;
        case 'l': dialog_leader_show();  break;
        case 'm': dialog_magic_show();   break;
        case 'h': gui_help_show();       break;   /* #if !PIXEL_PRECISE */
        case 'o': dialog_options_show(); break;   /* #ifndef DEMO */
        ...
    }
}
```

So a click resolves to *"push the right char into `lotr_keybuffer`"* and the entire
existing flow runs untouched.

### The missing piece: geometry

`draw_scroll(int x, int y, int width, int height, const char *text[])` at
`liblotr/gui.c:347` knows exactly where it drew and then discards it. Call sites to
audit: gui.c lines 699, 701, 832, 875, 1021, 1126, 1153.

Implement a per-frame clickable-region registry:

```c
typedef struct { int x, y, w, h; int key; } ClickRegion;

void gui_clickmap_clear(void);                      /* call at frame start */
void gui_clickmap_add(int x, int y, int w, int h, int key);
int  gui_clickmap_hit(int fx, int fy);              /* -> key, or 0 */
```

- Clear it at the top of each frame.
- Have `draw_scroll` (and the other dialog renderers) register one rect per
  selectable line, tagged with the char that line corresponds to.
- New `SDL_MOUSEBUTTONDOWN` case in `lotr_poll_events()` calls `gui_clickmap_hit()`
  and, on a hit, pushes the char into `lotr_keybuffer` using the same
  bounds-checked append the keyboard path uses (`lotr_sdl2.c:287-294`).

Respect `lotr_input_disabled` in the mouse path exactly as `SDL_KEYDOWN` does
(`lotr_sdl2.c:283`), or clicks will register during cutscenes.

Estimated ~200 new lines plus small edits at each call site. Low risk: a miss is a
no-op and the keyboard keeps working throughout.

---

## Tier 2 — click-to-move

### The transform

World→screen is at `liblotr/map.c:844`:

```c
map_draw_largetile(map_desc[i][j],
                   i * largetilesize - (x - (map_width  + 1) / 2),
                   j * largetilesize - (y - (map_height + 1) / 2));
```

Invert it to get a target tile from a framebuffer click. Note the engine keeps
character positions at 4x tile resolution — movement code consistently does
`leader->x / 4` when testing the map (`game.c:738-752`).

### Two implementations

**Stage A (~50 lines).** Store a target tile; each frame, drive the existing
directional flags toward it until arrival. Ships immediately, proves the transform,
snags on concave obstacles.

**Stage B.** A* over `map_can_move_to(leader, tx, ty)`, which already exists and is
the collision oracle the keyboard path uses. Then call `character_move_left/right/up/down()`
(`game.c:738-752`) directly rather than faking key state — cleaner, and avoids
fighting the flag lifecycle in `lotr_reset_keyboard()`.

Build A, confirm coordinates against real game data, then swap B in behind the same
interface.

### Don't break spot triggers

`game.c` runs `map_get_spot()` / `spot_action(..., SPOT_ACTION_MOVE, ...)` off
`leader->action` after each move. Any movement path that bypasses
`character_move_*()` will silently stop firing map events — which will look like
"scripted scenes stopped working" rather than an input bug.

---

## Tier 3 — cursor (optional, defer)

There is no `SDL_ShowCursor` call anywhere in the tree, so you get the default OS
cursor. That is fine and costs nothing. If you later want an in-world cursor, draw it
as an SDL2 overlay after `SDL_RenderCopy` (`lotr_sdl2.c`) — do not poke the
palettized framebuffer.

---

## Suggested commit sequence

1. Debug-only `SDL_MOUSEBUTTONDOWN` handler printing framebuffer coords. Verify
   `SCREEN_FACT` division. **Do not skip this.**
2. `gui_clickmap_*` registry, unwired.
3. Wire `draw_scroll` call sites to register regions.
4. Wire mouse hits into `lotr_keybuffer`. Tier 1 done.
5. Click-to-move Stage A.
6. Click-to-move Stage B (A*).
7. Separate cleanup commit: delete `liblotr/lotr_sdl.c`.

## Out of scope, noted for later

- `DATA_DIRECTORY` is hardcoded to a Unix path (`/usr/local/share/games/lotr`, via
  `liblotr/utils.c:140,175`). Needs to be relative or configurable before a Windows build.
- `MSVC8/` targets Visual Studio 2005. Windows route is MSYS2/mingw (keeps autotools)
  or vcpkg + fresh CMake.
- No `SDL_GameController` usage anywhere. Gamepad support would layer onto the same
  8-function input contract.
- No `SDL_HINT_RENDER_SCALE_QUALITY` set; fullscreen is compile-time only
  (`#ifdef FULLSCREEN`, `lotr_sdl2.c:123`). Runtime toggle is a cheap separate win.
