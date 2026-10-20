/****************************************************************************

    cdmusic.c
    CD audio music for the CD editions


    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.

    The CD editions played their music as audio tracks straight off the
    disc, which this engine never implemented (the CD build had no music at
    all). Here the tracks are played from files ripped off the disc:

        <data folder>/music/trackNN.ogg | .flac | .mp3 | .wav

    Settings come from <data folder>/music/music.txt:

        playlist 2 3 4 5     background tracks, played in order, repeated
        volume 80            0-100
        cue 3 7              script music cue 3 plays track 7 once
        combat 8             fight started
        victory 9            fight won

    Without music.txt, every track at least a minute long goes in the
    playlist. Which track the original played for which cue is not
    documented, so cues do nothing unless assigned; each one is logged.

*****************************************************************************/

#include "lotr.h"
#include "cdmusic.h"
#include "utils.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_LIBSDL_MIXER
#include <SDL_mixer.h>

#define MAX_TRACK 99
#define MAX_CUES 12

static int initialised = 0;
static int available = 0;       /* mixer open and at least one track */
static int user_off = 0;        /* music switched off in the options */

static int playlist[MAX_TRACK + 1];
static int playlist_len = 0;
static int have_playlist_line = 0;  /* music.txt set the playlist itself */
static int volume = 80;
static int cue_track[MAX_CUES];
static int combat_track = 0, victory_track = 0;

static Mix_Music *music = NULL;
static int playing_track = 0;   /* track currently loaded, 0 = none */
static int oneshot = 0;         /* playing a cue rather than the playlist */
static int bg_index = 0;        /* playlist position */
static double bg_pos = 0.0;     /* where to resume the background track */
static int paused = 0;          /* stopped for a movie */
static SDL_atomic_t finished;
static Uint32 started_ticks = 0;    /* when the current track (re)started */
static double started_pos = 0.0;    /* ...and from what position */

static const char *exts[] = { "ogg", "flac", "mp3", "wav", "OGG", "FLAC", "MP3", "WAV" };


static int
track_path(int track, char *path, size_t len)
{
    size_t i;
    FILE *f;

    for (i = 0; i < sizeof(exts) / sizeof(exts[0]); ++i) {
        snprintf(path, len, "%s/music/track%02d.%s", lotr_data_directory(),
                 track, exts[i]);
        if ((f = fopen(path, "rb")) != NULL) {
            fclose(f);
            return 1;
        }
    }
    return 0;
}


static void
on_finished(void)
{
    /* runs on the audio thread: only flag it */
    SDL_AtomicSet(&finished, 1);
}


/* where the current track is now; SDL_mixer's own figure ignores seeks */
static double
position(void)
{
    if (!music)
        return 0.0;
    return started_pos + (SDL_GetTicks() - started_ticks) / 1000.0;
}


static void
halt(void)
{
    if (music) {
        Mix_HaltMusic();
        Mix_FreeMusic(music);
        music = NULL;
    }
    playing_track = 0;
    SDL_AtomicSet(&finished, 0);
}


static int
play_track(int track, double pos)
{
    char path[1200];

    halt();
    if (!track_path(track, path, sizeof(path))) {
        fprintf(stderr, "lotr: music track %02d not found\n", track);
        return 0;
    }
    if ((music = Mix_LoadMUS(path)) == NULL) {
        fprintf(stderr, "lotr: can not load %s: %s\n", path, Mix_GetError());
        return 0;
    }
    Mix_HookMusicFinished(on_finished);
    Mix_VolumeMusic(volume * MIX_MAX_VOLUME / 100);
    if (Mix_PlayMusic(music, 1) < 0) {
        fprintf(stderr, "lotr: can not play %s: %s\n", path, Mix_GetError());
        Mix_FreeMusic(music);
        music = NULL;
        return 0;
    }
    if (pos > 0.5 && Mix_SetMusicPosition(pos) < 0)
        pos = 0.0;              /* format can't seek: it restarted */
    started_ticks = SDL_GetTicks();
    started_pos = pos > 0.5 ? pos : 0.0;
    playing_track = track;
    fprintf(stderr, "lotr: music: track %02d%s\n", track,
            pos > 0.5 ? " (resumed)" : "");
    return 1;
}


static void
read_config(void)
{
    char path[1200], line[512];
    FILE *f;

    snprintf(path, sizeof(path), "%s/music/music.txt", lotr_data_directory());
    if ((f = fopen(path, "r")) == NULL)
        return;

    while (fgets(line, sizeof(line), f)) {
        char *p = strchr(line, '#');
        char word[32];
        int a, b, n;

        if (p)
            *p = 0;
        if (sscanf(line, "%31s%n", word, &n) != 1)
            continue;
        p = line + n;

        if (strcmp(word, "playlist") == 0) {
            int consumed;
            have_playlist_line = 1;
            playlist_len = 0;
            while (playlist_len <= MAX_TRACK && sscanf(p, " %d%n", &a, &consumed) == 1) {
                if (a >= 1 && a <= MAX_TRACK)
                    playlist[playlist_len++] = a;
                p += consumed;
            }
        } else if (strcmp(word, "volume") == 0 && sscanf(p, "%d", &a) == 1) {
            volume = a < 0 ? 0 : a > 100 ? 100 : a;
        } else if (strcmp(word, "cue") == 0 && sscanf(p, "%d %d", &a, &b) == 2) {
            if (a >= 0 && a < MAX_CUES && b >= 1 && b <= MAX_TRACK)
                cue_track[a] = b;
        } else if (strcmp(word, "combat") == 0 && sscanf(p, "%d", &a) == 1) {
            combat_track = a;
        } else if (strcmp(word, "victory") == 0 && sscanf(p, "%d", &a) == 1) {
            victory_track = a;
        }
    }
    fclose(f);
}


