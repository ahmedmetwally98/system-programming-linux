# compile each .c file to .o file and put them in the obj folder
# then from each object file, create the executable file
# add rule to run the executable file with the input file name
# add rule to remove the object files 
# add rule to remove the executable files
# add rule to remove the output files

# define the compiler
CC = gcc

# define the compiler flags
CFLAGS = -Wall -Wextra -pedantic -g -std=c11 -O2 

# define the linker
LD = gcc

# define the linker flags
LDFLAGS = -lm

SRC_DIR = ./
OBJ_DIR = ./obj
EXE_DIR = ./bin

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))
femtoEXE ?= femto

EXE = $(addprefix $(EXE_DIR)/, $(femtoEXE))

all: $(EXE)

$(EXE_DIR)/$(femtoEXE): $(OBJ_DIR)/femto_shell.o 	| $(EXE_DIR)
	$(LD) -o $@ $(OBJ_DIR)/femto_shell.o $(LDFLAGS)


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR): 
	mkdir -p $(OBJ_DIR)

$(EXE_DIR):
	mkdir -p $(EXE_DIR)

clean:
	rm -fr $(OBJ_DIR)/* $(EXE_DIR)/*

.PHONY: all clean
