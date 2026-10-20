/****************************************************************************

    lotr_sdl.c
    system dependent part via SDL


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
#include "lotr_sdl.h"
#include "midi.h"
#include "cdmusic.h"
#include "hqx.h"
#include "utils.h"
#include <SDL.h>
#ifdef HAVE_LIBSDL_MIXER
#include <SDL_mixer.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>


/* main window */
static SDL_Window *main_window = NULL;
static SDL_Renderer *main_window_renderer = NULL;

/* palette */
SDL_Color active_palette[256];

/* display filter, see lotr_show_screen() */
static int display_filter = LOTR_DISPLAY_SHARP;
static const char *display_filter_names[LOTR_DISPLAY_FILTERS] =
    { "sharp", "smooth", "crt" };

/* filtered frame: streaming texture and its pixels */
static SDL_Texture *filter_texture = NULL;
static int filter_texture_w, filter_texture_h;
static Uint32 *filter_pixels = NULL;

/* CRT scanline overlay, built for one whole-number window scale */
static SDL_Texture *scanline_texture = NULL;
static int scanline_scale = 0;

/* CRT: the frame is enlarged this much with sharp pixels, then smoothly to
   the window, which softens pixel edges by about a third of a pixel */
#define CRT_PRESCALE 3

static void lotr_apply_fullscreen(int on);

/* CRT: brighten by this many 256ths to make up for the light the
   scanlines take away (about 1/8) */
#define CRT_BOOST 292

/* keyboard buffer */
#define KEYBOARDBUFFERESIZE 16
static int lotr_keybufferpos = 0;
static int lotr_keybuffer[KEYBOARDBUFFERESIZE];


#ifdef HAVE_LIBSDL_MIXER
Mix_Chunk *sound_samples[MIX_CHANNELS];
#endif


/* indicates which keys were pressed */

static int lotr_input_disabled = 0;

static int lotr_key_left_pressed;
static int lotr_key_right_pressed;
static int lotr_key_up_pressed;
static int lotr_key_down_pressed;

static int lotr_key_esc_pressed;
static int lotr_key_shift_pressed;
static int lotr_key_ctrl_pressed;


/*
  audio
*/

#ifdef HAVE_LIBSDL_MIXER
void
hook_channel_finished(int channel)
{
    if (channel < 0 || channel > MIX_CHANNELS || !sound_samples[channel]) {
        fprintf(stderr,
                "Non-existend or non-playing sound channel finished?\n");
        return;
    }

    Mix_FreeChunk(sound_samples[channel]);
    sound_samples[channel] = 0;
}
#endif

/*
  initialize SDL
*/
void
lotr_system_init(void)
{
    int i;

    SDL_AudioSpec desired;

#ifdef DEBUG
    if ((SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_NOPARACHUTE) ==
         -1))
#else
    if ((SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == -1))
#endif
    {
        printf("Could not initialize SDL: %s.\n", SDL_GetError());
        exit(-1);
    }

    unsigned win_flags = 0;
    int win_fact = SCREEN_FACT;

#ifdef FULLSCREEN
    win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
    win_flags |= SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;

    /* Sharp pixels rather than bilinear blur when the image is scaled up */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    /* Open the window at the largest whole multiple of 320x200 that fits
       in roughly 85% of the desktop, instead of a postage stamp */
    int win_w = SCREEN_WIDTH * win_fact;
    int win_h = SCREEN_HEIGHT * win_fact;
    {
        SDL_DisplayMode dm;
        if (SDL_GetDesktopDisplayMode(0, &dm) == 0) {
            int sx = (dm.w * 85 / 100) / SCREEN_WIDTH;
            int sy = (dm.h * 85 / 100) / SCREEN_HEIGHT;
            int s = sx < sy ? sx : sy;
            if (s > 1) {
                win_w = SCREEN_WIDTH * s;
                win_h = SCREEN_HEIGHT * s;
            }
        }
    }

    main_window =
        SDL_CreateWindow("Lord of the Rings",
                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                win_w, win_h,
                win_flags);

    if (main_window == NULL) {
        fprintf(stderr, "Couldn't create window: %s\n", SDL_GetError());
        exit(1);
    }

    main_window_renderer = SDL_CreateRenderer(main_window, -1, SDL_RENDERER_ACCELERATED);
    if (main_window_renderer == NULL) {
        fprintf(stderr, "Couldn't get window renderer: %s\n", SDL_GetError());
        exit(1);
    }
    SDL_RenderSetLogicalSize(main_window_renderer,
            SCREEN_WIDTH * win_fact, SCREEN_HEIGHT * win_fact);
