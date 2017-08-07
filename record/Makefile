INCLUDE_DIRS =
LIB_DIRS =
CC=gcc

CDEFS=
CFLAGS=  -g $(INCLUDE_DIRS) $(CDEFS)
LIBS=	-lasound

HFILES=
CFILES= test2.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	test2

clean:
	-rm -f *.o *.d
	-rm -f test2

test2: test2.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lasound -ldl -lm

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
