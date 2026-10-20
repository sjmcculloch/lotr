/****************************************************************************

    sound.c
    sound support


    Lord of the Rings game engine

    Copyright (C) 2003  Michal Benes

    Lord of the Rings game engine is free software;
    you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This code is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this code; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*****************************************************************************/

#include "lotr.h"
#include "archive.h"
#include "lotr_sdl.h"
#include "midi.h"
#include "sound.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>


#ifdef CD_VERSION
#include "sounds_cd.h"
#include "cdmusic.h"
#else
#define SOUNDS_NUM 25
#endif

Uint8 *music_data;
int music_size;

Uint8 *sound_data[SOUNDS_NUM];
int sound_sizes[SOUNDS_NUM];

#define MUSIC_MAIN_INDEX 8

/* settings menu state, stored in settings.txt */
static int effects_volume = 100;
#ifndef CD_VERSION
static int midi_music_on = 1;
static int midi_music_volume = 100;
#endif


/*
  sound initializations
*/

void
sound_init(void)
{
    int i;

#ifndef TTT

#ifndef CD_VERSION

    Archive *archive;

    archive = archive_ndx_open("snd");

    for (i = 0; i < SOUNDS_NUM; ++i) {
        sound_data[i] = archive_read(archive, i);
        sound_sizes[i] = archive_data_size(archive, i);

        /* Fix tempo in midi files */
        if (sound_sizes[i] > 13
            && sound_data[i][0] == 'M'
            && sound_data[i][1] == 'T'
            && sound_data[i][2] == 'h'
            && sound_data[i][3] == 'd'
            && sound_data[i][12] == 0x00 && sound_data[i][13] == 0x60) {
            int tempo = 240;
            sound_data[i][12] = tempo / 0x100;
            sound_data[i][13] = tempo & 0xff;
        }
    }

    music_data = sound_data[MUSIC_MAIN_INDEX];
    music_size = sound_sizes[MUSIC_MAIN_INDEX];

    archive_close(archive);

#else /* ifdef CD_VERSION */

    FILE *adlfile;

    for (i = 0; i < SOUNDS_NUM; ++i) {
        adlfile = lotr_fopen(sound_names[i], "rb");
        sound_sizes[i] = lotr_filelen(adlfile);
        sound_data[i] = lotr_malloc(sound_sizes[i]);
        if (fread(sound_data[i], 1, sound_sizes[i], adlfile) !=
            sound_sizes[i]) {
            fprintf(stderr, "lotr: can not read file %s\n", sound_names[i]);
            perror("lotr");
            exit(1);
        }
        fclose(adlfile);
    }

    music_data = NULL;
    music_size = 0;

#endif

#else /* ifdef TTT */

    for (i = 0; i < SOUNDS_NUM; ++i) {
        sound_data[i] = NULL;
        sound_sizes[i] = 0;
    }

    music_data = NULL;
    music_size = 0;

#endif

    {
        char v[16];
        if (lotr_setting_get("effects_volume", v, sizeof(v)))
            effects_volume = atoi(v);
        effects_set_volume(effects_volume);
#ifndef CD_VERSION
        if (lotr_setting_get("music", v, sizeof(v)))
            midi_music_on = SDL_strcasecmp(v, "off") != 0;
        if (lotr_setting_get("music_volume", v, sizeof(v)))
            midi_music_volume = atoi(v);
        lotr_set_music_volume(midi_music_volume);
#endif
    }
}


/*
  sound deinitializations
*/

void
sound_close(void)
{

    int i;

    stop_midi();
#ifdef CD_VERSION
    cdmusic_stop();
#endif
    for (i = 0; i < SOUNDS_NUM; ++i)
        if (sound_data[i])
            free(sound_data[i]);
}


/*
  sound play
*/

void
sound_play(int index)
{

#ifndef CD_VERSION
    if (index > 8)
        play_sample(sound_data[index - 8], sound_sizes[index - 8]);
    else
        play_midi(sound_data[index + 8], sound_sizes[index + 8], 0);
#else
    /* as in the CD edition's LORD.EXE: 0-10 are CD music, 11 and up the
       effects MAGICS, MAGICF, HIT1-4, MISS1-3, ... */
    if (index >= 11 && index - 11 < SOUNDS_NUM)
        play_sample(sound_data[index - 11], sound_sizes[index - 11]);
    else if (index >= 0 && index < 11)
        cdmusic_cue(index);     /* the original played a CD track here */
#endif

}



/*
  play the main music track
*/

void
play_music(void)
{
#ifdef CD_VERSION
    cdmusic_start();
#else
    if (midi_music_on)
        play_midi(music_data, music_size, TRUE);
#endif
}


/*
  starts stop playing the music track
*/

void
toggle_music(void)
{
    music_set_on(!music_is_on());
}



/*
  settings menu: music on/off and volumes, all remembered in settings.txt
*/

int
music_available(void)
{
#ifdef CD_VERSION
    return cdmusic_available();
#else
    return 1;
#endif
}


int
music_is_on(void)
{
#ifdef CD_VERSION
    return cdmusic_enabled();
#else
    return midi_music_on;
#endif
}


void
music_set_on(int on)
{
#ifdef CD_VERSION
    cdmusic_set_enabled(on);
#else
    midi_music_on = on;
    stop_midi();
    if (on)
        play_music();
#endif
    lotr_setting_set("music", on ? "on" : "off");
}


int
music_get_volume(void)
{
#ifdef CD_VERSION
    return cdmusic_volume();
#else
    return midi_music_volume;
#endif
}


void
music_set_volume(int percent)
{
    char v[16];

    percent = percent < 0 ? 0 : percent > 100 ? 100 : percent;
#ifdef CD_VERSION
    cdmusic_set_volume(percent);
#else
    midi_music_volume = percent;
    lotr_set_music_volume(percent);
#endif
    snprintf(v, sizeof(v), "%d", percent);
    lotr_setting_set("music_volume", v);
}


int
effects_get_volume(void)
{
    return effects_volume;
}


/* applies the volume; effects_store_volume() also remembers it */
void
effects_set_volume(int percent)
{
    effects_volume = percent < 0 ? 0 : percent > 100 ? 100 : percent;
    lotr_set_effects_volume(effects_volume);
}


void
effects_store_volume(int percent)
{
    char v[16];

    effects_set_volume(percent);
    snprintf(v, sizeof(v), "%d", effects_volume);
    lotr_setting_set("effects_volume", v);
}


/* a short sound effect, so the effects volume can be heard */
void
effects_preview(void)
{
    if (sound_sizes[0] > 0)
        play_sample(sound_data[0], sound_sizes[0]);
}


/*
  play music if spell was performed
*/

void
music_spell_performed(void)
{
#ifndef CD_VERSION
    play_midi(sound_data[15], sound_sizes[15], 0);
#else
    play_sample(sound_data[0], sound_sizes[0]);
#endif
}


/*
  play music if spell failed
*/

void
music_spell_failed(void)
{
#ifndef CD_VERSION
    play_midi(sound_data[16], sound_sizes[16], 0);
#else
    play_sample(sound_data[1], sound_sizes[1]);
#endif
}



/*
  play music if combat started
*/

void
music_combat_started(void)
{
#ifndef CD_VERSION
    play_midi(sound_data[21], sound_sizes[21], 0);
#else
    cdmusic_combat(0);
#endif
}


/*
  play music if combat was won
 */

void
music_combat_won(void)
{
#ifndef CD_VERSION
    play_midi(sound_data[22], sound_sizes[22], 0);
#else
    cdmusic_combat(1);
#endif
}
