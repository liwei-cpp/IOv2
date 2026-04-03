MODE = debug

# Compiler and flags
CXX := /usr/bin/g++
# CXX := /usr/bin/clang++
ifeq ($(MODE),debug)
	COMP_FLAGS := -Wall -std=c++23 -g -I./IOv2 -I./IOv2Test -O0 -I/usr/include/botan-2/
	LINK_FLAGS := -lz -lbotan-2
else
    COMP_FLAGS := -O3 -Wall -std=c++23 -g -DNDEBUG  -I./IOv2 -I./IOv2Test -I/usr/include/botan-2/
	LINK_FLAGS := -lz -lbotan-2
endif

# Directories
SRC_DIR = ./IOv2Test
ifeq ($(MODE),debug)
    OBJ_DIR := obj_debug
	BIN_DIR := bin_debug
else
	OBJ_DIR := obj_release
	BIN_DIR := bin_release
endif

# Source files, object files, and target
SOURCES := $(shell find $(SRC_DIR) -name '*.cpp')
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))
TARGET := $(BIN_DIR)/app

# Default target
all: $(TARGET)

# Linking
$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^ $(LINK_FLAGS)

# Compiling
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(COMP_FLAGS) -c $< -o $@

# Clean up
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Phony targets
.PHONY: all clean
