/**
 * @file dynamic_constraint_solver_main.cpp
 * @brief Truly Dynamic Musical Constraint Solver - Fixed JSON Parser
 * 
 * A fully dynamic constraint solver that properly parses JSON configuration files.
 * Usage: ./dynamic-solver <config_file.json>
 */

#include "musical_constraint_solver.hh"
#include "dynamic_rule_compiler.hh"
#include "wildcard_rule_extension.hh"
#include "rule_expression_parser.hh"
#include "neural_pitch_scorer.hh"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <memory>
#include <set>
#include <regex>
#include <algorithm>
#include <limits>
#include <random>
#include <cctype>
#include <functional>

using namespace DynamicRules;

// Utility function for variable-rhythm onset mapping
static std::vector<std::vector<int>> compute_voice_onsets(const std::vector<std::vector<int>>& voice_rhythms, int sequence_length) {
    std::vector<std::vector<int>> onsets(voice_rhythms.size(), std::vector<int>(sequence_length, 0));
    for (size_t v = 0; v < voice_rhythms.size(); ++v) {
        int running = 0;
        const auto& vr = voice_rhythms[v];
        const int n = std::min(sequence_length, static_cast<int>(vr.size()));
        for (int i = 0; i < n; ++i) {
            onsets[v][i] = running;
            running += std::abs(vr[i]);
        }
        for (int i = n; i < sequence_length; ++i) {
            onsets[v][i] = running;
        }
    }
    return onsets;
}

// Data structure for parsed rule configuration
struct RuleConfig {
    std::string rule_type;
    std::string function;
    std::vector<int> indices;
    std::vector<std::string> timepoints;
    // Bar-oriented time signature pattern (NEW)
    std::string bar_pattern_type;  // "fixed", "repeating", "random", "weighted"
    std::vector<std::string> bar_pattern;  // Time signatures
    int bar_pattern_count;
    int bar_pattern_repetitions;
    std::map<std::string, double> bar_pattern_distribution;
    bool allow_cross_barline = false;
    // End bar-oriented fields
    int voice;
    int target_voice = -1;
    std::vector<int> target_voices;
    std::string target_component;
    int target_engine = -1;
    std::vector<int> target_engines;
    std::string engine_type;
    bool enabled;
    int priority;
    std::string description;
    std::vector<double> parameters;
    std::vector<std::string> parameter_strings;
};

static int engine_index_for_voice_component(int voice, const std::string& component, int num_voices) {
    if (voice < 0 || voice >= num_voices) {
        throw std::runtime_error(
            "voice index " + std::to_string(voice) + " is out of range for " +
            std::to_string(num_voices) + " voices");
    }
    if (component == "rhythm") return voice * 2;
    if (component == "pitch") return voice * 2 + 1;
    throw std::runtime_error(
        "unsupported target_component '" + component + "'; expected 'pitch' or 'rhythm'");
}

// Coerce a JSON value to bool: true/1 -> true, false/0/absent -> false.
static bool json_as_bool(const nlohmann::json& j, const std::string& key, bool def = false) {
    if (!j.contains(key)) return def;
    const auto& v = j[key];
    if (v.is_boolean()) return v.get<bool>();
    if (v.is_number_integer()) return v.get<int>() != 0;
    if (v.is_number()) return v.get<double>() != 0.0;
    return def;
}

static void normalize_rule_targeting_for_cli_json(nlohmann::json& rule, int num_voices) {
    if (!rule.is_object()) return;

    const std::string rule_id = rule.value("id", rule.value("rule_type", std::string("rule")));
    std::string rule_type = rule.value("rule_type", std::string(""));
    const std::string type_field = rule.value("type", std::string(""));

    if (type_field == "index") return;

    if (rule_type == "r-rhythmic-uniformity") {
        rule_type = "r-uniformity";
        rule["rule_type"] = rule_type;
        if (!rule.contains("target_component") && !rule.contains("engine_type")) {
            rule["target_component"] = "rhythm";
            rule["engine_type"] = "rhythm";
        }
    } else if (rule_type == "r-metric-signature") {
        rule_type = "r-time-signature";
        rule["rule_type"] = rule_type;
    }

    if (rule_type == "r-time-signature") {
        // Allow empty target_voices array and target_component == "metric" — both are harmless defaults.
        const bool has_voice = rule.contains("target_voice");
        const bool has_voices = rule.contains("target_voices") &&
                                rule["target_voices"].is_array() &&
                                !rule["target_voices"].empty();
        const bool has_component = rule.contains("target_component") &&
                                   rule["target_component"].is_string() &&
                                   rule["target_component"].get<std::string>() != "metric";
        const bool has_target_engine = rule.contains("target_engine");
        const bool has_target_engines = rule.contains("target_engines");
        if (has_voice || has_voices || has_component || has_target_engine || has_target_engines) {
            throw std::runtime_error(
                "rule '" + rule_id + "' is metric-targeted implicitly and must not specify voice/component/engine targets");
        }
        rule["target_engine"] = num_voices * 2;
        rule["engine_type"] = "metric";
        return;
    }

    if (rule.contains("target_engine") || rule.contains("target_engines")) {
        throw std::runtime_error(
            "rule '" + rule_id + "' uses deprecated engine targeting; use target_voices instead");
    }

    // Canonicalize legacy aliases to target_voices.
    if (rule.contains("target_voice")) {
        if (rule["target_voice"].is_number_integer()) {
            if (!rule.contains("target_voices") || !rule["target_voices"].is_array()) {
                rule["target_voices"] = nlohmann::json::array();
            }
            rule["target_voices"].push_back(rule["target_voice"].get<int>());
        } else if (rule["target_voice"].is_array()) {
            rule["target_voices"] = rule["target_voice"];
        }
        rule.erase("target_voice");
    }

    if (rule.contains("voice") && rule["voice"].is_number_integer()) {
        if (!rule.contains("target_voices") || !rule["target_voices"].is_array()) {
            rule["target_voices"] = nlohmann::json::array();
        }
        rule["target_voices"].push_back(rule["voice"].get<int>());
        rule.erase("voice");
    }

    // Normalize temporal scope alias for timepoint rules.
    if (rule.contains("temporal_scope") && !rule.contains("time_scope")) {
        rule["time_scope"] = rule["temporal_scope"];
    }
    rule.erase("temporal_scope");

    bool has_target_voices = rule.contains("target_voices") && rule["target_voices"].is_array() && !rule["target_voices"].empty();

    if (!has_target_voices && rule.contains("constraint") && rule["constraint"].is_string()) {
        std::set<int> voices_from_constraint;
        static const std::regex kVoiceRef(R"(voice\[(\d+)\])");
        const std::string expr = rule["constraint"].get<std::string>();
        const bool has_voice_variable = expr.find("voice[v]") != std::string::npos;
        for (std::sregex_iterator it(expr.begin(), expr.end(), kVoiceRef), end; it != end; ++it) {
            voices_from_constraint.insert(std::stoi((*it)[1].str()));
        }

        if (!voices_from_constraint.empty()) {
            nlohmann::json many = nlohmann::json::array();
            for (int voice : voices_from_constraint) {
                if (voice >= 0 && voice < num_voices) many.push_back(voice);
            }
            if (!many.empty()) {
                rule["target_voices"] = std::move(many);
                has_target_voices = true;
            }
        } else if (has_voice_variable) {
            nlohmann::json all = nlohmann::json::array();
            for (int voice = 0; voice < num_voices; ++voice) all.push_back(voice);
            if (!all.empty()) {
                rule["target_voices"] = std::move(all);
                has_target_voices = true;
            }
        }
    }

    if (!has_target_voices) {
        throw std::runtime_error(
            "rule '" + rule_id + "' is missing target_voices");
    }

    // Deduplicate and validate target_voices.
    std::set<int> unique_voices;
    nlohmann::json normalized_target_voices = nlohmann::json::array();
    for (const auto& voice_json : rule["target_voices"]) {
        if (!voice_json.is_number_integer()) {
            throw std::runtime_error(
                "rule '" + rule_id + "' target_voices must contain integers only");
        }
        const int voice = voice_json.get<int>();
        if (voice < 0 || voice >= num_voices) {
            throw std::runtime_error(
                "rule '" + rule_id + "' has out-of-range voice " + std::to_string(voice));
        }
        if (unique_voices.insert(voice).second) {
            normalized_target_voices.push_back(voice);
        }
    }
    rule["target_voices"] = std::move(normalized_target_voices);

    std::string target_component = rule.value("target_component", std::string(""));
    if (target_component.empty()) {
        const std::string engine_type = rule.value("engine_type", std::string(""));
        if (engine_type == "pitch" || engine_type == "rhythm") {
            target_component = engine_type;
        }
    }
    if (target_component.empty() && rule_type == "r-metric-hierarchy") {
        target_component = "rhythm";
    }
    if (target_component.empty() && rule_type == "r-rhythm-rhythm") {
        target_component = "rhythm";
    }
    if (target_component.empty()) {
        target_component = "pitch";
    }
    if (!rule.contains("engine_type")) {
        rule["engine_type"] = target_component;
    }

    nlohmann::json target_engines = nlohmann::json::array();
    for (const auto& voice_json : rule["target_voices"]) {
        const int voice = voice_json.get<int>();
        target_engines.push_back(engine_index_for_voice_component(voice, target_component, num_voices));
    }
    rule["target_engines"] = std::move(target_engines);
}

// Robust JSON parser for constraint solver configuration
class ConstraintSolverJSONParser {
private:
    std::string name_;
    std::string description_;
    int solution_length_;
    int num_voices_;
    std::string export_path_;
    std::vector<RuleConfig> rules_;
    mutable int rhythm_base_ = 1;  // LCM of all duration denominators, set by getVoiceRhythmDomains()
    
    // Domain configuration
    struct DomainConfig {
        int engine_id;
        std::string type;
        std::vector<int> values;
        std::string description;
    };
    std::vector<DomainConfig> domains_;
    
    // Configuration options
    std::vector<int> pitch_range_; // User-specified [min, max] pitch range
    
