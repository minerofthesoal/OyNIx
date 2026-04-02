# OyNIx Browser - Makefile
# Builds native C launcher and C++ indexer library
#
# Targets:
#   make            - Build everything
#   make launcher   - Build C launcher only
#   make lib        - Build C++ shared library only
#   make cli        - Build C++ CLI indexer tool
#   make install    - Install to /usr/local
#   make clean      - Remove build artifacts
#
# Coded by Claude (Anthropic)

PREFIX    ?= /usr/local
CC        ?= gcc
CXX       ?= g++
CFLAGS    ?= -O2 -Wall -Wextra
CXXFLAGS  ?= -O2 -Wall -Wextra -std=c++17

BUILD_DIR  = build
SRC_DIR    = src

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LIB_EXT   = .dylib
    LIB_FLAGS = -dynamiclib
else
    LIB_EXT   = .so
    LIB_FLAGS = -shared
endif

# Targets
LAUNCHER   = $(BUILD_DIR)/oynix
LIBRARY    = $(BUILD_DIR)/libnyx_index$(LIB_EXT)
CLI_TOOL   = $(BUILD_DIR)/nyx-index

.PHONY: all launcher lib cli install clean

all: launcher lib

launcher: $(LAUNCHER)
lib: $(LIBRARY)
cli: $(CLI_TOOL)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# C launcher binary
$(LAUNCHER): $(SRC_DIR)/launcher.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

# C++ shared library (for Python ctypes)
$(LIBRARY): $(SRC_DIR)/native/fast_index.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LIB_FLAGS) -fPIC -o $@ $<

# C++ CLI tool
$(CLI_TOOL): $(SRC_DIR)/native/fast_index.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -DCLI_MODE -o $@ $<

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/lib/oynix
	install -m 755 $(LAUNCHER) $(DESTDIR)$(PREFIX)/bin/oynix
	install -m 755 $(LIBRARY) $(DESTDIR)$(PREFIX)/lib/oynix/
	cp -r oynix $(DESTDIR)$(PREFIX)/lib/oynix/
	cp requirements.txt $(DESTDIR)$(PREFIX)/lib/oynix/

clean:
	rm -rf $(BUILD_DIR)
