TARGET = gloom
SDIR = src
PREFIX = /usr/bin

CC = gcc
CFLAGS = -Wall
LIBS = -lX11 -lXrandr -lXfixes -lXi

gloom: $(SDIR)/gloom.c
	$(CC) $(CFLAGS) $(SDIR)/gloom.c $(LIBS) -o $(TARGET)

install:
	install -D -m 755 gloom /usr/bin/gloom

uninstall:
	rm -f /usr/bin/gloom
