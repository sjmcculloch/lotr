/****************************************************************************

    gui.c
    graphics user interface


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
#include "game.h"
#include "cartoon.h"
#include "character.h"
#include "combat.h"
#include "gui.h"
#include "graphics.h"
#include "journal.h"
#include "lotr_sdl.h"
#include "map.h"
#include "object.h"
#include "shape.h"
#include "sound.h"
#include "timing.h"
#include "utils.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef EXTENDED
#include "pythonspot.h"
#endif

extern int screen_width;
extern int screen_height;


Character *choosed_character;

int wizard_mode;
char wizard_text[1024];

#define PARAGRAPHS_NUM 259
char *paragraphs;
int paragraphs_off[PARAGRAPHS_NUM + 1];


int dialog_mode = DIALOG_NONE;
int dialog_confirm;


int gui_menu_x = -1;
int gui_menu_y = -1;

int gui_paragraph_pos;
int gui_paragraph_isquestion;


/* should we show spots? */
#ifdef DRAW_SPOTS
int gui_show_spots = 1;
#else
int gui_show_spots = 0;
#endif

Palette *gui_palette;

/* Show cart10 after DIALOG_DIED */
static int dialog_died_show_cartoon = FALSE;

/* various GUI components */
#define TEST 15
#define GUI_CHAIN_HORIZ   0
#define GUI_MOUSE         1
#define GUI_MENU          2
#define GUI_SCROLL_TOP    3
#define GUI_CHAIN_VERT    4
#define GUI_CHAIN_SW      5
#define GUI_SCROLL_SW     6
#define GUI_CHAIN_SE      7
#define GUI_SCROLL_SE     8
#define GUI_SCROLL_EMPTY  9
#define GUI_SCROLL_BOT   10
#define GUI_CHAIN_NW     11
#define GUI_SCROLL_NW    12
#define GUI_CHAIN_NE     13
#define GUI_SCROLL_NE    14
#define GUI_FONT         15
#define GUI_COMP_NUM     16

Pixmap *gui_components[GUI_COMP_NUM] = { NULL };
int gui_comp_widths[GUI_COMP_NUM] = { /*00 */ 5,
    /*01 */ 160,
    /*02 */ 320,
    /*03 */ 8,
    /*04 */ 6,
    /*05 */ 10,
    /*06 */ 8,
    /*07 */ 10,
    /*08 */ 8,
    /*09 */ 8,
    /*10 */ 8,
    /*11 */ 10,
    /*12 */ 8,
    /*13 */ 10,
    /*14 */ 8,
    /*15 */ 256
};

#define DEFAULT_SCROLL_WIDTH 20

#define SCROLL_ELEMENT_SIZE 8

Pixmap *gui_font[0x100];

Pixmap *gui_life_bar;

/*
  init gui
*/

void
gui_init(void)
{
    Archive *archive;
    FILE *pal;
    int i, j;


    /* menu mode initializations */
    dialog_mode = DIALOG_NONE;
    gui_menu_x = gui_menu_y = 0;


    for (i = 0; i < GUI_COMP_NUM; ++i)
        gui_components[i] = NULL;

    for (i = 0; i < 0x100; ++i)
        gui_font[i] = NULL;

    pal = lotr_fopen("arts.pal", "rb");
    if (lotr_filelen(pal) != sizeof(Palette)) {
        fprintf(stderr, "lotr: arts.pal is not a valid palette file\n");
        exit(1);
    }

    gui_palette = lotr_malloc(sizeof(Palette));

    if (fread(gui_palette, sizeof(Palette), 1, pal) != 1) {
        fprintf(stderr, "lotr: can not read arts.pal\n");
        exit(1);
    }

    fclose(pal);

    /* arts palette is stored in 0..0x19 and is mapped to 0x40..0x59 */

    palette_copy_colors(gui_palette, 0, 0x20, 0x40);
    gui_set_palette();


    archive = archive_ndx_open("arts");

    for (i = 0; i < GUI_COMP_NUM; ++i) {
        gui_components[i] = pixmap_read_from_ndx_archive(archive, i);
        pixmap_set_width(gui_components[i], gui_comp_widths[i]);
    }

    archive_close(archive);

    for (j = 0; j < 3; ++j)
        for (i = 0; i < 0x20; ++i)
            gui_font[0x20 + j * 0x20 + i] =
                pixmap_subpixmap(gui_components[GUI_FONT], i * 8, j * 8,
                                 i * 8 + 7, j * 8 + 7);


    gui_life_bar = pixmap_new(10, 100);

    memset(gui_life_bar->data, 0x4, gui_life_bar->datasize);


    /* init paragraphs */
#ifndef TTT
    char *para_filename = "para.dat";
#else
    char *para_filename = "para2.dat";
#endif
    if (lotr_file_exists(para_filename)) {
        FILE *parfile;
        int parlotr_filelen;
        int pos, n;
        parfile = lotr_fopen(para_filename, "rb");
        parlotr_filelen = lotr_filelen(parfile);
        paragraphs = lotr_malloc(parlotr_filelen);
        if (fread(paragraphs, 1, parlotr_filelen, parfile) != parlotr_filelen) {
            fprintf(stderr, "lotr: can not read file para.dat\n");
            perror("lotr");
            exit(1);
        }

        pos = 0;
        n = 0;
        while (pos < parlotr_filelen && n <= PARAGRAPHS_NUM) {
            paragraphs_off[n] = pos;
            while (pos < parlotr_filelen && paragraphs[pos] != 0x10)
                ++pos;
            pos += 3;
            ++n;
        }

        while (n <= PARAGRAPHS_NUM) {
            paragraphs_off[n] = 0;
            ++n;
        }

        fclose(parfile);
    } else {
        paragraphs = NULL;
    }

}




/*
  init close
*/

void
gui_close(void)
{
    int i;

    for (i = 0; i < GUI_COMP_NUM; ++i)
        if (gui_components[i])
            pixmap_free(gui_components[i]);

    for (i = 0; i < 0x100; ++i)
        if (gui_font[i])
            pixmap_free(gui_font[i]);

    free(gui_palette);

    pixmap_free(gui_life_bar);

    if (paragraphs != NULL)
        free(paragraphs);

}





/*
  set the gui palette
*/

void
gui_set_palette(void)
{
    graphics_set_palette(gui_palette, 0x40, 0x20);
}



/*
  mouse: clickable areas of what is on screen

  Every scroll, and the main menu, registers its area and the key each of
  its hotspots stands for ("L. Load Game" -> 'l', "eXit" -> 'x', ...). A
  click is turned into that key, so every dialog works with the mouse
  without knowing about it. The list is emptied whenever the screen is
  redrawn from scratch (gui_clear); the most recently drawn area wins.
*/

#define CLICKMAP_MAX 128

typedef struct {
    int x, y, w, h;
    int key;                    /* 0: covers what is below, no key */
} ClickRegion;

static ClickRegion clickmap[CLICKMAP_MAX];
static int clickmap_num = 0;

/* graphics.c */
extern int window_x, window_y;

/* first text character of the last scroll drawn, and its width */
static int scroll_last_x, scroll_last_y, scroll_last_width;

static void gui_clickmap_add(int x, int y, int w, int h, int key);

/* keys only the mouse produces: a row of a list (settings) */
#define GUI_MOUSE_KEY_ROW 0x20000

/* registers line `line` of the last scroll drawn as a whole */
static void
gui_clickmap_add_scroll_line(int line, int key)
{
    gui_clickmap_add(scroll_last_x, scroll_last_y + line * SCROLL_ELEMENT_SIZE,
                     scroll_last_width * SCROLL_ELEMENT_SIZE,
                     SCROLL_ELEMENT_SIZE, key);
}


static void
gui_clickmap_clear(void)
{
    clickmap_num = 0;
}


static void
gui_clickmap_add(int x, int y, int w, int h, int key)
{
    int i;

    /* drawn again: move it to the top instead of adding it twice */
    for (i = 0; i < clickmap_num; ++i)
        if (clickmap[i].x == x && clickmap[i].y == y && clickmap[i].w == w
            && clickmap[i].h == h && clickmap[i].key == key) {
            memmove(clickmap + i, clickmap + i + 1,
                    (clickmap_num - i - 1) * sizeof(ClickRegion));
            clickmap_num--;
            break;
        }

    if (clickmap_num == CLICKMAP_MAX)
        return;
    clickmap[clickmap_num].x = x;
    clickmap[clickmap_num].y = y;
    clickmap[clickmap_num].w = w;
    clickmap[clickmap_num].h = h;
    clickmap[clickmap_num].key = key;
    clickmap_num++;
}


/* key at x, y; 0 on a dialog but not on a hotspot; -1 on nothing */
static int
gui_clickmap_hit(int x, int y)
{
    int i;

    for (i = clickmap_num - 1; i >= 0; --i)
        if (x >= clickmap[i].x && x < clickmap[i].x + clickmap[i].w
            && y >= clickmap[i].y && y < clickmap[i].y + clickmap[i].h)
            return clickmap[i].key;
    return -1;
}


/* key for a hotspot word of a scroll line, 0 if it is none */
static int
gui_hotspot_key(const char *word, int len, int line_has_exit)
{
    int i, upper = 0, upper_at = -1;

    if (len == 2 && word[1] == '.' && isalnum((unsigned char)word[0]))
        return tolower((unsigned char)word[0]);         /* "L." "1." */
    if (len > 2 && word[1] == '.' && isdigit((unsigned char)word[0]))
        return word[0];                                 /* "1.*Sword" */
    if (len == 4 && !strncmp(word, "Tab.", 4))
        return KEY_TAB;
    if (len == 2 && !strncmp(word, "UP", 2))
        return KEY_UP;
    if (len == 4 && !strncmp(word, "DOWN", 4))
        return KEY_DOWN;
    if (len == 4 && !strncmp(word, "PgUp", 4))
        return KEY_PAGEUP;
    if (len == 4 && !strncmp(word, "PgDn", 4))
        return KEY_PAGEDOWN;

    /* "eXit": one capital inside a lower case word */
    for (i = 0; i < len; ++i)
        if (isupper((unsigned char)word[i])) {
            upper++;
            upper_at = i;
        }
    if (upper == 1 && upper_at > 0 && islower((unsigned char)word[0]))
        return tolower((unsigned char)word[upper_at]);

    /* "Question eXit" */
    if (line_has_exit && len > 1 && isupper((unsigned char)word[0])
        && islower((unsigned char)word[1]))
        return tolower((unsigned char)word[0]);

    return 0;
}


/*
  finds the hotspots of a scroll line drawn with its first character at
  x, y and registers them; a hotspot reaches to the next one or the end
  of the line, so "L. Load Game" is clickable as a whole
*/

static void
gui_clickmap_scan_line(const char *line, int width, int x, int y)
{
    int keys[DEFAULT_SCROLL_WIDTH * 2], starts[DEFAULT_SCROLL_WIDTH * 2];
    int n = 0, len = 0, i, k, has_exit = 0;
    int size = SCROLL_ELEMENT_SIZE;

    while (len < width && line[len])
        ++len;

    /* "<  Frodo  >": the arrows change the character */
    for (i = 0; i < len && line[i] == ' '; ++i)
        ;
    if (i < len && line[i] == '<' && line[len - 1] == '>') {
        gui_clickmap_add(x + i * size, y, size, size, KEY_LEFT);
        gui_clickmap_add(x + (len - 1) * size, y, size, size, KEY_RIGHT);
        return;
    }

    /* "(Y/N)" */
    for (i = 0; i + 4 < len; ++i)
        if (!strncmp(line + i, "(Y/N)", 5)) {
            gui_clickmap_add(x + i * size, y, 2 * size, size, 'y');
            gui_clickmap_add(x + (i + 3) * size, y, 2 * size, size, 'n');
            return;
        }

    for (i = 0; i < len; ++i)
        if (i == 0 || line[i - 1] == ' ') {
            for (k = i; k < len && line[k] != ' '; ++k)
                ;
            if (gui_hotspot_key(line + i, k - i, 0) == 'x'
                && islower((unsigned char)line[i]))
                has_exit = 1;
        }

    for (i = 0; i < len && n < DEFAULT_SCROLL_WIDTH * 2; ++i)
        if (line[i] != ' ' && (i == 0 || line[i - 1] == ' ')) {
            for (k = i; k < len && line[k] != ' '; ++k)
                ;
            keys[n] = gui_hotspot_key(line + i, k - i, has_exit);
            if (keys[n]) {
                starts[n] = i;
                n++;
            }
        }

    for (i = 0; i < n; ++i) {
        int end = i + 1 < n ? starts[i + 1] : len;
        gui_clickmap_add(x + starts[i] * size, y,
                         (end - starts[i]) * size, size, keys[i]);
    }
}


/*
  clears screen and draws the bounding chain
*/

void
gui_clear(void)
{
    int i;
    Pixmap *pixmap;

    if (*gui_components == NULL)
        return;

    gui_clickmap_clear();

    graphics_set_window(0, 0, screen_width - 1, screen_height - 1);
    graphics_clear_screen();

    gui_set_full_window();


    /* horizontal lines */
    pixmap = gui_components[GUI_CHAIN_HORIZ];
    for (i = gui_components[GUI_CHAIN_NW]->width;
         i < screen_width - gui_components[GUI_CHAIN_NE]->width;
         i += pixmap->width) {
        pixmap_draw(pixmap, i, 1);
        pixmap_draw(pixmap, i, screen_height - pixmap->height);
    }


    /* vertical lines */
    pixmap = gui_components[GUI_CHAIN_VERT];
    for (i = gui_components[GUI_CHAIN_NW]->height;
         i < screen_height - gui_components[GUI_CHAIN_SW]->height;
         i += pixmap->height) {
        pixmap_draw(pixmap, 0, i);
        pixmap_draw(pixmap, screen_width - pixmap->width - 1, i);
    }



    /* screen corners */
    pixmap = gui_components[GUI_CHAIN_NW];
    pixmap_draw(pixmap, 0, 0);

    pixmap = gui_components[GUI_CHAIN_NE];
    pixmap_draw(pixmap, screen_width - pixmap->width, 0);

    pixmap = gui_components[GUI_CHAIN_SW];
    pixmap_draw(pixmap, 0, screen_height - pixmap->height);

    pixmap = gui_components[GUI_CHAIN_SE];
    pixmap_draw(pixmap, screen_width - pixmap->width,
                screen_height - pixmap->height);

}



