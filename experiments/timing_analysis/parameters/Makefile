INCLUDE_DIRS =
LIB_DIRS =
CC=gcc

CDEFS=
CFLAGS=  -g $(INCLUDE_DIRS) $(CDEFS)
LIBS=	-lasound

HFILES=
CFILES= parameter.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	parameter

clean:
	-rm -f *.o *.d
	-rm -f parameter

parameter: parameter.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lasound -ldl -lm

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
