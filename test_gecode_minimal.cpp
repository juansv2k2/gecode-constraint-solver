/**
 * @file test_gecode_minimal.cpp
 * @brief Absolute minimal test to isolate Gecode crash
 */

#include <iostream>
#include <gecode/int.hh>
#include <gecode/search.hh>
#include <gecode/driver.hh>

// Simplest possible Gecode space
class MinimalSpace : public Gecode::Space {
public:
    Gecode::IntVarArray vars;
    
    MinimalSpace() : vars(*this, 3, 0, 10) {
        std::cout << "  ✓ MinimalSpace constructor completed" << std::endl;
        Gecode::distinct(*this, vars);
        std::cout << "  ✓ Constraints added" << std::endl;
    }
    
    MinimalSpace(MinimalSpace& s) : Space(s) {
        vars.update(*this, s.vars);
    }
    
    virtual Space* copy() {
        return new MinimalSpace(*this);
    }
};

int main() {
    std::cout << "🔧 ABSOLUTE MINIMAL GECODE TEST" << std::endl;
    std::cout << "===============================" << std::endl;
    
    try {
        std::cout << "📝 Creating MinimalSpace..." << std::endl;
        MinimalSpace* space = new MinimalSpace();
        std::cout << "✅ MinimalSpace created successfully" << std::endl;
        
        std::cout << "📝 Creating search engine..." << std::endl;
        Gecode::DFS<MinimalSpace> engine(space);
        std::cout << "✅ Search engine created" << std::endl;
        
        std::cout << "📝 Searching for solution..." << std::endl;
        MinimalSpace* sol = engine.next();
        std::cout << "✅ Search completed" << std::endl;
        
        if (sol) {
            std::cout << "✅ Found solution: ";
            for (int i = 0; i < sol->vars.size(); ++i) {
                if (sol->vars[i].assigned()) {
                    std::cout << sol->vars[i].val() << " ";
                } else {
                    std::cout << "? ";
                }
            }
            std::cout << std::endl;
            delete sol;
        } else {
            std::cout << "⚠️  No solution found" << std::endl;
        }
        
        std::cout << "✅ MINIMAL GECODE TEST COMPLETE - NO CRASHES!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown exception" << std::endl;
        return 1;
    }
}