/*
  draws scroll with a text
*/

void
draw_scroll(int x, int y, int width, int height, const char *text[])
{
    /* size of one scroll element */
    int size = SCROLL_ELEMENT_SIZE;

    int i, j;

    /* if width<0 draw the scroll in SE corner */


    if (width < 0) {
        width = -width;
        x = screen_width - 8 - size * (2 + width);
        y = screen_height - size * (2 + height);
    }

    /* first draw an empty scroll */

    for (i = 0; i < width; ++i) {
        pixmap_draw(gui_components[GUI_SCROLL_TOP], x + (i + 1) * size, y);
        for (j = 0; j < height; ++j)
            pixmap_draw(gui_components[GUI_SCROLL_EMPTY],
                        x + (i + 1) * size, y + (j + 1) * size);
        pixmap_draw(gui_components[GUI_SCROLL_BOT],
                    x + (i + 1) * size, y + (height + 1) * size);
    }

    pixmap_draw(gui_components[GUI_SCROLL_NW], x, y);
    pixmap_draw(gui_components[GUI_SCROLL_SW], x, y + (height + 1) * size);
    pixmap_draw(gui_components[GUI_SCROLL_NE], x + (width + 1) * size, y);
    pixmap_draw(gui_components[GUI_SCROLL_SE], x + (width + 1) * size,
                y + (height + 1) * size);

    /* next draw the text */
    for (j = 0; j < height; ++j)
        for (i = 0; i < width; ++i) {
            if (gui_font[(Uint8)text[j][i]])
                pixmap_draw(gui_font[(Uint8)text[j][i]],
                            x + (i + 1) * size, y + (j + 1) * size);
            if (text[j][i] == 0)
                break;
        }

    /* mouse: the scroll covers what is below it; its hotspots are keys
       (positions are relative to the graphics window) */
    gui_clickmap_add(window_x + x, window_y + y, (width + 2) * size,
                     (height + 2) * size, 0);
    for (j = 0; j < height; ++j)
        gui_clickmap_scan_line(text[j], width, window_x + x + size,
                               window_y + y + (j + 1) * size);
    scroll_last_x = window_x + x + size;
    scroll_last_y = window_y + y + size;
    scroll_last_width = width;
}








/*
  set the map drawing area
*/

void
gui_set_map_area(int *width, int *height)
{


    if (dialog_mode < MAIN_MENU
        || screen_width > 320 /* Draw whole map for large screens */ ) {

#if PIXEL_PRECISE

        *width = screen_width - 16;
        *height = screen_height - 16;

        graphics_set_window(8, 8, 8 + *width - 1, 8 + *height - 1);

#else

        *width = screen_width - 17;
        *height = screen_height - 17;

        graphics_set_window(8, 9, 8 + *width - 1, 9 + *height - 1);

#endif
    } else {
#if PIXEL_PRECISE

        *width = screen_width - 16;
        *height = screen_height - 8 - gui_components[GUI_MENU]->height;

        graphics_set_window(8, 8, 8 + *width - 1, 8 + *height - 1);

#else

        *width = screen_width - 17;
        *height = screen_height - 8 - gui_components[GUI_MENU]->height - 2;

        graphics_set_window(8, 9, 8 + *width - 1, 9 + *height - 1);

#endif
    }

}




/*
  set the window over whole screen
*/

void
gui_set_full_window(void)
{
#if PIXEL_PRECISE
    graphics_set_window(0, 0, screen_width - 1, screen_height - 2);
#else
    graphics_set_window(0, 0, screen_width - 1, screen_height - 1);
#endif
}




/*
  draws main menu
 */

#define MAX_LIFE_BAR 55

void
main_menu_draw(void)
{
    int life;

    /* the menu bar is 320 wide: centred on a wider screen */
    gui_menu_x = (screen_width - gui_components[GUI_MENU]->width) / 2;
    gui_menu_y = screen_height - gui_components[GUI_MENU]->height;

    pixmap_draw(gui_components[GUI_MENU], gui_menu_x, gui_menu_y);

    portrait_draw(choosed_character->portrait, gui_menu_x + 48,
                  gui_menu_y + 9);

    life = MAX_LIFE_BAR * choosed_character->life / choosed_character->end;
    if (life > MAX_LIFE_BAR)
        life = MAX_LIFE_BAR;
    gui_life_bar->height = life;
    pixmap_draw(gui_life_bar, gui_menu_x + 20,
                gui_menu_y + 9 + MAX_LIFE_BAR - life);
    gui_life_bar->height = 100;

    /* mouse: the ten picture buttons, two rows of five, and the portrait */
    {
        static const char top[5] = { 'a', 'v', 'g', 'u', 's' };
        static const char bottom[5] = { 'm', 't', 'l', 'o', 'x' };
        int i;

        gui_clickmap_add(window_x + gui_menu_x, window_y + gui_menu_y,
                         gui_components[GUI_MENU]->width,
                         gui_components[GUI_MENU]->height, 0);
        for (i = 0; i < 5; ++i) {
            gui_clickmap_add(window_x + gui_menu_x + 120 + 40 * i,
                             window_y + gui_menu_y, 40, 26, top[i]);
            gui_clickmap_add(window_x + gui_menu_x + 120 + 40 * i,
                             window_y + gui_menu_y + 26, 40, 30, bottom[i]);
        }
        gui_clickmap_add(window_x + gui_menu_x + 44, window_y + gui_menu_y,
                         76, 56, 'c');
    }
}





/*
  show main menu
 */


void
main_menu_show(void)
{
    dialog_mode = MAIN_MENU;

    if (dialog_mode == DIALOG_NONE && !combat_get_mode())
        choosed_character = game_get_leader();

    if (!choosed_character)
        choosed_character = game_get_leader();

    gui_clear();
    game_draw_map();

    main_menu_draw();
}





/*
  quits all menus
*/

void
quit_menu()
{
    dialog_mode = DIALOG_NONE;
    choosed_character = NULL;

    if (combat_get_mode()) {
        main_menu_show();
    } else {
        gui_clear();
        game_draw_map();
        lotr_reset_keyboard();
    }
}



/*
  Change selected character in party
 */
static void
rotate_choosed_character(int delta)
{
    Character *party[LOTR_PARTY_SIZE];
    int num_characters;
    int choosed_index;

    if (combat_get_mode()) {
        fprintf(stderr, "lord: Trying to change character during combat\n");
        return;
    }

    num_characters = game_get_party_characters(party);
    for (choosed_index = 0; choosed_index < num_characters; ++choosed_index)
        if (choosed_character == party[choosed_index])
            break;

    if (choosed_index >= num_characters) {
        fprintf(stderr, "lord: Choosed character is not in the party\n");
        return;
    }

    choosed_index += delta;
    choosed_index = choosed_index % num_characters;
    choosed_index += num_characters;
    choosed_index = choosed_index % num_characters;

    choosed_character = party[choosed_index];

    main_menu_show();
}



/*
  Choose the next character
 */
static void
choose_next_character(void)
{
    rotate_choosed_character(1);
}



/*
  Choose the previous character
 */
static void
choose_prev_character(void)
{
    rotate_choosed_character(-1);
}


#define MAX_MESSAGELINES 256


/*
  format message text
 */

char *
format_text(const char *_text, int *lines, int width,
            const char *formatted_text[])
{
    int n;
    char *text, *t;
    int i, j, line;

    n = strlen(_text);
    t = text = lotr_malloc(n + 1);
    memcpy(text, _text, n);
    text[n] = 0;

    /* format text */

    line = 0;
    i = j = 0;

    while (t[j] != 0) {
        if (t[j] == ' ')
            i = j;
        if (j == width && i > 0) {
            t[i] = 0;
            formatted_text[line++] = t;
            t = t + i + 1;
            i = j = 0;
        }
        ++j;
    }


    if (j != 0) {
        t[j] = 0;
        formatted_text[line++] = t;
    }

    if (line == 1) {
        formatted_text[line++] = t + j;
    }

    *lines = line;
    return text;

}


/*
  shows a message
 */
void
gui_message(const char *text, int small_window)
{
    const char *formatted_text[MAX_MESSAGELINES];
    int width;
    int y;
    char *tmptext;
    int lines;


    if (text == NULL)
        return;

    if (small_window) {
        main_menu_show();
        width = DEFAULT_SCROLL_WIDTH;
    } else {
        if (dialog_mode != DIALOG_BOOK) {
            gui_clear();
            game_draw_map();
        }
        width = screen_width / SCROLL_ELEMENT_SIZE - 4;
    }

    if (dialog_mode != DIALOG_BOOK)
        dialog_mode = DIALOG_MESSAGE;

    tmptext = format_text(text, &lines, width, formatted_text);

    y = 128;
    if (lines > 6)
        y -= SCROLL_ELEMENT_SIZE * (lines - 6);
    if (y < SCROLL_ELEMENT_SIZE)
        y = SCROLL_ELEMENT_SIZE;

    if (small_window) {
        while (lines < 7)
            formatted_text[lines++] = "";
        draw_scroll(0, 0, -DEFAULT_SCROLL_WIDTH, 7, formatted_text);
    } else {
        draw_scroll(SCROLL_ELEMENT_SIZE, 128, width, lines, formatted_text);
    }

    free(tmptext);

}


/*
  parse message keyboard input
*/
void
dialog_message_key(int key)
{
    if (key == KEY_ENTER || key == ' ' || key == 'x') {
        if (dialog_mode == DIALOG_DIED) {
            if (dialog_died_show_cartoon)
                cartoon_play("cart10");
            exit(0);
        }
#ifdef TTT
        if (dialog_mode == DIALOG_BOOK) {
            gui_set_palette();
            map_set_palette();
            shapes_set_palette();
            gui_clear();
        }
#endif
        quit_menu();
    }

}



/*
  shows a question
 */


void
gui_question(char *text)
{
    gui_message(text, 0);
    lotr_reset_keyboard();
    dialog_mode = DIALOG_MESSAGE_YN;
}



/*
  parse question keyboard input
*/


void
dialog_message_yn_key(int key)
{
    if (key == 'n') {
        spot_if_result(game_get_actual_spot(), 0);
        quit_menu();
    }
    if (key == 'y') {
        spot_if_result(game_get_actual_spot(), 1);
        quit_menu();
    }
}


void
gui_dialog_wait(int mode) {
    while (dialog_mode == mode) {
        lotr_poll_events();
        gui_frame();
        map_animate_frame();
        graphics_update_screen();
        lotr_timer(50);
    }
}


void
gui_message_wait(const char *text, int small_window)
{
    gui_message(text, small_window);
    gui_dialog_wait(DIALOG_MESSAGE);
}

/*
  shows dialog saying that the player is dead
  if no text is given a default is used
 */
void
gui_died_show(const char *text, int show_cartoon)
{
    if (text == NULL)
        text =
            "You have failed. Sauron has finally recovered the Ring. Your quest is Over.";
    gui_message(text, 1);
    dialog_mode = DIALOG_DIED;
    dialog_died_show_cartoon = show_cartoon;
    gui_dialog_wait(DIALOG_DIED);
}

/*
  shows a help message
 */

void
gui_help_show()
{
    const char *help_text[] = {
        "",
        " Arrows - move character",
        " Space  - game menu",
        " Escape - exit/options menu",
#ifndef DEBUG
        " Shift  - 4x time compression",
#endif
        "",
        " A  Attack        O  Options",
        " C  Choose char.  Q  Quit game",
        " G  Get/Buy       T  Talk",
        " H  Help          S  Skill",
        " L  Set leader    U  Use",
        " M  Magic         V  View char. ",
#ifdef DEBUG
        "",
        " Cheating:",
        "            W  Wizard console",
        "       Ctrl+W  Print script",
        "  Ctrl+Arrows  Noclip mode",
        " Shift+Arrows  Fast noclip",
#endif
        ""
    };
    int lines = sizeof(help_text) / sizeof(const char *);
    int width = 0;
    int xoff, yoff;
    int i;

    for (i = 0; i < lines; ++i) {
        int len = strlen(help_text[i]);
        width = max(len, width);
    }

    xoff = (screen_width - (width + 2) * SCROLL_ELEMENT_SIZE) / 2;
    yoff = (screen_height - (lines + 2) * SCROLL_ELEMENT_SIZE) / 2;

    dialog_mode = DIALOG_MESSAGE;

    draw_scroll(xoff, yoff, width, lines, help_text);
}


/*
  draws gui paragraph scroll
*/

void
gui_paragraph_scroll(void)
{
    const char *formatted_text[MAX_MESSAGELINES];
    int lines, width, x, y;

    gui_clear();
    game_draw_map();
#ifndef TTT
    width = 32;
#else
    width = 36;
#endif

    if (paragraphs[gui_paragraph_pos] == 0x5)
        ++gui_paragraph_pos;

    lines = 0;
    while (paragraphs[gui_paragraph_pos] != 0x10 &&
           paragraphs[gui_paragraph_pos] != 0x5 && lines < MAX_MESSAGELINES) {
        /* add one line */
        formatted_text[lines] = paragraphs + gui_paragraph_pos;

        while (paragraphs[gui_paragraph_pos] != 0x0d &&
               paragraphs[gui_paragraph_pos] != 0)
            ++gui_paragraph_pos;
        paragraphs[gui_paragraph_pos] = 0;

        gui_paragraph_pos += 2;
        lines++;
    }

    x = (screen_width - (width + 2) * SCROLL_ELEMENT_SIZE) / 2;
    y = (screen_height - (lines + 2) * SCROLL_ELEMENT_SIZE) / 2;

    draw_scroll(x, y, width, lines, formatted_text);
}



