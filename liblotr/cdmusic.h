/****************************************************************************

    cdmusic.h
    CD audio music for the CD editions

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.

*****************************************************************************/

#ifndef _CDMUSIC_H
#define _CDMUSIC_H

extern void cdmusic_start(void);    /* start/continue background music */
extern int cdmusic_stop(void);      /* stop; nonzero if it was playing */
extern int cdmusic_toggle(void);    /* options menu on/off */
extern int cdmusic_available(void); /* mixer open and track files found */
extern int cdmusic_enabled(void);   /* not switched off by the player */
extern void cdmusic_set_enabled(int on);
extern int cdmusic_volume(void);    /* 0-100 */
extern void cdmusic_set_volume(int percent);
extern void cdmusic_pause(void);    /* around movies */
extern void cdmusic_resume(void);
extern void cdmusic_cue(int index); /* script music cue 0..11 */
extern void cdmusic_combat(int won);
extern void cdmusic_update(void);   /* call regularly from the main loop */

#endif /* _CDMUSIC_H */
