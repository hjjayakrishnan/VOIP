INCLUDE_DIRS =
LIB_DIRS =
CC=gcc

CDEFS=
CFLAGS=  -g $(INCLUDE_DIRS) $(CDEFS)
LIBS=	-lasound

HFILES=
CFILES= record_server tcp_playback

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	 record_server tcp_playback

clean:
	-rm -f *.o *.d
	-rm -f record_server tcp_playback

record_server: record_server.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lasound -ldl -lm -lrt


tcp_playback: tcp_playback.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lasound -ldl -lm -lrt

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