/*
  shows a paragraph
*/

void
gui_paragraph(int num)
{

    if (paragraphs == NULL) {
        gui_message(game_get_paragraph_text(num, 0), 0);
        return;
    }


    if (num < 0 || num > PARAGRAPHS_NUM)
        num = 0;

    gui_paragraph_isquestion = 0;
    gui_paragraph_pos = paragraphs_off[num];

    gui_paragraph_scroll();

    if (paragraphs[gui_paragraph_pos] == 0x10) {
        /* paragraph does not continue */
        dialog_mode = DIALOG_MESSAGE;
    } else {
        /* paragraph continues */
        dialog_mode = DIALOG_PARAGRAPH;
    }

}



/*
  shows a question paragraph
*/

void
gui_paragraph_question(int num)
{
    if (paragraphs == NULL) {
        gui_question(game_get_paragraph_text(num, 1));
        return;
    }

    gui_paragraph(num);
    gui_paragraph_isquestion = 1;

    if (dialog_mode != DIALOG_PARAGRAPH) {
        lotr_reset_keyboard();
        dialog_mode = DIALOG_MESSAGE_YN;
    }
}


/*
  parse paragraph keyboard input
*/

void
dialog_paragraph_key(int key)
{
    if (key == KEY_ENTER || key == ' ' || key == 'x') {
        gui_paragraph_scroll();

        if (paragraphs[gui_paragraph_pos] == 0x10) {
            /* paragraph does not continue */
            if (gui_paragraph_isquestion) {
                lotr_reset_keyboard();
                dialog_mode = DIALOG_MESSAGE_YN;
            } else
                dialog_mode = DIALOG_MESSAGE;
        } else {
            /* paragraph continues */
            dialog_mode = DIALOG_PARAGRAPH;
        }
    }

}

#define MAX_SPOTLINES 4096
static const char *spot_formatted_text[MAX_SPOTLINES];
static int spot_formatted_text_lines = -1;
static int spot_formatted_text_pos = -1;
static int spot_print_xoff = -1;
static int spot_scroll_width = 256;

/*
  show the active spot
*/
void
dialog_print_active_spot()
{
    Character *leader;
    CommandSpot *spot;
    char *spot_string;
    int screen_height_els = screen_height / SCROLL_ELEMENT_SIZE;
    int lines;

    if (dialog_mode != DIALOG_SPOT_PRINT || spot_formatted_text_lines < 0) {
        char *buf;
        int i;

        if ((leader = game_get_leader()) == NULL)
            return;

        if ((spot = map_get_spot(leader)) == NULL)
            return;

        spot_string = spot_get_string(spot);

        dialog_mode = DIALOG_SPOT_PRINT;

        /* Format the code */
        spot_formatted_text_pos = 0;
        spot_formatted_text_lines = 0;
        spot_print_xoff = 0;
        buf = spot_string;

        while (spot_formatted_text_lines < MAX_SPOTLINES && *buf) {
            spot_formatted_text[spot_formatted_text_lines++] = buf;
            while (*buf && *buf != '\n')
                buf++;

            /* new line found */
            if (*buf) {
                *buf = 0;
                buf++;
            }
        }

        spot_scroll_width = screen_width / SCROLL_ELEMENT_SIZE;
        for (i = 0; i < spot_formatted_text_lines; ++i)
            spot_scroll_width =
                max(spot_scroll_width, strlen(spot_formatted_text[i]));
    }

    lines =
        min(screen_height_els,
            spot_formatted_text_lines - spot_formatted_text_pos);
    draw_scroll(-SCROLL_ELEMENT_SIZE * (spot_print_xoff + 1),
                -SCROLL_ELEMENT_SIZE, 256, lines,
                spot_formatted_text + spot_formatted_text_pos);
}


/*
  parse spot print key
*/

void
dialog_print_active_spot_key(int key)
{
    int screen_height_els = screen_height / SCROLL_ELEMENT_SIZE;
    int screen_width_els = screen_width / SCROLL_ELEMENT_SIZE;
    int pg_size = max(screen_height_els - 2, 1);
    int last_pos = spot_formatted_text_pos;
    int last_xoff = spot_print_xoff;

    if (key == KEY_ENTER || key == 'x' || key == 'q') {
        spot_formatted_text_lines = -1;
        quit_menu();
    }

    if (key == KEY_UP) {
        spot_formatted_text_pos--;
    } else if (key == KEY_DOWN) {
        spot_formatted_text_pos++;
    } else if (key == KEY_PAGEUP) {
        spot_formatted_text_pos -= pg_size;
    } else if (key == KEY_PAGEDOWN) {
        spot_formatted_text_pos += pg_size;
    } else if (key == KEY_LEFT) {
        spot_print_xoff--;
    } else if (key == KEY_RIGHT) {
        spot_print_xoff++;
    }

    if (spot_formatted_text_pos + screen_height_els > spot_formatted_text_lines)
        spot_formatted_text_pos = spot_formatted_text_lines - screen_height_els;

    if (spot_formatted_text_pos < 0)
        spot_formatted_text_pos = 0;

    if (spot_print_xoff + screen_width_els > spot_scroll_width)
        spot_print_xoff = spot_scroll_width - screen_width_els;

    if (spot_print_xoff < 0)
        spot_print_xoff = 0;

    /* Redraw the scroll */
    if (last_pos != spot_formatted_text_pos || last_xoff != spot_print_xoff) {
        dialog_print_active_spot();
    }
}


#ifdef TTT

/*
  shows a book with the next chapter in TTT
*/

void
gui_book(char *text)
{

    graphics_set_background("back");
    gui_set_palette();

    portrait_draw(game_get_leader()->portrait, 168, 51);

    dialog_mode = DIALOG_BOOK;
    gui_message(text, 0);
    lotr_reset_keyboard();
}

#endif




/*
  shows confirm scroll
 */


void
gui_confirm_scroll(char *text)
{
    const char *scroll_text[7];
    char *tmptext;
    const char *formatted_text[MAX_MESSAGELINES];
    int lines, i;

    tmptext = format_text(text, &lines, DEFAULT_SCROLL_WIDTH, formatted_text);

    for (i = 0; i < lines && formatted_text[i][0] && i < 6; ++i)
        scroll_text[i] = formatted_text[i];

    scroll_text[i++] = "Are you sure? (Y/N)";

    for (; i < 7; ++i)
        scroll_text[i] = "";

    draw_scroll(0, 0, -DEFAULT_SCROLL_WIDTH, 7, scroll_text);
    free(tmptext);
}


/*
  show options menu
 */


void
dialog_options_show(void)
{

    const char *text[8] = {
        "OPTIONS",
        "L. Load Game",
        "S. Save Game",
        "Q. Quit Game",
        "P. Pause Game",
        "M. Start/Stop Music",
        "C. Configure",
        "X. Exit"
    };

    dialog_mode = DIALOG_OPTIONS;
    dialog_confirm = 0;

    draw_scroll(0, 0, -DEFAULT_SCROLL_WIDTH, 8, text);

}



/*
  show save menu
 */


#if !PIXEL_PRECISE

/* the save/load list uses a full width scroll */
#define SAVELOAD_SCROLL_WIDTH 36
#define SAVELOAD_LINE_SIZE    (SAVELOAD_SCROLL_WIDTH + 1)


/*
  one line of the save/load list: key, leader, region and date of the slot
*/

static void
dialog_saveload_line(char *line, char key, int slot)
{
    int map, leader_id;
    time_t mtime;
    char date[16];
    char leader_name[11];
    struct tm *tm;

    switch (game_save_info(slot, &map, &leader_id, &mtime)) {
        case 0:
            snprintf(line, SAVELOAD_LINE_SIZE, "%c -- empty --", key);
            break;

        case -1:
            snprintf(line, SAVELOAD_LINE_SIZE, "%c (damaged save)", key);
            break;

        default:
            tm = localtime(&mtime);
            if (mtime == 0 || tm == NULL
                || !strftime(date, sizeof(date), "%d %b %H:%M", tm))
                date[0] = 0;
            /* first name only: "Frodo Baggins" would not fit */
            snprintf(leader_name, sizeof(leader_name), "%.10s",
                     character_get(leader_id)->name);
            leader_name[strcspn(leader_name, " ")] = 0;
            snprintf(line, SAVELOAD_LINE_SIZE, "%c %-10s %-10.10s %.12s",
                     key, leader_name, game_map_name(map), date);
            break;
    }
}


/*
  draws the save/load list; loading also offers the quick and auto saves
*/

static void
dialog_saveload_draw(int load)
{
    char lines[GAME_SLOT_MAX + 4][SAVELOAD_LINE_SIZE];
    const char *text[GAME_SLOT_MAX + 4];
    int n = 0;
    int i;

    snprintf(lines[n++], SAVELOAD_LINE_SIZE, "%s",
             load ? "Load Game" : "Save Game");

    if (load) {
        dialog_saveload_line(lines[n++], 'Q', GAME_SLOT_QUICK);
        dialog_saveload_line(lines[n++], 'A', GAME_SLOT_AUTO);
    }

    for (i = 1; i <= GAME_SLOT_MAX; ++i)
        dialog_saveload_line(lines[n++], '0' + i, i);

    snprintf(lines[n++], SAVELOAD_LINE_SIZE, "%s",
             load ? "Q. Quicksave  A. Autosave  X. Exit" : "X. Exit");

    for (i = 0; i < n; ++i)
        text[i] = lines[i];

    draw_scroll(0, 0, -SAVELOAD_SCROLL_WIDTH, n, text);

    /* mouse: each slot line is clickable as a whole */
    for (i = 1; i < n - 1; ++i)
        gui_clickmap_add_scroll_line(i, tolower((unsigned char)lines[i][0]));
}

#endif



void
dialog_save_show(void)
{
#if !PIXEL_PRECISE
    dialog_mode = DIALOG_SAVE;
    dialog_confirm = 0;

    dialog_saveload_draw(0);
#else
    const char *text[7] = {
        "Save Game",
        "1. Game One",
        "2. Game Two",
#if PIXEL_PRECISE
        "X. Exit",
        "",
        "",
        ""
#else
        "3. Game Three",
        "4. Game Four",
        "5. Game Five",
        "X. Exit"
#endif
    };

    dialog_mode = DIALOG_SAVE;
    dialog_confirm = 0;

    draw_scroll(0, 0, -DEFAULT_SCROLL_WIDTH, 7, text);
#endif
}



/*
  show load menu
 */


void
dialog_load_show(void)
{
#if !PIXEL_PRECISE
    dialog_mode = DIALOG_LOAD;
    dialog_confirm = 0;

    dialog_saveload_draw(1);
#else
    const char *text[7] = {
        "Load Game",
        "1. Game One",
        "2. Game Two",
#if PIXEL_PRECISE
        "X. Exit",
        "",
        "",
        ""
#else
        "3. Game Three",
        "4. Game Four",
        "5. Game Five",
        "X. Exit"
#endif
    };

    dialog_mode = DIALOG_LOAD;
    dialog_confirm = 0;

    draw_scroll(0, 0, -DEFAULT_SCROLL_WIDTH, 7, text);
#endif
}




/*
  show options message
 */


void
dialog_options_message_show(const char *text[7])
{
    dialog_mode = DIALOG_MESSAGE;
    draw_scroll(0, 0, -DEFAULT_SCROLL_WIDTH, 7, text);
}






/*
  settings: one row per setting, Up/Down choose, Left/Right/Enter change;
  every change is applied at once and remembered in settings.txt
*/

#define SETTINGS_SCROLL_WIDTH 36
#define SETTINGS_LINE_SIZE    (SETTINGS_SCROLL_WIDTH + 1)

enum {
    SETTING_MUSIC,
    SETTING_MUSIC_VOLUME,
    SETTING_EFFECTS_VOLUME,
    SETTING_DISPLAY,
    SETTING_FULLSCREEN,
    SETTING_ASPECT,
    SETTING_WIDESCREEN,
    SETTINGS_NUM
};

static int settings_row = 0;


/* "========--  80%" (the game font has no '#') */
static void
settings_volume_bar(char *buf, size_t size, int percent)
{
    char bar[11];
    int i;

    for (i = 0; i < 10; ++i)
        bar[i] = i < (percent + 5) / 10 ? '=' : '-';
    bar[10] = 0;
    snprintf(buf, size, "%s %3d%%", bar, percent);
}


