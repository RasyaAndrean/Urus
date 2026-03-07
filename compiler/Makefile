# NOTE: To prevent Makefile `*** missing separator.  Stop.` error:
# DONT USE expand tabs in your editor e.g (VIm, etc)

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -Iinclude
PREFIX := /usr/local/bin
TARGET = urusc

SRCDIR = src
SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(TARGET)
	install -m 0755 $(TARGET) $(PREFIX)/$(TARGET)

uninstall:
	rm -f $(PREFIX)/$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean install uninstall

