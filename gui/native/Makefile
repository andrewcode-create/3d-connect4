CXX ?= g++
CXXFLAGS ?= -std=c++23 -O3 -pthread -Wall -Wextra -Wpedantic
BIN_DIR := ../bin
ENGINE := $(BIN_DIR)/connect4-gui-engine
TEST := $(BIN_DIR)/native-tests

.PHONY: all test clean

all: $(ENGINE)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(ENGINE): bridge.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -I../../game-engine bridge.cpp -o $(ENGINE)

$(TEST): native_tests.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -I../../game-engine native_tests.cpp -o $(TEST)

test: $(ENGINE) $(TEST)
	$(TEST)
	$(ENGINE) inspect --moves 0,1,0,1,0,1,0 >/dev/null
	$(ENGINE) ais >/dev/null

clean:
	rm -f $(ENGINE) $(TEST)

