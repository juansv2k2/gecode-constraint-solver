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

# Phase 1 Foundation Architecture Test (no Gecode dependencies)
PHASE1_TARGET = phase1-test
PHASE1_SOURCE = src/phase1_test.cpp
PHASE1_CXXFLAGS = -std=c++11 -O2 -Wall -g -Iinclude

# Phase 2 Performance Revolution Test (no Gecode dependencies)
PHASE2_TARGET = phase2-test
PHASE2_SOURCE = src/phase2_test.cpp
PHASE2_CXXFLAGS = -std=c++11 -O2 -Wall -g -Iinclude

# Phase 3 Advanced Musical Intelligence Test (no Gecode dependencies)
PHASE3_TARGET = phase3-test
PHASE3_SOURCE = src/phase3_test.cpp
PHASE3_CXXFLAGS = -std=c++11 -O2 -Wall -g -Iinclude

all: $(TARGET) $(PHASE1_TARGET) $(PHASE2_TARGET) $(PHASE3_TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(GECODE_INC) -o $@ $< $(GECODE_LIB)

$(PHASE1_TARGET): $(PHASE1_SOURCE)
	$(CXX) $(PHASE1_CXXFLAGS) -o $@ $<

$(PHASE2_TARGET): $(PHASE2_SOURCE)
	$(CXX) $(PHASE2_CXXFLAGS) -o $@ $<

$(PHASE3_TARGET): $(PHASE3_SOURCE)
	$(CXX) $(PHASE3_CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) $(PHASE1_TARGET) $(PHASE2_TARGET) $(PHASE3_TARGET)

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

# Phase testing
test-phase1: $(PHASE1_TARGET)
	@echo "Running Phase 1: Foundation Architecture Test..."
	@./$(PHASE1_TARGET)

test-phase2: $(PHASE2_TARGET)
	@echo "Running Phase 2: Performance Revolution Test..."
	@./$(PHASE2_TARGET)

test-phase3: $(PHASE3_TARGET)
	@echo "Running Phase 3: Advanced Musical Intelligence Test..."
	@./$(PHASE3_TARGET)

test-all: test test-phase1 test-phase2 test-phase3

# Test Phase 1 Foundation Architecture
test-phase1: $(PHASE1_TARGET)
	@echo "Running Phase 1: Foundation Architecture Test Suite..."
	@./$(PHASE1_TARGET)

# Full test suite
test-all: test test-phase1
	@echo "All tests completed!"

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

.PHONY: all clean test test-phase1 test-all check-gecode install-deps-macos install-deps-ubuntu