static void
dialog_settings_draw(void)
{
    static const char *labels[SETTINGS_NUM] = {
        "Music", "Music volume", "Effects volume", "Display", "Fullscreen",
        "Pixel shape", "Widescreen"
    };
    static const char *display_names[LOTR_DISPLAY_FILTERS] = {
        "Sharp", "Smooth", "CRT"
    };
    char lines[SETTINGS_NUM + 3][SETTINGS_LINE_SIZE];
    char value[24];
    const char *text[SETTINGS_NUM + 3];
    int n = 0;
    int i;

    snprintf(lines[n++], SETTINGS_LINE_SIZE, "Settings");

    for (i = 0; i < SETTINGS_NUM; ++i) {
        switch (i) {
            case SETTING_MUSIC:
                snprintf(value, sizeof(value), "%s",
                         !music_available() ? "no music files"
                         : music_is_on() ? "On" : "Off");
                break;
            case SETTING_MUSIC_VOLUME:
                settings_volume_bar(value, sizeof(value), music_get_volume());
                break;
            case SETTING_EFFECTS_VOLUME:
                settings_volume_bar(value, sizeof(value),
                                    effects_get_volume());
                break;
            case SETTING_DISPLAY:
                snprintf(value, sizeof(value), "%s",
                         display_names[lotr_display_filter()]);
                break;
            case SETTING_FULLSCREEN:
                snprintf(value, sizeof(value), "%s",
                         lotr_fullscreen() ? "On" : "Off");
                break;
            case SETTING_ASPECT:
                snprintf(value, sizeof(value), "%s",
                         lotr_aspect_43() ? "4:3 (original)" : "Square");
                break;
            default:
                snprintf(value, sizeof(value), "%s",
                         lotr_widescreen() ? "On" : "Off");
                break;
        }
        snprintf(lines[n++], SETTINGS_LINE_SIZE, "%c %-15s %.18s",
                 i == settings_row ? '>' : ' ', labels[i], value);
    }

    snprintf(lines[n++], SETTINGS_LINE_SIZE, "Arrows choose and change");
    snprintf(lines[n++], SETTINGS_LINE_SIZE, "X. Exit");

    for (i = 0; i < n; ++i)
        text[i] = lines[i];

    draw_scroll(0, 0, -SETTINGS_SCROLL_WIDTH, n, text);

    /* mouse: click a line to choose it, click it again to change it */
    for (i = 0; i < SETTINGS_NUM; ++i)
        gui_clickmap_add_scroll_line(i + 1, GUI_MOUSE_KEY_ROW + i);
}


void
dialog_settings_show(void)
{
    dialog_mode = DIALOG_SETTINGS;
    dialog_confirm = 0;

    /* the wider scroll covers the options scroll; start from a clean map */
    gui_clear();
    game_draw_map();
    dialog_settings_draw();
}


void
dialog_settings_key(int key)
{
    int delta = 0;

    if (key == 'x') {
        quit_menu();
        return;
    }

    if (key >= GUI_MOUSE_KEY_ROW && key < GUI_MOUSE_KEY_ROW + SETTINGS_NUM) {
        if (settings_row == key - GUI_MOUSE_KEY_ROW)
            delta = 1;
        else
            settings_row = key - GUI_MOUSE_KEY_ROW;
    } else if (key == KEY_UP)
        settings_row = (settings_row + SETTINGS_NUM - 1) % SETTINGS_NUM;
    else if (key == KEY_DOWN)
        settings_row = (settings_row + 1) % SETTINGS_NUM;
    else if (key == KEY_LEFT)
        delta = -1;
    else if (key == KEY_RIGHT || key == KEY_ENTER || key == ' ')
        delta = 1;
    else
        return;

    if (delta)
        switch (settings_row) {
            case SETTING_MUSIC:
                music_set_on(!music_is_on());
                break;
            case SETTING_MUSIC_VOLUME:
                music_set_volume(music_get_volume() + 10 * delta);
                break;
            case SETTING_EFFECTS_VOLUME:
                effects_store_volume(effects_get_volume() + 10 * delta);
                effects_preview();
                break;
            case SETTING_DISPLAY:
                lotr_display_filter_step(delta);
                break;
            case SETTING_FULLSCREEN:
                lotr_set_fullscreen(!lotr_fullscreen());
                break;
            case SETTING_ASPECT:
            case SETTING_WIDESCREEN:
                /* both change the picture's shape; widescreen's width
                   depends on the pixel shape too */
                if (settings_row == SETTING_ASPECT)
                    lotr_set_aspect_43(!lotr_aspect_43());
                else
                    lotr_set_widescreen(!lotr_widescreen());
                graphics_apply_screen_mode();
                dialog_settings_show();
                return;
        }

    dialog_settings_draw();
}



/*
  map overview (Tab): the whole current map shrunk into the window, parts
  not yet explored left dark, a blinking cross where the party is
*/

#define OVERVIEW_SIZE 176       /* pixels; the map is square */
#define OVERVIEW_X    (14 + (screen_width - SCREEN_WIDTH) / 2)
#define OVERVIEW_Y    12

static Pixmap *overview_map = NULL;     /* clean overview */
static Pixmap *overview_shown = NULL;   /* with the marker */
static int overview_frame = 0;
static int overview_marker_x, overview_marker_y;
static int overview_marker_color, overview_marker_edge;


/* palette entry in [first, last] closest to r, g, b (0-63) */
static int
gui_nearest_color(int r, int g, int b, int first, int last)
{
    extern Uint8 main_palette[0x300];
    int c, d, best = first, bestd = 1 << 30;
    const Uint8 *p;

    for (c = first; c <= last; ++c) {
        p = main_palette + 3 * c;
        d = (p[0] - r) * (p[0] - r) + (p[1] - g) * (p[1] - g)
            + (p[2] - b) * (p[2] - b);
        if (d < bestd) {
            bestd = d;
            best = c;
        }
    }
    return best;
}


static void
dialog_map_draw_marker(int visible)
{
    int i, x, y;
    Uint8 *d = overview_shown->data;

    memcpy(d, overview_map->data, OVERVIEW_SIZE * OVERVIEW_SIZE);
    if (visible)
        for (i = -3; i <= 3; ++i) {
            /* a cross with a dark edge, readable on any ground */
            int pass, w;
            for (pass = 0; pass < 2; ++pass)
                for (w = -1; w <= 1; ++w) {
                    if (pass == 1 && w != 0)
                        continue;
                    x = overview_marker_x + i;
                    y = overview_marker_y + w;
                    if (x >= 0 && y >= 0 && x < OVERVIEW_SIZE
                        && y < OVERVIEW_SIZE)
                        d[y * OVERVIEW_SIZE + x] = pass
                            ? overview_marker_color : overview_marker_edge;
                    x = overview_marker_x + w;
                    y = overview_marker_y + i;
                    if (x >= 0 && y >= 0 && x < OVERVIEW_SIZE
                        && y < OVERVIEW_SIZE)
                        d[y * OVERVIEW_SIZE + x] = pass
                            ? overview_marker_color : overview_marker_edge;
                }
        }
    pixmap_draw(overview_shown, OVERVIEW_X, OVERVIEW_Y);
}


void
dialog_map_show(void)
{
    const char *text[9];
    char region[13];
    Character *leader = game_get_leader();

    dialog_mode = DIALOG_MAP;
    dialog_confirm = 0;

    if (overview_map == NULL) {
        overview_map = pixmap_new(OVERVIEW_SIZE, OVERVIEW_SIZE);
        overview_shown = pixmap_new(OVERVIEW_SIZE, OVERVIEW_SIZE);
    }
    map_render_overview(overview_map->data, OVERVIEW_SIZE, OVERVIEW_SIZE,
                        gui_nearest_color(0, 0, 0, 0x40, 0xff));

    /* the marker uses interface colours, which night does not darken */
    overview_marker_color = gui_nearest_color(63, 8, 8, 0x40, 0xff);
    overview_marker_edge = gui_nearest_color(63, 63, 63, 0x40, 0xff);
    if (leader) {
        /* character coordinates are half map pixels; tiles are 32 */
        overview_marker_x = leader->x * 2 / 32 * OVERVIEW_SIZE
            / (MAP_WIDTH * 4);
        overview_marker_y = leader->y * 2 / 32 * OVERVIEW_SIZE
            / (MAP_HEIGHT * 4);
    } else {
        overview_marker_x = overview_marker_y = -100;
    }

    snprintf(region, sizeof(region), "%s", game_map_name(map_get_id()));
    text[0] = region;
    text[1] = "";
    text[2] = "The cross";
    text[3] = "marks you.";
    text[4] = "";
    text[5] = "Dark parts:";
    text[6] = "unexplored.";
    text[7] = "";
    text[8] = "Tab. Close";

    gui_clear();
    draw_scroll(196 + (screen_width - SCREEN_WIDTH) / 2, OVERVIEW_Y, 12, 9,
                text);
    overview_frame = 0;
    dialog_map_draw_marker(1);
}


/* blink the marker */
static void
dialog_map_frame(void)
{
    overview_frame = (overview_frame + 1) % 8;
    if (overview_frame == 0 || overview_frame == 5)
        dialog_map_draw_marker(overview_frame == 0);
}


void
dialog_map_key(int key)
{
    if (key == KEY_TAB || key == 'x')
        quit_menu();
}



/*
  journal (J): every entry laid out as lines of a full screen scroll,
  newest at the bottom; Up/Down, PgUp/PgDn, Home/End scroll
*/

#define JOURNAL_WIDTH 36
#define JOURNAL_ROWS  21        /* text rows; then a gap and the key help */

static char **journal_lines = NULL;
static int journal_lines_num = 0;
static int journal_lines_max = 0;
static int journal_top = 0;


static void
journal_add_line(const char *line, int len)
{
    char *copy;

    if (journal_lines_num == journal_lines_max) {
        journal_lines_max = journal_lines_max ? 2 * journal_lines_max : 512;
        journal_lines = realloc(journal_lines,
                                journal_lines_max * sizeof(char *));
        if (journal_lines == NULL) {
            fprintf(stderr, "lotr: out of memory for the journal\n");
            exit(1);
        }
    }
    copy = lotr_malloc(len + 1);
    memcpy(copy, line, len);
    copy[len] = 0;
    journal_lines[journal_lines_num++] = copy;
}


/* word wrap text to the journal width; '\n' starts a new line */
static void
journal_wrap(const char *text, int indent)
{
    char line[JOURNAL_WIDTH + 1];
    int width = JOURNAL_WIDTH - indent;
    int len, n;

    while (*text) {
        if (*text == '\n') {
            ++text;
            continue;
        }

        /* longest run of words that fits, stopping at a line break */
        len = 0;
        n = 0;
        while (text[n] && text[n] != '\n' && n <= width) {
            if (text[n] == ' ')
                len = n;
            ++n;
        }
        if (!text[n] || text[n] == '\n' || n <= width)
            len = n;            /* the rest fits */
        else if (len == 0)
            len = width;        /* one very long word */
        while (len > 1 && text[len - 1] == ' ')
            --len;

        memset(line, ' ', indent);
        memcpy(line + indent, text, len);
        journal_add_line(line, indent + len);

        text += len;
        while (*text == ' ')
            ++text;
    }
}


/*
  paragraph num of para.dat as plain text: its lines joined, with '\n'
  where an indented line starts a new paragraph; NULL if there is none
  (gui_paragraph_scroll() turns line ends into 0, so accept both)
*/

static char *
gui_paragraph_plain(int num)
{
    char *out;
    int pos, end, n = 0;
    int line_start = 1;
    int spaces;

    if (paragraphs == NULL || num < 0 || num > PARAGRAPHS_NUM)
        return NULL;

    pos = paragraphs_off[num];
    for (end = pos; paragraphs[end] != 0x10; ++end)
        ;
    out = lotr_malloc(end - pos + 2);

    while (pos < end) {
        Uint8 c = paragraphs[pos];

        if (c == 0x0d || c == 0 || c == 0x05) {
            line_start = 1;
            ++pos;
            continue;
        }
        if (c == 0x0a) {
            ++pos;
            continue;
        }
        if (line_start) {
            for (spaces = 0; paragraphs[pos + spaces] == ' '; ++spaces)
                ;
            pos += spaces;
            while (n > 0 && out[n - 1] == ' ')
                --n;            /* lines often end in a blank */
            if (n > 0)
                out[n++] = spaces >= 2 ? '\n' : ' ';
            line_start = 0;
            continue;
        }
        out[n++] = (c >= 0x20 && c < 0x7f) ? c : ' ';
        ++pos;
    }
    out[n] = 0;
    return out;
}


static void
journal_build_lines(void)
{
    const JournalEntry *e;
    char line[JOURNAL_WIDTH + 1];
    char *text;
    int region = -2;
    int i, n;

    for (i = 0; i < journal_lines_num; ++i)
        free(journal_lines[i]);
    journal_lines_num = 0;

    for (i = 0; i < journal_size(); ++i) {
        e = journal_entry(i);

        /* a heading whenever the story moves to another region */
        if (e->map / 2 != region / 2 || region == -2) {
            if (journal_lines_num > 0)
                journal_add_line("", 0);
            n = snprintf(line, sizeof(line), "-- %s --",
                         e->map >= 0 ? game_map_name(e->map) : "?");
            journal_add_line(line, n);
            region = e->map;
        }
        journal_add_line("", 0);

        switch (e->type) {
            case JOURNAL_PARAGRAPH:
                text = gui_paragraph_plain(e->paragraph);
                if (text) {
                    journal_wrap(text, 0);
                    free(text);
                } else {
                    n = snprintf(line, sizeof(line), "(Paragraph %d)",
                                 e->paragraph);
                    journal_add_line(line, n);
                }
                break;

            case JOURNAL_TALK:
                if (e->topic[0])
                    n = snprintf(line, sizeof(line), "%.19s, about %.12s:",
                                 e->speaker, e->topic);
                else
                    n = snprintf(line, sizeof(line), "%.19s:", e->speaker);
                journal_add_line(line, n);
                journal_wrap(e->text, 2);
                break;

            default:
                journal_wrap(e->text, 0);
                break;
        }
    }

    if (journal_lines_num == 0) {
        journal_add_line("Your journal is empty.", 22);
        journal_add_line("", 0);
        journal_wrap("Story paragraphs, what people tell you and "
                     "important events will be written here.", 0);
    }
}


