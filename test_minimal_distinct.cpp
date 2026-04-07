#include <gecode/int.hh>
#include <gecode/search.hh>
#include <gecode/minimodel.hh>
#include <iostream>

using namespace Gecode;

class MinimalDistinctTest : public Space {
public:
    IntVarArray vars;
    
    MinimalDistinctTest() : vars(*this, 2, 60, 71) {
        std::cout << "Creating 2 variables with domain [60, 71]" << std::endl;
        
        // Post distinct constraint
        distinct(*this, vars);
        std::cout << "Posted distinct constraint" << std::endl;
        
        // Use first-fail variable selection and smallest value selection
        branch(*this, vars, INT_VAR_SIZE_MIN(), INT_VAL_MIN());
    }
    
    MinimalDistinctTest(MinimalDistinctTest& s) : Space(s) {
        vars.update(*this, s.vars);
    }
    
    virtual Space* copy() override {
        return new MinimalDistinctTest(*this);
    }
    
    void print() {
        std::cout << "Solution: ";
        for (int i = 0; i < vars.size(); ++i) {
            std::cout << vars[i] << " ";
        }
        std::cout << std::endl;
    }
};

int main() {
    std::cout << "🔬 MINIMAL DISTINCT CONSTRAINT TEST" << std::endl;
    std::cout << "Testing Gecode distinct constraint in isolation" << std::endl;
    
    MinimalDistinctTest* problem = new MinimalDistinctTest();
    DFS<MinimalDistinctTest> engine(problem);
    delete problem;
    
    std::cout << "\n🔍 SEARCHING FOR SOLUTIONS:" << std::endl;
    int solution_count = 0;
    
    while (MinimalDistinctTest* solution = engine.next()) {
        solution_count++;
        std::cout << "Solution " << solution_count << ": ";
        solution->print();
        
        if (solution_count >= 5) {
            std::cout << "... (stopping after 5 solutions)" << std::endl;
            delete solution;
            break;
        }
        delete solution;
    }
    
    if (solution_count == 0) {
        std::cout << "❌ NO SOLUTIONS FOUND - This indicates a problem with the constraint!" << std::endl;
    } else {
        std::cout << "\n✅ Found " << solution_count << " solutions" << std::endl;
    }
    
    return 0;
}