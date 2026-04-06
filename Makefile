# Makefile for Musical Constraint Solver
# Requires Gecode to be installed

CXX = g++
CXXFLAGS = -std=c++11 -O3 -DNDEBUG -Wall -g
LIBS = -lgecodeminimodel -lgecodeint -lgecodesearch -lgecodekernel -lgecodesupport -lgecodeflatzinc

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

# Foundation Architecture Test (no Gecode dependencies)
test_foundation_architecture: test_foundation_architecture.cpp include/enhanced_rule_architecture.hh include/dual_solution_storage.hh
	$(CXX) -std=c++17 -I./include -I./src test_foundation_architecture.cpp -o test_foundation_architecture

# Performance Revolution Test (no Gecode dependencies)  
test_performance_revolution: test_performance_revolution.cpp include/enhanced_rule_architecture.hh include/dual_solution_storage.hh
	$(CXX) -std=c++17 -I./include -I./src test_performance_revolution.cpp -o test_performance_revolution

# Complete Integration Test (no Gecode dependencies)
test_complete_integration: test_complete_integration.cpp include/enhanced_rule_architecture.hh include/advanced_backjumping_strategies.hh include/dual_solution_storage.hh src/advanced_backjumping_strategies.cpp
	$(CXX) -std=c++17 -I./include -I./src test_complete_integration.cpp src/advanced_backjumping_strategies.cpp -o test_complete_integration

# Advanced Backjumping Test (no Gecode dependencies)
test_advanced_backjumping: test_advanced_backjumping.cpp include/advanced_backjumping_strategies.hh src/advanced_backjumping_strategies.cpp 
	$(CXX) -std=c++17 -I./include -I./src test_advanced_backjumping.cpp src/advanced_backjumping_strategies.cpp -o test_advanced_backjumping

# GECODE INTEGRATION (PRODUCTION SYSTEM)
gecode_cluster_integration: test_gecode_cluster_integration.cpp include/gecode_cluster_integration.hh src/gecode_cluster_integration.cpp src/advanced_backjumping_strategies.cpp
	$(CXX) -std=c++17 $(GECODE_INC) -I./include -I./src test_gecode_cluster_integration.cpp src/gecode_cluster_integration.cpp src/advanced_backjumping_strategies.cpp $(GECODE_LIB) -o gecode_cluster_integration

# PRODUCTION MUSICAL CONSTRAINT SOLVER (COMPLETE SYSTEM)
PRODUCTION_SOLVER_TARGET = musical-constraint-solver
PRODUCTION_SOLVER_SOURCE = src/musical_constraint_solver.cpp
PRODUCTION_SOLVER_CXXFLAGS = -std=c++17 -O2 -Wall -g -Iinclude
PRODUCTION_SOLVER_LIBS = -lgecode -lgecodeminimodel -lgecodekernel -lgecodeint -lgecodesupport -lgecodeflatzinc

# Production Test Suite
PRODUCTION_TEST_TARGET = test-musical-solver
PRODUCTION_TEST_SOURCE = test_musical_constraint_solver.cpp src/musical_constraint_solver.cpp src/advanced_backjumping_strategies.cpp src/gecode_cluster_integration.cpp
PRODUCTION_TEST_CXXFLAGS = -std=c++17 -O2 -Wall -g -Iinclude

# Simple Validation System
SIMPLE_VALIDATION_TARGET = validate-cluster-gecode
SIMPLE_VALIDATION_SOURCE = simple_gecode_cluster_validation.cpp src/advanced_backjumping_strategies.cpp
SIMPLE_VALIDATION_CXXFLAGS = -std=c++17 -O2 -Wall -g -Iinclude

# Main Interface Test (lightweight)
MAIN_INTERFACE_TEST_TARGET = test-main-interface
MAIN_INTERFACE_TEST_SOURCE = test_main_interface.cpp src/musical_constraint_solver.cpp src/advanced_backjumping_strategies.cpp src/gecode_cluster_integration.cpp
MAIN_INTERFACE_TEST_CXXFLAGS = -std=c++17 -O2 -Wall -g -Iinclude

# Working Gecode Demo (full demonstration)
WORKING_DEMO_TARGET = test-working-demo
WORKING_DEMO_SOURCE = test_working_gecode_demo.cpp src/musical_constraint_solver.cpp src/advanced_backjumping_strategies.cpp src/gecode_cluster_integration.cpp
WORKING_DEMO_CXXFLAGS = -std=c++17 -O2 -Wall -g -Iinclude

# JSON Interface Test  
JSON_INTERFACE_TEST_TARGET = test-cluster-json-interface
JSON_INTERFACE_TEST_SOURCE = test_cluster_json_interface.cpp
JSON_INTERFACE_TEST_CXXFLAGS = -std=c++17 -O2 -Wall -g -Iinclude

