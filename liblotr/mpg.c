/****************************************************************************

    mpg.c
    MPEG-1 cutscene player for the ReelMagic CD edition


    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.

    The ReelMagic edition ships its cutscenes as standard MPEG-1 files
    (MOVIES\SACF.MPG ...) instead of the engine's own .av format. They are
    decoded with pl_mpeg (MIT, Dominic Szablewski) and drawn straight to the
    window in full colour, as the ReelMagic card overlaid them. Audio goes to
    its own SDL audio device at the film's native rate, because the game's
    mixer runs at 22 kHz mono 8-bit for its sound effects. If a second
    device can't be opened, it is converted and fed through the game mixer
    instead (set LOTR_MOVIE_AUDIO=mixer to force that).

*****************************************************************************/

#include "lotr.h"
#include "lotr_sdl.h"
#include "midi.h"
#include "mpg.h"
#include "utils.h"

#include <SDL.h>
#ifdef HAVE_LIBSDL_MIXER
#include <SDL_mixer.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"


typedef struct {
    SDL_Texture *texture;
    SDL_AudioDeviceID audio;    /* dedicated device, full quality */
    SDL_AudioStream *stream;    /* fallback: converted into the game mixer */
    SDL_mutex *lock;
    int have_frame;
} MpgState;


#ifdef HAVE_LIBSDL_MIXER
/* SDL_mixer calls this on its audio thread in place of music */
static void
mpg_mixer_hook(void *user, Uint8 *out, int len)
{
    MpgState *st = (MpgState *) user;
    int got = 0;

    SDL_LockMutex(st->lock);
    if (st->stream)
        got = SDL_AudioStreamGet(st->stream, out, len);
    SDL_UnlockMutex(st->lock);

    if (got < 0)
        got = 0;
    if (got < len)
        memset(out + got, 0, len - got);   /* silent (signed) or near enough */
}


/* route movie audio through the already-open game mixer */
static int
mpg_open_mixer_audio(MpgState *st, int rate)
{
    int freq, channels;
    Uint16 format;

    if (!Mix_QuerySpec(&freq, &format, &channels))
        return 0;

    st->stream = SDL_NewAudioStream(AUDIO_F32SYS, 2, rate,
                                    format, (Uint8) channels, freq);
    if (st->stream == NULL)
        return 0;

    st->lock = SDL_CreateMutex();
    Mix_HookMusic(mpg_mixer_hook, st);
    return 1;
}
#endif


static size_t
mpg_audio_pending(MpgState *st)
{
    size_t n = 0;

    if (st->audio)
        n = SDL_GetQueuedAudioSize(st->audio);
    else if (st->stream) {
        SDL_LockMutex(st->lock);
        n = (size_t) SDL_AudioStreamAvailable(st->stream);
        SDL_UnlockMutex(st->lock);
    }
    return n;
}


static void
mpg_on_video(plm_t *plm, plm_frame_t *frame, void *user)
{
    MpgState *st = (MpgState *) user;
    (void) plm;

    SDL_UpdateYUVTexture(st->texture, NULL,
                         frame->y.data, frame->y.width,
                         frame->cb.data, frame->cb.width,
                         frame->cr.data, frame->cr.width);
    st->have_frame = 1;
}


static void
mpg_on_audio(plm_t *plm, plm_samples_t *samples, void *user)
{
    MpgState *st = (MpgState *) user;
    (void) plm;

    if (st->audio)
        SDL_QueueAudio(st->audio, samples->interleaved,
                       sizeof(float) * samples->count * 2);
    else if (st->stream) {
        SDL_LockMutex(st->lock);
        SDL_AudioStreamPut(st->stream, samples->interleaved,
                           sizeof(float) * samples->count * 2);
        SDL_UnlockMutex(st->lock);
    }
}


/* full path of <name>.mpg in the data folder, trying lower then upper case */
static int
mpg_find(const char *name, char *path, size_t pathlen)
{
    char upper[64];
    size_t i;
    FILE *f;

    snprintf(path, pathlen, "%s/%s.mpg", lotr_data_directory(), name);
    if ((f = fopen(path, "rb")) != NULL) {
        fclose(f);
        return 1;
    }

    for (i = 0; name[i] && i < sizeof(upper) - 1; ++i)
        upper[i] = (char) toupper((unsigned char) name[i]);
    upper[i] = 0;

    snprintf(path, pathlen, "%s/%s.MPG", lotr_data_directory(), upper);
    if ((f = fopen(path, "rb")) != NULL) {
        fclose(f);
        return 1;
    }

    return 0;
}


int
mpg_exists(const char *name)
{
    char path[1200];
    return mpg_find(name, path, sizeof(path));
}


