CC = gcc
CFLAGS = -std=gnu17 -Wall -Wextra -Werror

OBJS = transport.o aux.o

all: transport

transport: $(OBJS)
	$(CC) $(CFLAGS) -lm $(OBJS) -o transport -lm

transport.o: transport.c transport.h
	$(CC) -c $(CFLAGS) transport.c -o transport.o -lm

aux.o : aux.c transport.h
	$(CC) -c $(CFLAGS) aux.c -o aux.o

clean:
	rm *.o

distclean:
	rm *.o transport

