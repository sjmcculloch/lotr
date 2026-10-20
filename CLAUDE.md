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
  down/esc/shift/ctrl()`. New input (mouse) should synthesise into these.
- `liblotr/lotr_sdl.c` is the **dead SDL1 backend**. Don't edit or copy from it.
- **Keys / dialogs:** `gui_frame()` in `gui.c` dispatches on literal chars
  (space, a c g h l m o q s t u v). Options menu: s save, l load, m music,
  p pause, q quit. Save/load list slots 1-9 with leader, region, date;
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
- **Display filters:** `lotr_show_screen()` in `lotr_sdl2.c` presents the
  frame as sharp (default), smooth (hq4x, then linear) or CRT (3x nearest,
  linear, scanline overlay sized to the integer window scale). F10 cycles,
  handled in `lotr_poll_events()`. hq4x is vendored (`hq4x.c`, `hqx.h`,
  `hqx_common.h`, from grom358/hqx) with three `LOTR patch` changes; the
  harness checked it byte-identical to the original library.
- **Settings:** `lotr_setting_get/set()` in `utils.c` read and rewrite
  `settings.txt` ("key = value", other lines kept): next to the exe on
  Windows, `~/.lotr` elsewhere. Keys so far: `display`.
- **Music:** the CD editions used CD audio. `cdmusic.c` plays ripped
  `cd/music/trackNN.(ogg|flac|mp3|wav)` with settings in `music.txt`
  (playlist, volume, `cue N track`, `combat`, `victory`). Script music cues
  (`sound_play(index<=11)` in `sound.c`) are logged and unassigned until the
  original mapping is known. Position is tracked with our own clock because
  SDL_mixer's position ignores seeks.
- `scripts/extract-data-track.ps1` / `extract-music.ps1` ship in `tools/`
  and were tested with pwsh against synthetic BIN/CUE images.

## Conventions

- Keep the original look as the default; new features should be toggleable.
- Small, focused commits with descriptive bodies; push `play` to `origin`.
- Update `scripts/README-windows.txt` (player-facing, CRLF line endings) when
  controls or setup change.
- See `ROADMAP.md` for what's planned and `docs/MOUSE_INPUT_PLAN.md` for mouse.
