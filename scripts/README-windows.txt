J.R.R. Tolkien's The Lord of the Rings, Vol. I - CD edition
Modern Windows build of the open-source engine recreation
=============================================================

SETUP
-----
1. Copy the game files from your CD edition (arts.dat and all the
   other data and video files) into the "cd" folder next to lotr.exe.
2. Double-click lotr.exe.

If the files are in the wrong place, a message box says exactly
which folder it looked in. You can also point it somewhere else by
setting a LOTR_DATA environment variable to the folder holding arts.dat.

Saved games go in the "saves" folder next to lotr.exe, created
automatically.

CONTROLS
--------
  Arrow keys      move
  Space           main menu
  A  attack       C  choose        G  get
  L  leader       M  magic         S  skill
  T  talk         U  use           V  view
  O  options      H  help          Q  quit
  Esc             cancel / options
  Alt+Enter, F11  toggle fullscreen

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

The engine is licensed under the GNU LGPL (see COPYING.txt).
SDL2 and SDL2_mixer are zlib-licensed (see the LICENSE-*.txt files).
Game data is not included.
