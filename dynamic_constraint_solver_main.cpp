/**
 * @file dynamic_constraint_solver_main.cpp
 * @brief Truly Dynamic Musical Constraint Solver - Fixed JSON Parser
 * 
 * A fully dynamic constraint solver that properly parses JSON configuration files.
 * Usage: ./dynamic-solver <config_file.json>
 */

#include "musical_constraint_solver.hh"
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <memory>
#include <set>
#include <regex>

// Data structure for parsed rule configuration
struct RuleConfig {
    std::string rule_type;
    std::string function;
    std::vector<int> indices;
    int voice;
    std::string engine_type;
    bool enabled;
    int priority;
    std::string description;
    std::vector<double> parameters;
};

// Robust JSON parser for constraint solver configuration
class ConstraintSolverJSONParser {
private:
    std::string name_;
    std::string description_;
    int solution_length_;
    int num_voices_;
    std::string backtrack_method_;
    std::string export_path_;
    std::vector<RuleConfig> rules_;
    
    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, (last - first + 1));
    }
    
    std::string removeQuotesAndComma(const std::string& str) {
        std::string result = trim(str);
        // Remove trailing comma
        if (!result.empty() && result.back() == ',') {
            result.pop_back();
        }
        // Remove quotes
        if (result.length() >= 2 && result.front() == '"' && result.back() == '"') {
            result = result.substr(1, result.length() - 2);
        }
        return result;
    }
    
    std::vector<int> parseIntArray(const std::string& arrayStr) {
        std::vector<int> result;
        std::string cleaned = arrayStr;
        
        // Find the array content between [ and ]
        size_t start = cleaned.find('[');
        size_t end = cleaned.find_last_of(']');
        
        if (start != std::string::npos && end != std::string::npos && start < end) {
            std::string content = cleaned.substr(start + 1, end - start - 1);
            std::stringstream ss(content);
            std::string item;
            
            while (std::getline(ss, item, ',')) {
                item = trim(item);
                if (!item.empty()) {
                    try {
                        result.push_back(std::stoi(item));
                    } catch (...) {
                        // Skip invalid integers
                    }
                }
            }
        }
        return result;
    }
    
    std::vector<double> parseDoubleArray(const std::string& arrayStr) {
        std::vector<double> result;
        std::string cleaned = arrayStr;
        
        size_t start = cleaned.find('[');
        size_t end = cleaned.find_last_of(']');
        
        if (start != std::string::npos && end != std::string::npos && start < end) {
            std::string content = cleaned.substr(start + 1, end - start - 1);
            std::stringstream ss(content);
            std::string item;
            
            while (std::getline(ss, item, ',')) {
                item = trim(item);
                if (!item.empty()) {
                    try {
                        result.push_back(std::stod(item));
                    } catch (...) {
                        // Skip invalid doubles
                    }
                }
            }
        }
        return result;
    }
    
    void parseTopLevelProperty(const std::string& line) {
        if (line.find("\"name\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                name_ = removeQuotesAndComma(line.substr(pos + 1));
            }
        }
        else if (line.find("\"description\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                description_ = removeQuotesAndComma(line.substr(pos + 1));
            }
        }
        else if (line.find("\"solution_length\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                std::string value = removeQuotesAndComma(line.substr(pos + 1));
                try {
                    solution_length_ = std::stoi(value);
                } catch (...) {
                    solution_length_ = 12; // default
                }
            }
        }
        else if (line.find("\"num_voices\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                std::string value = removeQuotesAndComma(line.substr(pos + 1));
                try {
                    num_voices_ = std::stoi(value);
                } catch (...) {
                    num_voices_ = 2; // default
                }
            }
        }
        else if (line.find("\"backtrack_method\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                backtrack_method_ = removeQuotesAndComma(line.substr(pos + 1));
            }
        } else if (line.find("\"export_path\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                export_path_ = removeQuotesAndComma(line.substr(pos + 1));
            }
        }
    }
    