#if SCREEN_FACT == 1
    /* whole-number scaling keeps every pixel the same size */
    SDL_RenderSetIntegerScale(main_window_renderer, SDL_TRUE);
#endif

    memset(active_palette, '\0', sizeof(active_palette));

    {
        char name[32];
        int i;

        if (lotr_setting_get("display", name, sizeof(name)))
            for (i = 0; i < LOTR_DISPLAY_FILTERS; ++i)
                if (!SDL_strcasecmp(name, display_filter_names[i]))
                    lotr_display_filter_set(i);
        if (lotr_setting_get("fullscreen", name, sizeof(name)))
            lotr_apply_fullscreen(!SDL_strcasecmp(name, "on"));
    }

    lotr_reset_keyboard();

    desired.freq = 22050;
#ifdef CD_VERSION
    desired.format = AUDIO_S8;
#else
    desired.format = AUDIO_U8;
#endif
    desired.samples = 4096;
    desired.userdata = NULL;
    desired.channels = 1;

#ifdef HAVE_LIBSDL_MIXER
    if (Mix_OpenAudio(desired.freq, desired.format, 1, desired.samples) < 0) {
        fprintf(stderr, "lotr: could not initialize mixer: %s\n",
                SDL_GetError());
        fprintf(stderr, "lotr: sfx and music disabled\n");
        midi_disabled = 1;
    }

    for (i = 0; i < MIX_CHANNELS; ++i)
        sound_samples[i] = 0;

    Mix_ChannelFinished(hook_channel_finished);
#endif

    lotr_input_disabled = 0;

    srand(time(NULL));
}



/*
  quit SDL
*/
void
lotr_system_close(void)
{
#ifdef HAVE_LIBSDL_MIXER
    if (!midi_disabled)
        Mix_CloseAudio();
#endif

    if (filter_texture)
        SDL_DestroyTexture(filter_texture);
    if (scanline_texture)
        SDL_DestroyTexture(scanline_texture);
    free(filter_pixels);

    /* Shutdown all subsystems */
    SDL_DestroyWindow(main_window);
    SDL_Quit();
}



/*
  play sound sample
*/
void
play_sample(Uint8 *data, int size)
{
#ifdef HAVE_LIBSDL_MIXER
    Mix_Chunk *sample;
    int channel;

    /* Do not play non-existent or too short samples */
    if (data == NULL || size <= 0)
        return;

    if ((sample = Mix_QuickLoad_RAW(data, (Uint32) size)) == NULL) {
        fprintf(stderr, "Can not load sound sample: %s\n", Mix_GetError());
        return;
    }

    if ((channel = Mix_PlayChannel(-1, sample, 0)) < 0) {
        fprintf(stderr, "Can not play sound sample: %s\n", Mix_GetError());
        Mix_FreeChunk(sample);
        return;
    }

    if (channel >= MIX_CHANNELS) {
        fprintf(stderr, "Mix_PlayChannel returned too big channel number\n");
        return;
    }

    sound_samples[channel] = sample;
#endif
}


/*
   stop sound sample
 */
void
stop_sample(Uint8 *data)
{
#ifdef HAVE_LIBSDL_MIXER
    int i;
    for (i = 0; i < MIX_CHANNELS; ++i)
        if (sound_samples[i] && sound_samples[i]->abuf == data)
            Mix_HaltChannel(i);
#endif
}

/*
   are we playing a sound?
*/
int
playing_sample(void)
{
    int res = 0;

#ifdef HAVE_LIBSDL_MIXER
    int i;
    for (i = 0; i < MIX_CHANNELS; ++i) {
        if (sound_samples[i]) {
            res = 1;
            break;
        }
    }
#endif

    return res;
}



