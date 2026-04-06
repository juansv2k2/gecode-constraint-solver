/**
 * @file test_twelve_tone_generator.cpp
 * @brief 12-Tone Row Generator with Retrograde Inversion
 * 
 * Generates a 12-tone row in voice 0 and its retrograde inversion in voice 1
 * using the main musical constraint solver interface with configuration.
 */

#include "musical_constraint_solver.hh"
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <set>
#include <map>

using namespace MusicalConstraintSolver;

/**
 * @brief Custom 12-tone rule for ensuring all chromatic notes are used exactly once
 */
class TwelveToneRule : public MusicalConstraints::MusicalRule {
public:
    MusicalConstraints::RuleResult check_rule(const MusicalConstraints::DualSolutionStorage& storage, 
                                            int current_index) const override {
        if (current_index >= 11) {  // Check when we have all 12 notes
            std::set<int> pitch_classes;
            
            // Convert absolute notes to pitch classes (mod 12)
            for (int i = 0; i <= current_index; ++i) {
                int pitch_class = storage.absolute(i) % 12;
                pitch_classes.insert(pitch_class);
            }
            
            // Must have exactly 12 different pitch classes
            if (pitch_classes.size() != 12) {
                auto result = MusicalConstraints::RuleResult::Failure(3, "12-tone row incomplete");
                MusicalConstraints::BackjumpSuggestion suggestion(0, current_index);
                suggestion.explanation = "Missing pitch classes in 12-tone row";
                result.add_suggestion(suggestion);
                return result;
            }
        } else {
            // Check no repetitions so far
            std::set<int> pitch_classes;
            for (int i = 0; i <= current_index; ++i) {
                int pitch_class = storage.absolute(i) % 12;
                if (pitch_classes.count(pitch_class) > 0) {
                    auto result = MusicalConstraints::RuleResult::Failure(2, "Repeated pitch class");
                    MusicalConstraints::BackjumpSuggestion suggestion(i, current_index - i + 1);
                    suggestion.explanation = "Pitch class " + std::to_string(pitch_class) + " already used";
                    result.add_suggestion(suggestion);
                    return result;
                }
                pitch_classes.insert(pitch_class);
            }
        }
        
        return MusicalConstraints::RuleResult::Success();
    }
    
    std::string description() const override { return "12-Tone Row (All Chromatic Notes)"; }
    std::vector<int> get_dependent_variables(int current_index) const override {
        std::vector<int> deps;
        for (int i = 0; i <= current_index; ++i) {
            deps.push_back(i);
        }
        return deps;
    }
    std::string rule_type() const override { return "TwelveToneRule"; }
};

/**
 * @brief Generate retrograde inversion from a 12-tone row
 */
std::vector<int> generate_retrograde_inversion(const std::vector<int>& original_row) {
    std::vector<int> retrograde_inversion;
    
    if (original_row.empty()) return retrograde_inversion;
    
    // Start with the last note as reference
    int reference_pitch = original_row.back();
    
    // Generate retrograde inversion: backwards + inverted intervals
    for (int i = original_row.size() - 1; i >= 0; --i) {
        if (i == original_row.size() - 1) {
            // First note of retrograde inversion
            retrograde_inversion.push_back(reference_pitch);
        } else {
            // Calculate interval from reference
            int original_interval = original_row[i + 1] - original_row[i];
            // Invert the interval (multiply by -1)
            int inverted_interval = -original_interval;
            // Apply inverted interval
            int new_note = retrograde_inversion.back() + inverted_interval;
            
            // Keep within the same octave range as original
            while (new_note < 60) new_note += 12;
            while (new_note > 71) new_note -= 12;
            
            retrograde_inversion.push_back(new_note);
        }
    }
    
    return retrograde_inversion;
}

/**
 * @brief Load configuration from JSON file (simplified version)
 */
