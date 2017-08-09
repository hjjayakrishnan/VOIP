INCLUDE_DIRS =
LIB_DIRS =
CC=gcc

CDEFS=
CFLAGS=  -g $(INCLUDE_DIRS) $(CDEFS)
LIBS=

HFILES=
CFILES= template.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	template record_playback

clean:
	-rm -f *.o *.d
	-rm -f template

template: template.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lrt -lm

record_playback: record_playback.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lasound -lrt -lm

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