# Custom Consensus Test
CUSTOM_CONSENSUS_TEST_TARGET = test-custom-consensus
CUSTOM_CONSENSUS_TEST_SOURCE = test_custom_consensus.cpp
CUSTOM_CONSENSUS_TEST_CXXFLAGS = -std=c++17 -O2 -Wall -g -Iinclude

# Cluster Engine Main Interface (Modular System)
CLUSTER_MAIN_INTERFACE_TARGET = cluster-engine-main
CLUSTER_MAIN_INTERFACE_SOURCE = cluster_engine_main_interface.cpp src/advanced_backjumping_strategies.cpp src/gecode_cluster_integration.cpp
CLUSTER_MAIN_INTERFACE_CXXFLAGS = -std=c++17 -O2 -Wall -g -Iinclude

# Cluster Engine Main Interface - Fixed Version (Modular System)
CLUSTER_MAIN_INTERFACE_FIXED_TARGET = cluster-engine-main-fixed
CLUSTER_MAIN_INTERFACE_FIXED_SOURCE = cluster_engine_main_interface_fixed.cpp src/advanced_backjumping_strategies.cpp src/gecode_cluster_integration.cpp
CLUSTER_MAIN_INTERFACE_FIXED_CXXFLAGS = -std=c++17 -O2 -Wall -g -Iinclude

all: $(TARGET) $(PHASE1_TARGET) $(PHASE2_TARGET) $(PHASE3_TARGET) $(PHASE4_TARGET) $(PHASE5_TARGET) $(CLUSTER_ENGINE_TARGET) $(CLUSTER_ENGINE_SIMPLE_TARGET) $(CLUSTER_ENGINE_STOP_TARGET) $(CLUSTER_ENGINE_BACKJUMP_TARGET) $(PRODUCTION_TEST_TARGET) $(SIMPLE_VALIDATION_TARGET) $(MAIN_INTERFACE_TEST_TARGET) $(WORKING_DEMO_TARGET) $(JSON_INTERFACE_TEST_TARGET) $(CUSTOM_CONSENSUS_TEST_TARGET) $(CLUSTER_MAIN_INTERFACE_TARGET) $(CLUSTER_MAIN_INTERFACE_FIXED_TARGET)

# Production system - main interface
$(PRODUCTION_SOLVER_TARGET): $(PRODUCTION_SOLVER_SOURCE) include/musical_constraint_solver.hh include/gecode_cluster_integration.hh
	$(CXX) $(PRODUCTION_SOLVER_CXXFLAGS) $(GECODE_INC) -o $@ $(PRODUCTION_SOLVER_SOURCE) $(GECODE_LIB) $(PRODUCTION_SOLVER_LIBS)

# Production test suite
$(PRODUCTION_TEST_TARGET): $(PRODUCTION_TEST_SOURCE) include/musical_constraint_solver.hh include/gecode_cluster_integration.hh
	$(CXX) $(PRODUCTION_TEST_CXXFLAGS) $(GECODE_INC) -o $@ $(PRODUCTION_TEST_SOURCE) $(GECODE_LIB)

# Simple validation system
$(SIMPLE_VALIDATION_TARGET): $(SIMPLE_VALIDATION_SOURCE) include/gecode_cluster_integration.hh
	$(CXX) $(SIMPLE_VALIDATION_CXXFLAGS) $(GECODE_INC) -o $@ $(SIMPLE_VALIDATION_SOURCE) $(GECODE_LIB)

# Main interface test (lightweight)
$(MAIN_INTERFACE_TEST_TARGET): $(MAIN_INTERFACE_TEST_SOURCE) include/musical_constraint_solver.hh
	$(CXX) $(MAIN_INTERFACE_TEST_CXXFLAGS) $(GECODE_INC) -o $@ $(MAIN_INTERFACE_TEST_SOURCE) $(GECODE_LIB)

# Working Gecode demo (full demonstration)
$(WORKING_DEMO_TARGET): $(WORKING_DEMO_SOURCE) include/musical_constraint_solver.hh
	$(CXX) $(WORKING_DEMO_CXXFLAGS) $(GECODE_INC) -o $@ $(WORKING_DEMO_SOURCE) $(GECODE_LIB)

# JSON interface test
$(JSON_INTERFACE_TEST_TARGET): $(JSON_INTERFACE_TEST_SOURCE) include/cluster_engine_json_interface.hh
	$(CXX) $(JSON_INTERFACE_TEST_CXXFLAGS) -o $@ $(JSON_INTERFACE_TEST_SOURCE)

