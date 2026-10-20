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
It writes track02.wav ... into cd\music plus a music.txt you can edit.
The music follows the original CD edition, as read from its program:
tracks 2-5 are the background, the story's music cues play track
cue + 2, a fight starts with track 6 and a won fight plays track 9.
music.txt can change any of it (playlist, volume, cue, combat, victory);
each cue the game fires is noted in lotr.log. Options > M switches the
music on and off; Options > C sets its volume.

If you have a .bin/.cue disc image, tools\extract-data-track.ps1 turns
its data track into an .iso that Windows and 7-Zip can open.

If the files are in the wrong place, a message box says exactly
which folder it looked in. You can also point it somewhere else by
setting a LOTR_DATA environment variable to the folder holding arts.dat.

Saved games go in the "saves" folder next to lotr.exe, created
automatically.

MOUSE
-----
  Left click on the map    walk there (around obstacles); hold the
                           button to keep following the pointer
  Right click on the map   main menu
  Left click in a menu     the line or button clicked, e.g. a slot,
                           "eXit", "(Y/N)", the < > arrows
  Right click in a menu    back / exit
  Click while reading      go on to the next page
  Wheel                    scroll lists, the journal and settings
  Main menu pictures       top: Attack, View, Get, Use, Skill;
                           bottom: Magic, Talk, Leader, Options, Exit;
                           the portrait chooses a character
The keyboard works as before. A key, or a dialog opening, stops a walk.
The pointer hides after a few seconds without moving.

MAP
---
Tab shows the whole map of the region you are in, shrunk to fit the
screen. Only the parts you have seen are drawn; a blinking red cross
marks your party. Tab, X or Esc closes it. What you have explored is
part of each saved game; games saved before this version start with
nothing explored. At night the map is drawn as dark as the game.

JOURNAL
-------
J opens your journal: the story paragraphs you have read, what people
told you (with the topic you asked about), and the important messages,
grouped by region, newest at the bottom. Arrows, PgUp/PgDn and Home/End
scroll; J or X closes it. Repeats and routine messages ("The door is
locked.") are left out. The journal is part of each saved game; games
saved before this version start with an empty one.

SETTINGS
--------
Options (O), then C (Configure) opens the settings: music on/off, music
volume, effects volume, display filter, fullscreen, pixel shape and
widescreen.
  Pixel shape  "4:3 (original)" shows each pixel 1.2 times taller than
               wide, as the game looked on a 1990 monitor; "Square" is
               the shape this build used before.
  Widescreen   shows more of the map to the left and right so the
               picture fills a wide monitor; the menu bar is centred. Up/Down picks a
line, Left/Right (or Enter) changes it; changes apply at once. They are
kept in settings.txt next to lotr.exe, as are F10, F11 and the Options
menu's M (music) key. The music volume set here overrides the "volume"
line in cd\music\music.txt. Delete settings.txt to go back to defaults.

DISPLAY
-------
F10 cycles the look of the game: sharp pixels (the original look),
smooth (hq4x smoothing) and CRT (soft pixels with scanlines; the
scanlines show when the window is at least 3x the game's 320x200).
The window title names the filter. Your choice is kept in settings.txt
next to lotr.exe. Cutscenes are not filtered.

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
  J  journal      Tab  map of the explored area
  Esc             cancel / options
  F5 / F9         quicksave / quickload
  F10             display: sharp pixels / smooth (hqx) / CRT scanlines
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
  - F10 display filters: sharp, smooth (hq4x by Maxim Stepin and
    Cameron Zemek, LGPL) and CRT, remembered in settings.txt
  - settings screen (Options > C): music, music and effects volume,
    display filter, fullscreen; all remembered in settings.txt
  - journal (J) of paragraphs, conversations and story messages, kept
    in each saved game
  - map of the explored area (Tab), kept in each saved game
  - mouse: menus, dialogs and the main menu; click on the map to walk
  - CD music cues, fight and victory music as in the original CD
    edition; the right sound effects for hits, misses and "cry"
  - original 4:3 pixel shape and widescreen, in the settings
  - diagnostics written to lotr.log next to lotr.exe
  - plays the CD soundtrack (ripped from the disc) as background music,
    pausing for films and resuming where it left off
  - plays the ReelMagic edition's MPEG-1 film cutscenes in full colour
    with full-quality sound (decoder: pl_mpeg, MIT licence); a missing
    cutscene is skipped instead of quitting the game

The engine is licensed under the GNU LGPL (see COPYING.txt).
SDL2 and SDL2_mixer are zlib-licensed (see the LICENSE-*.txt files).
Game data is not included.
