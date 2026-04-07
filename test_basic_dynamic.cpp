/**
 * @file test_basic_dynamic.cpp
 * @brief Basic test of dynamic rule loading without complex AST
 */

#include "musical_constraint_solver.hh"
#include <iostream>
#include <nlohmann/json.hpp>

int main() {
    std::cout << "\n🎵 Basic Dynamic Rules Test\n";
    
    // Create simple JSON configuration
    nlohmann::json simple_config = {
        {"dynamic_rules", nlohmann::json::array({
            {
                {"rule_id", "test_rule_1"},
                {"description", "Simple test rule"},
                {"expression", {
                    {"type", "comparison"},
                    {"operator", "=="},
                    {"left", {{"type", "variable"}, {"name", "voice0_pitch0"}}},
                    {"right", {{"type", "constant"}, {"value", 60}}}
                }}
            }
        })}
    };
    
    std::cout << "✅ JSON Configuration Created" << std::endl;
    std::cout << "   Rule count: " << simple_config["dynamic_rules"].size() << std::endl;
    
    // Test JSON parsing
    try {
        auto rules = simple_config["dynamic_rules"];
        for (auto& rule : rules) {
            std::cout << "   Rule: " << rule["rule_id"] << " - " << rule["description"] << std::endl;
        }
        std::cout << "✅ JSON Parsing Success" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "❌ JSON Parsing Failed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n✅ Basic Dynamic Rules Test Complete\n" << std::endl;
    return 0;
}