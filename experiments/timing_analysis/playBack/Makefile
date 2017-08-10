INCLUDE_DIRS =
LIB_DIRS =
CC=gcc

CDEFS=
CFLAGS=  -g $(INCLUDE_DIRS) $(CDEFS)
LIBS=	-lasound

HFILES=
CFILES= core.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	core

clean:
	-rm -f *.o *.d
	-rm -f core

core: core.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lasound -ldl -lm

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