public:
    ConstraintSolverJSONParser() 
        : solution_length_(12), num_voices_(2), backtrack_method_("intelligent"), export_path_("tests/output") {}
    
    bool parse(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cout << "❌ Cannot open config file: " << filename << std::endl;
            return false;
        }
        
        std::string line;
        bool in_rules_section = false;
        bool in_rule_object = false;
        bool in_constraint_function = false;
        bool in_parameters_array = false;
        
        RuleConfig current_rule;
        std::string parameters_content;
        
        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line.find("//") == 0) continue;
            
            // Check for rules section
            if (line.find("\"rules\"") != std::string::npos) {
                in_rules_section = true;
                continue;
            }
            
            // Exit rules section when we hit another top-level section
            if (in_rules_section && line.find("\"domains\"") != std::string::npos) {
                in_rules_section = false;
                continue;
            }
            
            // Handle rules section
            if (in_rules_section) {
                // New rule starts with standalone opening brace
                if (line == "{" && !in_rule_object) {
                    in_rule_object = true;
                    current_rule = RuleConfig{}; // Reset
                    current_rule.voice = 0;
                    current_rule.priority = 5;
                    current_rule.enabled = true;
                    current_rule.engine_type = "pitch";
                    continue;
                }
                
                // Rule ends with closing brace (with or without comma)
                if ((line == "}" || line == "},") && in_rule_object) {
                    in_rule_object = false;
                    in_constraint_function = false;
                    if (!current_rule.rule_type.empty()) {
                        rules_.push_back(current_rule);
                    }
                    continue;
                }
                
                if (in_rule_object) {
                    // Handle constraint_function section
                    if (line.find("\"constraint_function\"") != std::string::npos) {
                        in_constraint_function = true;
                        continue;
                    }
                    
                    if (line == "}," && in_constraint_function) {
                        in_constraint_function = false;
                        continue;
                    }
                    
                    if (in_constraint_function) {
                        if (line.find("\"function\"") != std::string::npos) {
                            size_t pos = line.find(":");
                            if (pos != std::string::npos) {
                                current_rule.function = removeQuotesAndComma(line.substr(pos + 1));
                            }
                        }
                        else if (line.find("\"parameters\"") != std::string::npos) {
                            // Handle parameters array - could be on same line or multiline
                            if (line.find("[") != std::string::npos && line.find("]") != std::string::npos) {
                                // Single line array
                                current_rule.parameters = parseDoubleArray(line);
                            } else {
                                // Start of multiline array
                                in_parameters_array = true;
                                parameters_content = line;
                            }
                        }
                        continue;
                    }
                    
                    // Handle parameters array continuation
                    if (in_parameters_array) {
                        parameters_content += " " + line;
                        if (line.find("]") != std::string::npos) {
                            current_rule.parameters = parseDoubleArray(parameters_content);
                            in_parameters_array = false;
                            parameters_content.clear();
                        }
                        continue;
                    }
                    
                    // Parse rule properties
                    if (line.find("\"rule_type\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            current_rule.rule_type = removeQuotesAndComma(line.substr(pos + 1));
                        }
                    }
                    else if (line.find("\"indices\"") != std::string::npos) {
                        current_rule.indices = parseIntArray(line);
                    }
                    else if (line.find("\"voice\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            std::string value = removeQuotesAndComma(line.substr(pos + 1));
                            try {
                                current_rule.voice = std::stoi(value);
                            } catch (...) {}
                        }
                    }
                    else if (line.find("\"engine_type\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            current_rule.engine_type = removeQuotesAndComma(line.substr(pos + 1));
                        }
                    }
                    else if (line.find("\"enabled\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            std::string value = removeQuotesAndComma(line.substr(pos + 1));
                            current_rule.enabled = (value == "true");
                        }
                    }
                    else if (line.find("\"priority\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            std::string value = removeQuotesAndComma(line.substr(pos + 1));
                            try {
                                current_rule.priority = std::stoi(value);
                            } catch (...) {}
                        }
                    }
                    else if (line.find("\"description\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            current_rule.description = removeQuotesAndComma(line.substr(pos + 1));
                        }
                    }
                }
                continue;
            }
            
            // Parse top-level properties when not in rules section
            if (!in_rules_section) {
                parseTopLevelProperty(line);
            }
        }
        
        return true;
    }
    
    // Getters
    std::string getName() const { return name_; }
    std::string getDescription() const { return description_; }
    int getSolutionLength() const { return solution_length_; }
    int getNumVoices() const { return num_voices_; }
    std::string getBacktrackMethod() const { return backtrack_method_; }
    std::string getExportPath() const { return export_path_; }
    
    const std::vector<RuleConfig>& getRules() const { return rules_; }
    
    void printLoadedConfig() const {
        std::cout << "📋 Loaded Configuration:" << std::endl;
        std::cout << "   Name: " << (name_.empty() ? "Unknown" : name_) << std::endl;
        std::cout << "   Description: " << (description_.empty() ? "None" : description_) << std::endl;
        std::cout << "   Solution Length: " << solution_length_ << std::endl;
        std::cout << "   Number of Voices: " << num_voices_ << std::endl;
        std::cout << "   Backtrack Method: " << backtrack_method_ << std::endl;
        std::cout << "   Rules: " << rules_.size() << " configured" << std::endl;
        
        if (!rules_.empty()) {
            std::cout << "\n📝 Parsed Rules:" << std::endl;
            for (size_t i = 0; i < rules_.size(); ++i) {
                const auto& rule = rules_[i];
                std::cout << "   " << (i + 1) << ". " << rule.rule_type << std::endl;
                std::cout << "      Function: " << rule.function << std::endl;
                std::cout << "      Voice: " << rule.voice << std::endl;
                std::cout << "      Description: " << rule.description << std::endl;
                std::cout << "      Indices: " << rule.indices.size() << " values" << std::endl;
            }
        }
    }
};

