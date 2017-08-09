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
	
d:
	-rm cli* ser*

ds:
	-rm ser*

dc:
	-rm cli*

server: server.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lrt -lm

client: client.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.o -lpthread -lrt -lm

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
