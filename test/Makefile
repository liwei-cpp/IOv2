# Build mode selection (debug/release)
MODE = debug

# Compiler and flags
CXX := /usr/bin/g++
# CXX := /usr/bin/clang++

ifeq ($(MODE),debug)
    COMP_FLAGS := -Wall -std=c++23 -g -O0 -I../include -I. -I/usr/include/botan-2/
    OBJ_DIR := obj_debug
    BIN_DIR := bin_debug
else
    COMP_FLAGS := -O3 -Wall -std=c++23 -g -DNDEBUG -I../include -I. -I/usr/include/botan-2/
    OBJ_DIR := obj_release
    BIN_DIR := bin_release
endif

LINK_FLAGS := -lz -lbotan-2

# Module definitions
MODULES := device cvt facet locale io

# Per-module source collection
device_SOURCES := $(shell find ./device -name '*.cpp')
cvt_SOURCES := $(shell find ./cvt -name '*.cpp')
facet_SOURCES := $(shell find ./facet -name '*.cpp')
locale_SOURCES := $(shell find ./locale -name '*.cpp')
io_SOURCES := $(shell find ./io -name '*.cpp')

# Per-module object files
device_OBJECTS := $(patsubst ./%.cpp,$(OBJ_DIR)/%.o,$(device_SOURCES))
cvt_OBJECTS := $(patsubst ./%.cpp,$(OBJ_DIR)/%.o,$(cvt_SOURCES))
facet_OBJECTS := $(patsubst ./%.cpp,$(OBJ_DIR)/%.o,$(facet_SOURCES))
locale_OBJECTS := $(patsubst ./%.cpp,$(OBJ_DIR)/%.o,$(locale_SOURCES))
io_OBJECTS := $(patsubst ./%.cpp,$(OBJ_DIR)/%.o,$(io_SOURCES))

# Target binaries
TARGETS := $(addprefix $(BIN_DIR)/test_,$(MODULES))

# Default target: build all test binaries
all: $(TARGETS)

# Individual binary targets
$(BIN_DIR)/test_device: $(device_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^ $(LINK_FLAGS)

$(BIN_DIR)/test_cvt: $(cvt_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^ $(LINK_FLAGS)

$(BIN_DIR)/test_facet: $(facet_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^ $(LINK_FLAGS)

$(BIN_DIR)/test_locale: $(locale_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^ $(LINK_FLAGS)

$(BIN_DIR)/test_io: $(io_OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) -o $@ $^ $(LINK_FLAGS)

# Convenience targets for building individual modules
device: $(BIN_DIR)/test_device
cvt: $(BIN_DIR)/test_cvt
facet: $(BIN_DIR)/test_facet
locale: $(BIN_DIR)/test_locale
io: $(BIN_DIR)/test_io

# Convenience targets for building and running individual modules
test-device: device
	@echo "Running $(BIN_DIR)/test_device..."; $(BIN_DIR)/test_device

test-cvt: cvt
	@echo "Running $(BIN_DIR)/test_cvt..."; $(BIN_DIR)/test_cvt

test-facet: facet
	@echo "Running $(BIN_DIR)/test_facet..."; $(BIN_DIR)/test_facet

test-locale: locale
	@echo "Running $(BIN_DIR)/test_locale..."; $(BIN_DIR)/test_locale

test-io: io
	@echo "Running $(BIN_DIR)/test_io..."; $(BIN_DIR)/test_io

# Generic compilation rule
$(OBJ_DIR)/%.o: ./%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(COMP_FLAGS) -c $< -o $@

# Run all tests
test: all
	@for t in $(TARGETS); do echo "Running $$t..."; $$t || exit 1; done

run: test

# Clean up
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Help target
help:
	@echo "Usage: make [TARGET] [MODE=debug|release]"
	@echo ""
	@echo "Targets:"
	@echo "  all          Build all test binaries (default)"
	@echo "  test, run    Build and run all tests"
	@echo "  clean        Remove build artifacts for current MODE"
	@echo ""
	@echo "  device       Build device tests"
	@echo "  cvt          Build cvt tests"
	@echo "  facet        Build facet tests"
	@echo "  locale       Build locale tests"
	@echo "  io           Build io tests"
	@echo ""
	@echo "  test-device  Build and run device tests"
	@echo "  test-cvt     Build and run cvt tests"
	@echo "  test-facet   Build and run facet tests"
	@echo "  test-locale  Build and run locale tests"
	@echo "  test-io      Build and run io tests"
	@echo ""
	@echo "Options:"
	@echo "  MODE=debug   Build with debug flags (default)"
	@echo "  MODE=release Build with release flags (-O3, -DNDEBUG)"
	@echo ""
	@echo "Examples:"
	@echo "  make                     Build all (debug)"
	@echo "  make -j8                 Build all in parallel"
	@echo "  make MODE=release        Build all (release)"
	@echo "  make device              Build device tests only"
	@echo "  make test-device         Build and run device tests"
	@echo "  make test                Build and run all tests"

# Phony targets
.PHONY: all clean test run help
.PHONY: device cvt facet locale io
.PHONY: test-device test-cvt test-facet test-locale test-io
