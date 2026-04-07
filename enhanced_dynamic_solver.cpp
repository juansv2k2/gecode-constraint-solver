/**
 * @file enhanced_dynamic_solver.cpp
 * @brief Enhanced Dynamic Constraint Solver with Wildcard Rule Support
 * 
 * Extends the existing dynamic solver to support cluster-engine style
 * wildcard rules for pattern-based musical constraints.
 * 
 * Usage: ./enhanced-dynamic-solver <config_file.json>
 */

#include "wildcard_rule_extension.hh"
#include "musical_constraint_solver.hh"
#include "dynamic_rule_compiler.hh"
#include "rule_expression_parser.hh"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>

using namespace DynamicRules;
using namespace GecodeClusterIntegration;

class EnhancedDynamicSolver {
private:
    nlohmann::json config_;
    int num_voices_;
    int sequence_length_;
    std::vector<int> pitch_range_;
    std::vector<int> rhythm_values_;
    
public:
    EnhancedDynamicSolver() : num_voices_(2), sequence_length_(8) {
        pitch_range_ = {60, 72};
        rhythm_values_ = {1, 2, 4};
    }
    
    bool load_config(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "❌ Error: Could not open config file: " << filename << std::endl;
            return false;
        }
        
        try {
            file >> config_;
            
            // Parse configuration
            if (config_.contains("configuration")) {
                auto cfg = config_["configuration"];
                num_voices_ = cfg.value("num_voices", 2);
                sequence_length_ = cfg.value("sequence_length", 8);
                
                if (cfg.contains("pitch_range") && cfg["pitch_range"].is_array()) {
                    pitch_range_ = cfg["pitch_range"].get<std::vector<int>>();
                }
                
                if (cfg.contains("rhythm_values") && cfg["rhythm_values"].is_array()) {
                    rhythm_values_ = cfg["rhythm_values"].get<std::vector<int>>();
                }
            }
            
            std::cout << "✅ Configuration loaded: " << num_voices_ << " voices, " 
                      << sequence_length_ << " positions" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "❌ Error parsing config file: " << e.what() << std::endl;
            return false;
        }
    }
    
    void solve() {
        std::cout << "\n🎼 Enhanced Dynamic Musical Constraint Solver" << std::endl;
        std::cout << "=============================================" << std::endl;
        
        // Create musical space
        IntegratedMusicalSpace space(num_voices_, sequence_length_);
        
        // Initialize variables with proper ranges
        int total_vars = num_voices_ * sequence_length_;
        IntVarArray pitch_vars(space, total_vars, pitch_range_[0], pitch_range_[1]);
        IntVarArray rhythm_vars(space, total_vars, rhythm_values_[0], *std::max_element(rhythm_values_.begin(), rhythm_values_.end()));
        
        // Create constraint context
        ConstraintContext ctx(&space, &pitch_vars, &rhythm_vars, num_voices_, sequence_length_);
        
        std::cout << "🔧 Compiling and applying rules..." << std::endl;
        
        int rule_count = 0;
        int wildcard_count = 0;
        int regular_count = 0;
        
        // Process rules from configuration
        if (config_.contains("rules") && config_["rules"].is_array()) {
            for (const auto& rule_json : config_["rules"]) {
                try {
                    rule_count++;
                    std::string rule_type = rule_json.value("rule_type", "unknown");
                    std::string rule_id = rule_json.value("rule_id", "unnamed");
                    
                    std::cout << "  📝 Processing rule: " << rule_id << " (type: " << rule_type << ")" << std::endl;
                    
                    if (rule_type == "wildcard_constraint") {
                        // Handle wildcard rules
                        auto compiled = WildcardRuleCompiler::compile_wildcard_from_json(rule_json);
                        if (compiled && compiled->post_constraint) {
                            compiled->execute(ctx);
                            wildcard_count++;
                            std::cout << "    ✅ Wildcard rule applied successfully" << std::endl;
                        }
                    } else {
                        // Handle regular rules
                        auto compiled = DynamicRuleCompiler::compile_from_json(rule_json);
                        if (compiled && compiled->post_constraint) {
                            compiled->execute(ctx);
                            regular_count++;
                            std::cout << "    ✅ Regular rule applied successfully" << std::endl;
                        }
                    }
                    
                } catch (const std::exception& e) {
                    std::cout << "    ❌ Rule compilation failed: " << e.what() << std::endl;
                }
            }
        }
        
        std::cout << "\n📊 Constraint Summary:" << std::endl;
        std::cout << "  • Total rules processed: " << rule_count << std::endl;
        std::cout << "  • Wildcard rules applied: " << wildcard_count << std::endl;
        std::cout << "  • Regular rules applied: " << regular_count << std::endl;
        
        // Solve the constraint problem
        std::cout << "\n🔍 Searching for solution..." << std::endl;
        
        DFS<IntegratedMusicalSpace> engine(&space);
        IntegratedMusicalSpace* solution = engine.next();
        
        if (solution) {
            std::cout << "🎉 Solution found!" << std::endl;
            display_solution(*solution, pitch_vars, rhythm_vars);
            delete solution;
        } else {
            std::cout << "❌ No solution found. Constraints may be over-constrained." << std::endl;
            analyze_failure(ctx);
        }
    }
    
