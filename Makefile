# IOv2 — library packaging Makefile
#
# IOv2 is HEADER-ONLY by default and needs none of this: just add include/ to
# your include path and #include the headers.
#
# This Makefile exists only for the optional SHARED-LIBRARY (DSO/DLL) mode, used
# when you want a single instance of the standard streams / localization cache
# across arbitrary DSOs. It builds libiov2.so from the single definition TU
# src/iov2_objects.cpp and installs the headers with the IOV2_SHARED switch
# turned on, so consumers never pass -DIOV2_SHARED by hand.
#
# See README "Usage Modes" and the header include/IOv2/common/iov2_export.h.

CXX       ?= g++
PREFIX    ?= /usr/local
PCDIR     ?= $(PREFIX)/lib/pkgconfig
INCLUDEDIR := $(PREFIX)/include
PACKAGE_INCLUDEDIR := $(INCLUDEDIR)/IOv2
LICENSEDIR := $(PREFIX)/share/licenses/IOv2

# Feature-test macro the library requires (POSIX.1-2008: locale_t, newlocale,
# uselocale, the *_l ctype helpers). Without it the headers do not compile.
CPPFLAGS  := -D_POSIX_C_SOURCE=200809L

# Shared-library flags. -fvisibility=hidden keeps the exported surface to exactly
# the IOV2_API-tagged stream references; every other symbol stays internal.
SO_FLAGS  := -std=c++23 -O2 -fPIC -fvisibility=hidden -shared \
             -Iinclude \
             $(CPPFLAGS)

# Linker flags. By default IOv2 does NOT force libiov2.so to stay resident: a
# dlclose behaves like it does for any other shared library -- once you unload it,
# you must not use anything that came from it (the stream objects, the localization
# cache, or any reference/pointer handed out from those). Closing a library and
# then calling into it is a use-after-unload bug on the caller's side.
#
# The one case this leaves exposed is a reference that outlives the unload: e.g. a
# plugin borrows a libiov2 stream, hands it back to the host, and is then dlclose'd
# while it held libiov2's *last* reference -- the library unmaps and the
# process-wide singletons (s_ori_facet_buf, the stream objects) are destroyed under
# the host's feet. If your deployment does that, uncomment the opt-in line below:
# -z nodelete marks libiov2.so non-unloadable, so those singletons live until real
# process exit (like libstdc++). See the lifetime/dlopen note in
# src/iov2_objects.cpp for the full rationale.
SO_LDFLAGS :=
# SO_LDFLAGS := -Wl,-z,nodelete

# The installed name, and where the build puts it. Out-of-source like the CMake
# presets: build/ is the one directory a clone generates, so nothing lands beside
# the sources. LIB_NAME is what gets installed and uninstalled under PREFIX;
# LIB is only where this Makefile writes it.
LIB_NAME  := libiov2.so
LIB_DIR   ?= build/make
LIB       := $(LIB_DIR)/$(LIB_NAME)
SRC       := src/iov2_objects.cpp

# Build the shared library.
$(LIB): $(SRC)
	@mkdir -p $(LIB_DIR)
	$(CXX) $(SO_FLAGS) $(SRC) $(SO_LDFLAGS) -o $(LIB)

shared: $(LIB)

# Header-only install: keep IOv2 below its own include directory rather than
# claiming generic names such as PREFIX/include/common and PREFIX/include/io.
# pkg-config adds INCLUDEDIR to the search path; public includes are rooted at
# <IOv2/...>. The IOV2_SHARED switch stays commented.
install:
	install -d $(PACKAGE_INCLUDEDIR) $(PCDIR) $(LICENSEDIR)
	cp -R include/IOv2/. $(PACKAGE_INCLUDEDIR)/
	install -m 0644 LICENSE NOTICE $(LICENSEDIR)/
	sed 's|^prefix=.*|prefix=$(PREFIX)|' pkgconfig/iov2.pc > $(PCDIR)/iov2.pc

# Shared install: headers WITH the IOV2_SHARED switch turned on, plus libiov2.so.
# The sed flips the one switch line in the *installed* copy of iov2_export.h; the
# repo copy stays header-only.
install-shared: $(LIB)
	install -d $(PACKAGE_INCLUDEDIR) $(PREFIX)/lib $(PCDIR) $(LICENSEDIR)
	cp -R include/IOv2/. $(PACKAGE_INCLUDEDIR)/
	sed -i 's|^// #define IOV2_SHARED 1|#define IOV2_SHARED 1|' \
	    $(PACKAGE_INCLUDEDIR)/common/iov2_export.h
	install -m 0644 $(LIB) $(PREFIX)/lib/
	install -m 0644 LICENSE NOTICE $(LICENSEDIR)/
	sed 's|^prefix=.*|prefix=$(PREFIX)|' pkgconfig/iov2-shared.pc > $(PCDIR)/iov2-shared.pc

clean:
	rm -rf $(LIB_DIR)

# Remove only IOv2-owned paths. Keeping the headers below one package directory
# makes this both safer and simpler than deleting generic top-level include names.
uninstall:
	rm -rf $(PACKAGE_INCLUDEDIR)
	rm -rf $(LICENSEDIR)
	rm -f $(PREFIX)/lib/$(LIB_NAME)
	rm -f $(PCDIR)/iov2.pc $(PCDIR)/iov2-shared.pc

help:
	@echo "IOv2 packaging Makefile (library is header-only by default)."
	@echo ""
	@echo "Targets:"
	@echo "  shared           Build $(LIB) from $(SRC)"
	@echo "  install          Install headers, licence files + iov2.pc -> header-only mode"
	@echo "  install-shared   Build + install headers, licence files, $(LIB) + iov2-shared.pc"
	@echo "  clean            Remove $(LIB_DIR)"
	@echo "  uninstall        Remove installed headers, $(LIB_NAME), and .pc files from PREFIX"
	@echo ""
	@echo "Variables (override on the command line):"
	@echo "  CXX=$(CXX)"
	@echo "  PREFIX=$(PREFIX)"
	@echo "  PCDIR=$(PCDIR)"
	@echo "  INCLUDEDIR=$(INCLUDEDIR)"
	@echo "  PACKAGE_INCLUDEDIR=$(PACKAGE_INCLUDEDIR)"
	@echo "  LICENSEDIR=$(LICENSEDIR)"
	@echo ""
	@echo "Examples:"
	@echo "  make install                         # header-only into /usr/local"
	@echo "  make install-shared PREFIX=/opt/iov2 # shared lib into /opt/iov2"

.PHONY: shared install install-shared uninstall clean help
