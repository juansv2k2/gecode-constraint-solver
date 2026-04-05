# Makefile for Musical Constraint Solver
# Requires Gecode to be installed

CXX = g++
CXXFLAGS = -std=c++11 -O3 -DNDEBUG -Wall -g
LIBS = -lgecodedriver -lgecodeminimodel -lgecodeint -lgecodesearch -lgecodekernel -lgecodesupport -lgecodegist

# Try to detect Gecode installation
GECODE_INC = $(shell pkg-config --cflags gecode 2>/dev/null || echo "")
GECODE_LIB = $(shell pkg-config --libs gecode 2>/dev/null || echo "$(LIBS)")

# If pkg-config doesn't work, try common locations
ifeq ($(GECODE_INC),)
    GECODE_INC = -I/usr/local/include -I/opt/homebrew/include -I/opt/homebrew/opt/gecode/include
endif

ifeq ($(GECODE_LIB),$(LIBS))
    GECODE_LIB = -L/usr/local/lib -L/opt/homebrew/lib -L/opt/homebrew/opt/gecode/lib $(LIBS)
endif

TARGET = musical-solver
SOURCE = musical-solver.cpp

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(GECODE_INC) -o $@ $< $(GECODE_LIB)

clean:
	rm -f $(TARGET)

# Run examples
test: $(TARGET)
	@echo "Running basic model (no repetitions only):"
	@./$(TARGET) -solutions 3 -model basic
	@echo
	@echo "Running interval model (with interval constraints):"
	@./$(TARGET) -solutions 3 -model intervals
	@echo
	@echo "Running melodic model (with melodic movement rules):"
	@./$(TARGET) -solutions 3 -model melodic

# Check if Gecode is installed
check-gecode:
	@echo "Checking for Gecode installation..."
	@pkg-config --exists gecode && echo "Gecode found via pkg-config" || echo "Gecode not found via pkg-config, will try standard paths"
	@echo "Include flags: $(GECODE_INC)"
	@echo "Library flags: $(GECODE_LIB)"

install-deps-macos:
	@echo "Installing Gecode via Homebrew (macOS)..."
	brew install gecode

install-deps-ubuntu:
	@echo "Installing Gecode via apt (Ubuntu/Debian)..."
	sudo apt-get update
	sudo apt-get install libgecode-dev

.PHONY: all clean test check-gecode install-deps-macos install-deps-ubuntu