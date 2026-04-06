/**
 * @file test_integrated_space.cpp  
 * @brief Test IntegratedMusicalSpace in isolation
 */

#include "gecode_cluster_integration.hh" 
#include <iostream>

int main() {
    std::cout << "🔧 TESTING IntegratedMusicalSpace IN ISOLATION" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    try {
        std::cout << "📝 Creating IntegratedMusicalSpace..." << std::endl;
        
        GecodeClusterIntegration::IntegratedMusicalSpace* space = 
            new GecodeClusterIntegration::IntegratedMusicalSpace(3, 1, 
                AdvancedBackjumping::BackjumpMode::NO_BACKJUMPING);
        
        std::cout << "✅ IntegratedMusicalSpace created successfully" << std::endl;
        
        std::cout << "📝 Getting absolute sequence..." << std::endl;
        auto abs_seq = space->get_absolute_sequence();
        std::cout << "✅ Absolute sequence retrieved" << std::endl;
        
        std::cout << "📝 Getting interval sequence..." << std::endl;
        auto int_seq = space->get_interval_sequence();
        std::cout << "✅ Interval sequence retrieved" << std::endl;
        
        std::cout << "📝 Testing search..." << std::endl;
        Gecode::DFS<GecodeClusterIntegration::IntegratedMusicalSpace> engine(space);
        std::cout << "✅ Search engine created" << std::endl;
        
        auto solution = engine.next();
        std::cout << "✅ Search completed" << std::endl;
        
        if (solution) {
            std::cout << "✅ Found musical solution!" << std::endl;
            auto solution_abs = solution->get_absolute_sequence(); 
            std::cout << "   Notes: ";
            for (int note : solution_abs) {
                std::cout << note << " ";
            }
            std::cout << std::endl;
            delete solution;
        } else {
            std::cout << "⚠️  No solution found" << std::endl;
        }
        
        std::cout << "✅ INTEGRATED MUSICAL SPACE TEST COMPLETE!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown exception" << std::endl;
        return 1;
    }
}