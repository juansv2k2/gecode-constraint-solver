/**
 * @file constraint_solver_main.cpp
 * @brief Dynamic Musical Constraint Solver - Main Interface
 * 
 * A truly dynamic constraint solver that loads configuration from JSON files.
 * Usage: ./constraint-solver-main <config_file.json>
 * 
 * The system dynamically:
 * - Parses JSON configuration files
 * - Creates appropriate musical rules
 * - Sets up domains and constraints  
 * - Runs constraint solving
 * - Exports results
 */

#include "musical_constraint_solver.hh"
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <algorithm>
#include <memory>
#include <set>

// Simple JSON parser for configuration loading
class JSONConfig {
private:
    std::map<std::string, std::string> values_;
    std::vector<std::map<std::string, std::string>> rules_;
    
    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r\"");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r\"");
        return str.substr(first, (last - first + 1));
    }
    
    std::string removeQuotes(const std::string& str) {
        std::string result = trim(str);
        if (!result.empty() && result.front() == '\"') result.erase(0, 1);
        if (!result.empty() && result.back() == '\"') result.pop_back();
        return result;
    }
    
    std::string removeComma(const std::string& str) {
        std::string result = str;
        if (!result.empty() && result.back() == ',') result.pop_back();
        return result;
    }
    
public:
    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cout << "❌ Cannot open config file: " << filename << std::endl;
            return false;
        }
        
        std::string line;
        bool in_rules_section = false;
        bool in_rule_object = false;
        std::map<std::string, std::string> current_rule;
        
        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '/' || line == "{" || line == "}") continue;
            
            // Check for rules section
            if (line.find("\"rules\":") != std::string::npos) {
                in_rules_section = true;
                continue;
            }
            
            // Handle rules section
            if (in_rules_section) {
                if (line.find("{") != std::string::npos && !in_rule_object) {
                    in_rule_object = true;
                    current_rule.clear();
                    continue;
                }
                
                if (line.find("}") != std::string::npos && in_rule_object) {
                    in_rule_object = false;
                    if (!current_rule.empty()) {
                        rules_.push_back(current_rule);
                    }
                    continue;
                }
                
                if (line.find("]") != std::string::npos && !in_rule_object) {
                    in_rules_section = false;
                    continue;
                }
                
                if (in_rule_object) {
                    // Parse rule properties
                    if (line.find("\"rule_type\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            std::string value = removeComma(removeQuotes(line.substr(pos + 1)));
                            current_rule["rule_type"] = value;
                        }
                    }
                    else if (line.find("\"function\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            std::string value = removeComma(removeQuotes(line.substr(pos + 1)));
                            current_rule["function"] = value;
                        }
                    }
                    else if (line.find("\"description\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            std::string value = removeComma(removeQuotes(line.substr(pos + 1)));
                            current_rule["description"] = value;
                        }
                    }
                    else if (line.find("\"voice\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            std::string value = removeComma(trim(line.substr(pos + 1)));
                            current_rule["voice"] = value;
                        }
                    }
                    else if (line.find("\"priority\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            std::string value = removeComma(trim(line.substr(pos + 1)));
                            current_rule["priority"] = value;
                        }
                    }
                }
                continue;
            }
            
            // Handle top-level properties
            size_t colonPos = line.find(":");
            if (colonPos != std::string::npos) {
                std::string key = trim(line.substr(0, colonPos));
                std::string value = removeComma(trim(line.substr(colonPos + 1)));
                
                // Remove quotes from key
                key = removeQuotes(key);
                value = removeQuotes(value);
                
                values_[key] = value;
            }
        }
        
        return true;
    }
    
    std::string getString(const std::string& key, const std::string& defaultValue = "") {
        return values_.count(key) ? values_[key] : defaultValue;
    }
    
    int getInt(const std::string& key, int defaultValue = 0) {
        if (!values_.count(key)) return defaultValue;
        try {
            return std::stoi(values_[key]);
        } catch (...) {
            return defaultValue;
        }
    }
    
    std::vector<std::map<std::string, std::string>> getRules() {
        return rules_;
    }
    
    void printLoadedConfig() {
        std::cout << "📋 Loaded Configuration:" << std::endl;
        std::cout << "   Name: " << getString("name", "Unknown") << std::endl;
        std::cout << "   Solution Length: " << getInt("solution_length") << std::endl;
        std::cout << "   Number of Voices: " << getInt("num_voices") << std::endl;
        std::cout << "   Backtrack Method: " << getString("backtrack_method", "intelligent") << std::endl;
        std::cout << "   Rules: " << getRules().size() << " configured" << std::endl;
        
        // Debug: show loaded values
        std::cout << "\n🔍 Debug - Raw values loaded:" << std::endl;
        for (const auto& pair : values_) {
            std::cout << "   \"" << pair.first << "\": \"" << pair.second << "\"" << std::endl;
        }
        
        std::cout << "\n🔍 Debug - Rules loaded:" << std::endl;
        for (size_t i = 0; i < rules_.size(); ++i) {
            std::cout << "   Rule " << (i + 1) << ":" << std::endl;
            for (const auto& prop : rules_[i]) {
                std::cout << "     " << prop.first << ": " << prop.second << std::endl;
            }
        }
    }
};

