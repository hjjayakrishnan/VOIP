INCLUDE_DIRS =
LIB_DIRS =
CC=gcc

CDEFS=
CFLAGS=  -g $(INCLUDE_DIRS) $(CDEFS)
LIBS=	-lasound

HFILES=
CFILES= client_duplex.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	client_duplex

clean:
	-rm -f *.o *.d
	-rm -f client_duplex

client_duplex: client_duplex.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lasound -ldl -lm

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
