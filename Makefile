DESKTOP_UI ?= 0
DEBUG_USING ?= none

VALID_DEBUG_MODES := none debug valgrind sanitizers perf remotery
ifneq (,$(filter-out $(VALID_DEBUG_MODES),$(DEBUG_USING)))
$(error Invalid DEBUG_USING value '$(DEBUG_USING)'; must be one of: $(VALID_DEBUG_MODES))
endif

CC := clang
CLANG_TIDY := clang-tidy
PROGRAM := chordel
SOURCES := main.c push2.c shared.c xoroshiro.c vendor/cbitset/bitset.c vendor/physfsrwops/physfsrwops.c
SOURCES_NO_VENDOR := $(filter-out vendor/%,$(SOURCES))
OBJECTS := $(SOURCES:.c=.o)
DEPS := sdl2 SDL2_ttf portmidi physfs libusb-1.0 cairo
# https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
CFLAGS := -std=c11  -Wall -Wextra  -Wconversion -Wdouble-promotion -Wno-unused-parameter -Wno-unused-function  $(shell pkg-config --cflags-only-other $(DEPS)) -DDESKTOP_UI=$(DESKTOP_UI)
# -pedantic -pedantic-errors
CFLAGS_INCLUDES := $(shell pkg-config --cflags-only-I $(DEPS))

# physfs library name needs to be patched in the binary.
PHYSFS_LIB_NAME := $(notdir $(shell pkg-config --variable=libdir physfs)/libphysfs.1.dylib)
PHYSFS_LIB_RPATH := $(shell pkg-config --variable=libdir physfs)
LDFLAGS := -lm $(shell pkg-config --libs $(DEPS)) -Wl,-rpath,$(shell pkg-config --variable=libdir physfs)

# Enable dependency generation.
DEPFLAGS := -MMD -MP