private:
    void display_solution(IntegratedMusicalSpace& solution, 
                         const IntVarArray& pitch_vars, 
                         const IntVarArray& rhythm_vars) {
        
        std::cout << "\n🎵 Musical Solution:" << std::endl;
        std::cout << "===================" << std::endl;
        
        for (int voice = 0; voice < num_voices_; ++voice) {
            std::cout << "Voice " << voice << " - Pitches: ";
            for (int pos = 0; pos < sequence_length_; ++pos) {
                int var_index = voice * sequence_length_ + pos;
                std::cout << solution.get_absolute_vars()[var_index].val() << " ";
            }
            std::cout << "\n         Rhythms:  ";
            for (int pos = 0; pos < sequence_length_; ++pos) {
                int var_index = voice * sequence_length_ + pos; 
                std::cout << solution.get_interval_vars()[var_index].val() << " ";
            }
            std::cout << std::endl;
        }
        
        // Musical analysis
        analyze_solution(solution, pitch_vars, rhythm_vars);
    }
    
    void analyze_solution(IntegratedMusicalSpace& solution,
                         const IntVarArray& pitch_vars,
                         const IntVarArray& rhythm_vars) {
        
        std::cout << "\n🧭 Musical Analysis:" << std::endl;
        
        // Analyze intervals in each voice
        for (int voice = 0; voice < num_voices_; ++voice) {
            std::vector<int> intervals;
            for (int pos = 1; pos < sequence_length_; ++pos) {
                int prev_idx = voice * sequence_length_ + (pos - 1);
                int curr_idx = voice * sequence_length_ + pos;
                int interval = solution.get_absolute_vars()[curr_idx].val() - solution.get_absolute_vars()[prev_idx].val();
                intervals.push_back(interval);
            }
            
            std::cout << "  Voice " << voice << " intervals: ";
            for (int interval : intervals) {
                std::cout << (interval >= 0 ? "+" : "") << interval << " ";
            }
            
            // Count stepwise motion
            int stepwise_count = 0;
            for (int interval : intervals) {
                if (std::abs(interval) <= 2) stepwise_count++;
            }
            
            double stepwise_percent = (double)stepwise_count / intervals.size() * 100;
            std::cout << "(" << stepwise_percent << "% stepwise)";
            std::cout << std::endl;
        }
    }
    
    void analyze_failure(const ConstraintContext& ctx) {
        std::cout << "\n🔍 Constraint Analysis (Failure Investigation):" << std::endl;
        std::cout << "• Check if constraints are too restrictive" << std::endl;
        std::cout << "• Verify wildcard rules are not over-constraining" << std::endl;
        std::cout << "• Consider relaxing some constraint weights" << std::endl;
        std::cout << "• Try reducing sequence length or number of voices" << std::endl;
        
        std::cout << "\n💡 Suggestions:" << std::endl;
        std::cout << "• Remove some wildcard constraints and retry" << std::endl;
        std::cout << "• Use heuristic rules instead of hard constraints" << std::endl;
        std::cout << "• Increase pitch/rhythm range for more flexibility" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    std::cout << "🚀 Enhanced Dynamic Musical Constraint Solver" << std::endl;
    std::cout << "With Cluster Engine Wildcard Rule Support" << std::endl;
    std::cout << "=========================================" << std::endl;
    
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <config_file.json>" << std::endl;
        std::cout << "\nExample config files:" << std::endl;
        std::cout << "• wildcard_example.json - Wildcard rule demonstrations" << std::endl;
        std::cout << "• Use: ./wildcard_rule_test --demo to create examples" << std::endl;
        return 1;
    }
    
    EnhancedDynamicSolver solver;
    
    if (!solver.load_config(argv[1])) {
        std::cerr << "Failed to load configuration file." << std::endl;
        return 1;
    }
    
    try {
        solver.solve();
    } catch (const std::exception& e) {
        std::cerr << "❌ Solver error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}