    // Output options
    bool export_json_;
    bool export_txt_;
    bool export_xml_;
    bool export_png_;
    bool export_midi_;
    bool show_statistics_;
    bool include_analysis_;
    std::string export_filename_;
    
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
            result = trim(result);
        }
        // Remove quotes if present
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

    std::vector<std::string> parseStringArray(const std::string& arrayStr) {
        std::vector<std::string> result;
        std::string cleaned = arrayStr;

        size_t start = cleaned.find('[');
        size_t end = cleaned.find_last_of(']');

        if (start != std::string::npos && end != std::string::npos && start < end) {
            std::string content = cleaned.substr(start + 1, end - start - 1);
            std::stringstream ss(content);
            std::string item;

            while (std::getline(ss, item, ',')) {
                item = trim(item);
                if (!item.empty() && item.length() >= 2 &&
                    ((item.front() == '"' && item.back() == '"') ||
                     (item.front() == '\'' && item.back() == '\''))) {
                    result.push_back(item.substr(1, item.length() - 2));
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
        else if (line.find("\"export_path\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                export_path_ = removeQuotesAndComma(line.substr(pos + 1));
            }
        } else if (line.find("\"export_filename\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                export_filename_ = removeQuotesAndComma(line.substr(pos + 1));
            }
        } else if (line.find("\"file_name\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                const std::string fn = removeQuotesAndComma(line.substr(pos + 1));
                if (!fn.empty()) {
                    // If file_name contains a directory component (e.g. "output/my_piece"),
                    // extract it into export_path_ so one key handles both dir and base name.
                    size_t last_slash = fn.find_last_of("/\\");
                    if (last_slash != std::string::npos) {
                        export_path_    = fn.substr(0, last_slash);
                        export_filename_ = fn.substr(last_slash + 1);
                    } else {
                        export_filename_ = fn;
                    }
                }
            }
        } else if (line.find("\"export_json\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                std::string value = removeQuotesAndComma(line.substr(pos + 1));
                export_json_ = (value == "true");
            }
        } else if (line.find("\"export_txt\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                std::string value = removeQuotesAndComma(line.substr(pos + 1));
                export_txt_ = (value == "true");
            }
        } else if (line.find("\"export_xml\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                std::string value = removeQuotesAndComma(line.substr(pos + 1));
                export_xml_ = (value == "true");
            }
        } else if (line.find("\"export_png\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                std::string value = removeQuotesAndComma(line.substr(pos + 1));
                export_png_ = (value == "true");
            }
        } else if (line.find("\"export_midi\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                std::string value = removeQuotesAndComma(line.substr(pos + 1));
                export_midi_ = (value == "true");
            }
        } else if (line.find("\"show_statistics\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                std::string value = removeQuotesAndComma(line.substr(pos + 1));
                show_statistics_ = (value == "true");
            }
        } else if (line.find("\"include_analysis\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                std::string value = removeQuotesAndComma(line.substr(pos + 1));
                include_analysis_ = (value == "true");
            }
        } else if (line.find("\"pitch_range\"") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                std::string value = line.substr(pos + 1);
                pitch_range_ = parseIntArray(value);
                std::cout << "DEBUG: Parsed pitch_range [" << pitch_range_[0] << ", " << pitch_range_[1] << "]" << std::endl;
            }
        }
    }
    
public:
    ConstraintSolverJSONParser() 
        : solution_length_(12), num_voices_(2), export_path_("tests/output"),
                    export_json_(true), export_txt_(true), export_xml_(false), export_png_(false), export_midi_(false), show_statistics_(true), include_analysis_(true) {}
    
    bool parse(const std::string& filename) {
        // Strict frontend schema checks before the legacy line parser runs.
        {
            std::ifstream strict_file(filename);
            if (strict_file.is_open()) {
                try {
                    nlohmann::json strict_cfg;
                    strict_file >> strict_cfg;

                    if (strict_cfg.contains("engine_domains")) {
                        std::cout << "❌ Config validation error: 'engine_domains' is deprecated. "
                                  << "Use top-level 'voices' with per-voice pitch/rhythm domains." << std::endl;
                        return false;
                    }

                    if (strict_cfg.contains("rules") && (strict_cfg["rules"].is_array() || strict_cfg["rules"].is_object())) {
                        std::vector<nlohmann::json> rule_entries;
                        if (strict_cfg["rules"].is_array()) {
                            for (const auto& r : strict_cfg["rules"]) rule_entries.push_back(r);
                        } else {
                            for (auto it = strict_cfg["rules"].begin(); it != strict_cfg["rules"].end(); ++it) {
                                rule_entries.push_back(it.value());
                            }
                        }

                        for (const auto& r : rule_entries) {
                            if (!r.is_object()) continue;
                            if (r.contains("target_engine") || r.contains("target_engines")) {
                                const std::string id = r.value("id", r.value("rule_type", std::string("rule")));
                                std::cout << "❌ Config validation error: rule '" << id
                                          << "' uses deprecated engine targeting. "
                                          << "Use target_voice/target_voices with target_component." << std::endl;
                                return false;
                            }
                        }
                    }
                } catch (...) {
                    // Let the existing line parser report malformed config details.
                }
            }
        }

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
        bool in_domains_section = false;
        bool in_domain_object = false;
        
        RuleConfig current_rule;
        DomainConfig current_domain;
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
            if (in_rules_section && (line.find("\"domains\"") != std::string::npos ||
                                     line.find("\"search_options\"") != std::string::npos ||
                                     line.find("\"search_strategy\"") != std::string::npos ||
                                     line.find("\"output_options\"") != std::string::npos ||
                                     line.find("\"display_options\"") != std::string::npos)) {
                in_rules_section = false;
                if (line.find("\"domains\"") != std::string::npos) {
                    in_domains_section = true;
                    std::cout << "DEBUG: Entering domains section" << std::endl;
                }
                continue;
            }
            
            // Exit domains section when we hit another top-level section
            if (in_domains_section && (line.find("\"search_strategy\"") != std::string::npos || 
                                      line.find("\"output_options\"") != std::string::npos ||
                                      line.find("\"display_options\"") != std::string::npos)) {
                in_domains_section = false;
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
                    current_rule.target_component = "";
                    continue;
                }
                
                // Rule ends with closing brace (with or without comma)
                if ((line == "}" || line == "},") && in_rule_object && !in_constraint_function) {
                    in_rule_object = false;
                    in_constraint_function = false;
                    if (!current_rule.rule_type.empty()) {
                        if (current_rule.rule_type == "r-rhythmic-uniformity") {
                            current_rule.rule_type = "r-uniformity";
                            if (current_rule.target_component.empty() && current_rule.engine_type == "pitch") {
                                current_rule.target_component = "rhythm";
                                current_rule.engine_type = "rhythm";
                            }
                        } else if (current_rule.rule_type == "r-metric-signature") {
                            current_rule.rule_type = "r-time-signature";
                        }

                        if (current_rule.rule_type == "r-time-signature" &&
                            current_rule.target_engine < 0 && current_rule.target_engines.empty()) {
                            current_rule.target_engine = num_voices_ * 2;
                            current_rule.engine_type = "metric";
                        }

                        // Canonicalize to target_voices for legacy line-parser paths.
                        if (current_rule.target_voice >= 0) {
                            current_rule.target_voices.push_back(current_rule.target_voice);
                            current_rule.target_voice = -1;
                        }
                        std::sort(current_rule.target_voices.begin(), current_rule.target_voices.end());
                        current_rule.target_voices.erase(
                            std::unique(current_rule.target_voices.begin(), current_rule.target_voices.end()),
                            current_rule.target_voices.end());

                        if (current_rule.target_engine < 0 && current_rule.target_engines.empty()) {
                            std::string target_component = current_rule.target_component;
                            if (target_component.empty() && current_rule.rule_type == "r-metric-hierarchy") {
                                target_component = "rhythm";
                            }
                            if (target_component.empty() &&
                                (current_rule.engine_type == "pitch" || current_rule.engine_type == "rhythm")) {
                                target_component = current_rule.engine_type;
                            }
                            if (target_component.empty()) {
                                target_component = "pitch";
                            }

                            if (!current_rule.target_voices.empty()) {
                                current_rule.target_engines.clear();
                                for (int voice_idx : current_rule.target_voices) {
                                    current_rule.target_engines.push_back(
                                        engine_index_for_voice_component(voice_idx, target_component, num_voices_));
                                }
                                if (current_rule.engine_type.empty()) {
                                    current_rule.engine_type = target_component;
                                }
                                if (current_rule.rule_type == "r-metric-hierarchy") {
                                    current_rule.target_component = "rhythm";
                                    current_rule.engine_type = "rhythm";
                                }
                            }
                        }

                        const bool has_target_engine = (current_rule.target_engine >= 0);
                        const bool has_target_engines = !current_rule.target_engines.empty();
                        if (!has_target_engine && !has_target_engines) {
                            std::cout << "❌ Rule validation error: rule '"
                                      << (current_rule.description.empty() ? current_rule.rule_type : current_rule.description)
                                      << "' is missing target_voices" << std::endl;
                            return false;
                        }
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
                                current_rule.parameter_strings = parseStringArray(line);
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
                            current_rule.parameter_strings = parseStringArray(parameters_content);
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
                        std::cout << "DEBUG: Parsed " << current_rule.indices.size() << " indices from line: " << line << std::endl;
                    }
                    else if (line.find("\"timepoints\"") != std::string::npos) {
                        current_rule.timepoints = parseStringArray(line);
                    }
                    else if (line.find("\"bar_pattern_type\"") != std::string::npos) {
                        size_t pos = line.find(':');
                        if (pos != std::string::npos) {
                            current_rule.bar_pattern_type = removeQuotesAndComma(line.substr(pos + 1));
                        }
                    }
                    else if (line.find("\"bar_pattern\"") != std::string::npos) {
                        current_rule.bar_pattern = parseStringArray(line);
                    }
                    else if (line.find("\"bar_pattern_count\"") != std::string::npos) {
                        size_t pos = line.find(':');
                        if (pos != std::string::npos) {
                            std::string num_str = removeQuotesAndComma(line.substr(pos + 1));
                            try {
                                current_rule.bar_pattern_count = std::stoi(num_str);
                            } catch (...) {}
                        }
                    }
                    else if (line.find("\"bar_pattern_repetitions\"") != std::string::npos) {
                        size_t pos = line.find(':');
                        if (pos != std::string::npos) {
                            std::string num_str = removeQuotesAndComma(line.substr(pos + 1));
                            try {
                                current_rule.bar_pattern_repetitions = std::stoi(num_str);
                            } catch (...) {}
                        }
                    }
                    else if (line.find("\"allow_cross_barline\"") != std::string::npos) {
                        std::string lowered = line;
                        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
                        current_rule.allow_cross_barline =
                            (lowered.find("true") != std::string::npos ||
                             lowered.find(": 1") != std::string::npos ||
                             lowered.find(":1") != std::string::npos);
                    }
                    else if (line.find("\"parameters\"") != std::string::npos) {
                        // Support shorthand top-level parameters in rules.
                        if (line.find("[") != std::string::npos && line.find("]") != std::string::npos) {
                            current_rule.parameters = parseDoubleArray(line);
                            current_rule.parameter_strings = parseStringArray(line);
                        } else {
                            in_parameters_array = true;
                            parameters_content = line;
                        }
                    }
                    else if (line.find("\"target_engine\"") != std::string::npos) {
                        std::cout << "❌ Rule validation error: deprecated field 'target_engine' is not allowed. "
                                  << "Use target_voices with target_component." << std::endl;
                        return false;
                    }
                    else if (line.find("\"target_engines\"") != std::string::npos) {
                        std::cout << "❌ Rule validation error: deprecated field 'target_engines' is not allowed. "
                                  << "Use target_voices with target_component." << std::endl;
                        return false;
                    }
                    else if (line.find("\"target_voice\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            std::string value = removeQuotesAndComma(line.substr(pos + 1));
                            try {
                                current_rule.target_voices.push_back(std::stoi(value));
                                current_rule.target_voice = -1;
                            } catch (...) {}
                        }
                    }
                    else if (line.find("\"target_voices\"") != std::string::npos) {
                        current_rule.target_voices = parseIntArray(line);
                    }
                    else if (line.find("\"target_component\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            current_rule.target_component = removeQuotesAndComma(line.substr(pos + 1));
                        }
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
                            std::transform(value.begin(), value.end(), value.begin(),
                                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                            current_rule.enabled =
                                (value == "true" || value == "1" || value == "yes" || value == "on");
                        }
                    }
                    else if (line.find("\"constraint\"") != std::string::npos &&
                             line.find("\"constraint_function\"") == std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            // Keep wildcard constraints in the dynamic-rule path; map others to function.
                            if (current_rule.rule_type != "wildcard_constraint") {
                                current_rule.function = removeQuotesAndComma(line.substr(pos + 1));
                            }
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
            
            // Handle domains section  
            if (in_domains_section) {
                std::cout << "DEBUG: In domains section, line: " << line << std::endl;
                // New domain starts with standalone opening brace
                if (line == "{" && !in_domain_object) {
                    in_domain_object = true;
                    current_domain = DomainConfig{}; // Reset
                    current_domain.engine_id = -1;
                    std::cout << "DEBUG: Starting new domain object" << std::endl;
                }
                // Domain ends with standalone closing brace
                else if (line == "}" && in_domain_object) {
                    in_domain_object = false;
                    if (!current_domain.type.empty()) {  // Add domain if it has a type
                        domains_.push_back(current_domain);
                        std::cout << "DEBUG: Added domain with " << current_domain.values.size() << " values" << std::endl;
                    }
                }
                // Parse domain properties
                else if (in_domain_object) {
                    if (line.find("\"engine_id\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            std::string value = removeQuotesAndComma(line.substr(pos + 1));
                            try {
                                current_domain.engine_id = std::stoi(value);
                            } catch (...) {}
                        }
                    }
                    else if (line.find("\"type\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            current_domain.type = removeQuotesAndComma(line.substr(pos + 1));
                        }
                    }
                    else if (line.find("\"values\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            std::string values_str = line.substr(pos + 1);
                            current_domain.values = parseIntArray(values_str);
                        }
                    }
                    else if (line.find("\"description\"") != std::string::npos) {
                        size_t pos = line.find(":");
                        if (pos != std::string::npos) {
                            current_domain.description = removeQuotesAndComma(line.substr(pos + 1));
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
    MusicalConstraintSolver::SolverConfig::SearchEngine getSearchEngine(const std::string& config_file) const {
        std::ifstream f(config_file);
        if (!f.is_open()) return MusicalConstraintSolver::SolverConfig::SearchEngine::DFS;
        nlohmann::json cfg;
        try { f >> cfg; } catch (...) { return MusicalConstraintSolver::SolverConfig::SearchEngine::DFS; }
        if (!cfg.contains("search_options") || !cfg["search_options"].is_object()) {
            return MusicalConstraintSolver::SolverConfig::SearchEngine::DFS;
        }
        std::string engine = cfg["search_options"].value("engine", "dfs");
        if (engine == "dfs") {
            return MusicalConstraintSolver::SolverConfig::SearchEngine::DFS;
        }
        throw std::runtime_error("Unsupported search_options.engine: " + engine);
    }
    GecodeClusterIntegration::VariableBranchingStrategy getVariableBranching(const std::string& config_file) const {
        std::ifstream f(config_file);
        if (!f.is_open()) return GecodeClusterIntegration::VariableBranchingStrategy::FIRST_FAIL;
        nlohmann::json cfg;
        try { f >> cfg; } catch (...) { return GecodeClusterIntegration::VariableBranchingStrategy::FIRST_FAIL; }
        if (!cfg.contains("search_options") || !cfg["search_options"].is_object()) {
            return GecodeClusterIntegration::VariableBranchingStrategy::FIRST_FAIL;
        }
        std::string branching = cfg["search_options"].value("branching", "first_fail");
        if (branching == "first_fail") {
            return GecodeClusterIntegration::VariableBranchingStrategy::FIRST_FAIL;
        }
        if (branching == "input_order") {
            return GecodeClusterIntegration::VariableBranchingStrategy::INPUT_ORDER;
        }
        throw std::runtime_error("Unsupported search_options.branching: " + branching);
    }
    GecodeClusterIntegration::ValueSelectionStrategy getValueSelection(const std::string& config_file) const {
        std::ifstream f(config_file);
        if (!f.is_open()) return GecodeClusterIntegration::ValueSelectionStrategy::MIN;
        nlohmann::json cfg;
        try { f >> cfg; } catch (...) { return GecodeClusterIntegration::ValueSelectionStrategy::MIN; }
        if (cfg.contains("search_options") && cfg["search_options"].is_object() &&
            cfg["search_options"].contains("value_order")) {
            std::string value_order = cfg["search_options"].value("value_order", "min");
            if (value_order == "min") {
                return GecodeClusterIntegration::ValueSelectionStrategy::MIN;
            }
            if (value_order == "random") {
                return GecodeClusterIntegration::ValueSelectionStrategy::RANDOM;
            }
            if (value_order == "heuristic") {
                return GecodeClusterIntegration::ValueSelectionStrategy::HEURISTIC;
            }
            if (value_order == "neural") {
                // Neural heuristic: treated as HEURISTIC strategy.
                // The scorer is registered separately via configure_pitch_heuristic_value_ordering.
                return GecodeClusterIntegration::ValueSelectionStrategy::HEURISTIC;
            }
            throw std::runtime_error("Unsupported search_options.value_order: " + value_order);
        }
        return GecodeClusterIntegration::ValueSelectionStrategy::MIN;
    }
    MusicalConstraintSolver::SolverConfig::RestartPolicy getRestartPolicy(const std::string& config_file) const {
        std::ifstream f(config_file);
        if (!f.is_open()) return MusicalConstraintSolver::SolverConfig::RestartPolicy::NONE;
        nlohmann::json cfg;
        try { f >> cfg; } catch (...) { return MusicalConstraintSolver::SolverConfig::RestartPolicy::NONE; }
        if (!cfg.contains("search_options") || !cfg["search_options"].is_object()) {
            return MusicalConstraintSolver::SolverConfig::RestartPolicy::NONE;
        }
        std::string restart_policy = cfg["search_options"].value("restart_policy", "none");
        if (restart_policy == "none") {
            return MusicalConstraintSolver::SolverConfig::RestartPolicy::NONE;
        }
        throw std::runtime_error("Unsupported search_options.restart_policy: " + restart_policy);
    }
    std::string getExportPath() const { return export_path_; }
    std::string getExportFilename() const { return export_filename_; }

    // Returns max_solutions from search_options. Returns -1 for "all".
    int getMaxSolutions(const std::string& config_file) const {
        std::ifstream f(config_file);
        if (!f.is_open()) return 1;
        nlohmann::json cfg;
        try { f >> cfg; } catch (...) { return 1; }
        if (!cfg.contains("search_options")) return 1;
        const auto& so = cfg["search_options"];
        if (!so.contains("max_solutions")) return 1;
        const auto& v = so["max_solutions"];
        if (v.is_string()) {
            std::string s = v.get<std::string>();
            if (s == "all") return -1;
            try { return std::stoi(s); } catch (...) { return 1; }
        }
        if (v.is_number_integer()) return v.get<int>();
        return 1;
    }
    
    // Output options getters
    bool getExportJSON() const { return export_json_; }
    bool getExportTXT() const { return export_txt_; }
    bool getExportXML() const { return export_xml_; }
    bool getExportPNG() const { return export_png_; }
    bool getExportMIDI() const { return export_midi_; }
    bool getShowStatistics() const { return show_statistics_; }

    int getTimeoutMs(const std::string& config_file) const {
        std::ifstream f(config_file);
        if (!f.is_open()) return 30000;
        nlohmann::json cfg;
        try { f >> cfg; } catch (...) { return 30000; }
        if (!cfg.contains("search_options")) return 30000;
        const auto& so = cfg["search_options"];
        return so.value("timeout_ms", 30000);
    }

    unsigned int getRandomSeed(const std::string& config_file) const {
        std::ifstream f(config_file);
        if (!f.is_open()) return std::numeric_limits<unsigned int>::max();
        nlohmann::json cfg;
        try { f >> cfg; } catch (...) { return std::numeric_limits<unsigned int>::max(); }
        if (!cfg.contains("search_options")) return std::numeric_limits<unsigned int>::max();
        const auto& so = cfg["search_options"];
        if (!so.contains("random_seed")) return std::numeric_limits<unsigned int>::max();
        if (so["random_seed"].is_number_unsigned()) {
            return so["random_seed"].get<unsigned int>();
        }
        if (so["random_seed"].is_number_integer()) {
            int seed = so["random_seed"].get<int>();
            return seed <= 0 ? 0u : static_cast<unsigned int>(seed);
        }
        return std::numeric_limits<unsigned int>::max();
    }

    int getHeuristicTopK(const std::string& config_file) const {
        std::ifstream f(config_file);
        if (!f.is_open()) return 0;
        nlohmann::json cfg;
        try { f >> cfg; } catch (...) { return 0; }
        if (!cfg.contains("search_options")) return 0;
        const auto& so = cfg["search_options"];
        if (!so.contains("heuristic_top_k") || !so["heuristic_top_k"].is_number_integer()) return 0;
        int k = so["heuristic_top_k"].get<int>();
        return std::max(0, k);
    }

    bool getHeuristicTrace(const std::string& config_file) const {
        std::ifstream f(config_file);
        if (!f.is_open()) return false;
        nlohmann::json cfg;
        try { f >> cfg; } catch (...) { return false; }
        if (!cfg.contains("search_options")) return false;
        const auto& so = cfg["search_options"];
        if (!so.contains("heuristic_trace") || !so["heuristic_trace"].is_boolean()) return false;
        return so["heuristic_trace"].get<bool>();
    }
    bool getIncludeAnalysis() const { return include_analysis_; }
    
    // Domain getters
    const std::vector<DomainConfig>& getDomains() const { return domains_; }
    
    // Get pitch domain range for solver configuration
    std::pair<int, int> getPitchDomainRange() const {
        // First check if user specified pitch_range in configuration
        if (pitch_range_.size() >= 2) {
            return {pitch_range_[0], pitch_range_[1]};
        }
        
        int min_pitch = 60; // Default C4
        int max_pitch = 71; // Default B4
        
        for (const auto& domain : domains_) {
            if (domain.type == "pitch" && !domain.values.empty()) {
                auto minmax = std::minmax_element(domain.values.begin(), domain.values.end());
                min_pitch = std::min(min_pitch, *minmax.first);
                max_pitch = std::max(max_pitch, *minmax.second);
            }
        }
        
        return {min_pitch, max_pitch};
    }
    
    // Get full array of pitch domain values
    std::vector<int> getPitchDomainValues() const {
        std::vector<int> all_values;
        
        // First check if user specified pitch_range in configuration
        if (pitch_range_.size() >= 2) {
            int min_pitch = pitch_range_[0];
            int max_pitch = pitch_range_[1];
            for (int i = min_pitch; i <= max_pitch; ++i) {
                all_values.push_back(i);
            }
            return all_values;
        }
        
        for (const auto& domain : domains_) {
            if (domain.type == "pitch" && !domain.values.empty()) {
                // Concatenate all pitch domain values
                all_values.insert(all_values.end(), domain.values.begin(), domain.values.end());
            }
        }
        
        // If no domains found, return default range
        if (all_values.empty()) {
            for (int i = 60; i <= 71; ++i) {
                all_values.push_back(i);
            }
        }
        
        // Remove duplicates and sort
        std::sort(all_values.begin(), all_values.end());
        all_values.erase(std::unique(all_values.begin(), all_values.end()), all_values.end());
        
        return all_values;
    }

    // Returns per-voice pitch domains from voices section.
    // voice_domains[v] = sorted unique midi values for voice v.
    std::vector<std::vector<int>> getVoicePitchDomains(const std::string& config_file) const {
        std::vector<std::vector<int>> result(num_voices_);

        std::ifstream f(config_file);
        if (!f.is_open())
            throw std::runtime_error("Cannot open config file: " + config_file);
        nlohmann::json cfg;
        f >> cfg;

        if (cfg.contains("engine_domains")) {
            throw std::runtime_error(
                "'engine_domains' is deprecated in CLI configs. Use top-level 'voices' with per-voice pitch/rhythm domains.");
        }
        const bool has_global_pitch = cfg.contains("global_domain") &&
                                      cfg["global_domain"].is_object() &&
                                      cfg["global_domain"].contains("pitch");
        if (!cfg.contains("voices") || (!cfg["voices"].is_array() && !cfg["voices"].is_object())) {
            if (!has_global_pitch)
                throw std::runtime_error(
                    "Config must have a 'voices' section or a 'global_domain.pitch' fallback");
        }

        // Returns a reference to the voice-node that contains "pitch" for voice v.
        // Priority: explicit "voice": v key > positional arr[v] > global_domain.
        auto resolve_pitch_node = [&](int v) -> const nlohmann::json& {
            if (cfg.contains("voices") && cfg["voices"].is_array()) {
                const auto& arr = cfg["voices"];
                // Explicit "voice": v key — scan the whole array.
                for (const auto& e : arr)
                    if (e.is_object() && e.contains("voice") &&
                        e["voice"].is_number_integer() && e["voice"].get<int>() == v &&
                        e.contains("pitch"))
                        return e;
                // Positional fallback (only when no conflicting "voice" key).
                if (v < (int)arr.size() && arr[v].is_object() && arr[v].contains("pitch")) {
                    const auto& e = arr[v];
                    if (!e.contains("voice") || e["voice"].get<int>() == v)
                        return e;
                }
            } else if (cfg.contains("voices") && cfg["voices"].is_object()) {
                const std::string key = std::to_string(v);
                if (cfg["voices"].contains(key) && cfg["voices"][key].is_object() &&
                    cfg["voices"][key].contains("pitch"))
                    return cfg["voices"][key];
            }
            if (has_global_pitch)
                return cfg["global_domain"];
            throw std::runtime_error(
                "voices[" + std::to_string(v) +
                "] is missing 'pitch' and no 'global_domain.pitch' is defined");
        };

        for (int v = 0; v < num_voices_; ++v) {
            const auto& voice = resolve_pitch_node(v);
            const auto& pitch = voice["pitch"];
            const auto* midi_values = static_cast<const nlohmann::json*>(nullptr);
            if (pitch.is_object() && pitch.contains("midi_values") && pitch["midi_values"].is_array()) {
                midi_values = &pitch["midi_values"];
            } else if (pitch.is_array()) {
                midi_values = &pitch;
            }
            if (!midi_values) {
                throw std::runtime_error(
                    "voices[" + std::to_string(v) + "].pitch must contain 'midi_values' array");
            }
            for (const auto& pv : *midi_values) {
                if (!pv.is_number_integer()) {
                    throw std::runtime_error(
                        "voices[" + std::to_string(v) + "].pitch.midi_values entries must be integers");
                }
                result[v].push_back(pv.get<int>());
            }
            std::sort(result[v].begin(), result[v].end());
            result[v].erase(std::unique(result[v].begin(), result[v].end()), result[v].end());
        }

        for (int v = 0; v < num_voices_; ++v) {
            if (result[v].empty())
                throw std::runtime_error("Voice " + std::to_string(v) + " has no pitch midi_values in voices");
        }

        return result;
    }

    // Returns per-voice rhythm domains from voices section.
    // voice_rhythm_domains[v] = list of allowed duration values (ints) for voice v.
    // Throws with a clear message if any voice is missing a rhythm engine entry.

    // Parses a fraction string "N/D" and validates it; returns {N, D}.
    // No divisibility restriction — any positive integer N and D are accepted.
    static std::pair<int,int> parse_duration_fraction(const std::string& s, const std::string& context) {
        // Strip optional leading '-' for rests; restore sign after parsing.
        bool is_rest = (!s.empty() && s[0] == '-');
        std::string abs_s = is_rest ? s.substr(1) : s;

        // Accept explicit zero forms.
        if (abs_s == "0" || abs_s == "0/0") {
            return {0, 1};
        }

        auto abs_slash = abs_s.find('/');
        if (abs_slash == std::string::npos)
            throw std::runtime_error(context + ": invalid duration value '" + s +
                "'. Use note-value fractions like \"1/4\" (quarter), \"-1/4\" (rest quarter), etc.");
        int num = 0, den = 0;
        try {
            num = std::stoi(abs_s.substr(0, abs_slash));
            den = std::stoi(abs_s.substr(abs_slash + 1));
        } catch (...) {
            throw std::runtime_error(context + ": cannot parse duration fraction '" + s + "'");
        }
        if (num == 0 && den == 0) {
            return {0, 1};
        }
        if (num <= 0 || den <= 0)
            throw std::runtime_error(context + ": numerator and denominator must be positive in '" + s + "'");
        return {is_rest ? -num : num, den};
    }

    static int parse_score_time_to_ticks(const std::string& token, int rhythm_base, const std::string& context) {
        if (token.empty()) {
            throw std::runtime_error(context + ": empty time token");
        }
        if (rhythm_base <= 0) {
            throw std::runtime_error(context + ": rhythm_base must be positive");
        }

        static const std::regex unit_token(R"(^\s*([0-9]+)\s*([WwHhQqEe])\s*$)");
        std::smatch match;
        if (std::regex_match(token, match, unit_token)) {
            const int count = std::stoi(match[1].str());
            const char unit = static_cast<char>(std::tolower(match[2].str()[0]));
            int denominator = 1;
            switch (unit) {
                case 'w': denominator = 1; break;
                case 'h': denominator = 2; break;
                case 'q': denominator = 4; break;
                case 'e': denominator = 8; break;
                default:
                    throw std::runtime_error(context + ": unsupported time token unit in '" + token + "'");
            }
            if ((rhythm_base * count) % denominator != 0) {
                throw std::runtime_error(context + ": time token '" + token + "' is not representable with rhythm_base=" + std::to_string(rhythm_base));
            }
            return (rhythm_base * count) / denominator;
        }

        const auto [num, den] = parse_duration_fraction(token, context);
        if (num < 0) {
            throw std::runtime_error(context + ": time token cannot be negative: '" + token + "'");
        }
        if ((rhythm_base * num) % den != 0) {
            throw std::runtime_error(context + ": time token '" + token + "' is not representable with rhythm_base=" + std::to_string(rhythm_base));
        }
        return (rhythm_base * num) / den;
    }

    // GCD (C++11-compatible).
    static int gcd_helper(int a, int b) { return b == 0 ? a : gcd_helper(b, a % b); }
    // LCM; returns a/gcd*b to avoid overflow for typical musical values.
    static int lcm_helper(int a, int b) { return a / gcd_helper(a, b) * b; }

    // Formats an internal tick value back to a fraction string "N/D"
    // given the LCM base (whole note = base ticks).
    static std::string format_duration(int ticks, int base) {
        if (ticks == 0) return "0";
        if (base <= 0) return "?";
        bool is_rest = (ticks < 0);
        int abs_ticks = std::abs(ticks);
        int g = gcd_helper(abs_ticks, base);
        int num = abs_ticks / g;
        int den = base / g;
        std::string frac = std::to_string(num) + "/" + std::to_string(den);
        return is_rest ? ("R:" + frac) : frac;
    }

    static bool json_to_bool(const nlohmann::json& value, bool default_value) {
        if (value.is_boolean()) return value.get<bool>();
        if (value.is_number_integer()) return value.get<long long>() != 0;
        if (value.is_number_unsigned()) return value.get<unsigned long long>() != 0;
        if (value.is_string()) {
            std::string s = value.get<std::string>();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (s == "true" || s == "1" || s == "yes" || s == "on") return true;
            if (s == "false" || s == "0" || s == "no" || s == "off") return false;
        }
        return default_value;
    }

    static std::pair<int, int> parse_time_signature_value(const nlohmann::json& value, const std::string& context) {
        if (value.is_array()) {
            if (value.size() != 2 || !value[0].is_number_integer() || !value[1].is_number_integer()) {
                throw std::runtime_error(context + ": time signature array must be [numerator, denominator]");
            }
            int n = value[0].get<int>();
            int d = value[1].get<int>();
            if (n <= 0 || d <= 0) {
                throw std::runtime_error(context + ": time signature values must be positive");
            }
            return {n, d};
        }

        if (value.is_string()) {
            const std::string s = value.get<std::string>();
            const auto slash = s.find('/');
            if (slash == std::string::npos) {
                throw std::runtime_error(context + ": invalid time signature string '" + s + "' (expected N/D)");
            }
            int n = 0, d = 0;
            try {
                n = std::stoi(s.substr(0, slash));
                d = std::stoi(s.substr(slash + 1));
            } catch (...) {
                throw std::runtime_error(context + ": cannot parse time signature '" + s + "'");
            }
            if (n <= 0 || d <= 0) {
                throw std::runtime_error(context + ": time signature values must be positive in '" + s + "'");
            }
            return {n, d};
        }

        if (value.is_object()) {
            if (!value.contains("numerator") || !value.contains("denominator") ||
                !value["numerator"].is_number_integer() || !value["denominator"].is_number_integer()) {
                throw std::runtime_error(context + ": time signature object requires integer numerator/denominator");
            }
            int n = value["numerator"].get<int>();
            int d = value["denominator"].get<int>();
            if (n <= 0 || d <= 0) {
                throw std::runtime_error(context + ": time signature values must be positive");
            }
            return {n, d};
        }

        throw std::runtime_error(context + ": unsupported time signature format");
    }

    static std::vector<int> parse_positive_int_array(const nlohmann::json& value, const std::string& context) {
        if (!value.is_array()) {
            throw std::runtime_error(context + ": expected array of positive integers");
        }
        std::vector<int> out;
        out.reserve(value.size());
        for (size_t i = 0; i < value.size(); ++i) {
            if (!value[i].is_number_integer()) {
                throw std::runtime_error(context + ": entry " + std::to_string(i) + " must be integer");
            }
            int v = value[i].get<int>();
            if (v <= 0) {
                throw std::runtime_error(context + ": entry " + std::to_string(i) + " must be positive");
            }
            out.push_back(v);
        }
        return out;
    }

    std::vector<std::vector<int>> getVoiceRhythmDomains(const std::string& config_file) const {
        // Step 1: collect raw (numerator, denominator) pairs per voice.
        std::vector<std::vector<std::pair<int,int>>> raw_per_voice(num_voices_);

        std::ifstream f(config_file);
        if (!f.is_open())
            throw std::runtime_error("Cannot open config file: " + config_file);
        nlohmann::json cfg;
        f >> cfg;

        if (cfg.contains("engine_domains")) {
            throw std::runtime_error(
                "'engine_domains' is deprecated in CLI configs. Use top-level 'voices' with per-voice pitch/rhythm domains.");
        }
        const bool has_global_rhythm = cfg.contains("global_domain") &&
                                       cfg["global_domain"].is_object() &&
                                       cfg["global_domain"].contains("rhythm");
        if (!cfg.contains("voices") || (!cfg["voices"].is_array() && !cfg["voices"].is_object())) {
            if (!has_global_rhythm)
                throw std::runtime_error(
                    "Config must have a 'voices' section or a 'global_domain.rhythm' fallback");
        }

        auto read_voice_rhythm = [&](int voice, const nlohmann::json& voice_node, const std::string& ctx) {
            if (!voice_node.is_object()) {
                throw std::runtime_error(ctx + " must be an object");
            }
            if (!voice_node.contains("rhythm")) {
                throw std::runtime_error(ctx + " is missing 'rhythm'");
            }

            const auto& rhythm = voice_node["rhythm"];
            const nlohmann::json* duration_values = nullptr;
            if (rhythm.is_object() && rhythm.contains("duration_values") && rhythm["duration_values"].is_array()) {
                duration_values = &rhythm["duration_values"];
            } else if (rhythm.is_array()) {
                duration_values = &rhythm;
            }

            if (!duration_values) {
                throw std::runtime_error(ctx + ".rhythm must contain 'duration_values' array");
            }

            std::vector<std::pair<int,int>> raw_fractions;
            for (const auto& item : *duration_values) {
                if (item.is_string()) {
                    raw_fractions.push_back(parse_duration_fraction(item.get<std::string>(), ctx));
                } else if (item.is_number_integer()) {
                    // Legacy integer: treat as N/1
                    raw_fractions.push_back({item.get<int>(), 1});
                } else {
                    throw std::runtime_error(ctx + ": duration_values entries must be fraction strings like \"1/4\".");
                }
            }
            if (raw_fractions.empty())
                throw std::runtime_error(ctx + ": 'duration_values' must not be empty");
            raw_per_voice[voice] = std::move(raw_fractions);
        };

        // Returns a reference to the voice-node that contains "rhythm" for voice v.
        // Priority: explicit "voice": v key > positional arr[v] > global_domain.
        auto resolve_rhythm_node = [&](int v) -> const nlohmann::json& {
            if (cfg.contains("voices") && cfg["voices"].is_array()) {
                const auto& arr = cfg["voices"];
                // Explicit "voice": v key — scan the whole array.
                for (const auto& e : arr)
                    if (e.is_object() && e.contains("voice") &&
                        e["voice"].is_number_integer() && e["voice"].get<int>() == v &&
                        e.contains("rhythm"))
                        return e;
                // Positional fallback (only when no conflicting "voice" key).
                if (v < (int)arr.size() && arr[v].is_object() && arr[v].contains("rhythm")) {
                    const auto& e = arr[v];
                    if (!e.contains("voice") || e["voice"].get<int>() == v)
                        return e;
                }
            } else if (cfg.contains("voices") && cfg["voices"].is_object()) {
                const std::string key = std::to_string(v);
                if (cfg["voices"].contains(key) && cfg["voices"][key].is_object() &&
                    cfg["voices"][key].contains("rhythm"))
                    return cfg["voices"][key];
            }
            if (has_global_rhythm)
                return cfg["global_domain"];
            throw std::runtime_error(
                "voices[" + std::to_string(v) +
                "] is missing 'rhythm' and no 'global_domain.rhythm' is defined");
        };

        for (int v = 0; v < num_voices_; ++v) {
            read_voice_rhythm(v, resolve_rhythm_node(v), "voices[" + std::to_string(v) + "]");
        }

        // Check every voice has a rhythm domain.
        for (int v = 0; v < num_voices_; ++v) {
            if (raw_per_voice[v].empty()) {
                throw std::runtime_error(
                    "Voice " + std::to_string(v) + " has no rhythm domain. "
                    "Add voices[" + std::to_string(v) + "].rhythm.duration_values, e.g. [\"1/4\"]");
            }
        }

        // Step 2: compute LCM of all denominators so every fraction maps to an integer tick count.
        // Use abs(n) — sign encodes rest vs note, not relevant for LCM.
        int base = 1;
        for (auto& fracs : raw_per_voice)
            for (auto& [n, d] : fracs)
                base = lcm_helper(base, d);  // d is always positive

        // Step 2.5: extend the LCM to cover any tuplet values declared in the meter section.
        // For tuplet T in time signature N/D the metric grid step is rhythm_base/(D*T).
        // For that to be an integer, rhythm_base must be divisible by D*T.
        // Without this, tuplets whose denominator does not divide the duration-value LCM
        // are silently ignored by parse_metric_hierarchy_grid_step_ticks.
        if ((cfg.contains("meter") && cfg["meter"].is_object()) ||
            (cfg.contains("metric") && cfg["metric"].is_object())) {
            const auto& meter = cfg.contains("meter") ? cfg["meter"] : cfg["metric"];
            if (meter.contains("tuplets") && meter["tuplets"].is_array()) {
                // Collect time-signature denominators from the meter section.
                std::vector<int> ts_denoms;
                if (meter.contains("time_signatures") && meter["time_signatures"].is_array()) {
                    for (const auto& ts_val : meter["time_signatures"]) {
                        try { ts_denoms.push_back(parse_time_signature_value(ts_val, "meter").second); }
                        catch (...) {}
                    }
                } else if (meter.contains("time_signature")) {
                    const auto& ts = meter["time_signature"];
                    try {
                        if (ts.is_array()) {
                            for (const auto& tsv : ts)
                                ts_denoms.push_back(parse_time_signature_value(tsv, "meter").second);
                        } else {
                            ts_denoms.push_back(parse_time_signature_value(ts, "meter").second);
                        }
                    } catch (...) {}
                } else if (meter.contains("denominator") && meter["denominator"].is_number_integer()) {
                    ts_denoms.push_back(meter["denominator"].get<int>());
                }
                if (ts_denoms.empty()) ts_denoms.push_back(4);  // default 4/4
                for (const auto& tv : meter["tuplets"]) {
                    if (!tv.is_number_integer()) continue;
                    const int t = tv.get<int>();
                    if (t <= 1) continue;
                    for (int den : ts_denoms)
                        if (den > 0) base = lcm_helper(base, den * t);
                }
            }
        }

        rhythm_base_ = base;  // stored for display

        // Step 3: convert each fraction to ticks = base * numerator / denominator.
        // Negative numerator (rest) produces a negative tick value.
        std::vector<std::vector<int>> result(num_voices_);
        for (int v = 0; v < num_voices_; ++v) {
            for (auto& [n, d] : raw_per_voice[v])
                result[v].push_back(base * n / d);
        }

        return result;
    }

    int getVoiceRhythmBase() const { return rhythm_base_; }

    std::vector<MusicalConstraintSolver::SolverConfig::MetricDomainEntry>
    getMetricDomain(const std::string& config_file) const {
        std::ifstream f(config_file);
        if (!f.is_open()) {
            throw std::runtime_error("Cannot open config file: " + config_file);
        }

        nlohmann::json cfg;
        f >> cfg;

        std::vector<MusicalConstraintSolver::SolverConfig::MetricDomainEntry> out;
        auto append_metric_entry = [&out](const std::pair<int, int>& ts,
                                          const std::vector<int>& tuplets,
                                          const std::vector<int>& beat_divisions) {
            MusicalConstraintSolver::SolverConfig::MetricDomainEntry entry;
            entry.numerator = ts.first;
            entry.denominator = ts.second;
            entry.tuplets = tuplets;
            entry.beat_divisions = beat_divisions;
            out.push_back(std::move(entry));
        };

        // Preferred source for dynamic CLI configs: top-level meter/metric section.
        if ((cfg.contains("meter") && cfg["meter"].is_object()) ||
            (cfg.contains("metric") && cfg["metric"].is_object())) {
            const auto& meter = cfg.contains("meter") && cfg["meter"].is_object() ? cfg["meter"] : cfg["metric"];
            std::vector<std::pair<int, int>> signatures;

            if (meter.contains("time_signatures")) {
                if (!meter["time_signatures"].is_array() || meter["time_signatures"].empty()) {
                    throw std::runtime_error("meter: 'time_signatures' must be a non-empty array");
                }
                for (size_t i = 0; i < meter["time_signatures"].size(); ++i) {
                    signatures.push_back(parse_time_signature_value(
                        meter["time_signatures"][i], "meter time_signatures[" + std::to_string(i) + "]"));
                }
            } else if (meter.contains("time_signature")) {
                const auto& ts = meter["time_signature"];
                if (ts.is_array() && !ts.empty() &&
                    (ts[0].is_string() || ts[0].is_array() || ts[0].is_object())) {
                    for (size_t i = 0; i < ts.size(); ++i) {
                        signatures.push_back(parse_time_signature_value(
                            ts[i], "meter time_signature[" + std::to_string(i) + "]"));
                    }
                } else {
                    signatures.push_back(parse_time_signature_value(ts, "meter time_signature"));
                }
            } else if (meter.contains("numerator") || meter.contains("denominator")) {
                signatures.push_back(parse_time_signature_value(meter, "meter"));
            }

            std::vector<int> tuplets;
            if (meter.contains("tuplets")) {
                tuplets = parse_positive_int_array(meter["tuplets"], "meter tuplets");
            }

            std::vector<int> beat_divisions;
            if (meter.contains("beat_divisions")) {
                beat_divisions = parse_positive_int_array(meter["beat_divisions"], "meter beat_divisions");
            }

            for (const auto& ts : signatures) {
                append_metric_entry(ts, tuplets, beat_divisions);
            }
        }

        if (out.empty() && cfg.contains("engine_domains")) {
            throw std::runtime_error(
                "metric domain via 'engine_domains' is deprecated in CLI configs. Use top-level 'meter' or 'metric'.");
        }

        return out;
    }

    bool getEnableMetricEngine(const std::string& config_file) const {
        std::ifstream f(config_file);
        if (!f.is_open()) return false;
        nlohmann::json cfg;
        try { f >> cfg; } catch (...) { return false; }

        if (cfg.contains("search_options") && cfg["search_options"].is_object() &&
            cfg["search_options"].contains("enable_metric_engine")) {
            return json_to_bool(cfg["search_options"]["enable_metric_engine"], false);
        }
        if (cfg.contains("enable_metric_engine")) {
            return json_to_bool(cfg["enable_metric_engine"], false);
        }
        return false;
    }

    const std::vector<RuleConfig>& getRules() const { return rules_; }
    
    void printLoadedConfig() const {
        std::cout << "📋 Loaded Configuration:" << std::endl;
        std::cout << "   Name: " << (name_.empty() ? "Unknown" : name_) << std::endl;
        std::cout << "   Description: " << (description_.empty() ? "None" : description_) << std::endl;
        std::cout << "   Solution Length: " << solution_length_ << std::endl;
        std::cout << "   Number of Voices: " << num_voices_ << std::endl;
        std::cout << "   Rules: " << rules_.size() << " configured" << std::endl;
        std::cout << "   Domains: " << domains_.size() << " configured" << std::endl;
        std::cout << "   Export Options: JSON=" << (export_json_ ? "✅" : "❌")
                  << ", TXT=" << (export_txt_ ? "✅" : "❌")
                  << ", XML=" << (export_xml_ ? "✅" : "❌")
                  << ", PNG=" << (export_png_ ? "✅" : "❌")
                  << ", MIDI=" << (export_midi_ ? "✅" : "❌") << std::endl;
        if (!export_filename_.empty()) {
            std::cout << "   Export Filename: " << export_filename_ << std::endl;
        }
        
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
        
        if (!domains_.empty()) {
            std::cout << "\n📝 Parsed Domains:" << std::endl;
            for (const auto& domain : domains_) {
                std::cout << "   Engine " << domain.engine_id << " (" << domain.type << "): ";
                std::cout << "[";
                for (size_t i = 0; i < domain.values.size(); ++i) {
                    std::cout << domain.values[i];
                    if (i < domain.values.size() - 1) std::cout << ", ";
                }
                std::cout << "] - " << domain.description << std::endl;
            }
            
            auto pitch_range = getPitchDomainRange();
            std::cout << "   Computed pitch range: [" << pitch_range.first << ", " << pitch_range.second << "]" << std::endl;
        }
    }
};

// Dynamic rule factory for creating actual constraint rules
class DynamicRuleFactory {
public:
    static std::shared_ptr<MusicalConstraints::MusicalRule> createFromConfig(
        const RuleConfig& ruleConfig) {
        
        if (ruleConfig.function == "all_different" || (ruleConfig.function == "not_equal" && ruleConfig.rule_type.find("pitches") != std::string::npos)) {
            return std::make_shared<AllDifferentPitchRule>(ruleConfig.description, ruleConfig.indices.size());
        }
        else if (ruleConfig.function == "consecutive_perfect_fifths") {
            int interval_size = !ruleConfig.parameters.empty() ? static_cast<int>(ruleConfig.parameters[0]) : 7;
            return std::make_shared<PerfectFifthIntervalRule>(ruleConfig.description, interval_size);
        }
        else if (ruleConfig.function == "palindrome_of_engine") {
            // Parameters: [source_engine, target_engine]
            int source_engine = !ruleConfig.parameters.empty() ? static_cast<int>(ruleConfig.parameters[0]) : 1;
            int target_engine = ruleConfig.parameters.size() > 1 ? static_cast<int>(ruleConfig.parameters[1]) : 3;
            return std::make_shared<PalindromeRule>(ruleConfig.description, source_engine, target_engine);
        }
        else if (ruleConfig.function == "retrograde_inversion_relationship" && ruleConfig.rule_type.find("cross-voice") != std::string::npos) {
            int center = !ruleConfig.parameters.empty() ? static_cast<int>(ruleConfig.parameters[0]) : 65;
            return std::make_shared<RetrogradeInversionRule>(ruleConfig.description, center, ruleConfig.indices.size());
        }
        else if (ruleConfig.function == "equal_values" && !ruleConfig.parameters.empty()) {
            int target_value = static_cast<int>(ruleConfig.parameters[0]);
            return std::make_shared<FixedValueRule>(ruleConfig.description, target_value);
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
    // Retrograde Inversion Rule - Voice 2 must be retrograde inversion of Voice 1
    class RetrogradeInversionRule : public MusicalConstraints::MusicalRule {
    private:
        std::string description_;
        int inversion_center_;
        int sequence_length_;
        
    public:
        RetrogradeInversionRule(const std::string& desc, int center, int length) 
            : description_(desc), inversion_center_(center), sequence_length_(length) {}
        
        MusicalConstraints::RuleResult check_rule(
            const MusicalConstraints::DualSolutionStorage& storage, 
            int current_index) const override {
            
            // For now, this is a placeholder that always succeeds
            // The actual retrograde inversion constraint needs to be implemented
            // at a higher level where we have access to multi-voice data
            
            // We can add some basic constraints here, like ensuring values are in range
            int current_value = storage.absolute(current_index);
            if (current_value < 60 || current_value > 71) {
                auto result = MusicalConstraints::RuleResult::Failure(1, "Value out of range");
                MusicalConstraints::BackjumpSuggestion suggestion(current_index, 1);
                suggestion.explanation = "Value " + std::to_string(current_value) + " is outside domain [60, 71]";
                result.add_suggestion(suggestion);
                return result;
            }
            
            return MusicalConstraints::RuleResult::Success();
        }
        
        std::string description() const override { return description_; }
        std::vector<int> get_dependent_variables(int current_index) const override {
            std::vector<int> deps;
            for (int i = 0; i <= current_index; ++i) deps.push_back(i);
            return deps;
        }
        std::string rule_type() const override { return "RetrogradeInversionRule"; }
    };

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
    
    // Perfect Fifth Interval Rule - Consecutive notes must differ by perfect fifth (7 semitones)
    class PerfectFifthIntervalRule : public MusicalConstraints::MusicalRule {
    private:
        std::string description_;
        int interval_size_;
        
    public:
        PerfectFifthIntervalRule(const std::string& desc, int interval_size = 7) 
            : description_(desc), interval_size_(interval_size) {}
        
        MusicalConstraints::RuleResult check_rule(
            const MusicalConstraints::DualSolutionStorage& storage, 
            int current_index) const override {
            
            if (current_index >= 1) {
                int current_note = storage.absolute(current_index);
                int previous_note = storage.absolute(current_index - 1);
                
                // Calculate interval (mod 12 for pitch class)
                int interval_up = (current_note - previous_note) % 12;
                if (interval_up < 0) interval_up += 12;
                
                int interval_down = (previous_note - current_note) % 12;
                if (interval_down < 0) interval_down += 12;
                
                // Perfect fifth is 7 semitones up or 5 semitones down (which is 7 up in opposite direction)
                if (interval_up != interval_size_ && interval_down != (12 - interval_size_)) {
                    auto result = MusicalConstraints::RuleResult::Failure(2, "Interval not perfect fifth");
                    MusicalConstraints::BackjumpSuggestion suggestion(current_index, 1);
                    suggestion.explanation = "Note " + std::to_string(current_note) + " after " + 
                                           std::to_string(previous_note) + " is not a perfect fifth away";
                    result.add_suggestion(suggestion);
                    return result;
                }
            }
            
            return MusicalConstraints::RuleResult::Success();
        }
        
        std::string description() const override { return description_; }
        std::vector<int> get_dependent_variables(int current_index) const override {
            std::vector<int> deps;
            if (current_index >= 1) {
                deps.push_back(current_index - 1);
                deps.push_back(current_index);
            }
            return deps;
        }
        std::string rule_type() const override { return "PerfectFifthIntervalRule"; }
    };
    
    // Palindrome Rule - One engine must be exact reverse of another
    class PalindromeRule : public MusicalConstraints::MusicalRule {
    private:
        std::string description_;
        int source_engine_;
        int target_engine_;
        
    public:
        PalindromeRule(const std::string& desc, int source_eng, int target_eng) 
            : description_(desc), source_engine_(source_eng), target_engine_(target_eng) {}
        
        MusicalConstraints::RuleResult check_rule(
            const MusicalConstraints::DualSolutionStorage& storage, 
            int current_index) const override {
            
            // For now, this is a placeholder that validates the basic constraint structure
            // The actual palindrome constraint would need multi-engine coordination
            // This could be implemented with custom propagators in the solver
            
            // Basic validation: ensure we're within valid range
            int current_value = storage.absolute(current_index);
            if (current_value < 60 || current_value > 71) {
                auto result = MusicalConstraints::RuleResult::Failure(1, "Value out of range");
                MusicalConstraints::BackjumpSuggestion suggestion(current_index, 1);
                suggestion.explanation = "Palindrome engine value " + std::to_string(current_value) + " out of range";
                result.add_suggestion(suggestion);
                return result;
            }
            
            return MusicalConstraints::RuleResult::Success();
        }
        
        std::string description() const override { return description_; }
        std::vector<int> get_dependent_variables(int current_index) const override {
            std::vector<int> deps;
            deps.push_back(current_index);
            return deps;
        }
        std::string rule_type() const override { return "PalindromeRule"; }
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
        
        // Load per-voice pitch domains from voices[*].pitch.midi_values
        solver_config.voice_domains = parser.getVoicePitchDomains(config_file);

        // Load per-voice rhythm domains from voices[*].rhythm.duration_values
        solver_config.voice_rhythm_domains = parser.getVoiceRhythmDomains(config_file);
        solver_config.rhythm_base = parser.getVoiceRhythmBase();
        solver_config.metric_domain = parser.getMetricDomain(config_file);
        solver_config.enable_metric_engine = parser.getEnableMetricEngine(config_file);
        solver_config.random_seed = parser.getRandomSeed(config_file);
        solver_config.heuristic_top_k = parser.getHeuristicTopK(config_file);
        solver_config.heuristic_trace = parser.getHeuristicTrace(config_file);
        solver_config.search_engine = parser.getSearchEngine(config_file);
        solver_config.variable_branching = parser.getVariableBranching(config_file);
        solver_config.value_selection = parser.getValueSelection(config_file);
        solver_config.restart_policy = parser.getRestartPolicy(config_file);

        // ── Parse harmonic_domain (Phase 3) ────────────────────────────────
        // Expected JSON shape:
        //   "harmonic_domain": [
        //     {"beat": 0, "chord": "C",  "quality": "major"},
        //     {"beat": 4, "chord": "F",  "quality": "major"},
        //     {"beat": 8, "chord": "G",  "quality": "dom7"},
        //     ...
        //   ]
        {
            nlohmann::json hcfg;
            {
                std::ifstream hf(config_file);
                if (hf.is_open()) { try { hf >> hcfg; } catch (...) {} }
            }
            if (hcfg.contains("harmonic_domain") && hcfg["harmonic_domain"].is_array() &&
                !hcfg["harmonic_domain"].empty()) {

                // Map note name -> pitch class (C=0)
                static const std::map<std::string, int> NOTE_MAP = {
                    {"C",0},{"C#",1},{"Db",1},{"D",2},{"D#",3},{"Eb",3},
                    {"E",4},{"F",5},{"F#",6},{"Gb",6},{"G",7},{"G#",8},
                    {"Ab",8},{"A",9},{"A#",10},{"Bb",10},{"B",11}
                };
                static const std::map<std::string, int> QUAL_MAP = {
                    {"major",0},{"maj",0},{"M",0},
                    {"minor",1},{"min",1},{"m",1},
                    {"dom7",2},{"dominant",2},{"dominant-seventh",2},{"7",2}
                };
                // Chord tones (pitch classes) per quality
                static const std::vector<std::vector<int>> CHORD_TONES = {
                    {0, 4, 7},        // major:  root, M3, P5
                    {0, 3, 7},        // minor:  root, m3, P5
                    {0, 4, 7, 10}     // dom7:   root, M3, P5, m7
                };

                auto& hd = solver_config.harmonic_domain;
                hd.enabled = true;

                for (const auto& entry : hcfg["harmonic_domain"]) {
                    MusicalConstraintSolver::SolverConfig::HarmonicEntry he;
                    he.beat_position = entry.value("beat", 0);

                    std::string chord_name = entry.value("chord", "C");
                    auto nit = NOTE_MAP.find(chord_name);
                    he.chord_root = (nit != NOTE_MAP.end()) ? nit->second : 0;

                    std::string qual_name = entry.value("quality", "major");
                    auto qit = QUAL_MAP.find(qual_name);
                    he.chord_quality = (qit != QUAL_MAP.end()) ? qit->second : 0;

                    // Derive chord tones from root + quality
                    const auto& tones = CHORD_TONES[std::min(he.chord_quality, 2)];
                    for (int t : tones)
                        he.chord_tones.push_back((he.chord_root + t) % 12);

                    hd.entries.push_back(he);
                }

                // Build tick-indexed harmonic_state so the neural scorer can resolve
                // chord by metric onset tick (computed live from rhythm_vars at score time).
                // beat_position is in quarter-note beats → convert to ticks via rhythm_base.
                const int seq_len       = solver_config.sequence_length;
                const int rb            = solver_config.rhythm_base;
                const int quarter_ticks = std::max(1, rb / 4);
                // Sort entries by beat_position ascending
                std::sort(hd.entries.begin(), hd.entries.end(),
                          [](const auto& a, const auto& b){
                              return a.beat_position < b.beat_position;
                          });
                // Total tick span: cover all chord entries plus seq_len quarter-note slots
                const int last_beat_tick = hd.entries.empty() ? 0
                                         : hd.entries.back().beat_position * quarter_ticks;
                const int total_ticks    = last_beat_tick + seq_len * quarter_ticks + 1;
                hd.harmonic_state.assign(total_ticks, -1);
                for (int i = 0; i < (int)hd.entries.size(); ++i) {
                    const auto& e       = hd.entries[i];
                    const int start_t   = e.beat_position * quarter_ticks;
                    const int end_t     = (i + 1 < (int)hd.entries.size())
                                          ? hd.entries[i+1].beat_position * quarter_ticks
                                          : total_ticks;
                    const int chord_cls = e.chord_root * 3 + std::min(e.chord_quality, 2);
                    for (int t = start_t; t < end_t && t < total_ticks; ++t)
                        hd.harmonic_state[t] = chord_cls;
                }

                std::cout << "   Harmonic domain: " << hd.entries.size()
                          << " chord entries parsed\n";
            }
        }

        solver_config.num_voices = parser.getNumVoices();
        solver_config.allow_repetitions = false;
        solver_config.style = MusicalConstraintSolver::SolverConfig::CONTEMPORARY;
        
        solver_config.verbose_output = false;
        
        // Create solver and add dynamic rules
        std::cout << "\n🔧 Creating Dynamic Constraint Solver" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        MusicalConstraintSolver::Solver solver(solver_config);
        solver.clear_rules();
        
        const auto& rules = parser.getRules();
        for (const auto& ruleConfig : rules) {
            if (ruleConfig.enabled) {
                // Pass normalized rule targeting to the solver's internal engine mapping.
                solver.add_rule_config(ruleConfig.rule_type, ruleConfig.function, ruleConfig.indices, 
                                     ruleConfig.target_engine, ruleConfig.target_engines, ruleConfig.engine_type, ruleConfig.description, 
                                     ruleConfig.parameters, ruleConfig.parameter_strings, ruleConfig.timepoints,
                                     ruleConfig.bar_pattern_type, ruleConfig.bar_pattern, ruleConfig.bar_pattern_count,
                                     ruleConfig.bar_pattern_repetitions, ruleConfig.bar_pattern_distribution,
                                     ruleConfig.allow_cross_barline);
                std::cout << "✅ Added rule: " << ruleConfig.description << std::endl;
            }
        }
        
        // Load dynamic rules (NEW SYSTEM)
        std::cout << "\n🎯 Loading Dynamic Rules (New System)" << std::endl;
        try {
            std::ifstream file(config_file);
            if (file.is_open()) {
                nlohmann::json config;
                file >> config;

                auto is_rule_enabled = [](const nlohmann::json& rule_json) -> bool {
                    if (!rule_json.is_object() || !rule_json.contains("enabled")) {
                        return true;
                    }
                    const auto& enabled = rule_json["enabled"];
                    if (enabled.is_boolean()) {
                        return enabled.get<bool>();
                    }
                    if (enabled.is_number_integer()) {
                        return enabled.get<int>() != 0;
                    }
                    if (enabled.is_string()) {
                        std::string s = enabled.get<std::string>();
                        std::transform(s.begin(), s.end(), s.begin(),
                                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                        return (s == "true" || s == "1" || s == "yes" || s == "on");
                    }
                    return true;
                };
                
                if (config.contains("dynamic_rules") && config["dynamic_rules"].is_array()) {
                    std::vector<nlohmann::json> dynamic_rules;
                    for (const auto& rule_json : config["dynamic_rules"]) {
                        if (is_rule_enabled(rule_json)) {
                            dynamic_rules.push_back(rule_json);
                        }
                    }

                    std::cout << "   Found " << config["dynamic_rules"].size()
                              << " dynamic rules (" << dynamic_rules.size() << " enabled)" << std::endl;
                    
                    solver.load_dynamic_rules(dynamic_rules);
                } else {
                    std::cout << "   No dynamic_rules section found (using legacy rules only)" << std::endl;
                }
                
                // NEW: Process enhanced rules array (includes wildcard rules)
                if (config.contains("rules") && config["rules"].is_array()) {
                    std::vector<nlohmann::json> enhanced_rules = config["rules"];
                    std::cout << "   Found " << enhanced_rules.size() << " enhanced rules (including wildcards)" << std::endl;
                    
                    int regular_count = 0;
                    int wildcard_count = 0;
                    int index_count = 0;
                    std::vector<nlohmann::json> compiled_rules; // Store for later application
                    
                    for (const auto& rule_json_in : enhanced_rules) {
                        try {
                            if (!is_rule_enabled(rule_json_in)) {
                                continue;
                            }

                            nlohmann::json rule_json = rule_json_in;
                            normalize_rule_targeting_for_cli_json(rule_json, solver_config.num_voices);

                            std::string rule_type = rule_json.value("rule_type", "unknown");
                            std::string type_field = rule_json.value("type", "");

                            const bool has_target_engine = rule_json.contains("target_engine") && rule_json["target_engine"].is_number_integer() && rule_json["target_engine"].get<int>() >= 0;
                            const bool has_target_engines = rule_json.contains("target_engines") && rule_json["target_engines"].is_array() && !rule_json["target_engines"].empty();
                            if (!rule_type.empty() && !has_target_engine && !has_target_engines && type_field != "index") {
                                throw std::runtime_error("rule '" + rule_json.value("id", rule_type) + "' is missing target_voices after normalization");
                            }

                            if (rule_type == "r-timepoint-relationship") {
                                std::vector<int> target_voices;
                                if (rule_json.contains("target_voices") && rule_json["target_voices"].is_array()) {
                                    for (const auto& v : rule_json["target_voices"]) {
                                        if (v.is_number_integer()) target_voices.push_back(v.get<int>());
                                    }
                                }
                                std::sort(target_voices.begin(), target_voices.end());
                                target_voices.erase(std::unique(target_voices.begin(), target_voices.end()), target_voices.end());
                                if (target_voices.size() < 2) {
                                    throw std::runtime_error("r-timepoint-relationship requires at least two target voices");
                                }

                                std::vector<std::pair<int, int>> voice_pairs;
                                const std::string pair_mode = rule_json.value("pair_mode", "adjacent");
                                if (pair_mode == "all_pairs") {
                                    for (size_t i = 0; i < target_voices.size(); ++i) {
                                        for (size_t j = i + 1; j < target_voices.size(); ++j) {
                                            voice_pairs.push_back({target_voices[i], target_voices[j]});
                                        }
                                    }
                                } else {
                                    for (size_t i = 0; i + 1 < target_voices.size(); ++i) {
                                        voice_pairs.push_back({target_voices[i], target_voices[i + 1]});
                                    }
                                }

                                std::vector<int> selected_indices;
                                if (rule_json.contains("indices") && rule_json["indices"].is_array()) {
                                    for (const auto& idx : rule_json["indices"]) {
                                        if (idx.is_number_integer()) selected_indices.push_back(idx.get<int>());
                                    }
                                }

                                // Check if we have fixed or variable rhythm
                                int fixed_step_ticks = -1;
                                bool fixed_rhythm = !solver_config.voice_rhythm_domains.empty();
                                for (const auto& rd : solver_config.voice_rhythm_domains) {
                                    if (rd.size() != 1 || rd[0] <= 0) {
                                        fixed_rhythm = false;
                                        break;
                                    }
                                    if (fixed_step_ticks < 0) {
                                        fixed_step_ticks = rd[0];
                                    } else if (fixed_step_ticks != rd[0]) {
                                        fixed_rhythm = false;
                                        break;
                                    }
                                }

                                if (selected_indices.empty()) {
                                    const std::string time_scope = rule_json.value(
                                        "time_scope", rule_json.value("temporal_scope", std::string("all_timepoints")));

                                    if (time_scope == "all_timepoints") {
                                        for (int i = 0; i < solver_config.sequence_length; ++i) {
                                            selected_indices.push_back(i);
                                        }
                                    } else if (time_scope == "at_timepoints") {
                                        if (!rule_json.contains("timepoints") || !rule_json["timepoints"].is_array()) {
                                            throw std::runtime_error(
                                                "r-timepoint-relationship with at_timepoints requires a timepoints array");
                                        }

                                        if (fixed_rhythm && fixed_step_ticks > 0) {
                                            // Fixed-rhythm case: direct tick-to-index conversion
                                            for (const auto& tp : rule_json["timepoints"]) {
                                                int tick = -1;
                                                if (tp.is_string()) {
                                                    tick = ConstraintSolverJSONParser::parse_score_time_to_ticks(
                                                        tp.get<std::string>(), solver_config.rhythm_base,
                                                        "r-timepoint-relationship timepoint");
                                                } else if (tp.is_number_integer()) {
                                                    tick = tp.get<int>();
                                                }
                                                if (tick < 0 || (tick % fixed_step_ticks) != 0) {
                                                    throw std::runtime_error(
                                                        "r-timepoint-relationship fixed-rhythm: timepoint not aligned with rhythmic step");
                                                }
                                                const int idx = tick / fixed_step_ticks;
                                                if (idx < 0 || idx >= solver_config.sequence_length) {
                                                    throw std::runtime_error("r-timepoint-relationship timepoint maps to out-of-range index");
                                                }
                                                selected_indices.push_back(idx);
                                            }
                                        } else {
                                            // Variable-rhythm case: use onset-aware mapping
                                            auto voice_onsets = compute_voice_onsets(solver_config.voice_rhythm_domains, solver_config.sequence_length);
                                            for (const auto& tp : rule_json["timepoints"]) {
                                                int tick = -1;
                                                if (tp.is_string()) {
                                                    tick = ConstraintSolverJSONParser::parse_score_time_to_ticks(
                                                        tp.get<std::string>(), solver_config.rhythm_base,
                                                        "r-timepoint-relationship timepoint");
                                                } else if (tp.is_number_integer()) {
                                                    tick = tp.get<int>();
                                                }
                                                if (tick < 0) {
                                                    throw std::runtime_error("r-timepoint-relationship timepoint is negative");
                                                }
                                                // Find first index at or after this tick for each voice
                                                for (int v = 0; v < static_cast<int>(voice_onsets.size()); ++v) {
                                                    for (int i = 0; i < solver_config.sequence_length; ++i) {
                                                        if (voice_onsets[v][i] >= tick) {
                                                            selected_indices.push_back(i);
                                                            break;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    } else if (time_scope == "on_beats") {
                                        if (solver_config.metric_domain.empty()) {
                                            throw std::runtime_error("r-timepoint-relationship on_beats requires metric_domain");
                                        }
                                        const auto& ts = solver_config.metric_domain.front();
                                        const int numerator = ts.numerator;
                                        const int denominator = ts.denominator;
                                        if (numerator <= 0 || denominator <= 0 ||
                                            (solver_config.rhythm_base % denominator) != 0) {
                                            throw std::runtime_error(
                                                "r-timepoint-relationship on_beats has incompatible metric_domain/rhythm_base");
                                        }
                                        const int beat_tick = solver_config.rhythm_base / denominator;

                                        std::set<int> beat_filter;
                                        if (rule_json.contains("beat_numbers") && rule_json["beat_numbers"].is_array()) {
                                            for (const auto& b : rule_json["beat_numbers"]) {
                                                if (b.is_number_integer()) beat_filter.insert(b.get<int>());
                                            }
                                        }

                                        if (fixed_rhythm && fixed_step_ticks > 0) {
                                            // Fixed-rhythm: compute beat position from tick = i * fixed_step_ticks
                                            for (int i = 0; i < solver_config.sequence_length; ++i) {
                                                const int tick = i * fixed_step_ticks;
                                                if ((tick % beat_tick) != 0) continue;
                                                const int beat = ((tick / beat_tick) % numerator) + 1;
                                                const bool selected = beat_filter.empty() || beat_filter.count(beat) > 0;
                                                if (selected) {
                                                    selected_indices.push_back(i);
                                                }
                                            }
                                        } else {
                                            // Variable-rhythm: find indices where voice onsets fall on beat boundaries
                                            auto voice_onsets = compute_voice_onsets(solver_config.voice_rhythm_domains, solver_config.sequence_length);
                                            for (int i = 0; i < solver_config.sequence_length; ++i) {
                                                for (int v = 0; v < static_cast<int>(voice_onsets.size()); ++v) {
                                                    if (i >= static_cast<int>(voice_onsets[v].size())) continue;
                                                    const int onset = voice_onsets[v][i];
                                                    if ((onset % beat_tick) == 0) {
                                                        const int beat = ((onset / beat_tick) % numerator) + 1;
                                                        if (beat_filter.empty() || beat_filter.count(beat) > 0) {
                                                            selected_indices.push_back(i);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        throw std::runtime_error(
                                            "r-timepoint-relationship time_scope must be all_timepoints, at_timepoints, or on_beats");
                                    }
                                }

                                std::sort(selected_indices.begin(), selected_indices.end());
                                selected_indices.erase(std::unique(selected_indices.begin(), selected_indices.end()), selected_indices.end());
                                selected_indices.erase(
                                    std::remove_if(selected_indices.begin(), selected_indices.end(), [&](int idx) {
                                        return idx < 0 || idx >= solver_config.sequence_length;
                                    }),
                                    selected_indices.end());

                                if (selected_indices.empty()) {
                                    throw std::runtime_error("r-timepoint-relationship produced no valid indices");
                                }

                                std::vector<int> interval_set;
                                if (rule_json.contains("parameters") && rule_json["parameters"].is_array()) {
                                    for (const auto& p : rule_json["parameters"]) {
                                        if (p.is_number_integer()) interval_set.push_back(p.get<int>());
                                    }
                                }
                                if (rule_json.contains("constraint_function") &&
                                    rule_json["constraint_function"].is_object() &&
                                    rule_json["constraint_function"].contains("parameters") &&
                                    rule_json["constraint_function"]["parameters"].is_array()) {
                                    for (const auto& p : rule_json["constraint_function"]["parameters"]) {
                                        if (p.is_number_integer()) interval_set.push_back(p.get<int>());
                                    }
                                }
                                std::sort(interval_set.begin(), interval_set.end());
                                interval_set.erase(std::unique(interval_set.begin(), interval_set.end()), interval_set.end());

                                std::string base_expr;
                                if (rule_json.contains("constraint") && rule_json["constraint"].is_string()) {
                                    base_expr = rule_json["constraint"].get<std::string>();
                                } else {
                                    const std::string function = rule_json.value("constraint", rule_json.value("function", std::string("interval_in_set")));
                                    std::ostringstream arr;
                                    arr << "[";
                                    for (size_t i = 0; i < interval_set.size(); ++i) {
                                        if (i > 0) arr << ", ";
                                        arr << interval_set[i];
                                    }
                                    arr << "]";

                                    if (function == "interval_not_in_set") {
                                        base_expr = "abs(voice[v2].pitch[i] - voice[v1].pitch[i]) not_in " + arr.str();
                                    } else {
                                        base_expr = "abs(voice[v2].pitch[i] - voice[v1].pitch[i]) in " + arr.str();
                                    }
                                }

                                static const std::regex bracket_i_expr(R"(\[\s*(i(?:\s*[+-]\s*\d+)?)\s*\])");
                                static const std::regex voice_v1_re(R"(voice\[(v1|a)\])");
                                static const std::regex voice_v2_re(R"(voice\[(v2|b)\])");

                                int generated = 0;
                                const std::string base_id = rule_json.value("id", std::string("timepoint_relationship"));
                                for (const auto& pr : voice_pairs) {
                                    for (int idx : selected_indices) {
                                        std::string expr = std::regex_replace(base_expr, voice_v1_re, "voice[" + std::to_string(pr.first) + "]");
                                        expr = std::regex_replace(expr, voice_v2_re, "voice[" + std::to_string(pr.second) + "]");

                                        std::string rebuilt;
                                        std::size_t cursor = 0;
                                        for (std::sregex_iterator it(expr.begin(), expr.end(), bracket_i_expr), end; it != end; ++it) {
                                            const auto& m = *it;
                                            rebuilt.append(expr, cursor, static_cast<std::size_t>(m.position()) - cursor);
                                            std::string inside = m[1].str();
                                            int concrete_idx = idx;
                                            inside.erase(std::remove_if(inside.begin(), inside.end(),
                                                                        [](unsigned char c) { return std::isspace(c); }),
                                                         inside.end());
                                            if (inside == "i") {
                                                concrete_idx = idx;
                                            } else if (inside.rfind("i+", 0) == 0) {
                                                concrete_idx = idx + std::stoi(inside.substr(2));
                                            } else if (inside.rfind("i-", 0) == 0) {
                                                concrete_idx = idx - std::stoi(inside.substr(2));
                                            }
                                            rebuilt += "[" + std::to_string(concrete_idx) + "]";
                                            cursor = static_cast<std::size_t>(m.position() + m.length());
                                        }
                                        rebuilt.append(expr, cursor, std::string::npos);

                                        nlohmann::json generated_rule = {
                                            {"id", base_id + "_v" + std::to_string(pr.first) + "_" + std::to_string(pr.second) + "_i" + std::to_string(idx)},
                                            {"type", "basic_constraint"},
                                            {"mode", "true_false"},
                                            {"expression", rebuilt}
                                        };

                                        auto compiled = DynamicRules::DynamicRuleCompiler::compile_from_json(generated_rule);
                                        if (compiled) {
                                            solver.apply_compiled_constraint(std::move(compiled));
                                            generated++;
                                        }
                                    }
                                }

                                regular_count += generated;
                                std::cout << "     ✅ Compiled timepoint relationship rule: " << base_id
                                          << " (" << generated << " concrete constraints)" << std::endl;
                            } else if (rule_type == "wildcard_constraint") {
                                // Process wildcard rule - apply directly using new integration method
                                auto compiled = DynamicRules::WildcardRuleCompiler::compile_wildcard_from_json(rule_json);
                                if (compiled) {
                                    wildcard_count++;
                                    std::cout << "     ✅ Compiled wildcard rule: " << compiled->rule_id << std::endl;
                                    
                                    // Apply the compiled constraint directly to the solver
                                    solver.apply_compiled_constraint(std::move(compiled));
                                }
                            } else if (type_field == "index") {
                                // ── INDEX RULE ───────────────────────────────────────────────
                                // Pins specific voice positions to fixed pitch + rhythm values.
                                // Format:
                                //   { "id": "...", "type": "index", "voice": V,
                                //     "events": [ [pos, "rhythm_fraction", pitch_or_null], ... ] }
                                // pitch_or_null == null  →  rest  (rhythm must be negative)
                                // pitch_or_null == int   →  note  (rhythm must be positive)
                                std::string rule_id = rule_json.value("id", "index_rule");
                                if (!rule_json.contains("voice") || !rule_json["voice"].is_number_integer())
                                    throw std::runtime_error("index rule '" + rule_id + "': missing or invalid 'voice' field");
                                if (!rule_json.contains("events") || !rule_json["events"].is_array())
                                    throw std::runtime_error("index rule '" + rule_id + "': missing or invalid 'events' array");

                                int voice = rule_json["voice"].get<int>();
                                int rhythm_base = solver_config.rhythm_base;

                                // Collect events: {pos, rhythm_ticks, pitch_or_sentinel}
                                struct IndexEvent { int pos; int rhythm_ticks; int pitch; };
                                std::vector<IndexEvent> events;

                                for (size_t ei = 0; ei < rule_json["events"].size(); ++ei) {
                                    const auto& ev = rule_json["events"][ei];
                                    if (!ev.is_array() || ev.size() != 3)
                                        throw std::runtime_error("index rule '" + rule_id + "': event[" + std::to_string(ei) + "] must be [pos, rhythm, pitch_or_null]");

                                    int pos = ev[0].get<int>();
                                    std::string rhythm_str = ev[1].get<std::string>();
                                    bool pitch_is_null = ev[2].is_null();
                                    int pitch_midi = pitch_is_null ? IntegratedMusicalSpace::REST_PITCH_SENTINEL : ev[2].get<int>();

                                    auto [r_num, r_den] = ConstraintSolverJSONParser::parse_duration_fraction(
                                        rhythm_str, "index rule '" + rule_id + "' event[" + std::to_string(ei) + "]");
                                    int rhythm_ticks = rhythm_base * r_num / r_den;

                                    bool is_rest = (rhythm_ticks < 0);
                                    if (is_rest && !pitch_is_null)
                                        throw std::runtime_error("index rule '" + rule_id + "' event[" + std::to_string(ei) + "]: rest rhythm (negative) requires null pitch");
                                    if (!is_rest && pitch_is_null)
                                        throw std::runtime_error("index rule '" + rule_id + "' event[" + std::to_string(ei) + "]: note rhythm (positive) requires a non-null pitch");

                                    events.push_back({pos, rhythm_ticks, pitch_midi});
                                }

                                auto compiled = std::make_unique<DynamicRules::CompiledConstraint>(rule_id, "index rule: pin voice " + std::to_string(voice));
                                compiled->post_constraint = [voice, events, rule_id](DynamicRules::ConstraintContext& ctx) {
                                    if (voice < 0 || voice >= ctx.num_voices)
                                        throw std::runtime_error("index rule '" + rule_id + "': voice " + std::to_string(voice) + " out of range (num_voices=" + std::to_string(ctx.num_voices) + ")");
                                    for (const auto& ev : events) {
                                        if (ev.pos < 0 || ev.pos >= ctx.sequence_length)
                                            throw std::runtime_error("index rule '" + rule_id + "': position " + std::to_string(ev.pos) + " out of range (sequence_length=" + std::to_string(ctx.sequence_length) + ")");
                                        int idx = voice * ctx.sequence_length + ev.pos;
                                        // Pin rhythm variable
                                        if (ctx.rhythm_vars && idx < (int)ctx.rhythm_vars->size())
                                            Gecode::dom(*ctx.space, (*ctx.rhythm_vars)[idx], Gecode::IntSet(ev.rhythm_ticks, ev.rhythm_ticks));
                                        // Pin pitch variable (notes only; rests use sentinel which is handled by gecode_cluster_integration)
                                        if (ev.pitch != IntegratedMusicalSpace::REST_PITCH_SENTINEL) {
                                            if (ctx.pitch_vars && idx < (int)ctx.pitch_vars->size())
                                                Gecode::dom(*ctx.space, (*ctx.pitch_vars)[idx], Gecode::IntSet(ev.pitch, ev.pitch));
                                        }
                                    }
                                };

                                index_count++;
                                std::cout << "     ✅ Compiled index rule: " << rule_id
                                          << " (" << events.size() << " event(s) on voice " << voice << ")" << std::endl;
                                solver.apply_compiled_constraint(std::move(compiled));
                                // ─────────────────────────────────────────────────────────────
                            } else if (rule_type == "r-rhythm-rhythm") {
                                // ── R-RHYTHM-RHYTHM ──────────────────────────────────────────
                                // Position-aligned cross-voice rhythm constraint.
                                // Modes: "isorhythm"           — r_v1[i] == r_v2[i]
                                //        "abs_isorhythm"       — |r_v1[i]| == |r_v2[i]|
                                //        "augmentation"        — |r_v1[i]| == ratio * |r_v2[i]|
                                //        "diminution"          — ratio * |r_v1[i]| == |r_v2[i]|
                                //        "no_simultaneous_rests" — r_v1[i]>=0 OR r_v2[i]>=0
                                //        "rest_complement"     — exactly one voice rests per position
                                std::vector<int> rr_voices;
                                if (rule_json.contains("target_voices") && rule_json["target_voices"].is_array())
                                    for (const auto& v : rule_json["target_voices"])
                                        if (v.is_number_integer()) rr_voices.push_back(v.get<int>());
                                if (rr_voices.size() != 2)
                                    throw std::runtime_error(
                                        "r-rhythm-rhythm '" + rule_json.value("id", "") + "' requires exactly 2 target_voices");

                                const int rr_v1   = rr_voices[0];
                                const int rr_v2   = rr_voices[1];
                                const std::string rr_mode = rule_json.value("constraint", "isorhythm");
                                const std::string rr_id   = rule_json.value(
                                    "id", "rr_v" + std::to_string(rr_v1) + "_v" + std::to_string(rr_v2));

                                // Parse ratio as rational num/den to support floats (e.g. 0.5 = 1/2, 1.5 = 3/2)
                                int rr_ratio_num = 2;
                                int rr_ratio_den = 1;
                                if (rule_json.contains("parameters") && rule_json["parameters"].is_array() &&
                                        !rule_json["parameters"].empty()) {
                                    const auto& p0 = rule_json["parameters"][0];
                                    if (p0.is_number()) {
                                        double ratio_d = p0.get<double>();
                                        int best_den = 1;
                                        for (int d = 1; d <= 64; ++d) {
                                            double scaled = ratio_d * d;
                                            if (std::abs(scaled - std::round(scaled)) < 1e-9) { best_den = d; break; }
                                        }
                                        rr_ratio_num = std::max(1, static_cast<int>(std::round(ratio_d * best_den)));
                                        rr_ratio_den = best_den;
                                    }
                                }

                                std::vector<int> rr_indices;
                                if (rule_json.contains("indices") && rule_json["indices"].is_array())
                                    for (const auto& idx : rule_json["indices"])
                                        if (idx.is_number_integer()) rr_indices.push_back(idx.get<int>());

                                auto compiled = std::make_unique<DynamicRules::CompiledConstraint>(
                                    rr_id, "r-rhythm-rhythm: " + rr_mode +
                                           " (voices " + std::to_string(rr_v1) + "," + std::to_string(rr_v2) + ")");

                                const bool rr_is_heuristic = json_as_bool(rule_json, "heuristic");
                                if (rr_is_heuristic) {
                                    // ── Heuristic mode: guide value ordering, do not hard-post ──
                                    compiled->is_heuristic = true;
                                    compiled->heuristic_mode = DynamicRules::HeuristicMode::REAL_HEURISTIC;
                                    compiled->heuristic_variable_type = "rhythm";
                                    compiled->applies_to_voices = {rr_v1, rr_v2};
                                    compiled->score_candidate = [rr_v1, rr_v2, rr_mode, rr_ratio_num, rr_ratio_den, rr_indices]
                                            (const DynamicRules::ConstraintContext& ctx,
                                             const DynamicRules::HeuristicCandidateContext& cand) -> double {
                                        if (!ctx.rhythm_vars) return 0.0;
                                        const int this_v  = cand.voice;
                                        const int other_v = (this_v == rr_v1) ? rr_v2 : rr_v1;
                                        const int pos = cand.position;
                                        const int cv  = cand.candidate_value;
                                        if (!rr_indices.empty() &&
                                            std::find(rr_indices.begin(), rr_indices.end(), pos) == rr_indices.end())
                                            return 0.0;
                                        const int oi = other_v * ctx.sequence_length + pos;
                                        if (oi < 0 || oi >= (int)ctx.rhythm_vars->size()) return 0.0;
                                        const Gecode::IntVar& ov = (*ctx.rhythm_vars)[oi];
                                        const int oe = (ov.min() + ov.max()) / 2;
                                        if (rr_mode == "isorhythm")
                                            return (cv == oe) ? 1.0 : 0.0;
                                        if (rr_mode == "abs_isorhythm")
                                            return (std::abs(cv) == std::abs(oe)) ? 1.0 : 0.0;
                                        if (rr_mode == "augmentation")
                                            return (std::abs(cv) * rr_ratio_den == std::abs(oe) * rr_ratio_num) ? 1.0 : 0.0;
                                        if (rr_mode == "diminution")
                                            return (std::abs(cv) * rr_ratio_num == std::abs(oe) * rr_ratio_den) ? 1.0 : 0.0;
                                        if (rr_mode == "no_simultaneous_rests") {
                                            const bool cv_rest = cv < 0, oe_rest = oe < 0;
                                            return (!cv_rest || !oe_rest) ? 1.0 : -1.0;
                                        }
                                        if (rr_mode == "rest_complement") {
                                            const bool cv_rest = cv < 0, oe_rest = oe < 0;
                                            return (cv_rest != oe_rest) ? 1.0 : -1.0;
                                        }
                                        return 0.0;
                                    };
                                } else {
                                compiled->post_constraint = [rr_v1, rr_v2, rr_mode, rr_ratio_num, rr_ratio_den, rr_indices, rr_id]
                                        (DynamicRules::ConstraintContext& ctx) {
                                    if (!ctx.rhythm_vars) return;
                                    if (rr_v1 < 0 || rr_v1 >= ctx.num_voices ||
                                        rr_v2 < 0 || rr_v2 >= ctx.num_voices)
                                        throw std::runtime_error(
                                            "r-rhythm-rhythm '" + rr_id + "': voice out of range");

                                    std::vector<int> indices = rr_indices;
                                    if (indices.empty())
                                        for (int i = 0; i < ctx.sequence_length; ++i) indices.push_back(i);

                                    for (int i : indices) {
                                        if (i < 0 || i >= ctx.sequence_length) continue;
                                        const int idx1 = rr_v1 * ctx.sequence_length + i;
                                        const int idx2 = rr_v2 * ctx.sequence_length + i;
                                        if (idx1 >= (int)ctx.rhythm_vars->size() ||
                                            idx2 >= (int)ctx.rhythm_vars->size()) continue;

                                        Gecode::IntVar r1 = (*ctx.rhythm_vars)[idx1];
                                        Gecode::IntVar r2 = (*ctx.rhythm_vars)[idx2];

                                        if (rr_mode == "isorhythm") {
                                            Gecode::rel(*ctx.space, r1, Gecode::IRT_EQ, r2);
                                        } else if (rr_mode == "abs_isorhythm") {
                                            Gecode::IntVar a1 = Gecode::expr(*ctx.space, Gecode::abs(r1));
                                            Gecode::IntVar a2 = Gecode::expr(*ctx.space, Gecode::abs(r2));
                                            Gecode::rel(*ctx.space, a1, Gecode::IRT_EQ, a2);
                                        } else if (rr_mode == "augmentation") {
                                            // Sign must match: both note (>=0) or both rest (<0)
                                            Gecode::BoolVar s1(*ctx.space, 0, 1);
                                            Gecode::BoolVar s2(*ctx.space, 0, 1);
                                            Gecode::rel(*ctx.space, r1, Gecode::IRT_GQ, 0, s1);
                                            Gecode::rel(*ctx.space, r2, Gecode::IRT_GQ, 0, s2);
                                            Gecode::rel(*ctx.space, s1, Gecode::IRT_EQ, s2);
                                            // Duration ratio: den * |r1| == num * |r2|
                                            Gecode::IntVar a1 = Gecode::expr(*ctx.space, Gecode::abs(r1));
                                            Gecode::IntVar a2 = Gecode::expr(*ctx.space, Gecode::abs(r2));
                                            Gecode::rel(*ctx.space,
                                                Gecode::expr(*ctx.space, rr_ratio_den * a1),
                                                Gecode::IRT_EQ,
                                                Gecode::expr(*ctx.space, rr_ratio_num * a2));
                                        } else if (rr_mode == "diminution") {
                                            // Sign must match
                                            Gecode::BoolVar s1(*ctx.space, 0, 1);
                                            Gecode::BoolVar s2(*ctx.space, 0, 1);
                                            Gecode::rel(*ctx.space, r1, Gecode::IRT_GQ, 0, s1);
                                            Gecode::rel(*ctx.space, r2, Gecode::IRT_GQ, 0, s2);
                                            Gecode::rel(*ctx.space, s1, Gecode::IRT_EQ, s2);
                                            // Duration ratio: num * |r1| == den * |r2|
                                            Gecode::IntVar a1 = Gecode::expr(*ctx.space, Gecode::abs(r1));
                                            Gecode::IntVar a2 = Gecode::expr(*ctx.space, Gecode::abs(r2));
                                            Gecode::rel(*ctx.space,
                                                Gecode::expr(*ctx.space, rr_ratio_num * a1),
                                                Gecode::IRT_EQ,
                                                Gecode::expr(*ctx.space, rr_ratio_den * a2));
                                        } else if (rr_mode == "no_simultaneous_rests") {
                                            Gecode::BoolVar b1(*ctx.space, 0, 1);
                                            Gecode::BoolVar b2(*ctx.space, 0, 1);
                                            Gecode::rel(*ctx.space, r1, Gecode::IRT_GQ, 0, b1);
                                            Gecode::rel(*ctx.space, r2, Gecode::IRT_GQ, 0, b2);
                                            Gecode::rel(*ctx.space, b1, Gecode::BOT_OR, b2, 1);
                                        } else if (rr_mode == "rest_complement") {
                                            Gecode::BoolVar b1(*ctx.space, 0, 1);
                                            Gecode::BoolVar b2(*ctx.space, 0, 1);
                                            Gecode::rel(*ctx.space, r1, Gecode::IRT_GQ, 0, b1);
                                            Gecode::rel(*ctx.space, r2, Gecode::IRT_GQ, 0, b2);
                                            Gecode::rel(*ctx.space, b1, Gecode::IRT_NQ, b2);
                                        } else {
                                            throw std::runtime_error(
                                                "r-rhythm-rhythm '" + rr_id +
                                                "': unknown constraint mode '" + rr_mode + "'");
                                        }
                                    }
                                }; // end post_constraint
                                } // end else (not heuristic)

                                if (!rr_is_heuristic) ++regular_count;
                                std::cout << "     ✅ Compiled r-rhythm-rhythm: " << rr_id
                                          << " (mode=" << rr_mode
                                          << ", voices " << rr_v1 << " and " << rr_v2 << ")" << std::endl;
                                solver.apply_compiled_constraint(std::move(compiled));
                                // ─────────────────────────────────────────────────────────────
                            } else if (rule_type == "r-pitch-pitch") {
                                // ── R-PITCH-PITCH ─────────────────────────────────────────────
                                // Position-aligned cross-voice pitch (harmonic) constraint.
                                // Phase 1 modes (single position):
                                //   "no_unison", "same_pitch", "voice_above", "voice_below",
                                //   "exact_interval", "min_interval", "max_interval",
                                //   "interval_range", "interval_class"
                                // Phase 2 modes (consecutive pairs i, i+1):
                                //   "no_consecutive_fifths", "no_consecutive_octaves",
                                //   "no_parallel_motion", "contrary_motion"
                                // Optional: "stride" (int) — apply every N positions; "offset" (int)
                                std::vector<int> pp_voices;
                                if (rule_json.contains("target_voices") && rule_json["target_voices"].is_array())
                                    for (const auto& v : rule_json["target_voices"])
                                        if (v.is_number_integer()) pp_voices.push_back(v.get<int>());
                                if (pp_voices.size() != 2)
                                    throw std::runtime_error(
                                        "r-pitch-pitch '" + rule_json.value("id", "") + "' requires exactly 2 target_voices");

                                const int pp_v1 = pp_voices[0];
                                const int pp_v2 = pp_voices[1];
                                const std::string pp_mode = rule_json.value("constraint", "no_unison");
                                const std::string pp_id   = rule_json.value(
                                    "id", "pp_v" + std::to_string(pp_v1) + "_v" + std::to_string(pp_v2));

                                // Parse parameters
                                std::vector<int> pp_params;
                                if (rule_json.contains("parameters") && rule_json["parameters"].is_array())
                                    for (const auto& p : rule_json["parameters"])
                                        if (p.is_number()) pp_params.push_back(static_cast<int>(p.get<double>()));

                                std::vector<int> pp_indices;
                                if (rule_json.contains("indices") && rule_json["indices"].is_array())
                                    for (const auto& idx : rule_json["indices"])
                                        if (idx.is_number_integer()) pp_indices.push_back(idx.get<int>());

                                // stride: apply constraint every N positions (0 = all)
                                int pp_stride = 0;
                                int pp_offset = 0;
                                if (rule_json.contains("stride") && rule_json["stride"].is_number_integer())
                                    pp_stride = std::max(1, rule_json["stride"].get<int>());
                                if (rule_json.contains("offset") && rule_json["offset"].is_number_integer())
                                    pp_offset = std::max(0, rule_json["offset"].get<int>());

                                // Validate mode-specific parameter requirements
                                if ((pp_mode == "exact_interval" || pp_mode == "min_interval" || pp_mode == "max_interval") &&
                                        pp_params.empty())
                                    throw std::runtime_error(
                                        "r-pitch-pitch '" + pp_id + "': mode '" + pp_mode + "' requires at least one parameter");
                                if (pp_mode == "interval_range" && pp_params.size() < 2)
                                    throw std::runtime_error(
                                        "r-pitch-pitch '" + pp_id + "': interval_range requires parameters [min, max]");
                                if (pp_mode == "interval_class" && pp_params.empty())
                                    throw std::runtime_error(
                                        "r-pitch-pitch '" + pp_id + "': interval_class requires at least one class in parameters");

                                auto compiled_pp = std::make_unique<DynamicRules::CompiledConstraint>(
                                    pp_id, "r-pitch-pitch: " + pp_mode +
                                           " (voices " + std::to_string(pp_v1) + "," + std::to_string(pp_v2) + ")");

                                const bool pp_is_heuristic = json_as_bool(rule_json, "heuristic");
                                if (pp_is_heuristic) {
                                    // ── Heuristic mode: guide value ordering, do not hard-post ──
                                    compiled_pp->is_heuristic = true;
                                    compiled_pp->heuristic_mode = DynamicRules::HeuristicMode::REAL_HEURISTIC;
                                    compiled_pp->heuristic_variable_type = "pitch";
                                    compiled_pp->applies_to_voices = {pp_v1, pp_v2};
                                    compiled_pp->score_candidate = [pp_v1, pp_v2, pp_mode, pp_params, pp_indices, pp_stride, pp_offset]
                                            (const DynamicRules::ConstraintContext& ctx,
                                             const DynamicRules::HeuristicCandidateContext& cand) -> double {
                                        if (!ctx.pitch_vars) return 0.0;
                                        const int this_v  = cand.voice;
                                        const int other_v = (this_v == pp_v1) ? pp_v2 : pp_v1;
                                        const int pos = cand.position;
                                        const int cv  = cand.candidate_value;
                                        // Check active position set
                                        if (pp_stride > 0) {
                                            if (pos < pp_offset || (pos - pp_offset) % pp_stride != 0) return 0.0;
                                        } else if (!pp_indices.empty()) {
                                            if (std::find(pp_indices.begin(), pp_indices.end(), pos) == pp_indices.end()) return 0.0;
                                        }
                                        const int oi = other_v * ctx.sequence_length + pos;
                                        if (oi < 0 || oi >= (int)ctx.pitch_vars->size()) return 0.0;
                                        const Gecode::IntVar& ov = (*ctx.pitch_vars)[oi];
                                        const int oe = (ov.min() + ov.max()) / 2;
                                        if (pp_mode == "no_unison")
                                            return (cv != oe) ? 1.0 : -1.0;
                                        if (pp_mode == "same_pitch")
                                            return (cv == oe) ? 1.0 : -0.5;
                                        if (pp_mode == "voice_above") {
                                            if (this_v == pp_v1) return cv > oe ? 1.0 : (cv == oe ? -0.5 : -1.0);
                                            else                  return cv < oe ? 1.0 : (cv == oe ? -0.5 : -1.0);
                                        }
                                        if (pp_mode == "voice_below") {
                                            if (this_v == pp_v1) return cv < oe ? 1.0 : (cv == oe ? -0.5 : -1.0);
                                            else                  return cv > oe ? 1.0 : (cv == oe ? -0.5 : -1.0);
                                        }
                                        if (pp_mode == "exact_interval") {
                                            if (pp_params.empty()) return 0.0;
                                            return (cv - oe == pp_params[0]) ? 1.0 : 0.0;
                                        }
                                        if (pp_mode == "min_interval") {
                                            if (pp_params.empty()) return 0.0;
                                            return (std::abs(cv - oe) >= pp_params[0]) ? 1.0 : -0.5;
                                        }
                                        if (pp_mode == "max_interval") {
                                            if (pp_params.empty()) return 0.0;
                                            return (std::abs(cv - oe) <= pp_params[0]) ? 1.0 : -0.5;
                                        }
                                        if (pp_mode == "interval_range") {
                                            if (pp_params.size() < 2) return 0.0;
                                            const int ad = std::abs(cv - oe);
                                            return (ad >= pp_params[0] && ad <= pp_params[1]) ? 1.0 : -0.5;
                                        }
                                        if (pp_mode == "interval_class") {
                                            const int ic = ((cv - oe) % 12 + 12) % 12;
                                            return (std::find(pp_params.begin(), pp_params.end(), ic) != pp_params.end()) ? 1.0 : -0.5;
                                        }
                                        // Phase 2 consecutive-pair modes are not meaningfully scorable per-position
                                        return 0.0;
                                    };
                                } else {
                                compiled_pp->post_constraint = [pp_v1, pp_v2, pp_mode, pp_params, pp_indices, pp_stride, pp_offset, pp_id]
                                        (DynamicRules::ConstraintContext& ctx) {
                                    if (!ctx.pitch_vars) return;
                                    if (pp_v1 < 0 || pp_v1 >= ctx.num_voices ||
                                        pp_v2 < 0 || pp_v2 >= ctx.num_voices)
                                        throw std::runtime_error(
                                            "r-pitch-pitch '" + pp_id + "': voice out of range");

                                    // Build working index list respecting stride/offset/explicit indices
                                    std::vector<int> indices;
                                    if (pp_stride > 0) {
                                        for (int i = pp_offset; i < ctx.sequence_length; i += pp_stride)
                                            indices.push_back(i);
                                    } else if (!pp_indices.empty()) {
                                        indices = pp_indices;
                                    } else {
                                        for (int i = 0; i < ctx.sequence_length; ++i) indices.push_back(i);
                                    }

                                    for (int i : indices) {
                                        if (i < 0 || i >= ctx.sequence_length) continue;
                                        const int idx1 = pp_v1 * ctx.sequence_length + i;
                                        const int idx2 = pp_v2 * ctx.sequence_length + i;
                                        if (idx1 >= (int)ctx.pitch_vars->size() ||
                                            idx2 >= (int)ctx.pitch_vars->size()) continue;

                                        Gecode::IntVar p1 = (*ctx.pitch_vars)[idx1];
                                        Gecode::IntVar p2 = (*ctx.pitch_vars)[idx2];

                                        if (pp_mode == "no_unison") {
                                            Gecode::rel(*ctx.space, p1, Gecode::IRT_NQ, p2);
                                        } else if (pp_mode == "same_pitch") {
                                            Gecode::rel(*ctx.space, p1, Gecode::IRT_EQ, p2);
                                        } else if (pp_mode == "voice_above") {
                                            Gecode::rel(*ctx.space, p1, Gecode::IRT_GR, p2);
                                        } else if (pp_mode == "voice_below") {
                                            Gecode::rel(*ctx.space, p1, Gecode::IRT_LE, p2);
                                        } else if (pp_mode == "exact_interval") {
                                            Gecode::IntVar diff = Gecode::expr(*ctx.space, p1 - p2);
                                            Gecode::rel(*ctx.space, diff, Gecode::IRT_EQ, pp_params[0]);
                                        } else if (pp_mode == "min_interval") {
                                            Gecode::IntVar adiff = Gecode::expr(*ctx.space, Gecode::abs(p1 - p2));
                                            Gecode::rel(*ctx.space, adiff, Gecode::IRT_GQ, pp_params[0]);
                                        } else if (pp_mode == "max_interval") {
                                            Gecode::IntVar adiff = Gecode::expr(*ctx.space, Gecode::abs(p1 - p2));
                                            Gecode::rel(*ctx.space, adiff, Gecode::IRT_LQ, pp_params[0]);
                                        } else if (pp_mode == "interval_range") {
                                            Gecode::IntVar adiff = Gecode::expr(*ctx.space, Gecode::abs(p1 - p2));
                                            Gecode::dom(*ctx.space, adiff, pp_params[0], pp_params[1]);
                                        } else if (pp_mode == "interval_class") {
                                            // 132 = 11*12, so (diff+132) mod 12 = diff mod 12; keeps shifted var positive
                                            Gecode::IntVar diff    = Gecode::expr(*ctx.space, p1 - p2);
                                            Gecode::IntVar shifted = Gecode::expr(*ctx.space, diff + 132);
                                            Gecode::IntVar twelve(*ctx.space, 12, 12);
                                            Gecode::IntVar mod12(*ctx.space, 0, 11);
                                            Gecode::mod(*ctx.space, shifted, twelve, mod12);
                                            Gecode::IntArgs classes;
                                            for (int c : pp_params) classes << c;
                                            Gecode::dom(*ctx.space, mod12, Gecode::IntSet(classes));
                                        } else if (pp_mode == "no_consecutive_fifths" ||
                                                   pp_mode == "no_consecutive_octaves") {
                                            // Forbid two consecutive occurrences of the same interval class.
                                            // For fifths: ic=7; for octaves: ic=0.
                                            if (i + 1 >= ctx.sequence_length) continue;
                                            const int tc = (pp_mode == "no_consecutive_fifths") ? 7 : 0;
                                            const int idx1n = pp_v1 * ctx.sequence_length + (i + 1);
                                            const int idx2n = pp_v2 * ctx.sequence_length + (i + 1);
                                            if (idx1n >= (int)ctx.pitch_vars->size() ||
                                                idx2n >= (int)ctx.pitch_vars->size()) continue;
                                            Gecode::IntVar p1n = (*ctx.pitch_vars)[idx1n];
                                            Gecode::IntVar p2n = (*ctx.pitch_vars)[idx2n];
                                            // Helper: build BoolVar ↔ (interval_class(pa,pb) == tc)
                                            auto ic_bool = [&](Gecode::IntVar pa, Gecode::IntVar pb) -> Gecode::BoolVar {
                                                Gecode::IntVar d  = Gecode::expr(*ctx.space, pa - pb);
                                                Gecode::IntVar sh = Gecode::expr(*ctx.space, d + 132);
                                                Gecode::IntVar tw(*ctx.space, 12, 12);
                                                Gecode::IntVar mc(*ctx.space, 0, 11);
                                                Gecode::mod(*ctx.space, sh, tw, mc);
                                                Gecode::BoolVar b(*ctx.space, 0, 1);
                                                Gecode::rel(*ctx.space, mc, Gecode::IRT_EQ, tc, b);
                                                return b;
                                            };
                                            Gecode::BoolVar b_i  = ic_bool(p1,  p2);
                                            Gecode::BoolVar b_i1 = ic_bool(p1n, p2n);
                                            // NOT (both consecutive positions have the same interval class)
                                            Gecode::rel(*ctx.space, b_i, Gecode::BOT_AND, b_i1, 0);
                                        } else if (pp_mode == "no_parallel_motion") {
                                            // Forbid both voices moving in the same direction.
                                            // Oblique (one stays) and contrary are allowed.
                                            if (i + 1 >= ctx.sequence_length) continue;
                                            const int idx1n = pp_v1 * ctx.sequence_length + (i + 1);
                                            const int idx2n = pp_v2 * ctx.sequence_length + (i + 1);
                                            if (idx1n >= (int)ctx.pitch_vars->size() ||
                                                idx2n >= (int)ctx.pitch_vars->size()) continue;
                                            Gecode::IntVar p1n = (*ctx.pitch_vars)[idx1n];
                                            Gecode::IntVar p2n = (*ctx.pitch_vars)[idx2n];
                                            Gecode::IntVar dir1 = Gecode::expr(*ctx.space, p1n - p1);
                                            Gecode::IntVar dir2 = Gecode::expr(*ctx.space, p2n - p2);
                                            Gecode::BoolVar up1(*ctx.space, 0, 1), up2(*ctx.space, 0, 1);
                                            Gecode::BoolVar dn1(*ctx.space, 0, 1), dn2(*ctx.space, 0, 1);
                                            Gecode::rel(*ctx.space, dir1, Gecode::IRT_GR,  0, up1); // up1 ↔ dir1 > 0
                                            Gecode::rel(*ctx.space, dir2, Gecode::IRT_GR,  0, up2);
                                            Gecode::rel(*ctx.space, dir1, Gecode::IRT_LE, -1, dn1); // dn1 ↔ dir1 < 0
                                            Gecode::rel(*ctx.space, dir2, Gecode::IRT_LE, -1, dn2);
                                            Gecode::rel(*ctx.space, up1, Gecode::BOT_AND, up2, 0); // NOT (both go up)
                                            Gecode::rel(*ctx.space, dn1, Gecode::BOT_AND, dn2, 0); // NOT (both go down)
                                        } else if (pp_mode == "contrary_motion") {
                                            // Require both voices to move in strictly opposite directions.
                                            if (i + 1 >= ctx.sequence_length) continue;
                                            const int idx1n = pp_v1 * ctx.sequence_length + (i + 1);
                                            const int idx2n = pp_v2 * ctx.sequence_length + (i + 1);
                                            if (idx1n >= (int)ctx.pitch_vars->size() ||
                                                idx2n >= (int)ctx.pitch_vars->size()) continue;
                                            Gecode::IntVar p1n = (*ctx.pitch_vars)[idx1n];
                                            Gecode::IntVar p2n = (*ctx.pitch_vars)[idx2n];
                                            Gecode::IntVar dir1 = Gecode::expr(*ctx.space, p1n - p1);
                                            Gecode::IntVar dir2 = Gecode::expr(*ctx.space, p2n - p2);
                                            Gecode::BoolVar up1(*ctx.space, 0, 1), up2(*ctx.space, 0, 1);
                                            Gecode::BoolVar dn1(*ctx.space, 0, 1), dn2(*ctx.space, 0, 1);
                                            Gecode::rel(*ctx.space, dir1, Gecode::IRT_GR,  0, up1);
                                            Gecode::rel(*ctx.space, dir2, Gecode::IRT_GR,  0, up2);
                                            Gecode::rel(*ctx.space, dir1, Gecode::IRT_LE, -1, dn1);
                                            Gecode::rel(*ctx.space, dir2, Gecode::IRT_LE, -1, dn2);
                                            // (v1 up AND v2 down) OR (v1 down AND v2 up)
                                            Gecode::BoolVar case1(*ctx.space, 0, 1), case2(*ctx.space, 0, 1);
                                            Gecode::rel(*ctx.space, up1, Gecode::BOT_AND, dn2, case1);
                                            Gecode::rel(*ctx.space, dn1, Gecode::BOT_AND, up2, case2);
                                            Gecode::rel(*ctx.space, case1, Gecode::BOT_OR, case2, 1);
                                        } else {
                                            throw std::runtime_error(
                                                "r-pitch-pitch '" + pp_id +
                                                "': unknown constraint mode '" + pp_mode + "'");
                                        }
                                    }
                                }; // end post_constraint
                                } // end else (not heuristic)

                                if (!pp_is_heuristic) ++regular_count;
                                std::cout << "     ✅ Compiled r-pitch-pitch: " << pp_id
                                          << " (mode=" << pp_mode
                                          << ", voices " << pp_v1 << " and " << pp_v2 << ")" << std::endl;
                                solver.apply_compiled_constraint(std::move(compiled_pp));
                                // ─────────────────────────────────────────────────────────────
                            } else if (rule_type == "r-melodic-step") {
                                // ── R-MELODIC-STEP ────────────────────────────────────────────
                                // Single-voice rule on consecutive pitch pairs.
                                // Hard:      |pitch[i+1] - pitch[i]| <= max_step for every i.
                                // Heuristic: score 1.0 for step (0..max_step), 0.3 for skip
                                //            (max_step+1..max_step+4), 0.0 for larger leaps.
                                std::vector<int> ms_voices;
                                if (rule_json.contains("target_voices") && rule_json["target_voices"].is_array())
                                    for (const auto& v : rule_json["target_voices"])
                                        if (v.is_number_integer()) ms_voices.push_back(v.get<int>());
                                if (ms_voices.empty())
                                    for (int v = 0; v < solver_config.num_voices; ++v) ms_voices.push_back(v);

                                const int ms_max = rule_json.contains("max_step") && rule_json["max_step"].is_number()
                                                   ? std::max(0, rule_json["max_step"].get<int>()) : 2;
                                const bool ms_heur = json_as_bool(rule_json, "heuristic");
                                const std::string ms_id = rule_json.value("id", "r-melodic-step");

                                auto compiled_ms = std::make_unique<DynamicRules::CompiledConstraint>(
                                    ms_id, "r-melodic-step (max_step=" + std::to_string(ms_max) + ")");

                                if (ms_heur) {
                                    compiled_ms->is_heuristic = true;
                                    compiled_ms->heuristic_mode = DynamicRules::HeuristicMode::REAL_HEURISTIC;
                                    compiled_ms->heuristic_variable_type = "pitch";
                                    compiled_ms->applies_to_voices = ms_voices;
                                    compiled_ms->score_candidate = [ms_voices, ms_max](
                                            const DynamicRules::ConstraintContext& ctx,
                                            const DynamicRules::HeuristicCandidateContext& cand) -> double {
                                        if (!ctx.pitch_vars) return 0.0;
                                        if (std::find(ms_voices.begin(), ms_voices.end(), cand.voice) == ms_voices.end()) return 0.0;
                                        if (cand.position == 0) return 0.0;
                                        const int prev_idx = cand.voice * ctx.sequence_length + (cand.position - 1);
                                        if (prev_idx < 0 || prev_idx >= (int)ctx.pitch_vars->size()) return 0.0;
                                        const Gecode::IntVar& pv = (*ctx.pitch_vars)[prev_idx];
                                        if (!pv.assigned()) return 0.0;
                                        const int interval = std::abs(cand.candidate_value - pv.val());
                                        if (interval <= ms_max)       return 1.0;   // step
                                        if (interval <= ms_max + 4)   return 0.3;   // skip
                                        return 0.0;                                  // leap
                                    };
                                } else {
                                    compiled_ms->post_constraint = [ms_voices, ms_max, ms_id](
                                            DynamicRules::ConstraintContext& ctx) {
                                        if (!ctx.pitch_vars || !ctx.space) return;
                                        for (int v : ms_voices) {
                                            if (v < 0 || v >= ctx.num_voices) continue;
                                            for (int i = 0; i + 1 < ctx.sequence_length; ++i) {
                                                const int ci = v * ctx.sequence_length + i;
                                                const int ni = v * ctx.sequence_length + (i + 1);
                                                if (ci >= (int)ctx.pitch_vars->size() ||
                                                    ni >= (int)ctx.pitch_vars->size()) continue;
                                                // |p_next - p_curr| <= ms_max via two linear rels
                                                Gecode::IntVar p_curr = (*ctx.pitch_vars)[ci];
                                                Gecode::IntVar p_next = (*ctx.pitch_vars)[ni];
                                                Gecode::IntVar diff_fwd = Gecode::expr(*ctx.space, p_next - p_curr);
                                                Gecode::IntVar diff_bwd = Gecode::expr(*ctx.space, p_curr - p_next);
                                                Gecode::rel(*ctx.space, diff_fwd, Gecode::IRT_LQ, ms_max);
                                                Gecode::rel(*ctx.space, diff_bwd, Gecode::IRT_LQ, ms_max);
                                            }
                                        }
                                    };
                                }

                                if (!ms_heur) ++regular_count;
                                std::cout << "     \u2705 Compiled r-melodic-step: " << ms_id
                                          << " max_step=" << ms_max
                                          << (ms_heur ? " (heuristic)" : " (hard)") << std::endl;
                                solver.apply_compiled_constraint(std::move(compiled_ms));
                                // ─────────────────────────────────────────────────────────────
                            } else if (rule_type == "r-no-interval-repetition") {
                                // ── R-NO-INTERVAL-REPETITION ────────────────────────────
                                // No two adjacent intervals equal.
                                // Hard: for every triple (i,i+1,i+2): interval(i→i+1) ≠ interval(i+1→i+2)
                                // interval_mode: "up-down" (signed) or "absolute" (same size)
                                std::vector<int> nir_voices;
                                if (rule_json.contains("target_voices") && rule_json["target_voices"].is_array())
                                    for (const auto& v : rule_json["target_voices"])
                                        if (v.is_number_integer()) nir_voices.push_back(v.get<int>());
                                if (nir_voices.empty())
                                    for (int v = 0; v < solver_config.num_voices; ++v) nir_voices.push_back(v);
                                const bool nir_absolute = (rule_json.value("interval_mode", std::string("up-down")) == "absolute");
                                const bool nir_heur     = json_as_bool(rule_json, "heuristic");
                                const std::string nir_id = rule_json.value("id", "r-no-interval-repetition");

                                auto compiled_nir = std::make_unique<DynamicRules::CompiledConstraint>(
                                    nir_id, "r-no-interval-repetition (" + std::string(nir_absolute ? "absolute" : "up-down") + ")");

                                if (nir_heur) {
                                    compiled_nir->is_heuristic = true;
                                    compiled_nir->heuristic_mode = DynamicRules::HeuristicMode::REAL_HEURISTIC;
                                    compiled_nir->heuristic_variable_type = "pitch";
                                    compiled_nir->applies_to_voices = nir_voices;
                                    compiled_nir->score_candidate = [nir_voices, nir_absolute](
                                            const DynamicRules::ConstraintContext& ctx,
                                            const DynamicRules::HeuristicCandidateContext& cand) -> double {
                                        if (!ctx.pitch_vars || cand.position < 2) return 0.0;
                                        if (std::find(nir_voices.begin(), nir_voices.end(), cand.voice) == nir_voices.end()) return 0.0;
                                        const int idx1 = cand.voice * ctx.sequence_length + (cand.position - 2);
                                        const int idx2 = cand.voice * ctx.sequence_length + (cand.position - 1);
                                        if (idx1 < 0 || idx2 >= (int)ctx.pitch_vars->size()) return 0.0;
                                        const auto& v1 = (*ctx.pitch_vars)[idx1];
                                        const auto& v2 = (*ctx.pitch_vars)[idx2];
                                        if (!v1.assigned() || !v2.assigned()) return 0.0;
                                        const int prev_int = v2.val() - v1.val();
                                        const int new_int  = cand.candidate_value - v2.val();
                                        if (nir_absolute) {
                                            if (std::abs(new_int) == std::abs(prev_int)) return 0.0;
                                        } else {
                                            if (new_int == prev_int) return 0.0;
                                        }
                                        return 1.0;
                                    };
                                } else {
                                    compiled_nir->post_constraint = [nir_voices, nir_absolute, nir_id](
                                            DynamicRules::ConstraintContext& ctx) {
                                        if (!ctx.pitch_vars || !ctx.space) return;
                                        for (int v : nir_voices) {
                                            if (v < 0 || v >= ctx.num_voices) continue;
                                            for (int i = 0; i + 2 < ctx.sequence_length; ++i) {
                                                const int i0 = v * ctx.sequence_length + i;
                                                const int i1 = v * ctx.sequence_length + i + 1;
                                                const int i2 = v * ctx.sequence_length + i + 2;
                                                if (i2 >= (int)ctx.pitch_vars->size()) continue;
                                                Gecode::IntVar p0 = (*ctx.pitch_vars)[i0];
                                                Gecode::IntVar p1 = (*ctx.pitch_vars)[i1];
                                                Gecode::IntVar p2 = (*ctx.pitch_vars)[i2];
                                                // d1 = p1-p0, d2 = p2-p1
                                                Gecode::IntVar d1 = Gecode::expr(*ctx.space, p1 - p0);
                                                Gecode::IntVar d2 = Gecode::expr(*ctx.space, p2 - p1);
                                                // up-down: d1 != d2
                                                Gecode::rel(*ctx.space, d1, Gecode::IRT_NQ, d2);
                                                if (nir_absolute) {
                                                    // also d1 != -d2  (same size opposite direction)
                                                    Gecode::IntVar neg_d2 = Gecode::expr(*ctx.space, -d2);
                                                    Gecode::rel(*ctx.space, d1, Gecode::IRT_NQ, neg_d2);
                                                }
                                            }
                                        }
                                    };
                                }
                                if (!nir_heur) ++regular_count;
                                std::cout << "     \u2705 Compiled r-no-interval-repetition: " << nir_id
                                          << " (" << (nir_absolute ? "absolute" : "up-down") << ")"
                                          << (nir_heur ? " heuristic" : " hard") << std::endl;
                                solver.apply_compiled_constraint(std::move(compiled_nir));
                                // ─────────────────────────────────────────────────────────────
                            } else if (rule_type == "r-no-consecutive-equal") {
                                // ── R-NO-CONSECUTIVE-EQUAL ──────────────────────────────
                                // No more than max_consecutive identical consecutive pitches.
                                // Hard: for every window of max+1, post BoolVar sum < max.
                                std::vector<int> nce_voices;
                                if (rule_json.contains("target_voices") && rule_json["target_voices"].is_array())
                                    for (const auto& v : rule_json["target_voices"])
                                        if (v.is_number_integer()) nce_voices.push_back(v.get<int>());
                                if (nce_voices.empty())
                                    for (int v = 0; v < solver_config.num_voices; ++v) nce_voices.push_back(v);
                                const int nce_max  = rule_json.contains("max_consecutive") && rule_json["max_consecutive"].is_number()
                                                     ? std::max(1, rule_json["max_consecutive"].get<int>()) : 2;
                                const bool nce_heur = json_as_bool(rule_json, "heuristic");
                                const std::string nce_id = rule_json.value("id", "r-no-consecutive-equal");

                                auto compiled_nce = std::make_unique<DynamicRules::CompiledConstraint>(
                                    nce_id, "r-no-consecutive-equal (max=" + std::to_string(nce_max) + ")");

                                if (nce_heur) {
                                    compiled_nce->is_heuristic = true;
                                    compiled_nce->heuristic_mode = DynamicRules::HeuristicMode::REAL_HEURISTIC;
                                    compiled_nce->heuristic_variable_type = "pitch";
                                    compiled_nce->applies_to_voices = nce_voices;
                                    compiled_nce->score_candidate = [nce_voices, nce_max](
                                            const DynamicRules::ConstraintContext& ctx,
                                            const DynamicRules::HeuristicCandidateContext& cand) -> double {
                                        if (!ctx.pitch_vars) return 0.0;
                                        if (std::find(nce_voices.begin(), nce_voices.end(), cand.voice) == nce_voices.end()) return 0.0;
                                        int run = 0;
                                        for (int k = 1; k <= nce_max && (cand.position - k) >= 0; ++k) {
                                            const int idx = cand.voice * ctx.sequence_length + (cand.position - k);
                                            if (idx < 0 || idx >= (int)ctx.pitch_vars->size()) break;
                                            const auto& var = (*ctx.pitch_vars)[idx];
                                            if (!var.assigned() || var.val() != cand.candidate_value) break;
                                            ++run;
                                        }
                                        return (run >= nce_max) ? 0.0 : 1.0;
                                    };
                                } else {
                                    compiled_nce->post_constraint = [nce_voices, nce_max, nce_id](
                                            DynamicRules::ConstraintContext& ctx) {
                                        if (!ctx.pitch_vars || !ctx.space) return;
                                        const int win = nce_max + 1;
                                        for (int v : nce_voices) {
                                            if (v < 0 || v >= ctx.num_voices) continue;
                                            for (int i = 0; i + win <= ctx.sequence_length; ++i) {
                                                // BoolVars: b[k] = (p[i+k] == p[i+k+1])
                                                Gecode::BoolVarArgs b(nce_max);
                                                for (int k = 0; k < nce_max; ++k) {
                                                    const int idx_a = v * ctx.sequence_length + i + k;
                                                    const int idx_b = v * ctx.sequence_length + i + k + 1;
                                                    if (idx_b >= (int)ctx.pitch_vars->size()) goto next_window;
                                                    Gecode::BoolVar bk(*ctx.space, 0, 1);
                                                    Gecode::rel(*ctx.space, (*ctx.pitch_vars)[idx_a],
                                                                Gecode::IRT_EQ, (*ctx.pitch_vars)[idx_b], bk);
                                                    b[k] = bk;
                                                }
                                                // sum(b) <= nce_max-1  (not all equal)
                                                Gecode::linear(*ctx.space, b, Gecode::IRT_LQ, nce_max - 1);
                                                next_window:;
                                            }
                                        }
                                    };
                                }
                                if (!nce_heur) ++regular_count;
                                std::cout << "     \u2705 Compiled r-no-consecutive-equal: " << nce_id
                                          << " max=" << nce_max
                                          << (nce_heur ? " heuristic" : " hard") << std::endl;
                                solver.apply_compiled_constraint(std::move(compiled_nce));
                                // ─────────────────────────────────────────────────────────────
                            } else if (rule_type == "r-no-consecutive-direction") {
                                // ── R-NO-CONSECUTIVE-DIRECTION ─────────────────────────
                                // No more than max_consecutive strictly ascending (or descending) steps.
                                // direction: "ascending", "descending", or "both"
                                std::vector<int> ncd_voices;
                                if (rule_json.contains("target_voices") && rule_json["target_voices"].is_array())
                                    for (const auto& v : rule_json["target_voices"])
                                        if (v.is_number_integer()) ncd_voices.push_back(v.get<int>());
                                if (ncd_voices.empty())
                                    for (int v = 0; v < solver_config.num_voices; ++v) ncd_voices.push_back(v);
                                const int ncd_max      = rule_json.contains("max_consecutive") && rule_json["max_consecutive"].is_number()
                                                         ? std::max(1, rule_json["max_consecutive"].get<int>()) : 3;
                                const std::string ncd_dir = rule_json.value("direction", std::string("both"));
                                const bool ncd_heur    = json_as_bool(rule_json, "heuristic");
                                const std::string ncd_id  = rule_json.value("id", "r-no-consecutive-direction");

                                auto compiled_ncd = std::make_unique<DynamicRules::CompiledConstraint>(
                                    ncd_id, "r-no-consecutive-direction (dir=" + ncd_dir + " max=" + std::to_string(ncd_max) + ")");

                                if (ncd_heur) {
                                    compiled_ncd->is_heuristic = true;
                                    compiled_ncd->heuristic_mode = DynamicRules::HeuristicMode::REAL_HEURISTIC;
                                    compiled_ncd->heuristic_variable_type = "pitch";
                                    compiled_ncd->applies_to_voices = ncd_voices;
                                    compiled_ncd->score_candidate = [ncd_voices, ncd_max, ncd_dir](
                                            const DynamicRules::ConstraintContext& ctx,
                                            const DynamicRules::HeuristicCandidateContext& cand) -> double {
                                        if (!ctx.pitch_vars || cand.position == 0) return 0.0;
                                        if (std::find(ncd_voices.begin(), ncd_voices.end(), cand.voice) == ncd_voices.end()) return 0.0;
                                        const int prev_idx = cand.voice * ctx.sequence_length + (cand.position - 1);
                                        if (prev_idx < 0 || prev_idx >= (int)ctx.pitch_vars->size()) return 0.0;
                                        const auto& prev_var = (*ctx.pitch_vars)[prev_idx];
                                        if (!prev_var.assigned()) return 0.0;
                                        // Count how many consecutive preceding intervals have the same direction
                                        int run_asc = 0, run_desc = 0;
                                        for (int k = 1; k <= ncd_max && (cand.position - k - 1) >= 0; ++k) {
                                            const int ia = cand.voice * ctx.sequence_length + (cand.position - k - 1);
                                            const int ib = cand.voice * ctx.sequence_length + (cand.position - k);
                                            if (ia < 0 || ib >= (int)ctx.pitch_vars->size()) break;
                                            const auto& va = (*ctx.pitch_vars)[ia];
                                            const auto& vb = (*ctx.pitch_vars)[ib];
                                            if (!va.assigned() || !vb.assigned()) break;
                                            const int diff = vb.val() - va.val();
                                            if (diff > 0) ++run_asc; else break;
                                        }
                                        for (int k = 1; k <= ncd_max && (cand.position - k - 1) >= 0; ++k) {
                                            const int ia = cand.voice * ctx.sequence_length + (cand.position - k - 1);
                                            const int ib = cand.voice * ctx.sequence_length + (cand.position - k);
                                            if (ia < 0 || ib >= (int)ctx.pitch_vars->size()) break;
                                            const auto& va = (*ctx.pitch_vars)[ia];
                                            const auto& vb = (*ctx.pitch_vars)[ib];
                                            if (!va.assigned() || !vb.assigned()) break;
                                            const int diff = vb.val() - va.val();
                                            if (diff < 0) ++run_desc; else break;
                                        }
                                        const int new_dir = cand.candidate_value - prev_var.val();
                                        const bool bad_asc  = (ncd_dir != "descending") && (new_dir > 0) && (run_asc >= ncd_max);
                                        const bool bad_desc = (ncd_dir != "ascending")  && (new_dir < 0) && (run_desc >= ncd_max);
                                        return (bad_asc || bad_desc) ? 0.0 : 1.0;
                                    };
                                } else {
                                    compiled_ncd->post_constraint = [ncd_voices, ncd_max, ncd_dir, ncd_id](
                                            DynamicRules::ConstraintContext& ctx) {
                                        if (!ctx.pitch_vars || !ctx.space) return;
                                        const int win = ncd_max + 1;
                                        const bool do_asc  = (ncd_dir != "descending");
                                        const bool do_desc = (ncd_dir != "ascending");
                                        for (int v : ncd_voices) {
                                            if (v < 0 || v >= ctx.num_voices) continue;
                                            for (int i = 0; i + win <= ctx.sequence_length; ++i) {
                                                if (do_asc) {
                                                    Gecode::BoolVarArgs b(ncd_max);
                                                    for (int k = 0; k < ncd_max; ++k) {
                                                        const int ia = v * ctx.sequence_length + i + k;
                                                        const int ib = v * ctx.sequence_length + i + k + 1;
                                                        if (ib >= (int)ctx.pitch_vars->size()) goto skip_asc;
                                                        Gecode::IntVar diff = Gecode::expr(*ctx.space,
                                                            (*ctx.pitch_vars)[ib] - (*ctx.pitch_vars)[ia]);
                                                        Gecode::BoolVar bk(*ctx.space, 0, 1);
                                                        Gecode::rel(*ctx.space, diff, Gecode::IRT_GQ, 1, bk);
                                                        b[k] = bk;
                                                    }
                                                    Gecode::linear(*ctx.space, b, Gecode::IRT_LQ, ncd_max - 1);
                                                    skip_asc:;
                                                }
                                                if (do_desc) {
                                                    Gecode::BoolVarArgs b(ncd_max);
                                                    for (int k = 0; k < ncd_max; ++k) {
                                                        const int ia = v * ctx.sequence_length + i + k;
                                                        const int ib = v * ctx.sequence_length + i + k + 1;
                                                        if (ib >= (int)ctx.pitch_vars->size()) goto skip_desc;
                                                        Gecode::IntVar diff = Gecode::expr(*ctx.space,
                                                            (*ctx.pitch_vars)[ia] - (*ctx.pitch_vars)[ib]);
                                                        Gecode::BoolVar bk(*ctx.space, 0, 1);
                                                        Gecode::rel(*ctx.space, diff, Gecode::IRT_GQ, 1, bk);
                                                        b[k] = bk;
                                                    }
                                                    Gecode::linear(*ctx.space, b, Gecode::IRT_LQ, ncd_max - 1);
                                                    skip_desc:;
                                                }
                                            }
                                        }
                                    };
                                }
                                if (!ncd_heur) ++regular_count;
                                std::cout << "     \u2705 Compiled r-no-consecutive-direction: " << ncd_id
                                          << " dir=" << ncd_dir << " max=" << ncd_max
                                          << (ncd_heur ? " heuristic" : " hard") << std::endl;
                                solver.apply_compiled_constraint(std::move(compiled_ncd));
                                // ─────────────────────────────────────────────────────────────
                            } else if (rule_json.contains("id") && rule_json.contains("expression")) {
                                // Process regular dynamic rule (expression-based format)
                                auto compiled = DynamicRules::DynamicRuleCompiler::compile_from_json(rule_json);
                                if (compiled) {
                                    regular_count++;
                                    std::cout << "     ✅ Compiled regular rule: " << compiled->rule_id << std::endl;
                                    // Add regular dynamic rules to solver
                                    std::vector<nlohmann::json> single_rule = {rule_json};
                                    solver.load_dynamic_rules(single_rule);
                                }
                            } else {
                                // Legacy-format rule (constraint_function style) — handled by the legacy system, skip here
                            }
                        } catch (const std::exception& e) {
                            std::cout << "     ❌ Failed to compile rule: " << e.what() << std::endl;
                        }
                    }
                    
                    std::cout << "   Compiled: " << regular_count << " regular + " << wildcard_count << " wildcard + " << index_count << " index rules" << std::endl;
                    
                    if (wildcard_count > 0) {
                        std::cout << "   ✅ Applied " << wildcard_count << " wildcard constraints to solver" << std::endl;
                    }
                    if (index_count > 0) {
                        std::cout << "   ✅ Applied " << index_count << " index constraints to solver" << std::endl;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cout << "   ⚠️  Could not parse dynamic rules: " << e.what() << std::endl;
            std::cout << "   Continuing with legacy rules only..." << std::endl;
        }
        
        std::cout << "\n📊 Solver configured with " << solver.get_rules_count() << " legacy rules";
        if (solver.get_dynamic_rules_count() > 0) {
            std::cout << " + " << solver.get_dynamic_rules_count() << " dynamic rules";
        }
        std::cout << std::endl;
        
        // Register neural pitch scorer when value_order == "neural"
        {
            nlohmann::json cfg_json;
            std::ifstream cfg_f(config_file);
            bool neural_active = false;
            if (cfg_f.is_open()) {
                try { cfg_f >> cfg_json; } catch (...) {}
                if (cfg_json.contains("search_options") &&
                    cfg_json["search_options"].value("value_order", "") == "neural") {
                    // neural_weights_file is mandatory — no silent fallback.
                    if (!cfg_json["search_options"].contains("neural_weights_file")) {
                        std::cerr << "\u274c Config error: value_order=\"neural\" requires "
                                     "\"neural_weights_file\" to be set explicitly in "
                                     "search_options.\n";
                        return 1;
                    }
                    const std::string weights_path =
                        cfg_json["search_options"].value("neural_weights_file", "");
                    const bool shadow =
                        cfg_json["search_options"].value("neural_shadow_mode", false);
                    const float temperature =
                        cfg_json["search_options"].value("neural_temperature", 1.0f);
                    // Pass solver seed; resolve 0 to a fresh random seed (same as Gecode solver).
                    unsigned int warmup_seed = solver_config.random_seed;
                    if (warmup_seed == 0u) {
                        std::random_device rd;
                        warmup_seed = rd();
                        if (warmup_seed == 0u) warmup_seed = 1u;
                    }

                    // ── Option A: pre-build rhythm_context from first voice's duration domain ──
                    // For isorhythm configs this produces a constant vector.
                    // beat_fraction = 4 * numerator / denominator  (quarter note = 1.0)
                    std::vector<float> rhythm_context;
                    {
                        float beat_frac = 1.0f; // default: quarter note
                        try {
                            if (cfg_json.contains("voices") && cfg_json["voices"].is_array() &&
                                !cfg_json["voices"].empty()) {
                                const auto& v0 = cfg_json["voices"][0];
                                if (v0.contains("rhythm") &&
                                    v0["rhythm"].contains("duration_values") &&
                                    v0["rhythm"]["duration_values"].is_array() &&
                                    !v0["rhythm"]["duration_values"].empty()) {
                                    std::string dv = v0["rhythm"]["duration_values"][0].get<std::string>();
                                    auto slash = dv.find('/');
                                    if (slash != std::string::npos) {
                                        float num  = std::stof(dv.substr(0, slash));
                                        float den  = std::stof(dv.substr(slash + 1));
                                        if (den > 0.0f) beat_frac = 4.0f * num / den;
                                    }
                                }
                            }
                        } catch (...) {}
                        const int seq_len = solver_config.sequence_length > 0
                                            ? solver_config.sequence_length : 16;
                        rhythm_context.assign(seq_len, beat_frac);
                    }

                    // harmonic_state: from parsed harmonic_domain (Phase 3); empty if not configured
                    const std::vector<int>& harmonic_state =
                        solver_config.harmonic_domain.harmonic_state;

                    auto scorer = NeuralPitch::make_scorer(
                        weights_path, shadow, warmup_seed, temperature,
                        harmonic_state, rhythm_context, solver_config.rhythm_base);
                    const int top_k = solver_config.heuristic_top_k;
                    const bool trace = solver_config.heuristic_trace;
                    GecodeClusterIntegration::configure_pitch_heuristic_value_ordering(
                        scorer, 0, top_k, trace);
                    neural_active = true;
                    std::cout << "   🧠 Neural pitch scorer active"
                              << (shadow ? " (shadow mode)" : "") << "\n";
                }
            }
            if (!neural_active) {
                GecodeClusterIntegration::clear_pitch_heuristic_value_ordering();
            }
        }

        // Solve the constraint problem
        std::cout << "\n🎯 Solving Constraint Problem" << std::endl;
        std::cout << std::string(50, '-') << std::endl;

        int max_sol = parser.getMaxSolutions(config_file);
        int timeout_ms = parser.getTimeoutMs(config_file);
        std::string max_sol_label = (max_sol < 0) ? "all" : std::to_string(max_sol);
        std::cout << "   Searching for " << max_sol_label << " solution(s)"
                  << " (timeout: " << timeout_ms << " ms)";
        if (solver_config.random_seed != 0)
            std::cout << " 🎲 random seed: " << solver_config.random_seed;
        std::cout << std::endl;

        auto start_time = std::chrono::high_resolution_clock::now();

        std::vector<MusicalConstraintSolver::MusicalSolution> found_solutions;

        // Streaming callback: print each solution to console immediately as it's found.
        auto on_solution = [&](const MusicalConstraintSolver::MusicalSolution& sol, int idx) {
            std::cout << "\n── Solution " << (idx + 1) << " ──" << std::endl;
            if (!sol.voice_solutions.empty()) {
                for (size_t voice = 0; voice < sol.voice_solutions.size(); ++voice) {
                    std::cout << "Voice " << voice << " Pitch: ";
                    for (size_t i = 0; i < sol.voice_solutions[voice].size(); ++i) {
                        if (i > 0) std::cout << " → ";
                        int midi = sol.voice_solutions[voice][i];
                        if (midi < 0)
                            std::cout << "R";
                        else
                            std::cout << MusicalConstraintSolver::Solver::midi_to_note_name(midi)
                                      << "(" << midi << ")";
                    }
                    std::cout << std::endl;
                    if (voice < sol.voice_rhythms.size()) {
                        std::cout << "Voice " << voice << " Rhythm: ";
                        for (size_t i = 0; i < sol.voice_rhythms[voice].size(); ++i) {
                            if (i > 0) std::cout << " + ";
                            std::cout << ConstraintSolverJSONParser::format_duration(
                                sol.voice_rhythms[voice][i], solver_config.rhythm_base);
                        }
                        std::cout << std::endl;
                    }
                }
            } else {
                std::cout << "Notes: ";
                for (size_t i = 0; i < sol.absolute_notes.size(); ++i) {
                    if (i > 0) std::cout << " → ";
                    std::cout << MusicalConstraintSolver::Solver::midi_to_note_name(sol.absolute_notes[i]);
                }
                std::cout << std::endl;
            }
        };

        auto all_solutions = solver.solve_multiple(max_sol, timeout_ms, on_solution);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Filter to successful solutions
        for (auto& sol : all_solutions)
            if (sol.found_solution) found_solutions.push_back(sol);

        if (found_solutions.empty()) {
            std::string reason = all_solutions.empty() ? "no solutions" : all_solutions[0].failure_reason;
            std::cout << "❌ No solution found: " << reason << std::endl;
            return 1;
        }

        std::cout << "✅ Found " << found_solutions.size() << " solution(s) in "
                  << duration.count() << " ms" << std::endl;

        // Export results
        std::cout << "\n💾 Exporting Results" << std::endl;
        std::cout << std::string(50, '-') << std::endl;

        std::string base_name = parser.getExportFilename();
        if (base_name.empty()) {
            base_name = config_file;
            size_t slash_pos = base_name.find_last_of("/\\");
            if (slash_pos != std::string::npos) {
                base_name = base_name.substr(slash_pos + 1);
            }
        }
        // Strip any file extension so we can append format-specific ones cleanly.
        {
            size_t dot_pos = base_name.find_last_of('.');
            if (dot_pos != std::string::npos) base_name = base_name.substr(0, dot_pos);
        }

        std::string export_dir = parser.getExportPath();
        std::string mkdir_cmd = "mkdir -p \"" + export_dir + "\"";
        system(mkdir_cmd.c_str());

        // ---- JSON export ----
        if (parser.getExportJSON()) {
            std::string json_file = export_dir + "/" + base_name + "_result.json";
            std::ofstream json_out(json_file);
            if (json_out.is_open()) {
            json_out << "{\n";
            json_out << "  \"config_file\": \"" << config_file << "\",\n";
            json_out << "  \"problem_name\": \"" << parser.getName() << "\",\n";
            json_out << "  \"solutions_found\": " << found_solutions.size() << ",\n";
            json_out << "  \"solve_time_ms\": " << duration.count() << ",\n";
            json_out << "  \"rules_applied\": " << rules.size() << ",\n";
            json_out << "  \"solutions\": [\n";
            for (size_t si = 0; si < found_solutions.size(); ++si) {
                const auto& solution = found_solutions[si];
                if (si > 0) json_out << ",\n";
                json_out << "    {\n";
                json_out << "      \"solution_index\": " << si << ",\n";
                std::vector<int> metric_denominators(solution.metric_signature.size(), 4);
                if (solution.has_canonical_score) {
                    for (const auto& seg : solution.canonical_score.metric_timeline) {
                        const int start = std::max(0, seg.start_index);
                        const int end = std::min(
                            static_cast<int>(solution.metric_signature.size()) - 1,
                            seg.end_index);
                        for (int idx = start; idx <= end; ++idx) {
                            metric_denominators[idx] = (seg.denominator > 0) ? seg.denominator : 4;
                        }
                    }
                }
                if (!solution.voice_solutions.empty()) {
                    json_out << "      \"voices\": [\n";
                    for (size_t voice = 0; voice < solution.voice_solutions.size(); ++voice) {
                        if (voice > 0) json_out << ",\n";
                        json_out << "        {\n";
                        json_out << "          \"voice\": " << voice << ",\n";
                        json_out << "          \"pitch_solution\": [";
                        for (size_t i = 0; i < solution.voice_solutions[voice].size(); ++i) {
                            if (i > 0) json_out << ", ";
                            json_out << solution.voice_solutions[voice][i];
                        }
                        json_out << "],\n";
                        json_out << "          \"rhythm_solution\": [";
                        if (voice < solution.voice_rhythms.size()) {
                            for (size_t i = 0; i < solution.voice_rhythms[voice].size(); ++i) {
                                if (i > 0) json_out << ", ";
                                json_out << solution.voice_rhythms[voice][i];
                            }
                        }
                        json_out << "],\n";
                        json_out << "          \"pitch_names\": [";
                        for (size_t i = 0; i < solution.voice_solutions[voice].size(); ++i) {
                            if (i > 0) json_out << ", ";
                            int midi = solution.voice_solutions[voice][i];
                            json_out << "\"" << (midi < 0 ? "R" : MusicalConstraintSolver::Solver::midi_to_note_name(midi)) << "\"";
                        }
                        json_out << "],\n";
                        json_out << "          \"rhythm_names\": [";
                        if (voice < solution.voice_rhythms.size()) {
                            for (size_t i = 0; i < solution.voice_rhythms[voice].size(); ++i) {
                                if (i > 0) json_out << ", ";
                                json_out << "\"" << ConstraintSolverJSONParser::format_duration(
                                    solution.voice_rhythms[voice][i], solver_config.rhythm_base) << "\"";
                            }
                        }
                        json_out << "]\n";
                        json_out << "        }";
                    }
                    json_out << "\n      ],\n";
                    json_out << "      \"metric_signature_numerators\": [";
                    for (size_t i = 0; i < solution.metric_signature.size(); ++i) {
                        if (i > 0) json_out << ", ";
                        json_out << solution.metric_signature[i];
                    }
                    json_out << "],\n";
                    json_out << "      \"metric_signature\": [";
                    for (size_t i = 0; i < solution.metric_signature.size(); ++i) {
                        if (i > 0) json_out << ", ";
                        json_out << "\"" << solution.metric_signature[i] << "/"
                                 << ((i < metric_denominators.size()) ? metric_denominators[i] : 4)
                                 << "\"";
                    }
                    json_out << "]";
                } else {
                    json_out << "      \"note_names\": [";
                    for (size_t i = 0; i < solution.absolute_notes.size(); ++i) {
                        if (i > 0) json_out << ", ";
                        json_out << "\"" << MusicalConstraintSolver::Solver::midi_to_note_name(solution.absolute_notes[i]) << "\"";
                    }
                    json_out << "]";
                }

                if (solution.has_canonical_score && parser.getIncludeAnalysis()) {
                    json_out << ",\n";
                    json_out << "      \"score\": {\n";
                    json_out << "        \"rhythm_base\": " << solution.canonical_score.rhythm_base << ",\n";
                    json_out << "        \"metric_timeline\": [";
                    for (size_t i = 0; i < solution.canonical_score.metric_timeline.size(); ++i) {
                        if (i > 0) json_out << ", ";
                        const auto& seg = solution.canonical_score.metric_timeline[i];
                        json_out << "{"
                                 << "\"start_index\":" << seg.start_index << ","
                                 << "\"end_index\":" << seg.end_index << ","
                                 << "\"start_tick\":" << seg.start_tick << ","
                                 << "\"end_tick\":" << seg.end_tick << ","
                                 << "\"numerator\":" << seg.numerator << ","
                                 << "\"denominator\":" << seg.denominator
                                 << "}";
                    }
                    json_out << "],\n";
                    json_out << "        \"measures\": [";
                    for (size_t mi = 0; mi < solution.canonical_score.measures.size(); ++mi) {
                        if (mi > 0) json_out << ", ";
                        const auto& measure = solution.canonical_score.measures[mi];
                        json_out << "{"
                                 << "\"measure_index\":" << measure.measure_index << ","
                                 << "\"start_ticks\":" << measure.start_ticks << ","
                                 << "\"end_ticks\":" << measure.end_ticks << ","
                                 << "\"numerator\":" << measure.numerator << ","
                                 << "\"denominator\":" << measure.denominator << ","
                                 << "\"events\":[";
                        for (size_t ei = 0; ei < measure.events.size(); ++ei) {
                            if (ei > 0) json_out << ", ";
                            const auto& ev = measure.events[ei];
                            json_out << "{"
                                     << "\"voice\":" << ev.voice << ","
                                     << "\"index\":" << ev.index << ","
                                     << "\"pitch\":" << ev.pitch << ","
                                     << "\"rhythm_ticks\":" << ev.rhythm << ","
                                     << "\"onset_ticks\":" << ev.onset_ticks << ","
                                     << "\"duration_ticks\":" << ev.duration_ticks << ","
                                     << "\"is_rest\":" << (ev.is_rest ? "true" : "false")
                                     << "}";
                        }
                        json_out << "]}";
                    }
                    json_out << "]\n";
                    json_out << "      }\n";
                } else {
                    json_out << "\n";
                }
                json_out << "    }";
            }
            json_out << "\n  ]\n}\n";
            json_out.close();
            std::cout << "✅ JSON: " << json_file << std::endl;
            } else {
                std::cout << "❌ JSON export failed: " << json_file << std::endl;
            }
        }

        // ---- TXT export ----
        if (parser.getExportTXT()) {
            std::string txt_file = export_dir + "/" + base_name + "_result.txt";
            std::ofstream txt_out(txt_file);
            if (txt_out.is_open()) {
            txt_out << "DYNAMIC CONSTRAINT SOLVER RESULTS\n";
            txt_out << "==================================\n\n";
            txt_out << "Configuration: " << config_file << "\n";
            txt_out << "Problem: " << parser.getName() << "\n";
            txt_out << "Description: " << parser.getDescription() << "\n";
            txt_out << "Solutions found: " << found_solutions.size() << "\n\n";
            for (size_t si = 0; si < found_solutions.size(); ++si) {
                const auto& solution = found_solutions[si];
                txt_out << "--- Solution " << (si + 1) << " ---\n";
                if (!solution.voice_solutions.empty()) {
                    for (size_t voice = 0; voice < solution.voice_solutions.size(); ++voice) {
                        txt_out << "\nVoice " << voice << ":\n";
                        txt_out << "  Pitches: ";
                        for (size_t i = 0; i < solution.voice_solutions[voice].size(); ++i) {
                            if (i > 0) txt_out << " → ";
                            int midi = solution.voice_solutions[voice][i];
                            txt_out << (midi < 0 ? "R" : MusicalConstraintSolver::Solver::midi_to_note_name(midi));
                        }
                        txt_out << "\n";
                        if (voice < solution.voice_rhythms.size()) {
                            txt_out << "  Rhythm:  ";
                            for (size_t i = 0; i < solution.voice_rhythms[voice].size(); ++i) {
                                if (i > 0) txt_out << " + ";
                                txt_out << ConstraintSolverJSONParser::format_duration(
                                    solution.voice_rhythms[voice][i], solver_config.rhythm_base);
                            }
                            txt_out << "\n";
                        }
                    }
                } else {
                    txt_out << "  Notes: ";
                    for (size_t i = 0; i < solution.absolute_notes.size(); ++i) {
                        if (i > 0) txt_out << " → ";
                        txt_out << MusicalConstraintSolver::Solver::midi_to_note_name(solution.absolute_notes[i]);
                    }
                    txt_out << "\n";
                }
                txt_out << "\n";
            }
            txt_out << "Performance:\n";
            txt_out << "  Total solve time: " << duration.count() << " ms\n";
            txt_out << "  Rules applied: " << rules.size() << "\n";
            txt_out.close();
            std::cout << "✅ Text: " << txt_file << std::endl;
            } else {
                std::cout << "❌ Text export failed: " << txt_file << std::endl;
            }
        }

        // ---- XML / PNG / MIDI (per solution) ----
        for (size_t si = 0; si < found_solutions.size(); ++si) {
            const auto& solution = found_solutions[si];
            std::string suffix = (found_solutions.size() > 1) ? "_sol" + std::to_string(si + 1) : "";
            if (parser.getExportXML()) {
                std::string xml_file = export_dir + "/" + base_name + suffix + "_result.xml";
                if (solver.export_solution_to_xml(solution, xml_file))
                    std::cout << "✅ XML: " << xml_file << std::endl;
                else
                    std::cout << "❌ XML export failed for solution " << (si + 1) << std::endl;
            }
            if (parser.getExportPNG()) {
                std::string png_file = export_dir + "/" + base_name + suffix + "_result.png";
                if (solver.export_solution_to_png(solution, png_file))
                    std::cout << "✅ PNG: " << png_file << std::endl;
                else
                    std::cout << "❌ PNG export failed for solution " << (si + 1) << std::endl;
            }
            if (parser.getExportMIDI()) {
                std::string midi_file = export_dir + "/" + base_name + suffix + "_result.mid";
                solution.export_to_midi(midi_file);
                std::cout << "✅ MIDI: " << midi_file << std::endl;
            }
        }

        // Solutions already printed by the streaming callback above.
        if (parser.getShowStatistics()) {
            std::cout << "\n📈 Performance:" << std::endl;
            std::cout << "   Solve time: " << duration.count() << " ms" << std::endl;
            std::cout << "   Solutions found: " << found_solutions.size() << std::endl;
            std::cout << "   Rules applied: " << rules.size() << std::endl;
        }

        std::cout << "\n🎉 Dynamic constraint solving complete!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}