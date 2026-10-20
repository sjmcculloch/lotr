/****************************************************************************

    mpg.h
    MPEG-1 cutscene player for the ReelMagic CD edition

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of the License, or
    (at your option) any later version.

*****************************************************************************/

#ifndef _MPG_H
#define _MPG_H

/* is there a <name>.mpg in the data folder? */
extern int mpg_exists(const char *name);

/* play <name>.mpg; returns 0 if it is missing or unreadable */
extern int mpg_play(const char *name);

#endif /* _MPG_H */
