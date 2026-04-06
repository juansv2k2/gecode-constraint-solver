/**
 * @file test_multiple_twelve_tone_rows.cpp
 * @brief Generate multiple different 12-tone rows with various starting notes
 * 
 * Shows the flexibility of the system to generate different serial compositions.
 */

#include "musical_constraint_solver.hh"
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <set>

using namespace MusicalConstraintSolver;

/**
 * @brief Custom 12-tone rule that allows different starting notes
 */
class FlexibleTwelveToneRule : public MusicalConstraints::MusicalRule {
private:
    int starting_note_;
    std::vector<int> forbidden_intervals_;
    
public:
    FlexibleTwelveToneRule(int starting_note = 60, std::vector<int> forbidden = {}) 
        : starting_note_(starting_note), forbidden_intervals_(forbidden) {}
    
    MusicalConstraints::RuleResult check_rule(const MusicalConstraints::DualSolutionStorage& storage, 
                                            int current_index) const override {
        // First note must be the starting note
        if (current_index == 0 && storage.absolute(0) != starting_note_) {
            auto result = MusicalConstraints::RuleResult::Failure(1, "Wrong starting note");
            return result;
        }
        
        // Check for pitch class repetitions
        std::set<int> pitch_classes;
        for (int i = 0; i <= current_index; ++i) {
            int pitch_class = storage.absolute(i) % 12;
            if (pitch_classes.count(pitch_class) > 0) {
                auto result = MusicalConstraints::RuleResult::Failure(2, "Repeated pitch class");
                return result;
            }
            pitch_classes.insert(pitch_class);
        }
        
        // Forbidden interval check (adds musical character)
        if (current_index > 0) {
            int interval = std::abs(storage.absolute(current_index) - storage.absolute(current_index - 1));
            for (int forbidden : forbidden_intervals_) {
                if (interval == forbidden) {
                    auto result = MusicalConstraints::RuleResult::Failure(3, "Forbidden interval");
                    return result;
                }
            }
        }
        
        // Final validation at completion
        if (current_index >= 11 && pitch_classes.size() != 12) {
            auto result = MusicalConstraints::RuleResult::Failure(4, "Incomplete 12-tone row");
            return result;
        }
        
        return MusicalConstraints::RuleResult::Success();
    }
    
    std::string description() const override { 
        return "Flexible 12-Tone Row (start: " + Solver::midi_to_note_name(starting_note_) + ")"; 
    }
    std::vector<int> get_dependent_variables(int current_index) const override {
        std::vector<int> deps;
        for (int i = 0; i <= std::min(current_index, 11); ++i) {
            deps.push_back(i);
        }
        return deps;
    }
    std::string rule_type() const override { return "FlexibleTwelveToneRule"; }
};

/**
 * @brief Generate retrograde inversion with proper interval inversion
 */
std::vector<int> compute_retrograde_inversion(const std::vector<int>& original) {
    std::vector<int> result;
    if (original.empty()) return result;
    
    // Start from the last note
    result.push_back(original.back());
    
    // Work backwards, inverting intervals
    for (int i = original.size() - 2; i >= 0; --i) {
        int forward_interval = original[i + 1] - original[i];
        int inverted_interval = -forward_interval;
        int new_note = result.back() + inverted_interval;
        
        // Keep in range
        while (new_note < 60) new_note += 12;
        while (new_note > 71) new_note -= 12;
        
        result.push_back(new_note);
    }
    
    return result;
}

/**
 * @brief Print row with analysis
 */
