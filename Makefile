# SFML 3 Framework
#
#   make              Build (incremental, header-aware)
#   make debug        Build with -g -O0 and sanitizers, into build/debug
#   make release      Build with -O2 -DNDEBUG, into build/release
#   make run          Build and run (also run-debug, run-release)
#   make clean        Remove build artefacts
#   make rebuild      Clean and build from scratch
#
# Pick which example to build with EXAMPLE (tetris by default):
#
#   make EXAMPLE=showcase run
#
# Works on Linux, macOS and Windows (MSYS2/MinGW). The platform block
# below picks the right include paths and link flags; everything else is
# shared.

# ---------------------------------------------------------------- setup

CXX      ?= g++

# MODE is set by `make debug` and `make release`. Each mode builds into
# its own folder, so debug objects (compiled with sanitizers) can never
# end up linked into a normal build, and switching modes needs no clean.
MODE     ?=
BUILD    := build$(if $(MODE),/$(MODE))

# Which folder under examples/ to build. Each example gets its own
# binary, so switching back and forth never runs a stale one.
EXAMPLE  ?= tetris
TARGET   := $(BUILD)/bin/$(EXAMPLE)

ifeq ($(wildcard examples/$(EXAMPLE)/*.cpp),)
    $(error No example called "$(EXAMPLE)". Available: $(notdir $(wildcard examples/*)))
endif

# Objects live under build/ instead of next to their sources, so the
# tree stays clean and `make clean` cannot miss one.
SRC_DIRS := src src/core src/framework src/gui src/scenes src/utilities \
            examples/$(EXAMPLE)

SOURCES  := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp))

# tests.cpp inside an example directory has its own main(), so it must
# not be swept into the game binary.
SOURCES  := $(filter-out %/tests.cpp,$(SOURCES))
OBJECTS  := $(patsubst %.cpp,$(BUILD)/%.o,$(SOURCES))

# -MMD -MP emit a .d file per object listing the headers it included.
# Without these, editing a header rebuilds nothing -- which is how the
# old SceneManager.o came to be built from source that no longer
# existed.
DEPS     := $(OBJECTS:.o=.d)

WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor
# The mode's flags are added to everything else rather than replacing
# it, so the platform include paths below survive into every mode.
ifeq ($(MODE),debug)
    OPT     := -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer
    LDFLAGS += -fsanitize=address,undefined
else ifeq ($(MODE),release)
    OPT     := -O2 -DNDEBUG
else
    OPT     := -O2
endif

CXXFLAGS ?= -std=c++20
CXXFLAGS += $(OPT) $(WARNINGS) -MMD -MP

# ------------------------------------------------------------- platform

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(OS),Windows_NT)
    PLATFORM := Windows
else ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
    PLATFORM := Windows
else ifeq ($(findstring MSYS,$(UNAME_S)),MSYS)
    PLATFORM := Windows
else ifeq ($(UNAME_S),Darwin)
    PLATFORM := macOS
else
    PLATFORM := Linux
endif

SFML_LIBS := -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-system

ifeq ($(PLATFORM),Linux)
    LDFLAGS += $(SFML_LIBS)
endif

ifeq ($(PLATFORM),macOS)
    # Homebrew lives in different places on Apple Silicon and Intel.
    BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
    ifneq ($(BREW_PREFIX),)
        CXXFLAGS += -I$(BREW_PREFIX)/include
        LDFLAGS  += -L$(BREW_PREFIX)/lib
    endif
    LDFLAGS += $(SFML_LIBS)
endif

ifeq ($(PLATFORM),Windows)
    TARGET := $(TARGET).exe
    # SFML_ROOT lets you point at an extracted SFML archive:
    #     mingw32-make SFML_ROOT=C:/SFML-3.0.0
    ifneq ($(SFML_ROOT),)
        CXXFLAGS += -I$(SFML_ROOT)/include
        LDFLAGS  += -L$(SFML_ROOT)/lib
    endif
    LDFLAGS += $(SFML_LIBS) -lopengl32 -lwinmm -lgdi32
    # Drop the console window for release builds:
    #     mingw32-make release WINDOWED=1
    ifdef WINDOWED
        LDFLAGS += -mwindows
    endif
endif

# --------------------------------------------------------------- rules

.PHONY: all debug release run run-debug run-release clean rebuild info

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	@echo "  LINK    $@"
	@$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

# The order-only prerequisite on the directory keeps mkdir from
# retriggering the rule every time the directory's mtime changes.
$(BUILD)/%.o: %.cpp | $(BUILD)
	@mkdir -p $(dir $@)
	@echo "  CXX     $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD):
	@mkdir -p $(BUILD)

# Each mode has its own folder, so these never need a clean first.
# (They used to list clean as a prerequisite, which let `make -j` run
# it alongside the compile and delete objects mid-build.)
debug release:
	@$(MAKE) --no-print-directory all MODE=$@ EXAMPLE=$(EXAMPLE)

run: $(TARGET)
	@./$(TARGET)

run-debug run-release:
	@$(MAKE) --no-print-directory run MODE=$(@:run-%=%) EXAMPLE=$(EXAMPLE)

clean:
	@rm -rf build game game.exe
	@echo "  CLEAN   done"

rebuild:
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory all EXAMPLE=$(EXAMPLE)

info:
	@echo "example  : $(EXAMPLE)  (available: $(notdir $(wildcard examples/*)))"
	@echo "platform : $(PLATFORM)"
	@echo "compiler : $(CXX)"
	@echo "sources  : $(words $(SOURCES)) files"
	@echo "ldflags  : $(LDFLAGS)"

# Pulls in the generated header dependencies. The dash suppresses the
# warning on a clean tree where none exist yet.
-include $(DEPS)

# ---------------------------------------------------------------- tests

# Logic tests only -- nothing here opens a window, so it runs in CI.
TEST_TARGET  := run_tests
TEST_SOURCES := tests/test_framework.cpp \
                src/core/Easing.cpp \
                src/framework/Camera.cpp \
                src/framework/EventBus.cpp \
                src/framework/Particle.cpp \
                src/framework/ParticleSystem.cpp \
                src/gui/Widget.cpp \
                src/gui/TextWidget.cpp \
                src/gui/Button.cpp \
                src/gui/WidgetManager.cpp \
                src/utilities/markov.cpp

.PHONY: test
test:
	@$(CXX) -std=c++20 -O1 -g $(WARNINGS) $(TEST_SOURCES) -o $(TEST_TARGET) $(LDFLAGS)
	@./$(TEST_TARGET)

# ----------------------------------------------------- tetris rule tests

# Cascade's rules: bag, SRS kicks, line clearing, T-spins, scoring.
# No window needed.
TETRIS_TEST_TARGET  := run_tetris_tests
TETRIS_TEST_SOURCES := examples/tetris/tests.cpp \
                       examples/tetris/Board.cpp \
                       examples/tetris/Tetromino.cpp \
                       examples/tetris/GameState.cpp \
                       src/core/Easing.cpp

.PHONY: test-tetris
test-tetris:
	@$(CXX) -std=c++20 -O1 -g $(WARNINGS) $(TETRIS_TEST_SOURCES) -o $(TETRIS_TEST_TARGET) $(LDFLAGS)
	@./$(TETRIS_TEST_TARGET)
