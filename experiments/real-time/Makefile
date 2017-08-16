INCLUDE_DIRS =
LIB_DIRS =
CC=gcc

CDEFS=
CFLAGS=  -g $(INCLUDE_DIRS) $(CDEFS)
LIBS=	-lasound

HFILES=
CFILES=  tcp_playback

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	  tcp_playback

clean:
	-rm -f *.o *.d
	-rm -f  tcp_playback

tcp_playback: tcp_playback.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lasound -ldl -lm -lrt

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
