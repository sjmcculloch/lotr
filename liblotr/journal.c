/****************************************************************************

    journal.c
    log of the story read so far


    Lord of the Rings game engine

    This code is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    The journal keeps story paragraphs, what characters said and the
    longer script messages, so the player can read them again (J). It is
    kept clean: repeats are skipped, and so are short or mechanical
    messages ("The door is locked.", "There's a trap here!").

    Paragraphs are stored by number and read from para.dat when shown;
    everything else is stored as text, because map texts are only loaded
    while their map is.

*****************************************************************************/

#include "lotr.h"
#include "journal.h"
#include "utils.h"
#include <SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* game.c */
extern int game_map_id;

static JournalEntry *entries = NULL;
static int entries_num = 0;
static int entries_max = 0;


/*
  forget everything
*/

void
journal_clear(void)
{
    int i;

    for (i = 0; i < entries_num; ++i)
        free(entries[i].text);
    entries_num = 0;
}


/*
  copy of text with only printable ASCII (the save game is ASCII and the
  font has nothing else) and no surrounding blanks
*/

static char *
journal_clean_copy(const char *text)
{
    char *copy, *d;
    size_t n;

    while (*text == ' ')
        ++text;
    n = strlen(text);
    copy = lotr_malloc(n + 1);
    for (d = copy; *text; ++text)
        *d++ = (*text >= 0x20 && *text < 0x7f) ? *text : ' ';
    while (d > copy && d[-1] == ' ')
        --d;
    *d = 0;
    return copy;
}


static JournalEntry *
journal_new_entry(int type)
{
    JournalEntry *e;

    if (entries_num == entries_max) {
        entries_max = entries_max ? 2 * entries_max : 256;
        entries = realloc(entries, entries_max * sizeof(JournalEntry));
        if (entries == NULL) {
            fprintf(stderr, "lotr: out of memory for the journal\n");
            exit(1);
        }
    }

    e = entries + entries_num++;
    memset(e, 0, sizeof(*e));
    e->type = type;
    e->map = game_map_id;
    return e;
}


/*
  is the same entry already there?
*/

static int
journal_has(int type, int paragraph, const char *speaker, const char *text)
{
    int i;

    for (i = 0; i < entries_num; ++i) {
        if (entries[i].type != type)
            continue;
        if (type == JOURNAL_PARAGRAPH) {
            if (entries[i].paragraph == paragraph)
                return 1;
        } else if (!strcmp(entries[i].text, text)
                   && (type != JOURNAL_TALK
                       || !strcmp(entries[i].speaker, speaker))) {
            return 1;
        }
    }
    return 0;
}


void
journal_add_paragraph(int num)
{
    if (num < 0 || journal_has(JOURNAL_PARAGRAPH, num, NULL, NULL))
        return;
    journal_new_entry(JOURNAL_PARAGRAPH)->paragraph = num;
}


/*
  "See paragraph number 54." and similar pointers into the printed book
*/

static int
journal_is_paragraph_pointer(const char *text)
{
    return !SDL_strncasecmp(text, "See par", 7);
}


void
journal_add_talk(const char *speaker, const char *topic, const char *text)
{
    JournalEntry *e;
    char *clean;

    clean = journal_clean_copy(text);
    if (!clean[0] || !strcmp(clean, "<null>") || !strcmp(clean, "Null")
        || journal_is_paragraph_pointer(clean)
        || journal_has(JOURNAL_TALK, 0, speaker, clean)) {
        free(clean);
        return;
    }

    e = journal_new_entry(JOURNAL_TALK);
    snprintf(e->speaker, sizeof(e->speaker), "%s", speaker);
    snprintf(e->topic, sizeof(e->topic), "%s", topic ? topic : "");
    e->text = clean;
}


/*
  short or mechanical script messages stay out of the journal: anything
  under 40 characters, and under 100 characters anything about locks,
  traps, chests or what cannot be done; longer messages are story
*/