static void
dialog_journal_draw(void)
{
    const char *text[JOURNAL_ROWS + 2];
    int i;

    for (i = 0; i < JOURNAL_ROWS; ++i)
        text[i] = journal_top + i < journal_lines_num
            ? journal_lines[journal_top + i] : "";
    text[JOURNAL_ROWS] = "";
    text[JOURNAL_ROWS + 1] = "Arrows PgUp PgDn scroll   X. Exit";

    draw_scroll(0, 0, -JOURNAL_WIDTH, JOURNAL_ROWS + 2, text);
}


static void
dialog_journal_scroll_to(int top)
{
    if (top > journal_lines_num - JOURNAL_ROWS)
        top = journal_lines_num - JOURNAL_ROWS;
    if (top < 0)
        top = 0;
    journal_top = top;
}


void
dialog_journal_show(void)
{
    dialog_mode = DIALOG_JOURNAL;
    dialog_confirm = 0;

    journal_build_lines();
    dialog_journal_scroll_to(journal_lines_num);    /* newest */

    gui_clear();
    game_draw_map();
    dialog_journal_draw();
}


void
dialog_journal_key(int key)
{
    if (key == 'x' || key == 'j') {
        quit_menu();
        return;
    }

    if (key == KEY_UP)
        dialog_journal_scroll_to(journal_top - 1);
    else if (key == KEY_DOWN)
        dialog_journal_scroll_to(journal_top + 1);
    else if (key == KEY_PAGEUP)
        dialog_journal_scroll_to(journal_top - (JOURNAL_ROWS - 1));
    else if (key == KEY_PAGEDOWN)
        dialog_journal_scroll_to(journal_top + (JOURNAL_ROWS - 1));
    else if (key == KEY_HOME)
        dialog_journal_scroll_to(0);
    else if (key == KEY_END)
        dialog_journal_scroll_to(journal_lines_num);
    else
        return;

    dialog_journal_draw();
}



/*
  parse options menu keyboard input
*/



void
dialog_options_key(int key)
{

    const char *paused_text[7] = { "Paused...", "", "", "", "", "", "" };


    if (dialog_confirm) {
        if (key == 'y')
            exit(0);
        if (key == 'n' || key == 'x')
            quit_menu();
    }

    switch (key) {
        case 'x':
            quit_menu();
            break;

        case 'p':
            dialog_options_message_show(paused_text);
            break;

        case 's':
            dialog_save_show();
            break;

        case 'l':
            dialog_load_show();
            break;

        case 'm':
            toggle_music();
            quit_menu();
            break;

        case 'c':
            dialog_settings_show();
            break;

        case 'q':
            gui_confirm_scroll("Quit Game");
            dialog_confirm = 1;

            break;

        default:
            break;

    }

}



/*
  parse save/load menu keyboard input
*/



void
dialog_saveload_key(int key)
{
    int n;

    const char *saved_text[7] =
        { "Save current game,", "done.", "", "", "", "", "" };
    const char *loaded_text[7] =
        { "Load previous game,", "done.", "", "", "", "", "" };
    const char *failed_text[7] =
        { "Save current game,", "failed!", "", "", "", "", "" };

    if (key == 'x')
        quit_menu();

    n = 0;
    if (key >= '1' && key <= '9')
        n = key - '0';
    if (dialog_mode == DIALOG_LOAD && key == 'q')
        n = GAME_SLOT_QUICK;
    if (dialog_mode == DIALOG_LOAD && key == 'a')
        n = GAME_SLOT_AUTO;

    if (n) {

#if PIXEL_PRECISE
        if (n > 2)
            return;
#endif
        if (dialog_mode == DIALOG_SAVE) {
#if !PIXEL_PRECISE
            /* clear the wide save list from under the message */
            gui_clear();
            game_draw_map();
#endif
            if (game_save(n))
                dialog_options_message_show(saved_text);
            else
                dialog_options_message_show(failed_text);
            return;
        }
        if (dialog_mode == DIALOG_LOAD) {
            if (game_load(n)) {
                quit_menu();
                dialog_options_message_show(loaded_text);
            }
            return;
        }
    }
}




/*
  show view dialog
 */


void
dialog_view_show(void)
{
    const char *text[7];
    char lines[7][25];
    int name_len;

    int i;

    main_menu_show();
    dialog_mode = DIALOG_VIEW;

#if PIXEL_PRECISE
    snprintf(lines[0], sizeof(lines[0]), "                    ");
#else
    snprintf(lines[0], sizeof(lines[0]), "<                  >");
#endif
    name_len = strlen(choosed_character->name);
    memcpy(lines[0] + (20 - name_len) / 2,
           choosed_character->name, name_len);


    snprintf(lines[1], sizeof(lines[0]), " Silver:%d", game_get_silver());
    lines[2][0] = 0;
    snprintf(lines[3], sizeof(lines[0]), " Dex.:%-4d Str.:%-4d",
             choosed_character->dex, choosed_character->str);
    snprintf(lines[4], sizeof(lines[0]), " End.:%-4d Luck:%-4d",
             choosed_character->end, choosed_character->luck);
    snprintf(lines[5], sizeof(lines[0]), " Life:%-4d Will:%-4d",
             choosed_character->life, choosed_character->will);
    snprintf(lines[6], sizeof(lines[0]), "       eXit");

    for (i = 0; i < 7; ++i)
        text[i] = lines[i];

    draw_scroll(0, 0, -DEFAULT_SCROLL_WIDTH, 7, text);

}




/*
  parse view keyboard input
*/


void
dialog_view_key(int key)
{

#if !PIXEL_PRECISE
    if (key == KEY_LEFT || key == KEY_RIGHT) {
        if (key == KEY_LEFT)
            choose_prev_character();
        if (key == KEY_RIGHT)
            choose_next_character();

        dialog_view_show();
        return;
    }
#endif

    if (key == KEY_ENTER || key == ' ' || key == 'x')
        main_menu_show();
}




/*
  draw list dialog
 */

static int dialog_list_offset;
static int dialog_list_can_change_character;
static int dialog_list_num;
static int dialog_list_codes[10];
static char *dialog_list_name;
static char *dialog_list_names[10];
static int dialog_list_can_scroll;

void
dialog_list_draw(int can_change_character)
{
    const char *text[20];
    char lines[20][25];

    int i;
    int num_lines;
    int text_lines = 0;

    int index;
    int object;

    char status_char;

    dialog_list_can_change_character = can_change_character
        && !combat_get_mode();
    num_lines = 5;

#if !PIXEL_PRECISE
    if (dialog_list_can_change_character && choosed_character) {
        int name_len = strlen(choosed_character->name);
        snprintf(lines[text_lines], sizeof(lines[0]), "<                  >");
        memcpy(lines[text_lines++] + (20 - name_len) / 2,
                choosed_character->name, name_len);
        strncpy(lines[text_lines++], "", 20);
    }

    if (dialog_list_num > 5)
        num_lines = dialog_list_num;
#endif

    /* Do not write dialog title if first line is used for something else */
    if (text_lines == 0)
        strncpy(lines[text_lines++], dialog_list_name, 20);

    for (i = 0; i < num_lines; ++i)
        if (dialog_list_offset + i < dialog_list_num) {
            index = dialog_list_offset + i;
            status_char = ' ';
            if (dialog_mode == DIALOG_USE || dialog_mode == DIALOG_DISCARD
                || dialog_mode == DIALOG_TRADE) {
                object = choosed_character->items[index];

                if (choosed_character->item_used[index]) {
                    if (object_is_ring(object)
                        && choosed_character->ring_mode == 2)
                        status_char = '!';
                    if (object_is_armour(object))
                        status_char = '&';
                    if (object_is_shield(object))
                        status_char = '^';
                    if (object_is_weapon(object))
                        status_char = '*';
                }
            }
            snprintf(lines[text_lines++], sizeof(lines[0]), "%d.%c%s", (i == 9) ? 0 : i + 1,
                     status_char, dialog_list_names[index]);
        } else {
            strncpy(lines[text_lines++], "", 20);
        }

    dialog_list_can_scroll = dialog_list_num > num_lines;

    if (dialog_list_can_scroll)
        strncpy(lines[text_lines++], "   UP DOWN eXit", 20);
    else
        strncpy(lines[text_lines++], "       eXit", 20);

    for (i = 0; i < text_lines; ++i)
        text[i] = lines[i];

    draw_scroll(0, 0, -DEFAULT_SCROLL_WIDTH, text_lines, text);

}





/*
  show attack dialog
 */
void
dialog_attack_show(void)
{
    int i;

    main_menu_show();
    dialog_mode = DIALOG_ATTACK;

    dialog_list_num = 0;

    if (combat_get_mode())
        dialog_list_num =
            combat_in_bow_range(choosed_character, dialog_list_codes);

    if (dialog_list_num == 0)
        dialog_list_num =
            map_npc_stay_near(choosed_character, dialog_list_codes, 0);

    if (dialog_list_num <= 0) {
        main_menu_show();
        return;
    }

    for (i = 0; i < dialog_list_num; ++i)
        dialog_list_names[i] = character_get(dialog_list_codes[i])->name;


    dialog_list_name = "ATTACK";
    dialog_list_offset = 0;

    dialog_list_draw(TRUE);
}





/*
  show skills dialog
 */
void
dialog_skill_show(void)
{
    int i;

    main_menu_show();
    dialog_mode = DIALOG_SKILL;

    dialog_list_num = choosed_character->skills_num;

    for (i = 0; i < dialog_list_num; ++i) {
        dialog_list_codes[i] = choosed_character->skills[i];
        dialog_list_names[i] = object_name(dialog_list_codes[i]);
    }

    dialog_list_name = "SKILL";
    dialog_list_offset = 0;

    dialog_list_draw(TRUE);

}





/*
  show magic dialog
 */


void
dialog_magic_show(void)
{
    int i;

    main_menu_show();
    dialog_mode = DIALOG_MAGIC;

    dialog_list_num = choosed_character->spells_num;

    for (i = 0; i < dialog_list_num; ++i) {
        dialog_list_codes[i] = i;
        dialog_list_names[i] = object_name(choosed_character->spells[i]);
    }

    dialog_list_name = "MAGIC";
    dialog_list_offset = 0;

    dialog_list_draw(TRUE);

}



/*
  show use dialog
 */


void
dialog_use_main_show(void)
{

    main_menu_show();
    dialog_mode = DIALOG_USE_MAIN;

    dialog_list_num = 3;

    dialog_list_codes[0] = 1;
    dialog_list_names[0] = "Discard";
    dialog_list_codes[1] = 2;
    dialog_list_names[1] = "Equip/Use";
    dialog_list_codes[2] = 3;
    dialog_list_names[2] = "Trade";

    dialog_list_name = "USE";
    dialog_list_offset = 0;

    dialog_list_draw(TRUE);

}



/*
  show discard dialog
 */


void
dialog_discard_show(void)
{
    int i;

    main_menu_show();
    dialog_mode = DIALOG_DISCARD;

    dialog_list_num = choosed_character->items_num;

    for (i = 0; i < dialog_list_num; ++i) {
        dialog_list_codes[i] = i;
        dialog_list_names[i] = object_name(choosed_character->items[i]);
    }

    dialog_list_name = "DISCARD";
    dialog_list_offset = 0;

    dialog_list_draw(TRUE);

}




/*
  show discard confirm dialog
 */

int dialog_what_discard;

void
dialog_discard_confirm_show(int what)
{

    char text[25];

    if (object_is_ring(choosed_character->items[what])) {
        dialog_discard_show();
        return;
    }

    main_menu_show();
    dialog_mode = DIALOG_DISCARD_CONFIRM;

    dialog_what_discard = what;

    snprintf(text, sizeof(text), "Discard %s?", object_name(choosed_character->items[what]));
    gui_confirm_scroll(text);

}



/*
  parse discard confirm keyboard input
*/

void
dialog_discard_confirm_key(int key)
{
    int what;

    if (key == 'y') {
        what = choosed_character->items[dialog_what_discard];

        character_discard_item(choosed_character, dialog_what_discard);

        if (what == OBJECT_TORCH)
            game_check_light();

        if (spot_action(game_get_actual_spot(), SPOT_ACTION_DISCARD,
                        dialog_what_discard, 0xff))
            return;

        dialog_discard_show();
    }
    if (key == 'x' || key == 'n')
        dialog_discard_show();
}





/*
  show use dialog
 */


void
dialog_use_show(void)
{
    int i;

    main_menu_show();
    dialog_mode = DIALOG_USE;

    dialog_list_num = choosed_character->items_num;

    for (i = 0; i < dialog_list_num; ++i) {
        dialog_list_codes[i] = i;
        dialog_list_names[i] = object_name(choosed_character->items[i]);
    }

    dialog_list_name = "EQUIP/USE";
    dialog_list_offset = 0;

    dialog_list_draw(TRUE);

}



/*
  show get dialog
 */


Uint8 *dialog_get_objects;
int dialog_get_to_buy;
char dialog_buy_texts[11][20];


void
dialog_get_show(void)
{
    int i;
    CommandSpot *spot;

    main_menu_show();

    if (!(spot = game_get_actual_spot()))
        return;

    if (!(dialog_get_objects = spot_get_objects(spot, &dialog_get_to_buy)))
        return;

    dialog_mode = DIALOG_GET;

    for (i = 0; dialog_get_objects[i] != 0xff && i < 10; ++i) {
        dialog_list_codes[i] = i;
        if (dialog_get_to_buy) {
            snprintf(dialog_buy_texts[i], sizeof(dialog_buy_texts[0]),
                     "%-14s%d", object_name(dialog_get_objects[i]),
                     object_price(dialog_get_objects[i]));
            dialog_list_names[i] = dialog_buy_texts[i];
        } else
            dialog_list_names[i] = object_name(dialog_get_objects[i]);
    }

    dialog_list_num = i;

    if (dialog_get_to_buy) {
        snprintf(dialog_buy_texts[10], sizeof(dialog_buy_texts[0]), "Silver Pennies:%d", game_get_silver());
        dialog_list_name = dialog_buy_texts[10];
    } else
        dialog_list_name = "TAKE ITEM(S)";
    dialog_list_offset = 0;

    dialog_list_draw(TRUE);

}




