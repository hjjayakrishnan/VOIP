INCLUDE_DIRS =
LIB_DIRS =
CC=gcc

CDEFS=
CFLAGS=  -g $(INCLUDE_DIRS) $(CDEFS)
LIBS=

HFILES=
CFILES= server.c client.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	server client

clean:
	-rm -f *.o *.d
	-rm -f server client

server: server.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o 

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