# Custom consensus test
$(CUSTOM_CONSENSUS_TEST_TARGET): $(CUSTOM_CONSENSUS_TEST_SOURCE) include/cluster_engine_json_interface.hh
	$(CXX) $(CUSTOM_CONSENSUS_TEST_CXXFLAGS) -o $@ $(CUSTOM_CONSENSUS_TEST_SOURCE)

# Cluster Engine Main Interface (Modular System)
$(CLUSTER_MAIN_INTERFACE_TARGET): $(CLUSTER_MAIN_INTERFACE_SOURCE) include/enhanced_rule_architecture.hh include/advanced_backjumping_strategies.hh include/dual_solution_storage.hh include/gecode_cluster_integration.hh
	$(CXX) $(CLUSTER_MAIN_INTERFACE_CXXFLAGS) $(GECODE_INC) -o $@ $(CLUSTER_MAIN_INTERFACE_SOURCE) $(GECODE_LIB)

# Cluster Engine Main Interface - Fixed Version (Modular System)
$(CLUSTER_MAIN_INTERFACE_FIXED_TARGET): $(CLUSTER_MAIN_INTERFACE_FIXED_SOURCE) include/enhanced_rule_architecture.hh include/advanced_backjumping_strategies.hh include/dual_solution_storage.hh include/gecode_cluster_integration.hh
	$(CXX) $(CLUSTER_MAIN_INTERFACE_FIXED_CXXFLAGS) $(GECODE_INC) -o $@ $(CLUSTER_MAIN_INTERFACE_FIXED_SOURCE) $(GECODE_LIB)

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
	rm -f $(PRODUCTION_SOLVER_TARGET) $(PRODUCTION_TEST_TARGET) $(SIMPLE_VALIDATION_TARGET) $(MAIN_INTERFACE_TEST_TARGET) $(CLUSTER_MAIN_INTERFACE_TARGET) $(CLUSTER_MAIN_INTERFACE_FIXED_TARGET)
	rm -rf test_musical_states session_states integrated_states
	rm -rf ./professional_workspace ./batch_results ./professional_exports
	rm -rf ./professional_integration ./professional_studio

# Production system test runners
test-production: $(PRODUCTION_TEST_TARGET)
	@echo "🚀 RUNNING PRODUCTION MUSICAL CONSTRAINT SOLVER TEST SUITE"
	@echo "==========================================================="
	@./$(PRODUCTION_TEST_TARGET)

validate-production: $(SIMPLE_VALIDATION_TARGET)
	@echo "🔍 RUNNING CLUSTER-GECODE VALIDATION"
	@echo "===================================="
	@./$(SIMPLE_VALIDATION_TARGET)

# Main interface test (lightweight, safe)
test-main-interface: $(MAIN_INTERFACE_TEST_TARGET)
	@echo "🎼 TESTING MAIN MUSICAL CONSTRAINT SOLVER INTERFACE"
	@echo "==================================================="

# Working Gecode demo (full demonstration)
demo: $(WORKING_DEMO_TARGET)
	@echo "🎼 WORKING GECODE MUSICAL CONSTRAINT SOLVER DEMO"
	@echo "==============================================="
	@./$(WORKING_DEMO_TARGET)

# JSON interface test 
test-json-interface: $(JSON_INTERFACE_TEST_TARGET)
	@echo "🎼 TESTING CLUSTER ENGINE JSON INTERFACE"
	@echo "========================================"
	@./$(JSON_INTERFACE_TEST_TARGET)

# XML export test
test-xml-export:
	@echo "🎼 TESTING XML EXPORT SYSTEM"
	@echo "============================"
	@python3 test_xml_export.py

# Custom consensus backjump test
test-custom-consensus: $(CUSTOM_CONSENSUS_TEST_TARGET)
	@echo "🎼 TESTING CUSTOM CONSENSUS_BACKJUMP CONFIGURATION"
	@echo "=================================================="
	@./$(CUSTOM_CONSENSUS_TEST_TARGET)
	@./$(MAIN_INTERFACE_TEST_TARGET)

# Complete production validation
production-ready: $(PRODUCTION_TEST_TARGET) $(SIMPLE_VALIDATION_TARGET) $(MAIN_INTERFACE_TEST_TARGET)
	@echo "🏆 COMPLETE PRODUCTION SYSTEM VALIDATION"
	@echo "========================================"
	@echo "Running cluster-Gecode integration validation..."
	@./$(SIMPLE_VALIDATION_TARGET)
	@echo ""
	@echo "Running comprehensive production test suite..."
	@./$(PRODUCTION_TEST_TARGET)
	@echo ""
	@echo "✅ PRODUCTION SYSTEM FULLY VALIDATED AND READY!"

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