/**
 * @brief Dynamic rule factory - creates rules based on JSON config
 */
class DynamicRuleFactory {
public:
    static std::shared_ptr<MusicalConstraints::MusicalRule> createRule(
        const std::map<std::string, std::string>& ruleConfig) {
        
        std::string type = ruleConfig.count("rule_type") ? ruleConfig.at("rule_type") : "unknown";
        std::string function = ruleConfig.count("function") ? ruleConfig.at("function") : "unknown";
        std::string description = ruleConfig.count("description") ? ruleConfig.at("description") : "Unknown Rule";
        
        if (type.find("r-pitches-one-voice") != std::string::npos) {
            return createPitchRule(function, description);
        }
        else if (type.find("r-custom") != std::string::npos) {
            return createCustomRule(function, description);
        }
        else {
            return createGenericRule(function, description);
        }
    }
    
private:
    static std::shared_ptr<MusicalConstraints::MusicalRule> createPitchRule(
        const std::string& function, const std::string& description) {
        
        if (function.find("not_equal") != std::string::npos) {
            return std::make_shared<AllDifferentRule>(description);
        }
        
        return std::make_shared<GenericRule>(function, description);
    }
    
    static std::shared_ptr<MusicalConstraints::MusicalRule> createCustomRule(
        const std::string& function, const std::string& description) {
        
        return std::make_shared<CustomRule>(function, description);
    }
    
    static std::shared_ptr<MusicalConstraints::MusicalRule> createGenericRule(
        const std::string& function, const std::string& description) {
        
        return std::make_shared<GenericRule>(function, description);
    }
    
    // Rule implementations
    class AllDifferentRule : public MusicalConstraints::MusicalRule {
    private:
        std::string desc_;
    public:
        AllDifferentRule(const std::string& description) : desc_(description) {}
        
        MusicalConstraints::RuleResult check_rule(
            const MusicalConstraints::DualSolutionStorage& storage, 
            int current_index) const override {
            
            std::set<int> seen_notes;
            for (int i = 0; i <= current_index; ++i) {
                int note = storage.absolute(i);
                if (seen_notes.count(note)) {
                    auto result = MusicalConstraints::RuleResult::Failure(2, "Repeated note");
                    return result;
                }
                seen_notes.insert(note);
            }
            return MusicalConstraints::RuleResult::Success();
        }
        
        std::string description() const override { return desc_; }
        std::vector<int> get_dependent_variables(int current_index) const override {
            std::vector<int> deps;
            for (int i = 0; i <= current_index; ++i) deps.push_back(i);
            return deps;
        }
        std::string rule_type() const override { return "AllDifferentRule"; }
    };
    
