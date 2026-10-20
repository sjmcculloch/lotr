AM_CPPFLAGS = -I../liblotr -DPREFIX='"@prefix@"' $(SDL_CFLAGS) $(XML2_CFLAGS)

bin_PROGRAMS = lotr
lotr_LDADD = ../liblotr/liblotr.la $(SDL_LIBS)
lotr_SOURCES = lotr.c
