J.R.R. Tolkien's The Lord of the Rings, Vol. I - CD edition
Modern Windows build of the open-source engine recreation
=============================================================

SETUP
-----
1. Copy the game files from your CD edition into the "cd" folder next
   to lotr.exe, all in one flat folder with no subfolders:
     - ReelMagic edition: everything from the disc's GAME folder AND
       everything from its MOVIES folder (the .MPG cutscenes)
     - Interplay CD edition: the data files and the .AV cutscenes
   arts.dat must sit directly inside "cd".
2. Double-click lotr.exe.

MUSIC (CD editions)
-------------------
The CD editions' music is CD audio, which has to be ripped from the
disc image. Put tools\extract-music.ps1 next to your .bin and .cue
(e.g. in the folder that also holds "cd") and run, in that folder:
    powershell -ExecutionPolicy Bypass -File .\extract-music.ps1
It writes track02.wav ... into cd\music plus a music.txt you can edit:
the background playlist, the volume, and which tracks to play for the
game's music cues. Each cue the game fires is noted in lotr.log.
The Options screen's music setting switches it on and off.

If you have a .bin/.cue disc image, tools\extract-data-track.ps1 turns
its data track into an .iso that Windows and 7-Zip can open.

If the files are in the wrong place, a message box says exactly
which folder it looked in. You can also point it somewhere else by
setting a LOTR_DATA environment variable to the folder holding arts.dat.

Saved games go in the "saves" folder next to lotr.exe, created
automatically.

SAVING
------
  F5   quicksave            F9   quickload (no confirmation)
  The game also autosaves each time you enter a new map.
  Options (O), then S save or L load, lists every slot with its party
  leader, region and date: slots 1-9 for your own saves; when loading,
  Q is the quicksave and A the autosave. Quick and auto saves have their
  own files (savegame.quick, savegame.auto) and never touch slots 1-9.
  Saving over slot N first keeps the old one as savegame.N.bak; rename it
  back to savegame.N to undo. You cannot save during combat or while a
  scene is playing out; the game tells you so.

CONTROLS
--------
  Arrow keys      move
  Space           main menu
  A  attack       C  choose        G  get
  L  leader       M  magic         S  skill
  T  talk         U  use           V  view
  O  options      H  help          Q  quit
  Esc             cancel / options
  F5 / F9         quicksave / quickload
  Alt+Enter, F11  toggle fullscreen
  Any key         skip a cutscene (ReelMagic movies)

ABOUT THIS BUILD
----------------
Engine: Michal Benes' recreation, from github.com/jflemer/lotr,
"combat" branch (combat and memory fixes, escape from battle,
extra save slots), built in CD mode.

Changes in this build:
  - finds game data next to the exe instead of a fixed Unix path
  - saves next to the exe
  - sharp, whole-number pixel scaling; window sized to your screen
  - fullscreen toggle
  - missing-data error shown as a dialog
  - F5/F9 quicksave and quickload, autosave on entering a new map, and a
    save/load screen showing leader, region and date for slots 1-9;
    saves are written safely (a crash mid-save cannot damage a slot)
  - diagnostics written to lotr.log next to lotr.exe
  - plays the CD soundtrack (ripped from the disc) as background music,
    pausing for films and resuming where it left off
  - plays the ReelMagic edition's MPEG-1 film cutscenes in full colour
    with full-quality sound (decoder: pl_mpeg, MIT licence); a missing
    cutscene is skipped instead of quitting the game

The engine is licensed under the GNU LGPL (see COPYING.txt).
SDL2 and SDL2_mixer are zlib-licensed (see the LICENSE-*.txt files).
Game data is not included.
