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

# Phase 4 State Persistence & Continuity Test (no Gecode dependencies)
PHASE4_TARGET = phase4-test
PHASE4_SOURCE = src/phase4_test.cpp
PHASE4_CXXFLAGS = -std=c++11 -O2 -Wall -g -Iinclude

# Phase 5 Professional Features Test (no Gecode dependencies)
PHASE5_TARGET = phase5-test
PHASE5_SOURCE = src/phase5_test.cpp
PHASE5_CXXFLAGS = -std=c++11 -O2 -Wall -g -Iinclude

# Phase 5 Simple Test (minimal features)
PHASE5_SIMPLE_TARGET = phase5-simple-test
PHASE5_SIMPLE_SOURCE = src/phase5_simple_test.cpp
PHASE5_SIMPLE_CXXFLAGS = -std=c++11 -O2 -Wall -g -Iinclude

# Phase 5 Demo (professional studio workflow)
PHASE5_DEMO_TARGET = phase5-demo
PHASE5_DEMO_SOURCE = src/phase5_demo.cpp
PHASE5_DEMO_CXXFLAGS = -std=c++11 -O2 -Wall -g -Iinclude

# Cluster Engine - Authentic Architecture (no Gecode dependencies)
CLUSTER_ENGINE_TARGET = cluster-engine-test
CLUSTER_ENGINE_SOURCE = src/cluster_engine_test.cpp src/cluster_engine_implementation.cpp
CLUSTER_ENGINE_CXXFLAGS = -std=c++11 -O2 -Wall -g -Iinclude

# Cluster Engine - Simple Test (working functionality only)
CLUSTER_ENGINE_SIMPLE_TARGET = cluster-engine-simple
CLUSTER_ENGINE_SIMPLE_SOURCE = src/cluster_engine_simple_test.cpp src/cluster_engine_implementation.cpp
CLUSTER_ENGINE_SIMPLE_CXXFLAGS = -std=c++11 -O2 -Wall -g -Iinclude

# Cluster Engine - Stop Rules Test (search termination system)
CLUSTER_ENGINE_STOP_TARGET = cluster-engine-stop-test
CLUSTER_ENGINE_STOP_SOURCE = src/cluster_engine_stop_rules_test.cpp src/cluster_engine_implementation.cpp
CLUSTER_ENGINE_STOP_CXXFLAGS = -std=c++11 -O2 -Wall -g -Iinclude

# Cluster Engine - Advanced Backjump Test (sophisticated backjumping system)
CLUSTER_ENGINE_BACKJUMP_TARGET = cluster-engine-backjump-test
CLUSTER_ENGINE_BACKJUMP_SOURCE = src/cluster_engine_backjump_test.cpp src/cluster_engine_implementation.cpp
CLUSTER_ENGINE_BACKJUMP_CXXFLAGS = -std=c++11 -O2 -Wall -g -Iinclude

all: $(TARGET) $(PHASE1_TARGET) $(PHASE2_TARGET) $(PHASE3_TARGET) $(PHASE4_TARGET) $(PHASE5_TARGET) $(CLUSTER_ENGINE_TARGET) $(CLUSTER_ENGINE_SIMPLE_TARGET) $(CLUSTER_ENGINE_STOP_TARGET) $(CLUSTER_ENGINE_BACKJUMP_TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(GECODE_INC) -o $@ $< $(GECODE_LIB)

$(PHASE1_TARGET): $(PHASE1_SOURCE)
	$(CXX) $(PHASE1_CXXFLAGS) -o $@ $<

$(PHASE2_TARGET): $(PHASE2_SOURCE)
	$(CXX) $(PHASE2_CXXFLAGS) -o $@ $<

$(PHASE3_TARGET): $(PHASE3_SOURCE)
	$(CXX) $(PHASE3_CXXFLAGS) -o $@ $<

$(PHASE4_TARGET): $(PHASE4_SOURCE)
	$(CXX) $(PHASE4_CXXFLAGS) -o $@ $<

$(PHASE5_TARGET): $(PHASE5_SOURCE)
	$(CXX) $(PHASE5_CXXFLAGS) -o $@ $<

$(PHASE5_SIMPLE_TARGET): $(PHASE5_SIMPLE_SOURCE)
	$(CXX) $(PHASE5_SIMPLE_CXXFLAGS) -o $@ $<

$(PHASE5_DEMO_TARGET): $(PHASE5_DEMO_SOURCE)
	$(CXX) $(PHASE5_DEMO_CXXFLAGS) -o $@ $<

$(CLUSTER_ENGINE_TARGET): $(CLUSTER_ENGINE_SOURCE)
	$(CXX) $(CLUSTER_ENGINE_CXXFLAGS) -o $@ $^

$(CLUSTER_ENGINE_SIMPLE_TARGET): $(CLUSTER_ENGINE_SIMPLE_SOURCE)
	$(CXX) $(CLUSTER_ENGINE_SIMPLE_CXXFLAGS) -o $@ $^

$(CLUSTER_ENGINE_STOP_TARGET): $(CLUSTER_ENGINE_STOP_SOURCE)
	$(CXX) $(CLUSTER_ENGINE_STOP_CXXFLAGS) -o $@ $^

$(CLUSTER_ENGINE_BACKJUMP_TARGET): $(CLUSTER_ENGINE_BACKJUMP_SOURCE)
	$(CXX) $(CLUSTER_ENGINE_BACKJUMP_CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGET) $(PHASE1_TARGET) $(PHASE2_TARGET) $(PHASE3_TARGET) $(PHASE4_TARGET) $(PHASE5_TARGET) $(PHASE5_SIMPLE_TARGET) $(PHASE5_DEMO_TARGET) $(CLUSTER_ENGINE_TARGET) $(CLUSTER_ENGINE_SIMPLE_TARGET) $(CLUSTER_ENGINE_STOP_TARGET) $(CLUSTER_ENGINE_BACKJUMP_TARGET)
	rm -rf test_musical_states session_states integrated_states
	rm -rf ./professional_workspace ./batch_results ./professional_exports
	rm -rf ./professional_integration ./professional_studio

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

test-phase4: $(PHASE4_TARGET)
	@echo "Running Phase 4: State Persistence & Continuity Test..."
	@./$(PHASE4_TARGET)

test-phase5: $(PHASE5_TARGET)
	@echo "Running Phase 5: Professional Features Test..."
	@./$(PHASE5_TARGET)

test-all: test test-phase1 test-phase2 test-phase3 test-phase4 test-phase5
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