# Roadmap

Goal: make the game feel modern to play while keeping the 1990 original
intact by default. Rough order, highest value first. Gamepad support is
explicitly out of scope.

## Quick wins
1. ~~**Quicksave / autosave**~~ (done): F5 quicksave and F9 quickload into a dedicated
   slot; autosave to its own slot on entering a new map. Never overwrite the
   player's manual slots.
2. ~~**Better save/load screen**~~ (done): show location, party leader, and
   date/time per slot; expose slots 6-9.
3. ~~**Display filters**~~ (done): runtime toggle between sharp pixels, hqx smoothing
   and a CRT-style look (hqx support already exists, behind HAVE_LIBHQX).
4. ~~**In-game settings**~~ (done): music volume, effects volume, display filter and
   fullscreen in a menu, saved to a settings file next to the exe.

## Medium
5. **Journal**: a scrollable log of every paragraph read, conversation and
   quest hint, saved alongside the save game. Opened with a key (J).
6. **Map overlay**: a full-screen map of explored areas (tools/maps2bmp.c
   and tools/ripmaps.c already render maps).
7. **Mouse support**: see docs/MOUSE_INPUT_PLAN.md. Menus and dialogs
   first, then click-to-move.

## Big
8. **Correct CD music cues**: read the original DOS executable from the disc
   to find which CD track it played for each script cue, combat start and
   combat win; ship that mapping as the default music.txt.
9. **Narrated paragraphs**: text-to-speech audio for para.dat paragraphs,
   generated offline and played when a paragraph is shown (optional).
10. **Volume II: The Two Towers**: the engine has partial, non-working
    `TTT` support.