/*
  show trade to who dialog
 */


void
dialog_trade_to_show(void)
{

    int i, j;

    if (choosed_character->items_num == 0) {
        dialog_use_main_show();
        return;
    }

    if (choosed_character->life > 0) {
        dialog_list_num =
            map_npc_stay_near(choosed_character, dialog_list_codes, 1);
    } else {
        /* trading from dead character */
        dialog_list_num = game_get_party(dialog_list_codes);

        if (dialog_list_num <= 1) {
            gui_died_show
                ("Entire party is dead. Sauron has finally recovered the Ring. Your quest is Over.",
                 TRUE);
            return;
        }

        /* do not show dead characters */

        for (i = 0, j = 0; i < dialog_list_num; ++i)
            if (character_get(dialog_list_codes[i])->life > 0)
                dialog_list_codes[j++] = dialog_list_codes[i];

        dialog_list_num = j;

    }

    if (dialog_list_num <= 0) {
        dialog_use_main_show();
        return;
    }

    for (i = 0; i < dialog_list_num; ++i)
        dialog_list_names[i] = character_get(dialog_list_codes[i])->name;

    main_menu_show();
    dialog_mode = DIALOG_TRADE_TO;

    dialog_list_name = "TRADE TO?";
    dialog_list_offset = 0;

    dialog_list_draw(FALSE);

}



/*
  show trade dialog
 */

int dialog_trade_to_who;

void
dialog_trade_show(int to_who)
{
    int i;

    main_menu_show();
    dialog_mode = DIALOG_TRADE;
    dialog_trade_to_who = to_who;

    dialog_list_num = choosed_character->items_num;

    for (i = 0; i < dialog_list_num; ++i) {
        dialog_list_codes[i] = i;
        dialog_list_names[i] = object_name(choosed_character->items[i]);
    }

    dialog_list_name = "TRADE";
    dialog_list_offset = 0;

    dialog_list_draw(TRUE);

}



/*
  show trade confirm dialog
 */

int dialog_what_trade;

void
dialog_trade_confirm_show(int what)
{

    char text[64];

    main_menu_show();
    dialog_mode = DIALOG_TRADE_CONFIRM;

    dialog_what_trade = what;

    snprintf(text, sizeof(text), "Trade %s to %s?",
             object_name(choosed_character->items[what]),
             character_get(dialog_trade_to_who)->name);

    gui_confirm_scroll(text);

}



/*
  parse trade confirm keyboard input
*/

void
dialog_trade_confirm_key(int key)
{
    Character *character;
    int what;

    if (key == 'y') {
        character = character_get(dialog_trade_to_who);

        what = choosed_character->items[dialog_what_trade];
        character_add_item(character, what);
        character_discard_item(choosed_character, dialog_what_trade);

        if (what == OBJECT_TORCH)
            game_check_light();

        if (spot_action(game_get_actual_spot(), SPOT_ACTION_TRADE,
                        what, dialog_trade_to_who))
            return;

        dialog_trade_show(dialog_trade_to_who);

    }
    if (key == 'x' || key == 'n')
        dialog_trade_show(dialog_trade_to_who);
}






/*
  show talk dialog
 */

void
dialog_talk_main_show(void)
{

    main_menu_show();
    dialog_mode = DIALOG_TALK_MAIN;

    dialog_list_num = 3;

    dialog_list_codes[0] = 1;
    dialog_list_names[0] = "Recruit";
    dialog_list_codes[1] = 2;
    dialog_list_names[1] = "Dismiss";
    dialog_list_codes[2] = 3;
    dialog_list_names[2] = "Talk";

    dialog_list_name = "TALK";
    dialog_list_offset = 0;

    dialog_list_draw(FALSE);

}



/*
  show recruit dialog
 */


void
dialog_recruit_show(void)
{

    int i;

    dialog_list_num =
        map_npc_stay_near(choosed_character, dialog_list_codes, 0);

    if (dialog_list_num <= 0) {
        dialog_talk_main_show();
        return;
    }

    for (i = 0; i < dialog_list_num; ++i)
        dialog_list_names[i] = character_get(dialog_list_codes[i])->name;

    main_menu_show();
    dialog_mode = DIALOG_RECRUIT;

    dialog_list_name = "RECRUIT WHO?";
    dialog_list_offset = 0;

    dialog_list_draw(FALSE);

}




/*
  show dismiss dialog
 */


void
dialog_dismiss_show(void)
{

    int i;
    Character *leader;
    Character *character;
    int n;
    int codes[10];

    leader = game_get_leader();
    n = game_get_party(codes);

    for (i = 0, dialog_list_num = 0; i < n; ++i) {
        character = character_get(codes[i]);
        if (character != leader && character != choosed_character) {
            dialog_list_names[dialog_list_num] = character->name;
            dialog_list_codes[dialog_list_num] = character->id;
            dialog_list_num++;
        }
    }

    if (dialog_list_num <= 0) {
        dialog_talk_main_show();
        return;
    }


    main_menu_show();
    dialog_mode = DIALOG_DISMISS;

    dialog_list_name = "DISMISS WHO?";
    dialog_list_offset = 0;

    dialog_list_draw(FALSE);

}



/*
  show talk to who dialog
 */


void
dialog_talk_who_show(void)
{

    int i;

    dialog_list_num =
        map_npc_stay_near(choosed_character, dialog_list_codes, 0);

    if (dialog_list_num <= 0) {
        dialog_talk_main_show();
        return;
    }

    for (i = 0; i < dialog_list_num; ++i)
        dialog_list_names[i] = character_get(dialog_list_codes[i])->name;

    main_menu_show();
    dialog_mode = DIALOG_TALK_WHO;

    dialog_list_name = "TALK TO WHO?";
    dialog_list_offset = 0;

    dialog_list_draw(FALSE);

}



/*
  show talk dialog
 */


Character *dialog_questioned_character;
int dialog_talk_character_text;
int dialog_talk_character_id;

/* for the journal: keyword of the question being answered, and whether
   the answer is just the character's reply to an unknown question */
static char dialog_talk_topic[24];
static int dialog_talk_default_answer;

void
dialog_talk_show(int character_id, int text_id)
{

    Character *character;
    Character *tmpcharacter;
    char *tmptext;
    const char *formatted_text[MAX_MESSAGELINES];
    const char *scroll_text[7];
    char *text;
    int lines;
    int i;

    /* character_id == -1 for wizard mode */

    dialog_talk_character_text = text_id;
    dialog_talk_character_id = character_id;

    if (character_id != -1) {
        wizard_mode = 0;

        character = character_get(character_id);

#ifdef DEMO
        /* there are not texts in the Demo */
        text =
            "Hullo Mr. Frodo. This demo is no fun. Please buy a full game and kick some Nazgul ass.";
#else
        if (character->texts[0] == 0 && character->texts[1] <= 0) {
            /* character has nothing to say */
            dialog_talk_who_show();
            return;
        }

        if (text_id >= 0x100) {
            text = "I don't know any more than you do about that.";
        } else {
            if (text_id < 0)
                text_id = (Uint8)character->texts[0];
#ifdef EXTENDED
            if (character->pythontexts)
                text = pythonspot_get_text(text_id);
            else
#endif
                text = game_get_text(text_id);

            if (!dialog_talk_default_answer)
                journal_add_talk(character->name,
                                 dialog_talk_topic[0] ? dialog_talk_topic
                                 : NULL, text);
        }
        dialog_talk_topic[0] = 0;
        dialog_talk_default_answer = 0;
#endif
    } /* character_id != -1 */
    else {
#ifdef WIZARD_MODE
        character = character_get(0xa5);        /* Gandalf */
        if (text_id < 0)
            text = "Wizard mode. Cast a spell.";
        else
            text = wizard_text;
        wizard_mode = 1;
#else
        character = character_get(character_id);        /* error */
        text = "";
#endif
    }

    tmpcharacter = choosed_character;
    choosed_character = character;
    main_menu_show();
    choosed_character = tmpcharacter;
    dialog_mode = DIALOG_TALK;

    tmptext = format_text(text, &lines, DEFAULT_SCROLL_WIDTH, formatted_text);

    if (lines > 6)
        lines = 6;

    for (i = 0; i < 6; ++i) {
        if (i < lines)
            scroll_text[i] = formatted_text[i];
        else
            scroll_text[i] = "";
    }

    if (!wizard_mode &&
        ((Uint8)character->texts[1] == 0xff ||
         (dialog_talk_character_text >= 0
          && dialog_talk_character_text < 0x100
          && spot_has_action(game_get_actual_spot(), SPOT_ACTION_QUESTION,
                             dialog_talk_character_text,
                             dialog_talk_character_id))
        )) {
        scroll_text[6] = "       eXit";
        dialog_questioned_character = NULL;
    } else {
        dialog_questioned_character = character;
        scroll_text[6] = "   Question eXit";
    }

    draw_scroll(0, 0, -DEFAULT_SCROLL_WIDTH, 7, scroll_text);

    free(tmptext);

}


/*
  show question dialog
 */

char dialog_question_text[DEFAULT_SCROLL_WIDTH + 1];
int dialog_question_len;
const char *dialog_question_scroll_text[7];
int dialog_cursor;

void
dialog_question_show()
{

    Character *tmpcharacter;

    tmpcharacter = choosed_character;
    choosed_character = dialog_questioned_character;
    main_menu_show();
    choosed_character = tmpcharacter;
    dialog_mode = DIALOG_QUESTION;

    dialog_question_len = 0;
    dialog_question_text[0] = 0;
    dialog_cursor = 0;

    dialog_question_scroll_text[0] = "ABOUT WHAT?";
    dialog_question_scroll_text[1] = dialog_question_text;
    dialog_question_scroll_text[2] = "";
    dialog_question_scroll_text[3] = "";
    dialog_question_scroll_text[4] = "";
    dialog_question_scroll_text[5] = "";
    dialog_question_scroll_text[6] = "";

    draw_scroll(0, 0, -DEFAULT_SCROLL_WIDTH, 7, dialog_question_scroll_text);
}



/*
  parse question keyboard input
*/


void
dialog_question_key(int key)
{

    char *texts;
    int answer, ans;
    int i, j, len;
    int matched = 0;
#ifdef WIZARD_MODE
    int reg;
#endif

    if (key == KEY_BACKSPACE)
        dialog_question_len--;

    if (key == KEY_ENTER) {

#ifdef WIZARD_MODE
        if (!wizard_mode) {
#endif
            if (dialog_question_len == 0) {
                dialog_talk_who_show();
                return;
            }

            dialog_question_text[dialog_question_len] = 0;
            answer = 0x100;
            texts = dialog_questioned_character->texts + 1;
            i = 0;
            while (spot_question_letter(texts[i]) || texts[i] == 0) {

                len = 0;
                while (spot_question_letter(texts[i + len]) && len < 20)
                    ++len;

                if (len == 20) {
                    j = i + len;
                    --len;
                } else {
                    if (texts[i + len] == 0)
                        j = i + len + 2;
                    else
                        j = i + len + 1;
                }

                ans = (Uint8)texts[j - 1];

                if (len == 0)
                    answer = ans;

                if (!strncmp(texts + i, dialog_question_text, len)) {
                    answer = ans;
                    matched = len > 0;  /* len 0 is the default reply */
                    break;
                }

                i = j;
            }

            /* a keyword matched: note it as the topic */
            if (matched) {
                snprintf(dialog_talk_topic, sizeof(dialog_talk_topic),
                         "%.*s", len, texts + i);
                dialog_talk_default_answer = 0;
            } else {
                dialog_talk_topic[0] = 0;
                dialog_talk_default_answer = 1;
            }
            dialog_talk_show(dialog_questioned_character->id, answer);
#ifdef WIZARD_MODE
        } else {
            int spell[10];
            char spell_type = 0;
            int spell_size;

            if (dialog_question_len == 0) {
                quit_menu();
                return;
            }

            spell_size = 0;
            spell_type = dialog_question_text[0];

            i = 1;
            while (i < dialog_question_len) {
                while (dialog_question_text[i] == ' '
                       && i < dialog_question_len)
                    ++i;
                if (i < dialog_question_len) {
                    spell[spell_size] = 0;
                    while (dialog_question_text[i] != ' '
                           && i < dialog_question_len) {
                        if (dialog_question_text[i] >= '0'
                            && dialog_question_text[i] <= '9')
                            spell[spell_size] =
                                0x10 * spell[spell_size] +
                                dialog_question_text[i] - '0';
                        else if (dialog_question_text[i] >= 'A'
                                 && dialog_question_text[i] <= 'F')
                            spell[spell_size] =
                                0x10 * spell[spell_size] +
                                dialog_question_text[i] - 'A' + 10;
                        else
                            goto spell_foul;

                        ++i;
                    }
                    spell_size++;
                }
            }

            snprintf(wizard_text, sizeof(wizard_text), "Spell casted.");


            switch (spell_type) {

#ifdef TTT
                case 'B':
                    map_enter_building(spell[0]);
                    game_leader_teleport(0, 16 * 4 * 4, 16 * 4 * 4, 0xff,
                                         0xff);
                    break;
#endif

                case 'E':
                    reg = !game_get_register(spell[0]);
                    snprintf(wizard_text, sizeof(wizard_text), "reg: %d", reg);
                    game_set_register(spell[0], reg);
                    break;

                case 'H':
                    snprintf(wizard_text, sizeof(wizard_text),
                             "Building, Help, Location, Teleport, Map, Day, Night, Spots, Get, Recruit, Kill, rEg, Paragraph");
                    break;

                case 'L':
                    snprintf(wizard_text, sizeof(wizard_text), "loc: %x %x",
                             game_get_leader()->x, game_get_leader()->y);
                    break;

                case 'M':
                    quit_menu();
                    game_leader_teleport(0, 0xffff, 0xffff, 0xff, spell[0]);
                    return;

                case 'T':
                    game_leader_teleport(0, spell[0], spell[1], 0xff, 0xff);
                    break;

                case 'D':
                    map_set_frame(0x400);
                    break;

                case 'N':
                    map_set_frame(0x0);
                    break;

                case 'S':
                    gui_show_spots = !gui_show_spots;
                    break;


                case 'P':
                    /* read hex number as dec */
                    j = spell[0] % 16;
                    spell[0] /= 16;
                    j += (spell[0] % 16) * 10;
                    spell[0] /= 16;
                    j += (spell[0] % 16) * 100;
                    quit_menu();
                    gui_paragraph(j);
                    return;

                case 'R':
                    map_character_teleport(character_get(spell[0]), 0,
                                           game_get_leader()->x,
                                           game_get_leader()->y, 0xff,
                                           game_get_leader()->map);
                    game_recruit(character_get(spell[0]), 1);
                    break;

                case 'G':
                    if (spell[0] == OBJECT_SILVER)
                        game_add_silver(1000);
                    if (object_is_item(spell[0]))
                        character_add_item(game_get_leader(), spell[0]);
                    if (object_is_spell(spell[0]))
                        character_add_spell(game_get_leader(), spell[0]);
                    if (object_is_skill(spell[0]))
                        character_add_skill(game_get_leader(), spell[0]);
                    break;


                case 'K':
                    if (combat_get_mode())
                        combat_genocide();
                    else
                        spell_type = 0;
                    break;

                default:
                    spell_type = 0;
            }


            if (!spell_type) {
spell_foul:
                snprintf(wizard_text, sizeof(wizard_text),
                         "Thou art not a true wizard. Thy foul spell failed\n");
            }

            dialog_talk_show(-1, 0);
        }
#endif

    }


    if (dialog_question_len >= DEFAULT_SCROLL_WIDTH - 1)
        return;

    if (key >= 'a' && key <= 'z')
        dialog_question_text[dialog_question_len++] = key - 'a' + 'A';

    if (key >= '0' && key <= '9')
        dialog_question_text[dialog_question_len++] = key;

    if (key == ' ' || key == '\'' || key == '*' || key == '-')
        dialog_question_text[dialog_question_len++] = key;



}