static void
init(void)
{
    int freq, channels, t, found = 0;
    Uint16 format;
    char path[1200];

    if (initialised)
        return;
    initialised = 1;

    if (!Mix_QuerySpec(&freq, &format, &channels))
        return;                 /* sound is off */

    read_config();

    if (!have_playlist_line) {
        /* no playlist given: every track of at least a minute */
        for (t = 1; t <= MAX_TRACK; ++t) {
            Mix_Music *m;
            double len;
            if (!track_path(t, path, sizeof(path)))
                continue;
            found++;
            if ((m = Mix_LoadMUS(path)) == NULL)
                continue;
            len = Mix_MusicDuration(m);
            Mix_FreeMusic(m);
            if (len < 0 || len >= 60.0)
                playlist[playlist_len++] = t;
        }
    } else {
        for (t = 1; t <= MAX_TRACK; ++t)
            if (track_path(t, path, sizeof(path)))
                found++;
    }

    available = found > 0;
    if (available) {
        int i;
        fprintf(stderr, "lotr: CD music: %d track file(s), playlist:", found);
        for (i = 0; i < playlist_len; ++i)
            fprintf(stderr, " %d", playlist[i]);
        fprintf(stderr, ", volume %d\n", volume);
    } else {
        fprintf(stderr, "lotr: CD music: no tracks in %s/music\n",
                lotr_data_directory());
    }
}


static void
play_background(void)
{
    if (playlist_len <= 0)
        return;
    if (bg_index >= playlist_len)
        bg_index = 0;
    oneshot = 0;
    if (!play_track(playlist[bg_index], bg_pos)) {
        /* skip a broken entry rather than going silent */
        bg_pos = 0.0;
        bg_index++;
    }
}


void
cdmusic_start(void)
{
    init();
    if (!available || user_off || paused)
        return;
    if (!music)
        play_background();
}


int
cdmusic_stop(void)
{
    int was = music != NULL;

    init();
    halt();
    oneshot = 0;
    return was;
}


int
cdmusic_toggle(void)
{
    init();
    if (!available)
        return 0;
    if (music || !user_off) {
        user_off = 1;
        if (!oneshot && music)
            bg_pos = position();
        cdmusic_stop();
        return 0;
    }
    user_off = 0;
    cdmusic_start();
    return 1;
}


void
cdmusic_pause(void)
{
    init();
    if (paused)
        return;
    paused = 1;
    if (music && !oneshot) {
        double pos = position();
        bg_pos = pos > 0 ? pos : 0.0;
    }
    halt();
    oneshot = 0;
}


void
cdmusic_resume(void)
{
    if (!paused)
        return;
    paused = 0;
    cdmusic_start();
}


static void
play_cue_track(const char *what, int index, int track)
{
    init();
    if (track <= 0) {
        if (index >= 0)
            fprintf(stderr, "lotr: music %s %d fired (unassigned)\n", what, index);
        else
            fprintf(stderr, "lotr: music %s fired (unassigned)\n", what);
        return;
    }
    if (index >= 0)
        fprintf(stderr, "lotr: music %s %d -> track %d\n", what, index, track);
    else
        fprintf(stderr, "lotr: music %s -> track %d\n", what, track);

    if (!available || user_off || paused)
        return;
    if (music && !oneshot) {
        double pos = position();
        bg_pos = pos > 0 ? pos : 0.0;
    }
    if (play_track(track, 0.0))
        oneshot = 1;
}


void
cdmusic_cue(int index)
{
    play_cue_track("cue", index,
                   (index >= 0 && index < MAX_CUES) ? cue_track[index] : 0);
}


void
cdmusic_combat(int won)
{
    play_cue_track(won ? "victory" : "combat", -1,
                   won ? victory_track : combat_track);
}


void
cdmusic_update(void)
{
    if (!SDL_AtomicGet(&finished))
        return;
    SDL_AtomicSet(&finished, 0);

    if (!available || user_off || paused)
        return;

    if (music) {
        Mix_FreeMusic(music);
        music = NULL;
        playing_track = 0;
    }

    if (oneshot) {
        oneshot = 0;            /* back to the background, where it was */
    } else {
        bg_index++;             /* next background track */
        bg_pos = 0.0;
    }
    play_background();
}


#else /* no SDL_mixer */

void cdmusic_start(void) { }
int cdmusic_stop(void) { return 0; }
int cdmusic_toggle(void) { return 0; }
void cdmusic_pause(void) { }
void cdmusic_resume(void) { }
void cdmusic_cue(int index) { (void) index; }
void cdmusic_combat(int won) { (void) won; }
void cdmusic_update(void) { }

#endif