/*
  polls all pending events
*/
void
lotr_poll_events(void)
{
    SDL_Event event;

    cdmusic_update();           /* next CD music track when one ends */

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:

                /* Alt+Enter or F11 toggles fullscreen; never reaches the game */
                if (event.key.keysym.sym == SDLK_F11 ||
                    (event.key.keysym.sym == SDLK_RETURN &&
                     (event.key.keysym.mod & KMOD_ALT))) {
                    if (!event.key.repeat)
                        lotr_set_fullscreen(!lotr_fullscreen());
                    break;
                }

                /* F10 cycles the display filter; never reaches the game */
                if (event.key.keysym.sym == SDLK_F10) {
                    if (!event.key.repeat)
                        lotr_display_filter_step(1);
                    break;
                }

                if (lotr_input_disabled)
                    break;

                if (lotr_keybufferpos < KEYBOARDBUFFERESIZE) {
                    int keysym = event.key.keysym.sym;

                    /* Map numeric keyboard to normal numbers */
                    if (keysym >= SDLK_KP_0 && keysym <= SDLK_KP_9)
                        keysym = keysym - SDLK_KP_0 + '0';

                    lotr_keybuffer[lotr_keybufferpos++] = keysym;
                }

                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        lotr_key_esc_pressed = 1;
                        break;
                    case SDLK_LEFT:
                        lotr_key_left_pressed = 1;
                        break;
                    case SDLK_RIGHT:
                        lotr_key_right_pressed = 1;
                        break;
                    case SDLK_UP:
                        lotr_key_up_pressed = 1;
                        break;
                    case SDLK_DOWN:
                        lotr_key_down_pressed = 1;
                        break;
                    case SDLK_LSHIFT:
                    case SDLK_RSHIFT:
                        lotr_key_shift_pressed = 1;
                        break;
                    case SDLK_LCTRL:
                    case SDLK_RCTRL:
                        lotr_key_ctrl_pressed = 1;
                        break;

                    default:
                        break;
                }

                break;


            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    case SDLK_LEFT:
                        lotr_key_left_pressed = 0;
                        break;
                    case SDLK_RIGHT:
                        lotr_key_right_pressed = 0;
                        break;
                    case SDLK_UP:
                        lotr_key_up_pressed = 0;
                        break;
                    case SDLK_DOWN:
                        lotr_key_down_pressed = 0;
                        break;
                    case SDLK_LSHIFT:
                    case SDLK_RSHIFT:
                        lotr_key_shift_pressed = 0;
                        break;
                    case SDLK_LCTRL:
                    case SDLK_RCTRL:
                        lotr_key_ctrl_pressed = 0;
                        break;
                    case SDLK_ESCAPE:
                        /* esc can be only reseted */
                        break;
                    default:
                        break;
                }

                break;

                /* SDL_QUIT event (window close) */
            case SDL_QUIT:
                exit(0);
                break;

            default:
                break;
        }

    }

}


/*
  resets keyboard status
*/
void
lotr_reset_keyboard(void)
{
    lotr_poll_events();

    lotr_keybufferpos = 0;

    lotr_key_left_pressed = 0;
    lotr_key_right_pressed = 0;
    lotr_key_up_pressed = 0;
    lotr_key_down_pressed = 0;

    lotr_key_esc_pressed = 0;

}


/*
  was a key pressed?
*/
int
lotr_kb_hit(void)
{
    lotr_poll_events();

    return lotr_keybufferpos != 0;
}


/*
  get last pressed key - return 0 if none
*/
int
lotr_get_key(void)
{
    int i;
    int key;

    lotr_poll_events();

    if (lotr_keybufferpos == 0)
        return 0;

    key = lotr_keybuffer[0];
    lotr_keybufferpos--;

    for (i = 0; i < lotr_keybufferpos; ++i)
        lotr_keybuffer[i] = lotr_keybuffer[i + 1];

    return key;

}


/*
  disable keyboard input
*/
void
lotr_input_disable(void)
{
    lotr_input_disabled = 1;
}

/*
  enable keyboard input
*/
void
lotr_input_enable(void)
{
    lotr_input_disabled = 0;
}


/*
  shows a new screen
*/
SDL_Renderer *
lotr_sdl_renderer(void)
{
    return main_window_renderer;
}


/*
  display filters
*/

int
lotr_display_filter(void)
{
    return display_filter;
}


const char *
lotr_display_filter_name(void)
{
    return display_filter_names[display_filter];
}


void
lotr_display_filter_set(int filter)
{
    char title[64];

    if (filter < 0 || filter >= LOTR_DISPLAY_FILTERS)
        return;
    display_filter = filter;

    /* the window title says which filter is on, unless it is the default */
    if (main_window != NULL) {
        if (filter == LOTR_DISPLAY_SHARP)
            snprintf(title, sizeof(title), "Lord of the Rings");
        else
            snprintf(title, sizeof(title), "Lord of the Rings (%s)",
                     filter == LOTR_DISPLAY_SMOOTH ? "smooth" : "CRT");
        SDL_SetWindowTitle(main_window, title);
    }
}


void
lotr_display_filter_step(int delta)
{
    lotr_display_filter_set((display_filter + delta + LOTR_DISPLAY_FILTERS)
                            % LOTR_DISPLAY_FILTERS);
    lotr_setting_set("display", lotr_display_filter_name());
    fprintf(stderr, "lotr: display filter: %s\n", lotr_display_filter_name());
}