/*
  parse talk keyboard input
*/
void
dialog_talk_key(int key)
{

    if (key == 'x') {
#ifdef WIZARD_MODE
        if (wizard_mode) {
            quit_menu();
            return;
        }
#endif
        if (dialog_talk_character_text >= 0
            && dialog_talk_character_text < 0x100
            && spot_action(game_get_actual_spot(), SPOT_ACTION_QUESTION,
                           dialog_talk_character_text,
                           dialog_talk_character_id))
            return;

        dialog_talk_who_show();
    }
    if (key == 'q')
        dialog_question_show();
}



/*
  show choose dialog
 */


void
dialog_choose_show(void)
{

    int i;

    dialog_list_num = game_get_party(dialog_list_codes);

    if (dialog_list_num <= 0) {
        dialog_talk_main_show();
        return;
    }

    for (i = 0; i < dialog_list_num; ++i)
        dialog_list_names[i] = character_get(dialog_list_codes[i])->name;

    main_menu_show();
    dialog_mode = DIALOG_CHOOSE;

    dialog_list_name = "CHOOSE WHO";
    dialog_list_offset = 0;

    dialog_list_draw(FALSE);

}




/*
  show leader dialog
 */


void
dialog_leader_show(void)
{

    int i;

    dialog_list_num = game_get_party(dialog_list_codes);

    if (dialog_list_num <= 0) {
        dialog_talk_main_show();
        return;
    }

    for (i = 0; i < dialog_list_num; ++i)
        dialog_list_names[i] = character_get(dialog_list_codes[i])->name;

    main_menu_show();
    dialog_mode = DIALOG_LEADER;

    dialog_list_name = "SELECT_LEADER";
    dialog_list_offset = 0;

    dialog_list_draw(FALSE);

}




/*
  parse list keyboard input
*/


void
dialog_list_key(int key)
{

    int i, item, code;
    int spell_code;
    Character *character;

#if !PIXEL_PRECISE
    if (dialog_list_can_change_character
        && (key == KEY_LEFT || key == KEY_RIGHT)) {
        int old_dialog_mode = dialog_mode;

        if (key == KEY_LEFT)
            choose_prev_character();
        if (key == KEY_RIGHT)
            choose_next_character();

        switch (old_dialog_mode) {
            case DIALOG_SKILL:
                dialog_skill_show();
                return;

            case DIALOG_MAGIC:
                dialog_magic_show();
                return;

            case DIALOG_USE_MAIN:
                dialog_use_main_show();
                return;

            case DIALOG_DISCARD:
                dialog_discard_show();
                return;

            case DIALOG_USE:
                dialog_use_show();
                return;

            case DIALOG_GET:
                dialog_get_show();
                return;

            case DIALOG_TRADE:
                dialog_trade_show(dialog_trade_to_who);
                return;

            case DIALOG_ATTACK:
                dialog_attack_show();
                return;

            case DIALOG_TRADE_TO:
            case DIALOG_TALK_MAIN:
            case DIALOG_TALK_WHO:
            case DIALOG_RECRUIT:
            case DIALOG_DISMISS:
            case DIALOG_LEADER:
            case DIALOG_CHOOSE:
            default:
                fprintf(stderr,
                        "lotr: Unexpected dialog mode %d while choosing character.\n",
                        old_dialog_mode);
        }
        return;
    }
#endif

    if (dialog_list_can_scroll && (key == 'd' || key == 'u')) {
        if (key == 'd' && dialog_list_offset + 5 < dialog_list_num) {
            dialog_list_offset++;
            dialog_list_draw(dialog_list_can_change_character);
        }

        if (key == 'u' && dialog_list_offset > 0) {
            dialog_list_offset--;
            dialog_list_draw(dialog_list_can_change_character);
        }

    }

    if (key == 'x') {
        switch (dialog_mode) {
            case DIALOG_DISCARD:
            case DIALOG_USE:
            case DIALOG_TRADE_TO:
                dialog_use_main_show();
                break;

            case DIALOG_TRADE:
                dialog_trade_to_show();
                break;

            case DIALOG_TALK_WHO:
            case DIALOG_RECRUIT:
            case DIALOG_DISMISS:
                dialog_talk_main_show();
                break;

            default:
                main_menu_show();
        }
    }

    if (key < '0' || key > '9')
        return;

    i = key - '0';
    if (i == 0)
        i = 10;

    if (dialog_list_can_scroll && i > 5)
        return;

    i += dialog_list_offset - 1;

    if (i >= dialog_list_num)
        return;

    code = dialog_list_codes[i];


    switch (dialog_mode) {

        case DIALOG_ATTACK:
            if (combat_get_mode()) {
                main_menu_show();
                combat_attack(choosed_character, combat_get_enemy(code));
            } else {
                if (spot_action
                    (game_get_actual_spot(), SPOT_ACTION_ATTACK, 0xff, code))
                    return;
            }
            break;

        case DIALOG_SKILL:
            if (code == 0x48)   /*climb */
                map_set_climb_mode(1);
            else
                map_set_climb_mode(0);
            if (spot_action
                (game_get_actual_spot(), SPOT_ACTION_SKILL, code, 0xff))
                quit_menu();
            else
                dialog_skill_show();
            return;

        case DIALOG_MAGIC:
            if (choosed_character->life <= 5)
                break;
            if (dialog_list_names[code][0] != '!')
                choosed_character->life -= lotr_rnd(4);

            spell_code = choosed_character->spells[code];
            if (!combat_get_mode()) {
                if (spot_has_action(game_get_actual_spot(), SPOT_ACTION_MAGIC,
                                    spell_code, 0xff)) {
                    if (dialog_list_names[code][0] == '!') {
                        for (i = code; i + 1 < choosed_character->spells_num;
                             ++i)
                            choosed_character->spells[i] =
                                choosed_character->spells[i + 1];
                        choosed_character->spells_num--;
                    }
                    music_spell_performed();
                    spot_action(game_get_actual_spot(), SPOT_ACTION_MAGIC,
                                spell_code, 0xff);
                } else {
                    quit_menu();
                    if (spell_code == SPELL_ILLUMINATE) {
                        music_spell_performed();
                        map_set_light(2);
                        return;
                    }
                    music_spell_failed();
                }
                return;
            } /* combat_get_mode */
            else {
                if (spell_code == SPELL_ILLUMINATE) {
                    music_spell_performed();
                    map_set_light(2);
                    return;
                }
            }
            main_menu_show();
            break;

        case DIALOG_USE_MAIN:
            if (code == 1)
                dialog_discard_show();
            if (code == 2)
                dialog_use_show();
            if (code == 3)
                dialog_trade_to_show();
            break;

        case DIALOG_DISCARD:
            dialog_discard_confirm_show(code);
            break;

        case DIALOG_USE:
            if (!spot_action
                (game_get_actual_spot(), SPOT_ACTION_OBJECT,
                 choosed_character->items[code], 0xff)) {
                character_use(choosed_character, code);
                dialog_use_show();
            }
            break;

        case DIALOG_GET:
            item = dialog_get_objects[code];

            if (dialog_get_to_buy
                && spot_has_action(game_get_actual_spot(), SPOT_ACTION_BUY,
                                   item, 0xff)) {
                if (game_pay_silver(object_price(item))
                    && spot_action(game_get_actual_spot(), SPOT_ACTION_BUY,
                                   item, 0xff))
                    return;
            }

            if (dialog_get_to_buy &&
                (choosed_character->items_num == 10
                 || !game_pay_silver(object_price(item))))
                break;

            if (!dialog_get_objects[code] == 0 &&
                !character_add_item(choosed_character, item) &&
                !character_add_skill(choosed_character, item))
                break;

            if (!dialog_get_to_buy) {
                for (; dialog_get_objects[code] != 0xff && code + 1 < 10;
                     ++code)
                    dialog_get_objects[code] = dialog_get_objects[code + 1];
                dialog_get_objects[code] = 0xff;

                if (item == 0) {        /* silver pennies */
                    for (; dialog_get_objects[code] == 0xff && code < 12;
                         ++code);
                    if (dialog_get_objects[code] != 0xff)
                        game_add_silver(dialog_get_objects[code]);

                }

                if (spot_action
                    (game_get_actual_spot(), SPOT_ACTION_GET, item, 0xff))
                    return;
                else
                    dialog_get_show();
            } /* if( !dialog_get_to_buy ) */
            else {
                dialog_get_show();
            }

            break;

        case DIALOG_TRADE_TO:
            dialog_trade_show(code);
            break;

        case DIALOG_TRADE:
            character = character_get(dialog_trade_to_who);
            if (game_in_party(character)) {

                item = choosed_character->items[code];

                if (object_is_ring(item) && choosed_character->life > 0)
                    break;

                if (character_add_item(character, item)) {
                    character_discard_item(choosed_character, code);

                    if (item == OBJECT_TORCH)
                        game_check_light();
                }

                if (object_is_ring(item)) {
                    character->ring_mode = 1;
                    choosed_character->ring_mode = 0;
                }

                dialog_trade_show(dialog_trade_to_who);
            } else {
                if (spot_has_action(game_get_actual_spot(), SPOT_ACTION_TRADE,
                                    choosed_character->items[code],
                                    dialog_trade_to_who))
                    dialog_trade_confirm_show(code);
                else
                    dialog_trade_show(dialog_trade_to_who);
            }
            break;

        case DIALOG_TALK_MAIN:
            if (code == 1)
                dialog_recruit_show();
            if (code == 2)
                dialog_dismiss_show();
            if (code == 3)
                dialog_talk_who_show();
            break;

        case DIALOG_RECRUIT:
            character = character_get(code);
            game_recruit(character, 0);
            dialog_recruit_show();
            break;

        case DIALOG_DISMISS:
            character = character_get(code);
            if (!character->ring_mode)
                game_dismiss(character);
            dialog_dismiss_show();
            break;

        case DIALOG_TALK_WHO:
            if (spot_action
                (game_get_actual_spot(), SPOT_ACTION_TALK, 0xff, code))
                break;
            dialog_talk_show(code, -1);
            break;

        case DIALOG_CHOOSE:
            choosed_character = character_get(code);
            main_menu_show();
            break;

        case DIALOG_LEADER:
            character = character_get(code);
            if (game_set_leader(character))
                choosed_character = character;
            main_menu_show();
            break;

        default:
            break;
    }

}