ASSETS_ARCHIVE := assets.zip
ASSETS_DIR := assets
ASSETS := $(wildcard $(ASSETS_DIR)/*)

ifeq ($(OS), Windows_NT)
    OS_NAME = Windows
    CFLAGS += -DWINDOWS
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S), Linux)
        OS_NAME = Linux
        CFLAGS += -DLINUX
        NPROCS := $(shell grep -c ^processor /proc/cpuinfo)
    endif
    ifeq ($(UNAME_S), Darwin)
        OS_NAME = macOS
        CFLAGS += -DMACOS
		NPROCS := 6
    endif
endif

ifeq ($(DEBUG_USING), sanitizers)
	CFLAGS += -fsanitize=address,pointer-compare,undefined -fsanitize-trap
	LDFLAGS += -fsanitize=address
endif

ifeq ($(DEBUG_USING), valgrind)
	CFLAGS += -DDEBUG -g3 -O0 -fno-omit-frame-pointer
endif

ifeq ($(DEBUG_USING), perf)
    CFLAGS += -g -O0
endif

ifeq ($(DEBUG_USING), remotery)
    CFLAGS += -g0 -O3
    CFLAGS += -DRMT_ENABLED=1
else
    CFLAGS += -DRMT_ENABLED=0
endif

ifeq ($(DEBUG_USING), debug)
    CFLAGS += -DDEBUG -g3 -O0
endif

ifeq ($(DEBUG_USING), none)
    CFLAGS += -g0 -O3
endif

define banner
	@echo "====================================================================================="
	@echo " $1"
	@echo "====================================================================================="
endef

all: $(PROGRAM) $(ASSETS_ARCHIVE)

$(ASSETS_ARCHIVE): $(ASSETS)
	zip -r $(ASSETS_ARCHIVE) $(ASSETS_DIR)

$(PROGRAM): $(OBJECTS) generate-clangd render_plugin.so
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)
ifeq ($(OS_NAME), macOS)
	install_name_tool -change $(PHYSFS_LIB_NAME) @rpath/$(PHYSFS_LIB_NAME) $@
endif
ifeq ($(DEBUG), 0)
	strip $(PROGRAM)
endif

# Compile .c to .o, and generate dependency files (.d).
%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_INCLUDES) $(DEPFLAGS) -c $< -o $@

# Include dependency files.
-include $(SOURCES:.c=.d)

.PHONY: docs
docs:
	python3 -c "import webbrowser; webbrowser.open('localhost:8000')"
	cd docs && python3 -m http.server

# https://clangd.llvm.org/config
.PHONY: generate-clangd
generate-clangd:
	@echo "Generating .clangd file..."
	@echo "InlayHints:" > .clangd
	@echo "  Enabled: No" >> .clangd
	@echo "CompileFlags:" >> .clangd
	@echo "  Add:" >> .clangd
	@$(foreach flag, $(CFLAGS), echo "    - $(flag)" >> .clangd;)
	@$(foreach flag, $(CFLAGS_INCLUDES), echo "    - $(flag)" >> .clangd;)

render_plugin.so: render_plugin.c render_plugin.h shared.c shared.h vendor/cbitset/bitset.c vendor/cbitset/bitset.h
	$(CC) -fPIC -shared $(CFLAGS) $(CFLAGS_INCLUDES) $(DEPFLAGS) $(LDFLAGS) -o render_plugin.so render_plugin.c shared.c vendor/cbitset/bitset.c

.PHONY: clang-tidy
clang-tidy:
	$(call banner,clang-tidy)
	-$(CLANG_TIDY) $(SOURCES_NO_VENDOR) -- $(CFLAGS) $(CFLAGS_INCLUDES) $(DEPFLAGS)

# NOTE: Run flawfinder on .c and .h files. cppcheck and clang-tidy already check headers automatically.
.PHONY: flawfinder
flawfinder:
	$(call banner,flawfinder)
	-find . -path ./vendor -prune -o \( -name '*.c' -o -name '*.h' \) -print | flawfinder -

.PHONY: cppcheck
cppcheck:
	$(call banner,cppcheck)
	-cppcheck -j $(NPROCS) --std=c11 --check-level=exhaustive --enable=performance,portability,warning $(CFLAGS_INCLUDES) $(SOURCES_NO_VENDOR)

.PHONY: lint
lint: clang-tidy flawfinder cppcheck

.PHONY: benchmark
benchmark:
	$(CC) -O3 -g0 $(shell pkg-config --cflags-only-I $(DEPS)) -lm $(shell pkg-config --libs $(DEPS)) benchmark.c push2.c -o benchmark
	./benchmark

.PHONY: clean
clean:
	git clean -dfX

# For a list of events run `perf list`.
.PHONY: perf
perf:
ifeq ($(UNAME_S), Linux)
	make clean
	@rm -f perf.*
	make PERF=1 RUN=0
	@echo "====================================================================================="
	@echo "Starting recording. Stop using CTRL+C. After stopping, View data using 'perf report'."
	@echo "====================================================================================="
	perf record -g --timestamp ---freq=1000 -- ./$(PROGRAM) &> /dev/null
#perf script report flamegraph --allow-download
#xdg-open flamegraph.html
	perf script report gecko
	xdg-open "https://profiler.firefox.com/"
else
	@echo "Only available on linux."
endif

.PHONY: callgrind
callgrind:
ifeq ($(UNAME_S), Linux)
	make clean
	make DEBUG=1 VALGRIND=1 RUN=0
	valgrind --tool=callgrind --suppressions=./valgrind.supp  ./$(PROGRAM)
else
	@echo "Only available on linux."
endif

.PHONY: helgrind
helgrind:
ifeq ($(UNAME_S), Linux)
	make clean
	make DEBUG=1 VALGRIND=1 RUN=0
	valgrind --tool=helgrind --suppressions=./valgrind.supp  ./$(PROGRAM)
else
	@echo "Only available on linux."
endif

.PHONY: valgrind
valgrind:
ifeq ($(UNAME_S), Linux)
	make clean
	make DEBUG=1 VALGRIND=1 RUN=0
# --show-leak-kinds=all --show-error-list=all
	valgrind --leak-check=full --suppressions=./valgrind.supp  ./$(PROGRAM)
else
	@echo "Only available on linux."
endif

