CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lncurses -lpthread
TARGET  = matter
SRC     = main.c irc.c ui.c state.c
OBJ     = $(addprefix build/,$(SRC:.c=.o))

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/%.o: %.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p build

clean:
	rm -rf build $(TARGET)

.PHONY: all clean
