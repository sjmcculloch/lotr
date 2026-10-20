/****************************************************************************

    journal.h
    log of the story read so far


    Lord of the Rings game engine

    This code is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

*****************************************************************************/


#ifndef _JOURNAL_H
#define _JOURNAL_H

#include <libxml/tree.h>

/* entry types */
#define JOURNAL_PARAGRAPH 0     /* story paragraph from para.dat, by number */
#define JOURNAL_TALK      1     /* what a character said */
#define JOURNAL_TEXT      2     /* message shown by a map script */

typedef struct {
    int type;
    int map;                    /* where it happened */
    int paragraph;              /* JOURNAL_PARAGRAPH */
    char speaker[20];           /* JOURNAL_TALK */
    char topic[24];             /* JOURNAL_TALK, empty for a greeting */
    char *text;                 /* JOURNAL_TALK, JOURNAL_TEXT */
} JournalEntry;


/* forget everything (new game) */
extern void journal_clear(void);

/* record a paragraph */
extern void journal_add_paragraph(int num);

/* record an answer of a character; topic may be NULL */
extern void journal_add_talk(const char *speaker, const char *topic,
                             const char *text);

/* record a script message, unless it is a short or mechanical one */
extern void journal_add_text(const char *text);

/* number of entries */
extern int journal_size(void);

/* entry i, oldest first */
extern const JournalEntry *journal_entry(int i);

/* store into / restore from a save game node; NULL node clears */
extern void journal_save(xmlNodePtr node);
extern void journal_load(xmlNodePtr node);

#endif /* _JOURNAL_H */