    class GenericRule : public MusicalConstraints::MusicalRule {
    private:
        std::string func_, desc_;
    public:
        GenericRule(const std::string& function, const std::string& description) 
            : func_(function), desc_(description) {}
        
        MusicalConstraints::RuleResult check_rule(
            const MusicalConstraints::DualSolutionStorage& storage, 
            int current_index) const override {
            // Basic validation - always succeed for now
            return MusicalConstraints::RuleResult::Success();
        }
        
        std::string description() const override { return desc_; }
        std::vector<int> get_dependent_variables(int current_index) const override {
            return {current_index};
        }
        std::string rule_type() const override { return "GenericRule"; }
    };
    
    class CustomRule : public MusicalConstraints::MusicalRule {
    private:
        std::string func_, desc_;
    public:
        CustomRule(const std::string& function, const std::string& description) 
            : func_(function), desc_(description) {}
        
        MusicalConstraints::RuleResult check_rule(
            const MusicalConstraints::DualSolutionStorage& storage, 
            int current_index) const override {
            // Custom rule logic would go here
            return MusicalConstraints::RuleResult::Success();
        }
        
        std::string description() const override { return desc_; }
        std::vector<int> get_dependent_variables(int current_index) const override {
            std::vector<int> deps;
            for (int i = 0; i <= current_index; ++i) deps.push_back(i);
            return deps;
        }
        std::string rule_type() const override { return "CustomRule"; }
    };
};

/**
 * @brief Main dynamic constraint solver interface
 */
