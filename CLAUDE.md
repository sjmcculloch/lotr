# LOTR engine — project notes for Claude

Open-source recreation (Michal Beneš, LGPL 2.1+) of Interplay's
*J.R.R. Tolkien's The Lord of the Rings, Vol. I* (1990). This fork
(`sjmcculloch/lotr`) modernises it so Scott can play the **ReelMagic CD
edition** on Windows. Work happens on the `play` branch, which sits on top
of upstream's `combat` branch (`upstream` = jflemer/lotr).

## Build

Two builds; both must stay at **zero errors and zero warnings**.

- Native Linux (fast compile check + test harnesses):
  `autoreconf -fi && ./configure && make -j8`
  Needs: build-essential autoconf automake libtool pkg-config libsdl2-dev
  libsdl2-mixer-dev libxml2-dev
- Windows (what Scott plays): `sh scripts/build-windows.sh`
  Cross-compiles with mingw-w64 and assembles `build-win/pkg/LOTR/`
  (lotr.exe, SDL2.dll, SDL2_mixer.dll, README.txt, tools/). It builds a
  `git clone` of the repo, so **commit before building** or the exe won't
  have your changes (it warns). Extra needs:
  gcc-mingw-w64-x86-64 cmake curl git.

Scott runs Claude Code in WSL (Ubuntu). His install is `C:\games\lotr`
(`/mnt/c/games/lotr` from WSL), with game data in `cd\` and ripped music
in `cd\music\`. To deploy, copy `build-win/pkg/LOTR/lotr.exe` there (the DLLs
rarely change). He tests by playing; ask him to try a change, then read
`C:\games\lotr\lotr.log`, where all stderr goes on Windows.

There is no game data in the repo, and there must never be any. Test without
it: write small harnesses that link `liblotr/.libs/liblotr.a` and call engine
functions directly; use `SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software`,
and `SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE=out.raw` to capture and analyse
audio. Generate test media with ffmpeg. `LOTR_DATA=<dir>` points the engine
at a data folder.

## Build switches (lotr.h)

- `CD_VERSION` is **on** (data in `<exe dir>/cd`). Lots of code differs under
  `#ifdef CD_VERSION`; check both sides when editing.
- `SCREEN_FACT` is always 1 (libhqx build option removed). Framebuffer is
  320x200, palettised. `PIXEL_PRECISE` is 0.
- `TTT` (The Two Towers) and `EXTENDED` (Python scripting) are off.

## Architecture facts worth knowing

- **Input:** every SDL event is handled in `lotr_poll_events()` in
  `liblotr/lotr_sdl2.c`. The rest of the engine only sees the contract in
  `lotr_sdl.h`: `lotr_get_key()`, `lotr_kb_hit()`, `lotr_key_left/right/up/
  down/esc/shift/ctrl()`, plus `lotr_get_mouse()` / `lotr_mouse_held()`
  (clicks in 320x200 screen pixels: the renderer's logical size converts
  them; `SCREEN_FACT` is 1). The backend never calls into the GUI.
- **Mouse:** `gui_frame()` turns clicks into keys via a click map:
  `draw_scroll()` registers its area and hotspots found in its text
  (`gui_hotspot_key()`: "L.", "1.*", "eXit", "Question", "(Y/N)",
  "< >", "UP DOWN", "PgUp PgDn", "Tab."), `main_menu_draw()` its ten
  buttons and portrait, settings/save lists their rows. `gui_clear()`
  empties it. Clicks on the map go to `game_walk_to()`: a BFS over
  `map_can_move_to()` cells, walked by feeding `game_leader_movement()`
  the same direction wishes as the arrow keys (so spots fire). Keys, a
  dialog, a map change or being moved stop it.
- `liblotr/lotr_sdl.c` is the **dead SDL1 backend**. Don't edit or copy from it.
- **Keys / dialogs:** `gui_frame()` in `gui.c` dispatches on literal chars
  (space, a c g h l m o q s t u v). Options menu: s save, l load, m music,
  p pause, c configure (settings screen, `dialog_settings_*` in gui.c),
  q quit. Save/load list slots 1-9 with leader, region, date;
  load also offers Q (`savegame.quick`, F5/F9) and A (`savegame.auto`,
  written when a new map is entered, once `game_can_save()`). Saves are
  plain XML, written to `.tmp` then swapped in; slots 1-9 keep a `.bak`.
  `game_open_save()` validates a file before `game_load()` touches state.
- **Data paths:** `lotr_data_directory()` in `utils.c` resolves `$LOTR_DATA`,
  then `<exe dir>/cd`, then the compiled default. Saves: `<exe dir>/saves`
  on Windows, `~/.lotr` elsewhere. Reads go through `lotr_fopen()` /
  `lotr_file_exists()`.
- **Paragraphs:** story text comes from `para.dat`; see `gui_paragraph()`,
  `gui_paragraph_question()` in `gui.c`.