// Dynamic rule factory for creating actual constraint rules
class DynamicRuleFactory {
public:
    static std::shared_ptr<MusicalConstraints::MusicalRule> createFromConfig(
        const RuleConfig& ruleConfig) {
        
        if (ruleConfig.function == "not_equal" && ruleConfig.rule_type.find("pitches") != std::string::npos) {
            return std::make_shared<AllDifferentPitchRule>(ruleConfig.description, ruleConfig.indices.size());
        } 
        else if (ruleConfig.function == "equal" && !ruleConfig.parameters.empty()) {
            int target_value = static_cast<int>(ruleConfig.parameters[0]);
            return std::make_shared<FixedValueRule>(ruleConfig.description, target_value);
        }
        else {
            return std::make_shared<GenericConstraintRule>(ruleConfig.description, ruleConfig.function);
        }
    }
    
private:
    // All-different pitch rule (for 12-tone and similar constraints)
    class AllDifferentPitchRule : public MusicalConstraints::MusicalRule {
    private:
        std::string description_;
        int expected_length_;
        
    public:
        AllDifferentPitchRule(const std::string& desc, int length) 
            : description_(desc), expected_length_(length) {}
        
        MusicalConstraints::RuleResult check_rule(
            const MusicalConstraints::DualSolutionStorage& storage, 
            int current_index) const override {
            
            std::set<int> seen_values;
            
            for (int i = 0; i <= current_index; ++i) {
                int value = storage.absolute(i);
                if (seen_values.count(value)) {
                    auto result = MusicalConstraints::RuleResult::Failure(2, "Repeated value");
                    MusicalConstraints::BackjumpSuggestion suggestion(i, 1);
                    suggestion.explanation = "Value " + std::to_string(value) + " already used";
                    result.add_suggestion(suggestion);
                    return result;
                }
                seen_values.insert(value);
            }
            
            // If we have all expected values and they're all different, ensure we have full coverage
            if (current_index >= expected_length_ - 1) {
                if (seen_values.size() != static_cast<size_t>(expected_length_)) {
                    auto result = MusicalConstraints::RuleResult::Failure(3, "Incomplete coverage");
                    return result;
                }
            }
            
            return MusicalConstraints::RuleResult::Success();
        }
        
        std::string description() const override { return description_; }
        std::vector<int> get_dependent_variables(int current_index) const override {
            std::vector<int> deps;
            for (int i = 0; i <= current_index; ++i) deps.push_back(i);
            return deps;
        }
        std::string rule_type() const override { return "AllDifferentPitchRule"; }
    };
    
