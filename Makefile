CC = gcc
CFLAGS = -Wall -g -pthread -std=gnu99
LIBS = -lrt

LIB_SRC = rl_lock_library.c
LIB_OBJ = $(LIB_SRC:.c=.o)
LIB = librl_lock.a

TARGET = main
SRC = main.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ) $(LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(LIB_OBJ) $(LIB) $(TARGET)

.PHONY: all clean
