CC = gcc
CFLAGS = -Wall -Wextra -Werror -g

LIB_SRC = rl_lock_library.c
LIB_OBJ = $(LIB_SRC:.c=.o)
LIB = librl_lock.a

TARGET = main
SRC = main.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ) $(LIB)
	$(CC) $(CFLAGS) -o $@ $^

$(LIB): $(LIB_OBJ)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(LIB_OBJ) $(LIB) $(TARGET)

.PHONY: all clean
