CC := gcc
CFLAGS := -O3

INC_DIR := include
SRC_DIR := src
OBJ_DIR := build
BIN_DIR := bin
LIB_DIR := lib

SRC_FILES := $(wildcard $(SRC_DIR)/*.c)
BIN_PROG := $(patsubst $(SRC_DIR)/%.c,$(BIN_DIR)/%,$(SRC_FILES))

LIB_FILES := $(wildcard $(LIB_DIR)/*.c)
LIB_OBJS := $(patsubst $(LIB_DIR)/%.c,$(OBJ_DIR)/%.o,$(LIB_FILES))

run: $(BIN_PROG)
	./$<

bin/%: $(LIB_OBJS) $(OBJ_DIR)/%.o
	$(CC) $(CFLAGS) -o $@ $^ -lz

build/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -I $(INC_DIR) -c -o $@ $<

build/%.o: $(LIB_DIR)/%.c $(INC_DIR)/%.h
	$(CC) $(CFLAGS) -I $(INC_DIR) -c -o $@ $<

clean:
	rm -f build/*.o bin/*
