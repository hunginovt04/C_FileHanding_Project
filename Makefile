CC = gcc
CFLAGS = -Wall -Wextra -pedantic -Iinc
SRC = $(wildcard src/*.c)
TARGET = main

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)