/* destination rectangle in the renderer's logical space, keeping 4:3 */
static void
mpg_dest_rect(SDL_Renderer *r, SDL_Rect *dst)
{
    int lw, lh;

    SDL_RenderGetLogicalSize(r, &lw, &lh);
    if (lw <= 0 || lh <= 0)
        SDL_GetRendererOutputSize(r, &lw, &lh);

    /* MPEG-1 SIF frames (352x240 / 352x288) were shown on a 4:3 screen */
    if (lw * 3 > lh * 4) {
        dst->h = lh;
        dst->w = lh * 4 / 3;
    } else {
        dst->w = lw;
        dst->h = lw * 3 / 4;
    }
    dst->x = (lw - dst->w) / 2;
    dst->y = (lh - dst->h) / 2;
}


int
mpg_play(const char *name)
{
    char path[1200];
    SDL_Renderer *renderer;
    plm_t *plm;
    MpgState st;
    SDL_Rect dst;
    Uint64 last, now, freq;
    int skipped = 0;

    if (!mpg_find(name, path, sizeof(path)))
        return 0;

    renderer = lotr_sdl_renderer();
    if (renderer == NULL)
        return 0;

    plm = plm_create_with_filename(path);
    if (plm == NULL) {
        fprintf(stderr, "lotr: can not open movie %s\n", path);
        return 0;
    }
    if (!plm_probe(plm, 5000 * 1024) || plm_get_num_video_streams(plm) < 1) {
        fprintf(stderr, "lotr: %s is not an MPEG-1 movie this player can read\n",
                path);
        plm_destroy(plm);
        return 0;
    }

    memset(&st, 0, sizeof(st));

    st.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV,
                                   SDL_TEXTUREACCESS_STREAMING,
                                   plm_get_width(plm), plm_get_height(plm));
    if (st.texture == NULL) {
        fprintf(stderr, "lotr: can not create movie texture: %s\n",
                SDL_GetError());
        plm_destroy(plm);
        return 0;
    }
    /* smooth scaling for film footage, unlike the sharp game pixels */
    SDL_SetTextureScaleMode(st.texture, SDL_ScaleModeLinear);

    /* game music would play over the film */
    stop_midi();

    if (plm_get_num_audio_streams(plm) > 0) {
        SDL_AudioSpec want, have;

        if (!SDL_WasInit(SDL_INIT_AUDIO))
            SDL_InitSubSystem(SDL_INIT_AUDIO);

        const char *force = getenv("LOTR_MOVIE_AUDIO");

        SDL_zero(want);
        want.freq = plm_get_samplerate(plm);
        want.format = AUDIO_F32SYS;
        want.channels = 2;
        want.samples = 4096;
        if (!(force && strcmp(force, "mixer") == 0))
            st.audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (st.audio) {
            plm_set_audio_lead_time(plm, (double) have.samples / have.freq);
            SDL_PauseAudioDevice(st.audio, 0);
        }
#ifdef HAVE_LIBSDL_MIXER
        else if (mpg_open_mixer_audio(&st, want.freq)) {
            plm_set_audio_lead_time(plm, 0.25);
        }
#endif
        else {
            fprintf(stderr, "lotr: movie audio unavailable: %s\n",
                    SDL_GetError());
            plm_set_audio_enabled(plm, 0);
        }
    } else {
        plm_set_audio_enabled(plm, 0);
    }

    plm_set_video_decode_callback(plm, mpg_on_video, &st);
    plm_set_audio_decode_callback(plm, mpg_on_audio, &st);
    plm_set_loop(plm, 0);

    lotr_reset_keyboard();

    freq = SDL_GetPerformanceFrequency();
    last = SDL_GetPerformanceCounter();

    while (!plm_has_ended(plm)) {
        double elapsed;

        lotr_poll_events();
        if (lotr_key_esc() || lotr_kb_hit()) {
            skipped = 1;
            break;
        }

        now = SDL_GetPerformanceCounter();
        elapsed = (double) (now - last) / (double) freq;
        last = now;
        /* after a stall (window drag etc.) don't fast-forward */
        if (elapsed > 1.0 / 15.0)
            elapsed = 1.0 / 15.0;

        plm_decode(plm, elapsed);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (st.have_frame) {
            mpg_dest_rect(renderer, &dst);
            SDL_RenderCopy(renderer, st.texture, NULL, &dst);
        }
        SDL_RenderPresent(renderer);

        SDL_Delay(2);
    }

    /* let the last of the sound finish unless the player skipped */
    if (!skipped) {
        while (mpg_audio_pending(&st) > 0) {
            lotr_poll_events();
            if (lotr_key_esc() || lotr_kb_hit())
                break;
            SDL_Delay(10);
        }
    }

    if (st.audio)
        SDL_CloseAudioDevice(st.audio);
#ifdef HAVE_LIBSDL_MIXER
    if (st.stream) {
        Mix_HookMusic(NULL, NULL);      /* also waits for the audio thread */
        SDL_FreeAudioStream(st.stream);
        SDL_DestroyMutex(st.lock);
    }
#endif
    SDL_DestroyTexture(st.texture);
    plm_destroy(plm);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    lotr_reset_keyboard();

    return 1;
}
