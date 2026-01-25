#         (make) - Build, only recompiles changed files
#   (make clean) - Remove build files
# (make rebuild) - Clean and build from scratch

# Defines which C++ compiler to use
CXX = g++

# Use C++ standard; Use optimization level 2
# -Wall; Enable all common warnings
# -Wextra ; Enable extra warnings
CXXFLAGS = -std=c++20 -O2

# Linker flags
LDFLAGS = -lsfml-system -lsfml-window -lsfml-graphics

# Source files
SOURCES = $(wildcard src/*.cpp) \
          $(wildcard src/gui/*.cpp) \
          $(wildcard src/framework/*.cpp) \
          $(wildcard src/utilities/*.cpp)

# Object files (one per source)
OBJECTS = $(SOURCES:.cpp=.o)

# Output executable
TARGET = game

# Build target
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile .cpp to .o (incremental compilation!)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJECTS) $(TARGET)

# Rebuild everything
rebuild: clean $(TARGET)

.PHONY: clean rebuild
