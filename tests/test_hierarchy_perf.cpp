#include <iostream>
#include <chrono>
#include "include/musical_constraint_solver.hh"
#include "include/json.hpp"

using json = nlohmann::json;

int main() {
    // Load the metric_domain_complex.json config
    std::ifstream config_file("configs/metric_domain_complex.json");
    if (!config_file.is_open()) {
        std::cerr << "Failed to open config file" << std::endl;
        return 1;
    }
    json config_json;
    config_file >> config_json;
    config_file.close();

    // Create solver config
    MusicalConstraintSolver::SolverConfig config;
    config.sequence_length = config_json.value("solution_length", 12);
    config.num_voices = config_json.value("num_voices", 2);
    config.verbose_output = true;  // Enable verbose output for instrumentation
    config.style = MusicalConstraintSolver::SolverConfig::CLASSICAL;

    // Set up voices
    config.voice_domains.resize(config.num_voices);
    config.voice_rhythm_domains.resize(config.num_voices);
    
    if (config_json.contains("voices")) {
        auto voices = config_json["voices"];
        for (size_t i = 0; i < voices.size() && i < config.voice_domains.size(); i++) {
            // Set up pitch domains
            if (voices[i].contains("pitch") && voices[i]["pitch"].contains("midi_values")) {
                auto midi_vals = voices[i]["pitch"]["midi_values"];
                for (auto val : midi_vals) {
                    config.voice_domains[i].push_back(val.get<int>());
                }
            }

            // Set up rhythm domains (convert duration strings to ticks)
            if (voices[i].contains("rhythm") && voices[i]["rhythm"].contains("duration_values")) {
                auto durations = voices[i]["rhythm"]["duration_values"];
                for (auto dur : durations) {
                    // Simple conversion: "1/4" -> 4 ticks (at rhythm_base=16)
                    std::string dur_str = dur.get<std::string>();
                    config.voice_rhythm_domains[i].push_back(4);  // Simplified
                }
            }
        }
    }

    // Test at different sequence lengths
    std::vector<int> test_lengths = {8, 12, 16, 20};

    for (int length : test_lengths) {
        config.sequence_length = length;
        std::cout << "\n========================================" << std::endl;
        std::cout << "Testing hierarchical_voices at length " << length << std::endl;
        std::cout << "========================================" << std::endl;

        MusicalConstraintSolver::Solver solver(config);

        // Load rules from config
        if (config_json.contains("rules")) {
            auto rules = config_json["rules"];
            for (auto& rule : rules) {
                if (rule.value("enabled", false)) {
                    std::cout << "Loaded rule: " << rule["description"] << std::endl;
                    // (Rules loading would happen via specialized functions)
                }
            }
        }

        auto start = std::chrono::high_resolution_clock::now();
        auto solution = solver.solve();
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::cout << "Result: " << (solution.found_solution ? "FOUND" : "NOT FOUND") << std::endl;
        std::cout << "Time: " << elapsed_ms << " ms" << std::endl;

        // Print stats
        auto stats = solver.get_performance_stats();
        std::cout << "\nPerformance stats:" << std::endl;
        for (const auto& [key, val] : stats) {
            if (key.find("hierarchy") != std::string::npos) {
                std::cout << "  " << key << ": " << val << std::endl;
            }
        }
    }

    return 0;
}