/*
  fullscreen, remembered in settings.txt
*/

int
lotr_fullscreen(void)
{
    return main_window != NULL && (SDL_GetWindowFlags(main_window)
                                   & SDL_WINDOW_FULLSCREEN_DESKTOP);
}


static void
lotr_apply_fullscreen(int on)
{
    if (main_window == NULL)
        return;
    SDL_SetWindowFullscreen(main_window,
                            on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    SDL_ShowCursor(on ? SDL_DISABLE : SDL_ENABLE);
}


void
lotr_set_fullscreen(int on)
{
    lotr_apply_fullscreen(on);
    lotr_setting_set("fullscreen", on ? "on" : "off");
}



/*
  volumes in percent
*/

void
lotr_set_effects_volume(int percent)
{
#ifdef HAVE_LIBSDL_MIXER
    Mix_Volume(-1, percent * MIX_MAX_VOLUME / 100);
#else
    (void)percent;
#endif
}


void
lotr_set_music_volume(int percent)
{
#ifdef HAVE_LIBSDL_MIXER
    Mix_VolumeMusic(percent * MIX_MAX_VOLUME / 100);
#else
    (void)percent;
#endif
}


/*
  streaming texture of the given size for a filtered frame, drawn with
  smooth scaling; filter_pixels can hold one frame of it
*/

static SDL_Texture *
lotr_filter_texture(int w, int h)
{
    if (filter_texture != NULL
        && (filter_texture_w != w || filter_texture_h != h)) {
        SDL_DestroyTexture(filter_texture);
        filter_texture = NULL;
        free(filter_pixels);
        filter_pixels = NULL;
    }

    if (filter_texture == NULL) {
        filter_texture = SDL_CreateTexture(main_window_renderer,
                                           SDL_PIXELFORMAT_ARGB8888,
                                           SDL_TEXTUREACCESS_STREAMING, w, h);
        filter_pixels = malloc((size_t)w * h * sizeof(Uint32));
        if (filter_texture == NULL || filter_pixels == NULL) {
            fprintf(stderr, "lotr: display filter disabled: %s\n",
                    SDL_GetError());
            if (filter_texture != NULL)
                SDL_DestroyTexture(filter_texture);
            filter_texture = NULL;
            free(filter_pixels);
            filter_pixels = NULL;
            display_filter = LOTR_DISPLAY_SHARP;
            return NULL;
        }
        SDL_SetTextureScaleMode(filter_texture, SDL_ScaleModeLinear);
        filter_texture_w = w;
        filter_texture_h = h;
    }

    return filter_texture;
}


/*
  darkened lines between the rows of game pixels, one texture row per
  window pixel row; needs at least 3 window pixels per game pixel
*/

static void
lotr_draw_scanlines(void)
{
    float sx, sy;
    int n, rows, i, r;
    Uint32 *line;
    Uint8 alpha;

    SDL_RenderGetScale(main_window_renderer, &sx, &sy);
    n = (int)(sy + 0.5f);
    if (n < 3)
        return;

    if (scanline_texture == NULL || scanline_scale != n) {
        if (scanline_texture != NULL)
            SDL_DestroyTexture(scanline_texture);
        rows = SCREEN_HEIGHT * n;
        scanline_texture = SDL_CreateTexture(main_window_renderer,
                                             SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STATIC, 1,
                                             rows);
        line = malloc(rows * sizeof(Uint32));
        if (scanline_texture == NULL || line == NULL) {
            free(line);
            scanline_scale = 0;
            return;
        }

        /* the last row of each game pixel is darkest, the one before it
           a little dark on large windows: transparent black otherwise */
        for (i = 0; i < rows; ++i) {
            r = i % n;
            if (r == n - 1)
                alpha = 0x80;
            else if (n >= 5 && r == n - 2)
                alpha = 0x30;
            else
                alpha = 0;
            line[i] = (Uint32)alpha << 24;
        }
        SDL_UpdateTexture(scanline_texture, NULL, line, sizeof(Uint32));
        free(line);
        SDL_SetTextureBlendMode(scanline_texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(scanline_texture, SDL_ScaleModeNearest);
        scanline_scale = n;
    }

    SDL_RenderCopy(main_window_renderer, scanline_texture, NULL, NULL);
}


/*
  smooth: hq4x, then smooth scaling to the window
*/

static void
lotr_present_smooth(SDL_Surface *frame)
{
    SDL_Texture *texture;

    texture = lotr_filter_texture(SCREEN_WIDTH * 4, SCREEN_HEIGHT * 4);
    if (texture == NULL)
        return;

    hq4x_32((const uint32_t *)frame->pixels, filter_pixels,
            SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_UpdateTexture(texture, NULL, filter_pixels,
                      SCREEN_WIDTH * 4 * sizeof(Uint32));
    SDL_RenderCopy(main_window_renderer, texture, NULL, NULL);
}


/*
  CRT: slightly soft pixels and scanlines
*/

static void
lotr_present_crt(SDL_Surface *frame)
{
    SDL_Texture *texture;
    const Uint32 *src;
    Uint32 *dst;
    Uint32 c;
    int x, y, i, w;
    static Uint8 boost[256];

    if (boost[255] == 0)
        for (i = 0; i < 256; ++i)
            boost[i] = i * CRT_BOOST / 256 > 255 ? 255 : i * CRT_BOOST / 256;

    w = SCREEN_WIDTH * CRT_PRESCALE;
    texture = lotr_filter_texture(w, SCREEN_HEIGHT * CRT_PRESCALE);
    if (texture == NULL)
        return;

    for (y = 0; y < SCREEN_HEIGHT; ++y) {
        src = (const Uint32 *)((const Uint8 *)frame->pixels
                               + y * frame->pitch);
        dst = filter_pixels + (size_t)y * CRT_PRESCALE * w;
        for (x = 0; x < SCREEN_WIDTH; ++x) {
            c = 0xff000000u | (Uint32)boost[(src[x] >> 16) & 0xff] << 16
                | (Uint32)boost[(src[x] >> 8) & 0xff] << 8
                | boost[src[x] & 0xff];
            for (i = 0; i < CRT_PRESCALE; ++i)
                *dst++ = c;
        }
        for (i = 1; i < CRT_PRESCALE; ++i)
            memcpy(filter_pixels + ((size_t)y * CRT_PRESCALE + i) * w,
                   filter_pixels + (size_t)y * CRT_PRESCALE * w,
                   w * sizeof(Uint32));
    }

    SDL_UpdateTexture(texture, NULL, filter_pixels, w * sizeof(Uint32));
    SDL_RenderCopy(main_window_renderer, texture, NULL, NULL);
    lotr_draw_scanlines();
}


/*
  shows a new screen
*/

void
lotr_show_screen(Uint8 *newscreen)
{
    SDL_Surface *src;
    SDL_Surface *frame;
    SDL_Texture *texture;

    if (main_window_renderer == NULL)
        return;

    /* convert pixels to 8-bit SDL surface */
    src = SDL_CreateRGBSurfaceFrom(
        newscreen, SCREEN_WIDTH, SCREEN_HEIGHT, 8, SCREEN_WIDTH, 0, 0, 0, 0);
    SDL_SetPaletteColors(src->format->palette, active_palette, 0, 256);

    SDL_RenderClear(main_window_renderer);

    if (display_filter == LOTR_DISPLAY_SHARP) {
        texture = SDL_CreateTextureFromSurface(main_window_renderer, src);
        SDL_RenderCopy(main_window_renderer, texture, NULL, NULL);
        SDL_DestroyTexture(texture);
    } else {
        /* the filters work on 32-bit pixels */
        frame = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_ARGB8888, 0);
        if (frame != NULL) {
            if (display_filter == LOTR_DISPLAY_SMOOTH)
                lotr_present_smooth(frame);
            else
                lotr_present_crt(frame);
            SDL_FreeSurface(frame);
        }
    }

    SDL_RenderPresent(main_window_renderer);

    SDL_FreeSurface(src);
}


/*
  return key states
*/

int
lotr_key_left()
{
    return lotr_key_left_pressed;
}

int
lotr_key_right()
{
    return lotr_key_right_pressed;
}

int
lotr_key_up()
{
    return lotr_key_up_pressed;
}

int
lotr_key_down()
{
    return lotr_key_down_pressed;
}

int
lotr_key_esc()
{
    return lotr_key_esc_pressed;
}

int
lotr_key_shift()
{
    return lotr_key_shift_pressed;
}

int
lotr_key_ctrl()
{
    return lotr_key_ctrl_pressed;
}


/*
  sets palette
*/
void
lotr_system_set_palette(Uint8 *palette, int firstcolor, int ncolors)
{
    int i, c;

    if (ncolors > 0x100)
        return;

    for (i = 0; i < ncolors; ++i) {
        c = firstcolor + i;
        active_palette[c].r = palette[3 * (i + firstcolor) + 0] * 4;
        active_palette[c].g = palette[3 * (i + firstcolor) + 1] * 4;
        active_palette[c].b = palette[3 * (i + firstcolor) + 2] * 4;
    }
}