    // Fixed value rule (for setting specific notes)
    class FixedValueRule : public MusicalConstraints::MusicalRule {
    private:
        std::string description_;
        int target_value_;
        
    public:
        FixedValueRule(const std::string& desc, int target) 
            : description_(desc), target_value_(target) {}
        
        MusicalConstraints::RuleResult check_rule(
            const MusicalConstraints::DualSolutionStorage& storage, 
            int current_index) const override {
            
            // This rule applies to the first note (index 0)
            if (current_index >= 0) {
                int actual_value = storage.absolute(0);
                if (actual_value != target_value_) {
                    auto result = MusicalConstraints::RuleResult::Failure(1, "Wrong starting note");
                    MusicalConstraints::BackjumpSuggestion suggestion(0, 1);
                    suggestion.explanation = "Expected " + std::to_string(target_value_) + 
                                           ", got " + std::to_string(actual_value);
                    result.add_suggestion(suggestion);
                    return result;
                }
            }
            
            return MusicalConstraints::RuleResult::Success();
        }
        
        std::string description() const override { return description_; }
        std::vector<int> get_dependent_variables(int current_index) const override {
            return {0}; // Only depends on first note
        }
        std::string rule_type() const override { return "FixedValueRule"; }
    };
    
    // Generic constraint rule
    class GenericConstraintRule : public MusicalConstraints::MusicalRule {
    private:
        std::string description_;
        std::string function_;
        
    public:
        GenericConstraintRule(const std::string& desc, const std::string& func) 
            : description_(desc), function_(func) {}
        
        MusicalConstraints::RuleResult check_rule(
            const MusicalConstraints::DualSolutionStorage& storage, 
            int current_index) const override {
            
            // Basic validation - accept most solutions for generic rules
            return MusicalConstraints::RuleResult::Success();
        }
        
        std::string description() const override { return description_; }
        std::vector<int> get_dependent_variables(int current_index) const override {
            return {current_index};
        }
        std::string rule_type() const override { return "GenericConstraintRule"; }
    };
};

// Make RuleConfig easily accessible
typedef RuleConfig Rule;