void print_row_analysis(const std::vector<int>& row, const std::string& title, int row_number) {
    std::cout << "\n" << row_number << ". " << title << std::endl;
    std::cout << std::string(title.length() + 4, '-') << std::endl;
    
    std::cout << "Notes: ";
    for (size_t i = 0; i < row.size(); ++i) {
        if (i > 0) std::cout << " → ";
        std::cout << Solver::midi_to_note_name(row[i]);
    }
    std::cout << std::endl;
    
    std::cout << "P.C.: ";
    for (size_t i = 0; i < row.size(); ++i) {
        if (i > 0) std::cout << " ";
        std::cout << (row[i] % 12);
    }
    std::cout << std::endl;
    
    // Show intervals
    std::cout << "Intervals: ";
    for (size_t i = 1; i < row.size(); ++i) {
        if (i > 1) std::cout << ", ";
        int interval = row[i] - row[i-1];
        std::cout << std::showpos << interval << std::noshowpos;
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "🎼 MULTIPLE 12-TONE ROW GENERATOR" << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << "Generating different 12-tone rows with various characteristics" << std::endl;
    
    // Test configurations
    struct TestConfig {
        std::string name;
        int starting_note;
        std::vector<int> forbidden_intervals;
        std::string description;
    };
    
    std::vector<TestConfig> test_configs = {
        {"Chromatic Start on C4", 60, {}, "Starting on C4, no interval restrictions"},
        {"Start on F#4", 66, {}, "Starting on F#4 (tritone from C)"},
        {"No Large Jumps", 60, {7, 8, 9, 10, 11, 12}, "Starting on C4, no intervals larger than 6 semitones"},
        {"Avoid Perfect 4th/5th", 64, {5, 7}, "Starting on E4, avoiding perfect 4th and 5th"},
        {"Small Intervals Only", 67, {4, 5, 6, 7, 8, 9, 10, 11, 12}, "Starting on G4, only steps and minor 3rds"}
    };
    
    int row_count = 1;
    
    for (const auto& config : test_configs) {
        std::cout << "\n🎯 Generating: " << config.name << std::endl;
        std::cout << "   " << config.description << std::endl;
        std::cout << "   " << std::string(50, '-') << std::endl;
        
        try {
            // Configure solver
            SolverConfig solver_config;
            solver_config.sequence_length = 12;
            solver_config.min_note = 60;
            solver_config.max_note = 71;
            solver_config.allow_repetitions = false;
            solver_config.style = SolverConfig::CONTEMPORARY;
            solver_config.backjump_mode = AdvancedBackjumping::BackjumpMode::INTELLIGENT_BACKJUMP;
            solver_config.verbose_output = false;  // Quiet for multiple tests
            
            // Create solver with custom rule
            Solver solver(solver_config);
            solver.clear_rules();
            solver.add_rule(std::make_shared<FlexibleTwelveToneRule>(config.starting_note, config.forbidden_intervals));
            
            // Generate solution
            auto start_time = std::chrono::high_resolution_clock::now();
            MusicalSolution solution = solver.solve();
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            if (solution.found_solution) {
                // Generate retrograde inversion
                std::vector<int> retrograde_inversion = compute_retrograde_inversion(solution.absolute_notes);
                
                // Print analysis
                print_row_analysis(solution.absolute_notes, config.name + " (Original Row)", row_count++);
                print_row_analysis(retrograde_inversion, config.name + " (Retrograde Inversion)", row_count++);
                
                std::cout << "⏱️  Generated in " << duration.count() << " ms" << std::endl;
                
                // Verify 12-tone property
                std::set<int> pitch_classes;
                for (int note : solution.absolute_notes) {
                    pitch_classes.insert(note % 12);
                }
                std::cout << "✅ Valid 12-tone row: " << (pitch_classes.size() == 12 ? "YES" : "NO") << std::endl;
                
            } else {
                std::cout << "❌ Failed to generate row: " << solution.failure_reason << std::endl;
                std::cout << "   This constraint set may be too restrictive" << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "❌ Error: " << e.what() << std::endl;
        }
    }
    
    std::cout << "\n🎭 SUMMARY" << std::endl;
    std::cout << "=========" << std::endl;
    std::cout << "The system demonstrates flexibility in generating 12-tone rows" << std::endl;
    std::cout << "with different starting notes and interval restrictions." << std::endl;
    std::cout << "Each row uses all 12 chromatic pitch classes exactly once," << std::endl;
    std::cout << "following the principles of Schoenberg's twelve-tone technique." << std::endl;
    
    return 0;
}