- **Cutscenes:** `av_play()` in `av.c` plays `<name>.av`, else falls back to
  `<name>.mpg` via `mpg.c`, else skips (never exits). ReelMagic MPEGs are
  "magical": scrambled P/B f_codes and frame-rate code; `mpg.c` recovers the
  f_code (TSN 3/8 method from DOSBox Staging). `pl_mpeg.h` is vendored and
  patched in four places marked `LOTR patch`. Keep them if you update it.
- **Screen size:** `screen_width` (graphics.c) is 320, or wider with
  widescreen: `lotr_screen_width_wanted()` from the desktop's shape,
  multiple of 8, max 864 (32:9 with 4:3 pixels; Scott's monitor is
  5120x1440); `graphics_apply_screen_mode()` reallocates.
  The engine's large-screen path draws the map full height with the menu
  bar (centred) over it. Cartoons and .av frames are centred. With the
  4:3 pixel shape the backend's logical size is w x 240 (no integer
  scale; sharp mode prescales by whole numbers, then linear) and mouse y
  is mapped back to 200 rows. Settings: `aspect` (4:3|square),
  `widescreen` (on|off). Use `screen_width`, not `SCREEN_WIDTH`, for
  layout.
- **Display filters:** `lotr_show_screen()` in `lotr_sdl2.c` presents the
  frame as sharp (default), smooth (hq4x, then linear) or CRT (3x nearest,
  linear, scanline overlay sized to the integer window scale). F10 cycles,
  handled in `lotr_poll_events()`. hq4x is vendored (`hq4x.c`, `hqx.h`,
  `hqx_common.h`, from grom358/hqx) with three `LOTR patch` changes; the
  harness checked it byte-identical to the original library.
- **Journal:** `journal.c` records paragraphs (by number, text read from
  para.dat when shown; `gui_paragraph_plain()` copes with
  `gui_paragraph_scroll()` zeroing line ends), conversations (speaker,
  matched keyword, text; not the 0x100 "don't know" or an NPC's default
  reply) and script `TEXT` messages minus `journal_is_noise()`; repeats
  are skipped. Hooks: `spot.c` TEXT/TEXT_PAR/QUESTION_PAR,
  `dialog_talk_show()`, `dialog_question_key()`. Saved as `<journal>` in
  the save game (`<message>`, never `<text>`: libxml names whitespace
  nodes "text"). Viewer: `dialog_journal_*` in gui.c, key J.
  NPC talk tables are filled by map scripts as you play, so a harness
  needs a loaded save or must set `character->texts` itself.
- **Map overview:** `map_display()` marks every large tile on screen as
  explored (`map_explored`, 64x64 bits per map, saved as `<explored>`;
  older saves start empty). `map_render_overview()` shrinks the current
  map to w x h map colours from cached per-tile averages of the unfaded
  `map_palette`; `dialog_map_*` in gui.c shows it on Tab with a blinking
  marker in interface colours (0x40+), which night does not fade.
  Positions: character x/y are half map pixels; tiles are 32 px, large
  tiles 128 px.
- **Settings:** `lotr_setting_get/set()` in `utils.c` read and rewrite
  `settings.txt` ("key = value", other lines kept): next to the exe on
  Windows, `~/.lotr` elsewhere. Keys: `display`, `fullscreen` (F11 too),
  `music` (on/off, also Options M), `music_volume` (overrides music.txt),
  `effects_volume`. Each is read where it is applied: `lotr_system_init()`
  (display, fullscreen), `sound_init()` (effects), cdmusic `init()` (music).
  The menu font has no `#`; check glyphs before using new symbols.
- **Music:** the CD editions used CD audio. `cdmusic.c` plays ripped
  `cd/music/trackNN.(ogg|flac|mp3|wav)` with settings in `music.txt`
  (playlist, volume, `cue N track`, `combat`, `victory`). Defaults come
  from the CD edition's GAME\LORD.EXE (EXEPACK'd; its `play_music(i)`
  plays track i+2 for i 0-10, i >= 11 are effects in `sounds_cd.h` order;
  tracks 2-5 loop; combat start plays index 4, victory index 7): cue i ->
  track i+2, combat 6, victory 9, playlist 2-5. `sound_play()`: 0-10 cue,
  11+ effect. Position is tracked with our own clock because SDL_mixer's
  position ignores seeks.
- `scripts/extract-data-track.ps1` / `extract-music.ps1` ship in `tools/`
  and were tested with pwsh against synthetic BIN/CUE images.

## Conventions

- Keep the original look as the default; new features should be toggleable.
- Small, focused commits with descriptive bodies; push `play` to `origin`.
- Update `scripts/README-windows.txt` (player-facing, CRLF line endings) when
  controls or setup change.
- See `ROADMAP.md` for what's planned. `docs/MOUSE_INPUT_PLAN.md` is the
original mouse plan (now implemented; see Mouse above).