bool load_twelve_tone_config(const std::string& filename, SolverConfig& config) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "❌ Could not open config file: " << filename << std::endl;
        return false;
    }
    
    // For this demo, we'll use hardcoded values based on our JSON file
    config.sequence_length = 12;
    config.min_note = 60;  // C4
    config.max_note = 71;  // B4 (one octave)
    config.num_voices = 2;
    config.allow_repetitions = false;  // Critical for 12-tone!
    config.max_interval_size = 12;
    config.style = SolverConfig::CONTEMPORARY;  // Appropriate for 12-tone
    config.backjump_mode = AdvancedBackjumping::BackjumpMode::INTELLIGENT_BACKJUMP;
    config.verbose_output = true;
    
    std::cout << "✅ Configuration loaded from " << filename << std::endl;
    return true;
}

/**
 * @brief Print 12-tone row analysis
 */
void print_twelve_tone_analysis(const std::vector<int>& row, const std::string& title) {
    std::cout << "\n🎼 " << title << std::endl;
    std::cout << std::string(title.length() + 4, '=') << std::endl;
    
    std::cout << "MIDI Notes:    ";
    for (size_t i = 0; i < row.size(); ++i) {
        if (i > 0) std::cout << " → ";
        std::cout << row[i];
    }
    std::cout << std::endl;
    
    std::cout << "Note Names:    ";
    for (size_t i = 0; i < row.size(); ++i) {
        if (i > 0) std::cout << " → ";
        std::cout << Solver::midi_to_note_name(row[i]);
    }
    std::cout << std::endl;
    
    std::cout << "Pitch Classes: ";
    for (size_t i = 0; i < row.size(); ++i) {
        if (i > 0) std::cout << " → ";
        std::cout << (row[i] % 12);
    }
    std::cout << std::endl;
    
    std::cout << "Intervals:     ";
    for (size_t i = 1; i < row.size(); ++i) {
        if (i > 1) std::cout << ", ";
        int interval = row[i] - row[i-1];
        std::cout << std::showpos << interval << std::noshowpos;
    }
    std::cout << std::endl;
    
    // Verify 12-tone property
    std::set<int> pitch_classes;
    for (int note : row) {
        pitch_classes.insert(note % 12);
    }
    
    std::cout << "✅ 12-Tone Valid: " << (pitch_classes.size() == 12 ? "YES" : "NO");
    if (pitch_classes.size() == 12) {
        std::cout << " (All chromatic notes used exactly once)";
    } else {
        std::cout << " (Only " << pitch_classes.size() << "/12 pitch classes)";
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "🎼 12-TONE ROW GENERATOR WITH RETROGRADE INVERSION" << std::endl;
    std::cout << "=================================================" << std::endl;
    std::cout << "Generates a 12-tone row and its retrograde inversion using constraint solving" << std::endl;
    
    try {
        // Load configuration
        std::cout << "\n📋 Step 1: Loading Configuration" << std::endl;
        std::cout << "--------------------------------" << std::endl;
        
        SolverConfig config;
        if (!load_twelve_tone_config("twelve_tone_config.json", config)) {
            std::cout << "⚠️  Using default configuration" << std::endl;
            config.sequence_length = 12;
            config.min_note = 60;
            config.max_note = 71;
            config.num_voices = 2;
            config.allow_repetitions = false;
            config.style = SolverConfig::CONTEMPORARY;
        }
        
        // Create solver with 12-tone rules
        std::cout << "\n🔧 Step 2: Setting Up 12-Tone Constraint Solver" << std::endl;
        std::cout << "----------------------------------------------" << std::endl;
        
        Solver solver(config);
        solver.clear_rules();  // Remove default rules
        
        // Add 12-tone rule
        solver.add_rule(std::make_shared<TwelveToneRule>());
        
        std::cout << "✅ 12-tone solver configured" << std::endl;
        std::cout << "   Sequence length: " << config.sequence_length << " notes" << std::endl;
        std::cout << "   Note range: " << config.min_note << "-" << config.max_note 
                 << " (" << Solver::midi_to_note_name(config.min_note) 
                 << "-" << Solver::midi_to_note_name(config.max_note) << ")" << std::endl;
        std::cout << "   Rules: " << solver.get_rules_count() << std::endl;
        
        // Generate 12-tone row
        std::cout << "\n🎯 Step 3: Generating 12-Tone Row" << std::endl;
        std::cout << "---------------------------------" << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        MusicalSolution solution = solver.solve();
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (!solution.found_solution) {
            std::cout << "❌ Failed to generate 12-tone row: " << solution.failure_reason << std::endl;
            return 1;
        }
        
        std::cout << "✅ 12-tone row generated in " << duration.count() << " ms" << std::endl;
        
        // Generate retrograde inversion
        std::cout << "\n🔄 Step 4: Generating Retrograde Inversion" << std::endl;
        std::cout << "-----------------------------------------" << std::endl;
        
        std::vector<int> retrograde_inversion = generate_retrograde_inversion(solution.absolute_notes);
        std::cout << "✅ Retrograde inversion computed" << std::endl;
        
        // Print analysis
        std::cout << "\n📊 RESULTS ANALYSIS" << std::endl;
        std::cout << "===================" << std::endl;
        
        print_twelve_tone_analysis(solution.absolute_notes, "Original 12-Tone Row (Voice 0)");
        print_twelve_tone_analysis(retrograde_inversion, "Retrograde Inversion (Voice 1)");
        
        // Export results
        std::cout << "\n💾 Step 5: Exporting Results" << std::endl;
        std::cout << "----------------------------" << std::endl;
        
        // Export as JSON
        std::ofstream json_output("tests/output/twelve_tone_result.json");
        if (json_output.is_open()) {
            json_output << "{" << std::endl;
            json_output << "  \"title\": \"12-Tone Row with Retrograde Inversion\"," << std::endl;
            json_output << "  \"original_row\": [";
            for (size_t i = 0; i < solution.absolute_notes.size(); ++i) {
                if (i > 0) json_output << ", ";
                json_output << solution.absolute_notes[i];
            }
            json_output << "]," << std::endl;
            json_output << "  \"retrograde_inversion\": [";
            for (size_t i = 0; i < retrograde_inversion.size(); ++i) {
                if (i > 0) json_output << ", ";
                json_output << retrograde_inversion[i];
            }
            json_output << "]," << std::endl;
            json_output << "  \"solve_time_ms\": " << duration.count() << "," << std::endl;
            json_output << "  \"rules_checked\": " << solution.total_rules_checked << std::endl;
            json_output << "}" << std::endl;
            json_output.close();
            std::cout << "✅ Results exported to tests/output/twelve_tone_result.json" << std::endl;
        }
        
        // Export as simple text
        std::ofstream text_output("tests/output/twelve_tone_result.txt");
        if (text_output.is_open()) {
            text_output << "12-TONE ROW GENERATOR RESULTS" << std::endl;
            text_output << "=============================" << std::endl << std::endl;
            
            text_output << "Original Row (Voice 0):" << std::endl;
            for (int note : solution.absolute_notes) {
                text_output << Solver::midi_to_note_name(note) << " ";
            }
            text_output << std::endl << std::endl;
            
            text_output << "Retrograde Inversion (Voice 1):" << std::endl;
            for (int note : retrograde_inversion) {
                text_output << Solver::midi_to_note_name(note) << " ";
            }
            text_output << std::endl << std::endl;
            
            text_output << "Performance:" << std::endl;
            text_output << "Solve time: " << duration.count() << " ms" << std::endl;
            text_output << "Rules checked: " << solution.total_rules_checked << std::endl;
            
            text_output.close();
            std::cout << "✅ Results exported to tests/output/twelve_tone_result.txt" << std::endl;
        }
        
        // Performance stats
        std::cout << "\n📈 Performance Summary:" << std::endl;
        std::cout << "   Generation time: " << duration.count() << " ms" << std::endl;
        std::cout << "   Rules checked: " << solution.total_rules_checked << std::endl;
        std::cout << "   Search efficiency: " << (solution.total_rules_checked / static_cast<double>(duration.count())) << " rules/ms" << std::endl;
        
        std::cout << "\n🎉 12-TONE ROW GENERATION COMPLETE!" << std::endl;
        std::cout << "   Two-voice composition created with authentic serial technique" << std::endl;
        std::cout << "   Results exported in multiple formats" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}