/*
  parse main menu keyboard input
*/
void
main_menu_key(int key)
{


    switch (key) {
        case 'x':
        case ' ':
            if (combat_get_mode()) {
                choosed_character->ap = 0;
                combat_character_finished();
            } else
                quit_menu();
            break;

        case 'a':
            dialog_attack_show();
            break;

        case 'c':
            if (!combat_get_mode())
                dialog_choose_show();
            break;

        case 'l':
            if (!combat_get_mode())
                dialog_leader_show();
            break;

        case 'm':
            dialog_magic_show();
            break;

#if !PIXEL_PRECISE
        case 'h':
            quit_menu();
            gui_help_show();
            break;
#endif

        case 'q':
            gui_confirm_scroll("Quit Game");
            dialog_mode = DIALOG_OPTIONS;
            dialog_confirm = 1;
            break;

        case 's':
            dialog_skill_show();
            break;

        case 't':
            if (!combat_get_mode())
                dialog_talk_main_show();
            break;

        case 'u':
            dialog_use_main_show();
            break;

        case 'g':
            if (!combat_get_mode())
                dialog_get_show();
            break;

        case 'v':
            dialog_view_show();
            break;

        case 'o':
            if (!combat_get_mode())
                dialog_options_show();
            break;

#ifdef WIZARD_MODE
        case 'w':
            if (lotr_key_ctrl())
                dialog_print_active_spot();
            else
                dialog_talk_show(-1, -1);
            break;
#endif

#if !PIXEL_PRECISE
        case KEY_LEFT:
            if (!combat_get_mode())
                choose_prev_character();
            break;

        case KEY_RIGHT:
            if (!combat_get_mode())
                choose_next_character();
            break;
#endif

        default:
            break;

    }

}








/*
  gui frame - returns nonzero if a dialog is active (game paused)
*/

/*
  mouse: turn the next click or wheel turn into a key, or into a walk
*/

static int
gui_mouse_key(void)
{
    int x, y, hit;

    switch (lotr_get_mouse(&x, &y)) {
        case LOTR_MOUSE_LEFT:
            hit = gui_clickmap_hit(x, y);
            if (hit > 0)
                return hit;
            if (dialog_mode == DIALOG_NONE) {
                if (hit < 0)
                    game_walk_to(x, y);
                return 0;
            }
            /* reading: a click anywhere goes on */
            if (dialog_mode == DIALOG_MESSAGE
                || dialog_mode == DIALOG_PARAGRAPH
                || dialog_mode == DIALOG_BOOK || dialog_mode == DIALOG_DIED)
                return ' ';
            return 0;

        case LOTR_MOUSE_RIGHT:
            /* back out; on the map it opens the menu */
            switch (dialog_mode) {
                case DIALOG_NONE:
                    return ' ';
                case DIALOG_MESSAGE:
                case DIALOG_PARAGRAPH:
                case DIALOG_BOOK:
                    return ' ';
                case MAIN_MENU:
                    /* in combat X would end the character's turn */
                    return combat_get_mode() ? 0 : 'x';
                case DIALOG_QUESTION:  /* typing */
                case DIALOG_MESSAGE_YN:
                case DIALOG_DIED:
                    return 0;
                default:
                    return 'x';
            }

        case LOTR_MOUSE_WHEEL_UP:
            return dialog_mode != DIALOG_NONE ? KEY_UP : 0;

        case LOTR_MOUSE_WHEEL_DOWN:
            return dialog_mode != DIALOG_NONE ? KEY_DOWN : 0;

        default:
            return 0;
    }
}



#ifndef DEMO
/*
  F5: save to the quicksave slot
*/

static void
gui_quicksave(void)
{
    if (!game_can_save())
        gui_message("You cannot save the game right now.", 1);
    else if (game_save(GAME_SLOT_QUICK))
        gui_message("Quicksave done.", 1);
    else
        gui_message("Quicksave failed!", 1);
}



/*
  F9: load the quicksave slot
*/

static void
gui_quickload(void)
{
    int map, leader_id;
    time_t mtime;

    if (combat_get_mode()) {
        gui_message("You cannot load a game during combat.", 1);
        return;
    }

    switch (game_save_info(GAME_SLOT_QUICK, &map, &leader_id, &mtime)) {
        case 0:
            gui_message("There is no quicksave yet. Press F5 to make one.",
                        1);
            return;
        case -1:
            gui_message("The quicksave is damaged and cannot be loaded.", 1);
            return;
        default:
            break;
    }

    if (game_load(GAME_SLOT_QUICK))
        gui_message("Quickload done.", 1);
    else
        gui_message("Quickload failed!", 1);
}
#endif



int
gui_frame(void)
{

    int key;

    if (dialog_mode == DIALOG_QUESTION) {
        dialog_question_text[dialog_question_len] =
            0x80 - 4 + abs(dialog_cursor);
        dialog_question_text[dialog_question_len + 1] = 0;

        draw_scroll(0, 0, -DEFAULT_SCROLL_WIDTH, 7,
                    dialog_question_scroll_text);
        dialog_cursor++;
        if (dialog_cursor == 4)
            dialog_cursor = -2;
    }


    if (dialog_mode == DIALOG_MAP)
        dialog_map_frame();

    key = lotr_get_key();
    if (key == 0)
        key = gui_mouse_key();

    if (key == 0 && !lotr_key_esc())
        return dialog_mode;

    if (dialog_mode == DIALOG_NONE) {

        if (lotr_key_esc()) {
#ifdef DEMO
            exit(0);
#endif
            lotr_reset_keyboard();
            dialog_options_show();
        }

        switch (key) {
            case ' ':
                main_menu_show();
                break;

            case 'a':
                dialog_attack_show();
                break;

            case 'c':
                dialog_choose_show();
                break;

            case 'l':
                dialog_leader_show();
                break;

            case 'm':
                dialog_magic_show();
                break;

#if !PIXEL_PRECISE
            case 'h':
                gui_help_show();
                break;
#endif

            case 'o':
#ifndef DEMO
                dialog_options_show();
#endif
                break;

            case 'q':
                gui_confirm_scroll("Quit Game");
                dialog_mode = DIALOG_OPTIONS;
                dialog_confirm = 1;
                break;

            case 's':
                dialog_skill_show();
                break;

            case 't':
                dialog_talk_main_show();
                break;

            case 'u':
                dialog_use_main_show();
                break;

            case 'g':
                dialog_get_show();
                break;

            case 'v':
                dialog_view_show();
                break;

            case 'j':
                dialog_journal_show();
                break;

            case KEY_TAB:
                dialog_map_show();
                break;

#ifndef DEMO
            case KEY_QUICKSAVE:
                gui_quicksave();
                break;

            case KEY_QUICKLOAD:
                gui_quickload();
                break;
#endif

#ifdef WIZARD_MODE
            case 'w':
                if (lotr_key_ctrl())
                    dialog_print_active_spot();
                else
                    dialog_talk_show(-1, -1);
                break;
#endif

            default:
                break;
        }
    } else {

        if (lotr_key_esc()) {
            if (dialog_mode == DIALOG_DIED)
                exit(0);
#ifdef DEMO
            exit(0);
#endif
            lotr_reset_keyboard();
            quit_menu();
        }

        switch (dialog_mode) {
            case MAIN_MENU:
                main_menu_key(key);
                break;

            case DIALOG_OPTIONS:
                dialog_options_key(key);
                break;

            case DIALOG_SAVE:
            case DIALOG_LOAD:
                dialog_saveload_key(key);
                break;

            case DIALOG_SETTINGS:
                dialog_settings_key(key);
                break;

            case DIALOG_JOURNAL:
                dialog_journal_key(key);
                break;

            case DIALOG_MAP:
                dialog_map_key(key);
                break;

            case DIALOG_MESSAGE:
            case DIALOG_BOOK:
            case DIALOG_DIED:
                dialog_message_key(key);
                break;

            case DIALOG_PARAGRAPH:
                dialog_paragraph_key(key);
                break;

            case DIALOG_SPOT_PRINT:
                dialog_print_active_spot_key(key);
                break;

            case DIALOG_MESSAGE_YN:
                dialog_message_yn_key(key);
                break;

            case DIALOG_VIEW:
                dialog_view_key(key);
                break;

            case DIALOG_SKILL:
            case DIALOG_MAGIC:
            case DIALOG_USE_MAIN:
            case DIALOG_DISCARD:
            case DIALOG_USE:
            case DIALOG_GET:
            case DIALOG_TRADE_TO:
            case DIALOG_TRADE:
            case DIALOG_TALK_MAIN:
            case DIALOG_TALK_WHO:
            case DIALOG_RECRUIT:
            case DIALOG_DISMISS:
            case DIALOG_LEADER:
            case DIALOG_CHOOSE:
            case DIALOG_ATTACK:
                dialog_list_key(key);
                break;

            case DIALOG_DISCARD_CONFIRM:
                dialog_discard_confirm_key(key);
                break;

            case DIALOG_TRADE_CONFIRM:
                dialog_trade_confirm_key(key);
                break;

            case DIALOG_TALK:
                dialog_talk_key(key);
                break;

            case DIALOG_QUESTION:
                dialog_question_key(key);
                break;

            default:
                break;
        }

    }

    return dialog_mode;

}


/*
  returns nonzero if a dialog is active (game paused)
*/

int
gui_mode(void)
{
    return dialog_mode;
}




/*
  returns choosed character
*/

Character *
gui_choosed(void)
{
    if (dialog_mode)
        return choosed_character;

    return game_get_leader();
}




/*
  should we show spots?
*/

int
gui_if_show_spots(void)
{
    return gui_show_spots;
}


/*
  set choose character
*/

void
gui_set_choosed(Character *choosed)
{
    choosed_character = choosed;
    main_menu_show();
}


/*
  get gui font
*/

Pixmap **
gui_get_font(void)
{
    return gui_font;
}




/*
  player is dead dialog
*/

void
gui_player_dead(Character *who, int show_message)
{

    char message[1024];
    Character *tmpchar;


    if (show_message) {
        snprintf(message, sizeof(message), "%s is dead.", who->name);
        gui_message(message, 1);

        gui_dialog_wait(DIALOG_MESSAGE);
    }

    tmpchar = choosed_character;
    who->life = 0;
    choosed_character = who;

    dialog_trade_to_show();
    while (dialog_mode == DIALOG_TRADE_TO || dialog_mode == DIALOG_TRADE) {
        lotr_poll_events();
        gui_frame();
        map_animate_frame();
        graphics_update_screen();
        lotr_timer(50);
    }

    if (who->ring_mode) {
        /* Ringbearer is dead but nobody carries the ring further */
        gui_died_show
            ("The ringbearer is dead and nobody is willing to take the Burden.",
             TRUE);
        return;
    }

    choosed_character = tmpchar;
    quit_menu();

}




/*
  proceed all gui frames
*/
void
gui_proceed_frames(void)
{
    while (gui_frame()) {
        map_animate_frame();
        graphics_update_screen();
        lotr_timer(FRAME_TIME);
        lotr_poll_events();
    }
}



#ifdef TTT

/* write a text */
void
gui_draw_text(char *text, int x, int y)
{
    int i;
    for (i = 0; text[i]; ++i)
        if (gui_font[(Uint8)text[i]])
            pixmap_draw(gui_font[(Uint8)text[i]],
                        x + SCROLL_ELEMENT_SIZE * i, y);
}


/*
  dialog when starting TTT
*/

int
gui_ttt_start_dialog(void)
{
    int key;
    int i;
    int c;
    int mode = 0;
    int savegames[10];
    int numsaves = 0;
    char buf[20];
    FILE *testfile;
    int y;

    graphics_set_background("ongame");

    /* set alpha for font */
    c = gui_font[(Uint8)' ']->data[0];
    for (i = 0; i < 0x100; ++i)
        if (gui_font[i])
            pixmap_set_alpha(gui_font[i], c);

    gui_draw_text("Old or", 30, 45);
    gui_draw_text("New Game (O/N)", 30, 45 + 8);
    graphics_update_screen();

    for (i = 0; i < 10; ++i)
        savegames[i] = 0;
    numsaves = 0;

    while (!lotr_key_esc()) {
        key = lotr_get_key();
        if (mode == 0) {
            if (key == 'n') {
                for (i = 0; i < 10; ++i) {
                    snprintf(buf, sizeof(buf), "savegame.%d", i);
                    testfile = fopen(lotr_homedir_filename(buf), "rb");
                    if (testfile != NULL) {
                        numsaves++;
                        savegames[i] = 1;
                        fclose(testfile);
                    }
                }

                if (numsaves == 0) {
                    return 0;
                } else {
                    mode = 1;
                    y = 68;
                    gui_draw_text("Convert from", 40, y);
                    y += 8;
                    gui_draw_text("which game?", 44, y);
                    y += 2;
                    for (i = 0; i < 10; ++i)
                        if (savegames[i]) {
                            snprintf(buf, sizeof(buf), "%d.Game %d", i, i);
                            y += 8;
                            gui_draw_text(buf, 52, y);
                        }
                    y += 8;
                    gui_draw_text("x.None", 52, y);
                    graphics_update_screen();
                }
            }
            if (key == 'o') {
                for (i = 0; i < 10; ++i) {
                    snprintf(buf, sizeof(buf), "savegame2.%d", i);
                    testfile = fopen(lotr_homedir_filename(buf), "rb");
                    if (testfile != NULL) {
                        numsaves++;
                        savegames[i] = 1;
                        fclose(testfile);
                    }
                }

                if (numsaves > 0) {
                    mode = 2;
                    y = 68;
                    gui_draw_text("Which Game?", 44, y);
                    for (i = 0; i < 10; ++i)
                        if (savegames[i]) {
                            snprintf(buf, sizeof(buf), "%d.Game %d", i, i);
                            y += 8;
                            gui_draw_text(buf, 52, y);
                        }
                    graphics_update_screen();
                }
            }
        }

        if (mode == 1) {
            if (key == 'x')
                return 0;
            if (key >= '0' && key <= '9')
                if (savegames[key - '0']) {
                    game_convert(key - '0');
                    return 0;
                }
        }

        if (mode == 2) {
            if (key >= '0' && key <= '9')
                if (savegames[key - '0'])
                    return key - '0';
        }
        lotr_poll_events();
        lotr_timer(50);
    }

    /* unset font alpha */
    for (i = 0; i < 0x100; ++i)
        if (gui_font[i])
            gui_font[i]->hasalpha = 0;

    lotr_reset_keyboard();
    return 0;

}


#endif
