# Compiler and flags
CC = /mingw64/bin/g++
OPT = -O3
#OPT = -g
WARN = -Wall
# Select the C++ standard (C++17)
STD = -std=c++17
CFLAGS = $(OPT) $(WARN) $(STD) $(INC) $(LIB) -v

# Linker flags (for console-based applications on Windows)
LDFLAGS = -mconsole

# List of source files
SIM_SRC = sim.cc cache.cc prefetcher.cc

# Automatically generate list of object files from source files
SIM_OBJ = $(SIM_SRC:.cc=.o)

#################################

# Default rule to build the program
all: sim
	@echo "my work is done here..."

# Rule to build the executable 'sim'
sim: $(SIM_OBJ)
	$(CC) -o sim $(CFLAGS) $(SIM_OBJ) $(LDFLAGS) -lm
	@echo "-----------DONE WITH sim-----------"

# Generic rule to compile .cc files into .o object files
%.o: %.cc
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule to remove object files and the executable
clean:
	rm -f *.o sim

# Clobber rule to remove only object files (leaves executable)
clobber:
	rm -f *.o
