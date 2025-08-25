# Makefile for the Tic-Tac-Toe Minimax Project

# Compiler and flags
# CXX is the variable for the C++ compiler
# CXXFLAGS are the flags passed to the compiler
# -std=c++17 specifies the C++17 standard
# -Wall enables all common warnings
# -Wextra enables extra warnings
# -g adds debugging information
CXX = clang++
CXXFLAGS = -std=c++17 -O2 -g

# The name of the final executable file
TARGET = tictactoe_game.out

# Automatically find all .cpp files in the current directory
# wildcard finds files matching the pattern
# notdir strips the directory path, leaving just the filename
SOURCES = $(wildcard *.cpp)

# Generate the corresponding .o (object) file names from the source file names
# e.g., main.cpp -> main.o
OBJECTS = $(SOURCES:.cpp=.o)

# The default rule, which is run when you just type "make"
# This rule depends on the executable file (TARGET)
all: $(TARGET)

# Rule to link all the object files (.o) into the final executable
# $@ is an automatic variable that means "the target of the rule" (tictactoe_game)
# $^ is an automatic variable that means "all the prerequisites" (all .o files)
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# A pattern rule for compiling .cpp files into .o files
# This says "to make a .o file, you need the corresponding .cpp file"
# $< is an automatic variable that means "the first prerequisite" (the .cpp file)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# A rule to clean up the project directory
# It removes the executable and all object files
clean:
	rm -f $(TARGET) $(OBJECTS)

# Phony targets are targets that are not actual files.
# This tells "make" that "all" and "clean" are just names for commands to be executed.
.PHONY: all clean
