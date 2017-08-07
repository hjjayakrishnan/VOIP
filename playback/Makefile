INCLUDE_DIRS =
LIB_DIRS =
CC=gcc

CDEFS=
CFLAGS=  -g $(INCLUDE_DIRS) $(CDEFS)
LIBS=	-lasound

HFILES=
CFILES= test1.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	test1

clean:
	-rm -f *.o *.d
	-rm -f test1

test1: test1.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lasound -ldl -lm

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