static int
journal_is_noise(const char *text)
{
    static const char *phrases[] = {
        "lock", "trap", "chest", "you cannot", "can't", "useful item",
        "items here", "disarm", "paragraph"
    };
    char lower[100];
    size_t n = strlen(text);
    size_t i;

    if (n < 40)
        return 1;
    if (n >= 100)
        return 0;

    for (i = 0; i <= n; ++i)
        lower[i] = tolower((unsigned char)text[i]);
    for (i = 0; i < sizeof(phrases) / sizeof(phrases[0]); ++i)
        if (strstr(lower, phrases[i]))
            return 1;
    return 0;
}


void
journal_add_text(const char *text)
{
    char *clean;

    clean = journal_clean_copy(text);
    if (journal_is_noise(clean)
        || journal_has(JOURNAL_TEXT, 0, NULL, clean)) {
        free(clean);
        return;
    }
    journal_new_entry(JOURNAL_TEXT)->text = clean;
}


int
journal_size(void)
{
    return entries_num;
}


const JournalEntry *
journal_entry(int i)
{
    if (i < 0 || i >= entries_num)
        return NULL;
    return entries + i;
}


/*
  save game:
  <journal><entry><type>1</type><map>4</map><speaker>..</speaker>
  <topic>..</topic><message>..</message></entry>...</journal>
  (not <text>: libxml calls its whitespace nodes "text" too)
*/

void
journal_save(xmlNodePtr node)
{
    xmlNodePtr entry;
    int i;

    for (i = 0; i < entries_num; ++i) {
        entry = xmlNewChild(node, NULL, (const xmlChar *)"entry", NULL);
        lotr_save_prop_int(entry, "type", entries[i].type);
        lotr_save_prop_int(entry, "map", entries[i].map);
        if (entries[i].type == JOURNAL_PARAGRAPH) {
            lotr_save_prop_int(entry, "paragraph", entries[i].paragraph);
        } else {
            if (entries[i].type == JOURNAL_TALK) {
                xmlNewTextChild(entry, NULL, (const xmlChar *)"speaker",
                                (const xmlChar *)entries[i].speaker);
                xmlNewTextChild(entry, NULL, (const xmlChar *)"topic",
                                (const xmlChar *)entries[i].topic);
            }
            xmlNewTextChild(entry, NULL, (const xmlChar *)"message",
                            (const xmlChar *)entries[i].text);
        }
    }
}


/* text of child element name, "" if missing; result must be freed */
static char *
journal_load_string(xmlNodePtr node, const char *name)
{
    xmlNodePtr child;
    xmlChar *content = NULL;
    char *copy;

    for (child = xmlFirstElementChild(node); child != NULL;
         child = xmlNextElementSibling(child))
        if (!xmlStrcmp(child->name, (const xmlChar *)name)) {
            content = xmlNodeGetContent(child);
            break;
        }
    copy = journal_clean_copy(content ? (const char *)content : "");
    if (content)
        xmlFree(content);
    return copy;
}


/*
  restore the journal; damaged entries are skipped, older saves without
  a journal start with an empty one
*/

void
journal_load(xmlNodePtr node)
{
    xmlNodePtr entry;
    JournalEntry *e;
    char *s;
    int type;

    journal_clear();
    if (node == NULL)
        return;

    for (entry = xmlFirstElementChild(node); entry != NULL;
         entry = xmlNextElementSibling(entry)) {
        type = lotr_load_prop_int_default(entry, "type", -1);
        if (type != JOURNAL_PARAGRAPH && type != JOURNAL_TALK
            && type != JOURNAL_TEXT)
            continue;

        e = journal_new_entry(type);
        e->map = lotr_load_prop_int_default(entry, "map", -1);
        if (type == JOURNAL_PARAGRAPH) {
            e->paragraph = lotr_load_prop_int_default(entry, "paragraph", 0);
            continue;
        }
        if (type == JOURNAL_TALK) {
            s = journal_load_string(entry, "speaker");
            snprintf(e->speaker, sizeof(e->speaker), "%s", s);
            free(s);
            s = journal_load_string(entry, "topic");
            snprintf(e->topic, sizeof(e->topic), "%s", s);
            free(s);
        }
        e->text = journal_load_string(entry, "message");
    }
}
