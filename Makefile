# Makefile for PA2

CC = g++
CFLAGS = -g -Wall -Werror -pthread

all: proxy

proxy: proxy.c
	$(CC) $(CFLAGS) -o proxy.o -c proxy.c
	$(CC) $(CFLAGS) -o proxy proxy.o

clean:
	rm -f proxy *.o