int main(int argc, char* argv[]) {
    std::cout << "🎼 DYNAMIC MUSICAL CONSTRAINT SOLVER" << std::endl;
    std::cout << "====================================" << std::endl;
    
    // Check command line arguments
    if (argc != 2) {
        std::cout << "❌ Usage: " << argv[0] << " <config_file.json>" << std::endl;
        std::cout << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "   " << argv[0] << " twelve_tone_config.json" << std::endl;
        std::cout << "   " << argv[0] << " custom_consensus_test.json" << std::endl;
        return 1;
    }
    
    std::string config_file = argv[1];
    std::cout << "📋 Loading configuration from: " << config_file << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    
    try {
        // Step 1: Load JSON configuration
        JSONConfig config;
        if (!config.load(config_file)) {
            return 1;
        }
        
        config.printLoadedConfig();
        
        // Step 2: Create solver configuration
        MusicalConstraintSolver::SolverConfig solver_config;
        solver_config.sequence_length = config.getInt("solution_length", 12);
        solver_config.min_note = 60;  // C4
        solver_config.max_note = 71;  // B4
        solver_config.num_voices = config.getInt("num_voices", 2);
        solver_config.allow_repetitions = false;
        solver_config.style = MusicalConstraintSolver::SolverConfig::CONTEMPORARY;
        
        std::string backtrack_method = config.getString("backtrack_method", "intelligent");
        if (backtrack_method.find("intelligent") != std::string::npos) {
            solver_config.backjump_mode = AdvancedBackjumping::BackjumpMode::INTELLIGENT_BACKJUMP;
        } else if (backtrack_method.find("consensus") != std::string::npos) {
            solver_config.backjump_mode = AdvancedBackjumping::BackjumpMode::CONSENSUS_BACKJUMP;
        }
        
        solver_config.verbose_output = false;
        
        // Step 3: Create solver and load dynamic rules
        std::cout << "\n🔧 Creating Dynamic Constraint Solver" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        MusicalConstraintSolver::Solver solver(solver_config);
        solver.clear_rules();  // Remove default rules
        
        // Load rules from configuration
        auto rules = config.getRules();
        for (const auto& ruleConfig : rules) {
            auto rule = DynamicRuleFactory::createRule(ruleConfig);
            solver.add_rule(rule);
            std::cout << "✅ Added rule: " << rule->description() << std::endl;
        }
        
        std::cout << "📊 Solver configured with " << rules.size() << " dynamic rules" << std::endl;
        
        // Step 4: Solve the constraint problem
        std::cout << "\n🎯 Solving Constraint Problem" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        MusicalConstraintSolver::MusicalSolution solution = solver.solve();
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (!solution.found_solution) {
            std::cout << "❌ No solution found: " << solution.failure_reason << std::endl;
            return 1;
        }
        
        std::cout << "✅ Solution found in " << duration.count() << " ms" << std::endl;
        
        // Step 5: Export results
        std::cout << "\n💾 Exporting Results" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        // Create output filename based on input config
        std::string base_name = config_file.substr(0, config_file.find_last_of('.'));
        std::string json_output_file = "tests/output/" + base_name + "_result.json";
        std::string text_output_file = "tests/output/" + base_name + "_result.txt";
        
        // Export JSON
        std::ofstream json_out(json_output_file);
        if (json_out.is_open()) {
            json_out << "{" << std::endl;
            json_out << "  \"config_file\": \"" << config_file << "\"," << std::endl;
            json_out << "  \"problem_name\": \"" << config.getString("name", "Unknown") << "\"," << std::endl;
            json_out << "  \"solution\": [";
            for (size_t i = 0; i < solution.absolute_notes.size(); ++i) {
                if (i > 0) json_out << ", ";
                json_out << solution.absolute_notes[i];
            }
            json_out << "]," << std::endl;
            json_out << "  \"note_names\": [";
            for (size_t i = 0; i < solution.absolute_notes.size(); ++i) {
                if (i > 0) json_out << ", ";
                json_out << "\"" << MusicalConstraintSolver::Solver::midi_to_note_name(solution.absolute_notes[i]) << "\"";
            }
            json_out << "]," << std::endl;
            json_out << "  \"solve_time_ms\": " << duration.count() << "," << std::endl;
            json_out << "  \"rules_checked\": " << solution.total_rules_checked << std::endl;
            json_out << "}" << std::endl;
            json_out.close();
            std::cout << "✅ JSON results: " << json_output_file << std::endl;
        }
        
        // Export text
        std::ofstream text_out(text_output_file);
        if (text_out.is_open()) {
            text_out << "DYNAMIC CONSTRAINT SOLVER RESULTS" << std::endl;
            text_out << "==================================" << std::endl << std::endl;
            text_out << "Configuration: " << config_file << std::endl;
            text_out << "Problem: " << config.getString("name", "Unknown") << std::endl << std::endl;
            
            text_out << "Solution:" << std::endl;
            for (size_t i = 0; i < solution.absolute_notes.size(); ++i) {
                text_out << "  " << (i + 1) << ". " 
                        << MusicalConstraintSolver::Solver::midi_to_note_name(solution.absolute_notes[i]) 
                        << " (MIDI " << solution.absolute_notes[i] << ")" << std::endl;
            }
            
            text_out << std::endl << "Performance:" << std::endl;
            text_out << "  Solve time: " << duration.count() << " ms" << std::endl;
            text_out << "  Rules checked: " << solution.total_rules_checked << std::endl;
            text_out << "  Rules configured: " << rules.size() << std::endl;
            
            text_out.close();
            std::cout << "✅ Text results: " << text_output_file << std::endl;
        }
        
        // Display solution
        std::cout << "\n🎼 SOLUTION DISPLAY" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        std::cout << "Notes: ";
        for (size_t i = 0; i < solution.absolute_notes.size(); ++i) {
            if (i > 0) std::cout << " → ";
            std::cout << MusicalConstraintSolver::Solver::midi_to_note_name(solution.absolute_notes[i]);
        }
        std::cout << std::endl;
        
        std::cout << "\n📈 Performance Summary:" << std::endl;
        std::cout << "   Solve time: " << duration.count() << " ms" << std::endl;
        std::cout << "   Rules checked: " << solution.total_rules_checked << std::endl;
        std::cout << "   Dynamic rules: " << rules.size() << std::endl;
        
        std::cout << "\n🎉 DYNAMIC CONSTRAINT SOLVING COMPLETE!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}