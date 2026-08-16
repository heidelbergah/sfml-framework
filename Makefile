# SFML 3 Framework
#
#   make              Build (incremental, header-aware)
#   make debug        Build with -g -O0 and sanitizers
#   make release      Build with -O2 -DNDEBUG
#   make run          Build and run
#   make clean        Remove build artefacts
#   make rebuild      Clean and build from scratch
#
# Works on Linux, macOS and Windows (MSYS2/MinGW). The platform block
# below picks the right include paths and link flags; everything else is
# shared.

# ---------------------------------------------------------------- setup

CXX      ?= g++
TARGET   := game
BUILD    := build

# Objects live under build/ instead of next to their sources, so the
# tree stays clean and `make clean` cannot miss one.
SRC_DIRS := src src/core src/framework src/gui src/scenes src/utilities \
            examples/tetris

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
CXXFLAGS ?= -std=c++20 -O2
CXXFLAGS += $(WARNINGS) -MMD -MP

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
    TARGET := game.exe
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

.PHONY: all debug release run clean rebuild info

all: $(TARGET)

$(TARGET): $(OBJECTS)
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

debug: CXXFLAGS := -std=c++20 -O0 -g $(WARNINGS) -MMD -MP \
                   -fsanitize=address,undefined -fno-omit-frame-pointer
debug: LDFLAGS  += -fsanitize=address,undefined
debug: clean $(TARGET)

release: CXXFLAGS := -std=c++20 -O2 -DNDEBUG $(WARNINGS) -MMD -MP
release: clean $(TARGET)

run: $(TARGET)
	@./$(TARGET)

clean:
	@rm -rf $(BUILD) $(TARGET) game game.exe
	@echo "  CLEAN   done"

rebuild: clean all

info:
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