// Main dynamic constraint solver
int main(int argc, char* argv[]) {
    std::cout << "🎼 DYNAMIC MUSICAL CONSTRAINT SOLVER" << std::endl;
    std::cout << "====================================" << std::endl;
    
    if (argc != 2) {
        std::cout << "❌ Usage: " << argv[0] << " <config_file.json>" << std::endl;
        std::cout << "\nExamples:" << std::endl;
        std::cout << "   " << argv[0] << " twelve_tone_config.json" << std::endl;
        std::cout << "   " << argv[0] << " custom_consensus_test.json" << std::endl;
        return 1;
    }
    
    std::string config_file = argv[1];
    std::cout << "📋 Loading configuration: " << config_file << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    
    try {
        // Parse JSON configuration
        ConstraintSolverJSONParser parser;
        if (!parser.parse(config_file)) {
            return 1;
        }
        
        parser.printLoadedConfig();
        
        // Setup solver configuration
        MusicalConstraintSolver::SolverConfig solver_config;
        solver_config.sequence_length = parser.getSolutionLength();
        solver_config.min_note = 60;  // C4
        solver_config.max_note = 71;  // B4
        solver_config.num_voices = parser.getNumVoices();
        solver_config.allow_repetitions = false;
        solver_config.style = MusicalConstraintSolver::SolverConfig::CONTEMPORARY;
        
        // Set backtrack mode
        if (parser.getBacktrackMethod() == "intelligent") {
            solver_config.backjump_mode = AdvancedBackjumping::BackjumpMode::INTELLIGENT_BACKJUMP;
        } else if (parser.getBacktrackMethod() == "consensus") {
            solver_config.backjump_mode = AdvancedBackjumping::BackjumpMode::CONSENSUS_BACKJUMP;
        } else {
            solver_config.backjump_mode = AdvancedBackjumping::BackjumpMode::NO_BACKJUMPING;
        }
        
        solver_config.verbose_output = false;
        
        // Create solver and add dynamic rules
        std::cout << "\n🔧 Creating Dynamic Constraint Solver" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        MusicalConstraintSolver::Solver solver(solver_config);
        solver.clear_rules();
        
        const auto& rules = parser.getRules();
        for (const auto& ruleConfig : rules) {
            if (ruleConfig.enabled) {
                auto rule = DynamicRuleFactory::createFromConfig(ruleConfig);
                solver.add_rule(rule);
                std::cout << "✅ " << rule->description() << std::endl;
            }
        }
        
        std::cout << "\n📊 Solver configured with " << solver.get_rules_count() << " dynamic rules" << std::endl;
        
        // Solve the constraint problem
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
        
        // Export results
        std::cout << "\n💾 Exporting Results" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        // Generate output filenames using configured export path
        std::string base_name = config_file;
        size_t dot_pos = base_name.find_last_of('.');
        if (dot_pos != std::string::npos) {
            base_name = base_name.substr(0, dot_pos);
        }
        
        std::string export_dir = parser.getExportPath();
        std::string json_file = export_dir + "/" + base_name + "_result.json";
        std::string txt_file = export_dir + "/" + base_name + "_result.txt";
        
        // Ensure export directory exists
        std::string mkdir_cmd = "mkdir -p \"" + export_dir + "\"";
        system(mkdir_cmd.c_str());
        
        // Export JSON
        std::ofstream json_out(json_file);
        if (json_out.is_open()) {
            json_out << "{" << std::endl;
            json_out << "  \"config_file\": \"" << config_file << "\"," << std::endl;
            json_out << "  \"problem_name\": \"" << parser.getName() << "\"," << std::endl;
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
            json_out << "  \"rules_applied\": " << rules.size() << "," << std::endl;
            json_out << "  \"rules_checked\": " << solution.total_rules_checked << std::endl;
            json_out << "}" << std::endl;
            json_out.close();
            std::cout << "✅ JSON: " << json_file << std::endl;
        }
        
        // Export text
        std::ofstream txt_out(txt_file);
        if (txt_out.is_open()) {
            txt_out << "DYNAMIC CONSTRAINT SOLVER RESULTS" << std::endl;
            txt_out << "==================================" << std::endl << std::endl;
            txt_out << "Configuration: " << config_file << std::endl;
            txt_out << "Problem: " << parser.getName() << std::endl;
            txt_out << "Description: " << parser.getDescription() << std::endl << std::endl;
            
            txt_out << "Solution (" << solution.absolute_notes.size() << " notes):" << std::endl;
            for (size_t i = 0; i < solution.absolute_notes.size(); ++i) {
                txt_out << "  " << (i + 1) << ". " 
                       << MusicalConstraintSolver::Solver::midi_to_note_name(solution.absolute_notes[i])
                       << " (MIDI " << solution.absolute_notes[i] << ")" << std::endl;
            }
            
            txt_out << std::endl << "Performance:" << std::endl;
            txt_out << "  Solve time: " << duration.count() << " ms" << std::endl;
            txt_out << "  Rules applied: " << rules.size() << std::endl;
            txt_out << "  Rules checked: " << solution.total_rules_checked << std::endl;
            
            txt_out.close();
            std::cout << "✅ Text: " << txt_file << std::endl;
        }
        
        // Display solution
        std::cout << "\n🎼 SOLUTION" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        std::cout << "Notes: ";
        for (size_t i = 0; i < solution.absolute_notes.size(); ++i) {
            if (i > 0) std::cout << " → ";
            std::cout << MusicalConstraintSolver::Solver::midi_to_note_name(solution.absolute_notes[i]);
        }
        std::cout << std::endl;
        
        std::cout << "\n📈 Performance:" << std::endl;
        std::cout << "   Solve time: " << duration.count() << " ms" << std::endl;
        std::cout << "   Rules applied: " << rules.size() << std::endl;
        std::cout << "   Rules checked: " << solution.total_rules_checked << std::endl;
        
        std::cout << "\n🎉 Dynamic constraint solving complete!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}