#include "max_msp_solver_wrapper.hh"
#include "wildcard_rule_extension.hh"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <numeric>
#include <nlohmann/json.hpp>
#include <sstream>
#include <set>
#include <stdexcept>
#include <vector>
#include <cctype>
#include <regex>

namespace MaxMSPWrapper {

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

namespace {

int gcd_int(int a, int b) {
    a = std::abs(a);
    b = std::abs(b);
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a == 0 ? 1 : a;
}

int lcm_int(int a, int b) {
    if (a == 0 || b == 0) return 0;
    return std::abs(a / gcd_int(a, b) * b);
}

int parse_duration_denominator(const std::string& s) {
    // Accepted forms: "1/4", "-1/8", "3/8".
    const std::string trimmed = s;
    const std::string core = (!trimmed.empty() && trimmed[0] == '-') ? trimmed.substr(1) : trimmed;

    const auto slash = core.find('/');
    if (slash == std::string::npos) {
        throw std::runtime_error("Invalid duration value (missing '/'): " + s);
    }

    const int num = std::stoi(core.substr(0, slash));
    const int den = std::stoi(core.substr(slash + 1));
    if (num <= 0 || den <= 0) {
        throw std::runtime_error("Invalid duration fraction: " + s);
    }
    return den;
}

int parse_duration_to_ticks(const std::string& s, int rhythm_base) {
    const bool is_rest = !s.empty() && s[0] == '-';
    const std::string core = is_rest ? s.substr(1) : s;

    const auto slash = core.find('/');
    if (slash == std::string::npos) {
        throw std::runtime_error("Invalid duration value (missing '/'): " + s);
    }

    const int num = std::stoi(core.substr(0, slash));
    const int den = std::stoi(core.substr(slash + 1));
    if (num <= 0 || den <= 0) {
        throw std::runtime_error("Invalid duration fraction: " + s);
    }

    if ((rhythm_base % den) != 0) {
        throw std::runtime_error("Duration denominator does not divide rhythm base: " + s);
    }

    const int ticks = num * (rhythm_base / den);
    return is_rest ? -ticks : ticks;
}

int parse_score_time_to_ticks(const std::string& token, int rhythm_base, const std::string& context) {
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

    const auto slash = token.find('/');
    if (slash == std::string::npos) {
        throw std::runtime_error(context + ": invalid time token '" + token + "'");
    }
    const int num = std::stoi(token.substr(0, slash));
    const int den = std::stoi(token.substr(slash + 1));
    if (num < 0 || den <= 0) {
        throw std::runtime_error(context + ": invalid time token '" + token + "'");
    }
    if ((rhythm_base * num) % den != 0) {
        throw std::runtime_error(context + ": time token '" + token + "' is not representable with rhythm_base=" + std::to_string(rhythm_base));
    }
    return (rhythm_base * num) / den;
}

bool json_value_to_bool_with_default(const nlohmann::json& value, bool default_value) {
    if (value.is_boolean()) return value.get<bool>();
    if (value.is_number_integer()) return value.get<long long>() != 0;
    if (value.is_number_unsigned()) return value.get<unsigned long long>() != 0;
    if (value.is_number_float()) return std::abs(value.get<double>()) > std::numeric_limits<double>::epsilon();
    if (value.is_string()) {
        std::string text = value.get<std::string>();
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (text == "true" || text == "1" || text == "yes" || text == "on") return true;
        if (text == "false" || text == "0" || text == "no" || text == "off") return false;
    }
    return default_value;
}

std::pair<int, int> parse_time_signature_value(const nlohmann::json& value, const std::string& context) {
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

std::vector<int> parse_positive_int_array(const nlohmann::json& value, const std::string& context) {
    if (value.is_number_integer()) {
        const int v = value.get<int>();
        if (v <= 0) {
            throw std::runtime_error(context + ": value must be positive");
        }
        return {v};
    }
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

std::vector<MusicalConstraintSolver::SolverConfig::MetricDomainEntry>
parse_metric_domain_from_meter(const nlohmann::json& cfg) {
    std::vector<MusicalConstraintSolver::SolverConfig::MetricDomainEntry> out;
    if (!cfg.contains("meter") || !cfg["meter"].is_object()) {
        if (!cfg.contains("metric") || !cfg["metric"].is_object()) {
            return out;
        }
    }

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
        MusicalConstraintSolver::SolverConfig::MetricDomainEntry entry;
        entry.numerator = ts.first;
        entry.denominator = ts.second;
        entry.tuplets = tuplets;
        entry.beat_divisions = beat_divisions;
        out.push_back(std::move(entry));
    }

    return out;
}

void normalize_voice_domain_entry(const nlohmann::json& source,
                                 int voice_index,
                                 nlohmann::json& mapped) {
    if (!source.is_object()) return;

    if (source.contains("rhythm")) {
        const auto& rhythm = source["rhythm"];
        const std::string rhythm_key = "engine_" + std::to_string(voice_index * 2);
        mapped[rhythm_key] = nlohmann::json::object();
        mapped[rhythm_key]["type"] = "rhythm";
        mapped[rhythm_key]["voice"] = voice_index;
        if (rhythm.is_object()) {
            if (rhythm.contains("duration_values")) {
                mapped[rhythm_key]["duration_values"] = rhythm["duration_values"];
            }
            if (rhythm.contains("description")) {
                mapped[rhythm_key]["description"] = rhythm["description"];
            }
        } else {
            mapped[rhythm_key]["duration_values"] = rhythm;
        }
    }

    if (source.contains("pitch")) {
        const auto& pitch = source["pitch"];
        const std::string pitch_key = "engine_" + std::to_string(voice_index * 2 + 1);
        mapped[pitch_key] = nlohmann::json::object();
        mapped[pitch_key]["type"] = "pitch";
        mapped[pitch_key]["voice"] = voice_index;
        if (pitch.is_object()) {
            if (pitch.contains("midi_values")) {
                mapped[pitch_key]["midi_values"] = pitch["midi_values"];
            }
            if (pitch.contains("description")) {
                mapped[pitch_key]["description"] = pitch["description"];
            }
        } else {
            mapped[pitch_key]["midi_values"] = pitch;
        }
    }
}

std::string fraction_array_to_duration_string(const nlohmann::json& value) {
    if (!value.is_array() || value.size() != 2 ||
        !value[0].is_number() || !value[1].is_number()) {
        return "";
    }

    const int num = static_cast<int>(std::lround(value[0].get<double>()));
    const int den = static_cast<int>(std::lround(value[1].get<double>()));
    if (num <= 0 || den <= 0) return "";
    return std::to_string(num) + "/" + std::to_string(den);
}

void preprocess_legacy_config(nlohmann::json& cfg) {
    if (cfg.contains("configuration") && cfg["configuration"].is_object()) {
        const auto& legacy = cfg["configuration"];
        if (!cfg.contains("solution_length") && legacy.contains("sequence_length") && legacy["sequence_length"].is_number_integer()) {
            cfg["solution_length"] = legacy["sequence_length"];
        }
        if (!cfg.contains("num_voices") && legacy.contains("num_voices") && legacy["num_voices"].is_number_integer()) {
            cfg["num_voices"] = legacy["num_voices"];
        }
        if (!cfg.contains("meter") && legacy.contains("time_signature") && legacy["time_signature"].is_string()) {
            cfg["meter"] = nlohmann::json::object();
            cfg["meter"]["time_signatures"] = nlohmann::json::array({legacy["time_signature"]});
        }
        if (!cfg.contains("voices") && legacy.contains("pitch_range") && legacy["pitch_range"].is_array() && legacy["pitch_range"].size() == 2) {
            const int num_voices = cfg.value("num_voices", 1);
            const int lo = static_cast<int>(std::lround(legacy["pitch_range"][0].get<double>()));
            const int hi = static_cast<int>(std::lround(legacy["pitch_range"][1].get<double>()));
            nlohmann::json voices = nlohmann::json::array();
            for (int v = 0; v < std::max(1, num_voices); ++v) {
                nlohmann::json voice = nlohmann::json::object();
                voice["rhythm"] = nlohmann::json::object({{"duration_values", nlohmann::json::array({"1/4"})}});
                nlohmann::json pitches = nlohmann::json::array();
                for (int p = lo; p <= hi; ++p) pitches.push_back(p);
                voice["pitch"] = nlohmann::json::object({{"midi_values", pitches}});
                voices.push_back(std::move(voice));
            }
            cfg["voices"] = std::move(voices);
        }
    }

    if (!cfg.contains("voices") && cfg.contains("note_domain") && cfg["note_domain"].is_array()) {
        const int num_voices = std::max(1, cfg.value("num_voices", 1));
        nlohmann::json voices = nlohmann::json::array();
        for (int v = 0; v < num_voices; ++v) {
            nlohmann::json voice = nlohmann::json::object();
            voice["rhythm"] = nlohmann::json::object({{"duration_values", nlohmann::json::array({"1/4"})}});
            voice["pitch"] = nlohmann::json::object({{"midi_values", cfg["note_domain"]}});
            voices.push_back(std::move(voice));
        }
        cfg["voices"] = std::move(voices);
    }

    if (cfg.contains("domains") && cfg["domains"].is_object()) {
        const auto& domains = cfg["domains"];

        if (!cfg.contains("meter") && domains.contains("metric_domain") && domains["metric_domain"].is_object()) {
            const auto& metric = domains["metric_domain"];
            nlohmann::json meter = nlohmann::json::object();

            if (metric.contains("time_signatures") && metric["time_signatures"].is_array()) {
                nlohmann::json ts = nlohmann::json::array();
                for (const auto& item : metric["time_signatures"]) {
                    if (item.is_string()) {
                        ts.push_back(item);
                    } else if (item.is_array() && item.size() >= 2 && item[0].is_number() && item[1].is_number()) {
                        const int n = static_cast<int>(std::lround(item[0].get<double>()));
                        const int d = static_cast<int>(std::lround(item[1].get<double>()));
                        if (n > 0 && d > 0) ts.push_back(std::to_string(n) + "/" + std::to_string(d));
                    }
                }
                if (!ts.empty()) {
                    meter["time_signatures"] = std::move(ts);
                }
            }

            if (metric.contains("subdivisions")) {
                if (metric["subdivisions"].is_array() && !metric["subdivisions"].empty() && metric["subdivisions"][0].is_array()) {
                    meter["beat_divisions"] = metric["subdivisions"][0];
                } else {
                    meter["beat_divisions"] = metric["subdivisions"];
                }
            }

            if (!meter.empty()) {
                cfg["meter"] = std::move(meter);
            }
        }

        if (!cfg.contains("voices") && domains.contains("voice_domains") && domains["voice_domains"].is_array()) {
            int max_voice = -1;
            for (const auto& vd : domains["voice_domains"]) {
                if (vd.is_object() && vd.contains("voice_index") && vd["voice_index"].is_number_integer()) {
                    max_voice = std::max(max_voice, vd["voice_index"].get<int>());
                }
            }
            if (max_voice >= 0) {
                nlohmann::json voices = nlohmann::json::array();
                for (int i = 0; i <= max_voice; ++i) voices.push_back(nlohmann::json::object());

                for (const auto& vd : domains["voice_domains"]) {
                    if (!vd.is_object() || !vd.contains("voice_index") || !vd["voice_index"].is_number_integer()) continue;
                    const int v = vd["voice_index"].get<int>();
                    if (v < 0 || v > max_voice) continue;

                    nlohmann::json voice = nlohmann::json::object();

                    if (vd.contains("pitch_domain") && vd["pitch_domain"].is_array()) {
                        voice["pitch"] = nlohmann::json::object({{"midi_values", vd["pitch_domain"]}});
                    }

                    if (vd.contains("rhythm_domain") && vd["rhythm_domain"].is_array()) {
                        nlohmann::json durations = nlohmann::json::array();
                        for (const auto& rv : vd["rhythm_domain"]) {
                            if (rv.is_string()) {
                                durations.push_back(rv);
                            } else if (rv.is_number()) {
                                const double raw = rv.get<double>();
                                if (std::isfinite(raw) && std::abs(raw) > 1e-12) {
                                    const bool is_rest = raw < 0.0;
                                    const double abs_value = std::abs(raw);
                                    int best_num = 0;
                                    int best_den = 1;
                                    double best_err = std::numeric_limits<double>::infinity();
                                    for (int den = 1; den <= 1024; ++den) {
                                        const int num = static_cast<int>(std::round(abs_value * den));
                                        if (num <= 0) continue;
                                        const double approx = static_cast<double>(num) / static_cast<double>(den);
                                        const double err = std::abs(approx - abs_value);
                                        if (err < best_err) {
                                            best_err = err;
                                            best_num = num;
                                            best_den = den;
                                            if (err < 1e-9) break;
                                        }
                                    }
                                    if (best_num > 0) {
                                        const int g = gcd_int(best_num, best_den);
                                        best_num /= g;
                                        best_den /= g;
                                        std::ostringstream oss;
                                        if (is_rest) oss << "-";
                                        oss << best_num << "/" << best_den;
                                        durations.push_back(oss.str());
                                    }
                                }
                            } else if (rv.is_array()) {
                                const std::string frac = fraction_array_to_duration_string(rv);
                                if (!frac.empty()) durations.push_back(frac);
                            }
                        }
                        if (!durations.empty()) {
                            voice["rhythm"] = nlohmann::json::object({{"duration_values", durations}});
                        }
                    }

                    voices[v] = std::move(voice);
                }

                cfg["voices"] = std::move(voices);
                cfg["num_voices"] = max_voice + 1;
            }
        }
    }

    if (!cfg.contains("voices")) {
        const int num_voices = std::max(1, cfg.value("num_voices", 1));
        nlohmann::json default_pitches = nlohmann::json::array();
        for (int midi = 60; midi <= 72; ++midi) {
            default_pitches.push_back(midi);
        }

        nlohmann::json voices = nlohmann::json::array();
        for (int v = 0; v < num_voices; ++v) {
            nlohmann::json voice = nlohmann::json::object();
            voice["rhythm"] = nlohmann::json::object({{"duration_values", nlohmann::json::array({"1/4"})}});
            voice["pitch"] = nlohmann::json::object({{"midi_values", default_pitches}});
            voices.push_back(std::move(voice));
        }
        cfg["voices"] = std::move(voices);
        cfg["num_voices"] = num_voices;
    }
}

bool normalize_voices_to_engine_domains(const nlohmann::json& cfg, nlohmann::json& mapped, int& normalized_voice_count) {
    if (!cfg.contains("voices")) {
        normalized_voice_count = 0;
        return false;
    }

    const auto& voices = cfg["voices"];
    if (!voices.is_object() && !voices.is_array()) {
        throw std::runtime_error("voices must be an object or array");
    }

    mapped = nlohmann::json::object();
    int next_voice_index = 0;

    if (voices.is_array()) {
        for (size_t i = 0; i < voices.size(); ++i) {
            const auto& voice = voices[i];
            if (voice.is_null()) {
                throw std::runtime_error(
                    "voices array must not contain gaps; missing voice at index " + std::to_string(i));
            }
            if (!voice.is_object()) {
                throw std::runtime_error(
                    "voices array entries must be objects; invalid entry at index " + std::to_string(i));
            }

            if (voice.contains("voice") && voice["voice"].is_number_integer() &&
                voice["voice"].get<int>() != static_cast<int>(i)) {
                throw std::runtime_error(
                    "voices array must use contiguous positions; entry " + std::to_string(i) +
                    " conflicts with explicit voice index " + std::to_string(voice["voice"].get<int>()));
            }

            const int voice_index = static_cast<int>(i);
            normalize_voice_domain_entry(voice, voice_index, mapped);
            next_voice_index = std::max(next_voice_index, voice_index + 1);
        }
    } else {
        std::vector<int> object_voice_indices;
        for (auto it = voices.begin(); it != voices.end(); ++it) {
            const auto& voice = it.value();
            if (!voice.is_object()) continue;

            int voice_index = -1;
            try {
                voice_index = std::stoi(it.key());
            } catch (...) {
                throw std::runtime_error("voices object keys must be contiguous numeric strings starting at 0");
            }

            if (voice_index < 0) {
                throw std::runtime_error("voices object keys must be contiguous numeric strings starting at 0");
            }

            if (voice.contains("voice") && voice["voice"].is_number_integer() &&
                voice["voice"].get<int>() != voice_index) {
                throw std::runtime_error(
                    "voices object entry '" + it.key() + "' conflicts with explicit voice index " +
                    std::to_string(voice["voice"].get<int>()));
            }

            object_voice_indices.push_back(voice_index);
            next_voice_index = std::max(next_voice_index, voice_index + 1);
        }

        std::sort(object_voice_indices.begin(), object_voice_indices.end());
        object_voice_indices.erase(
            std::unique(object_voice_indices.begin(), object_voice_indices.end()),
            object_voice_indices.end());

        for (int expected = 0; expected < static_cast<int>(object_voice_indices.size()); ++expected) {
            if (object_voice_indices[expected] != expected) {
                throw std::runtime_error(
                    "voices object must use contiguous keys 0..N-1; missing voice " + std::to_string(expected));
            }
        }

        for (auto it = voices.begin(); it != voices.end(); ++it) {
            const auto& voice = it.value();
            if (!voice.is_object()) continue;

            const int voice_index = std::stoi(it.key());
            normalize_voice_domain_entry(voice, voice_index, mapped);
        }
    }

    normalized_voice_count = next_voice_index;
    return true;
}

int engine_index_for_voice_component(int voice, const std::string& component, int num_voices) {
    if (voice < 0 || voice >= num_voices) {
        throw std::runtime_error(
            "voice index " + std::to_string(voice) + " is out of range for " +
            std::to_string(num_voices) + " voices");
    }

    if (component == "rhythm") {
        return voice * 2;
    }
    if (component == "pitch") {
        return voice * 2 + 1;
    }

    throw std::runtime_error(
        "unsupported target_component '" + component + "'; expected 'pitch' or 'rhythm'");
}

void normalize_rule_targeting(nlohmann::json& rule, int num_voices) {
    if (!rule.is_object()) return;

    const std::string rule_id = rule.value("id", rule.value("rule_type", std::string("rule")));
    std::string rule_type = rule.value("rule_type", std::string(""));
    const std::string type_field = rule.value("type", std::string(""));

    // Shorthand alias for built-in rules:
    // "constraint": "all_different"
    // -> "constraint_function": {"type":"builtin","function":"all_different","parameters":[]}
    if (rule_type != "wildcard_constraint" && type_field != "index") {
        if (!rule.contains("constraint_function") && rule.contains("constraint") &&
            rule["constraint"].is_string()) {
            rule["constraint_function"] = nlohmann::json::object();
            rule["constraint_function"]["type"] = "builtin";
            rule["constraint_function"]["function"] = rule["constraint"].get<std::string>();
            rule["constraint_function"]["parameters"] = nlohmann::json::array();
        }

        // If top-level shorthand parameters are present, keep constraint_function
        // parameters in sync even when constraint_function already exists.
        if (rule.contains("constraint_function") && rule["constraint_function"].is_object() &&
            rule.contains("parameters")) {
            if (rule["parameters"].is_array()) {
                rule["constraint_function"]["parameters"] = rule["parameters"];
            } else {
                rule["constraint_function"]["parameters"] = nlohmann::json::array({rule["parameters"]});
            }
        }
    }

    if (type_field == "index") {
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

    if (rule.contains("voice")) {
        if (rule["voice"].is_number_integer()) {
            if (!rule.contains("target_voices") || !rule["target_voices"].is_array()) {
                rule["target_voices"] = nlohmann::json::array();
            }
            rule["target_voices"].push_back(rule["voice"].get<int>());
            rule.erase("voice");
        } else if (rule["voice"].is_null()) {
            nlohmann::json all = nlohmann::json::array();
            for (int voice = 0; voice < num_voices; ++voice) {
                all.push_back(voice);
            }
            if (!all.empty()) {
                rule["target_voices"] = std::move(all);
            }
            rule.erase("voice");
        }
    }

    // Normalize temporal scope alias for timepoint rules.
    if (rule.contains("temporal_scope") && !rule.contains("time_scope")) {
        rule["time_scope"] = rule["temporal_scope"];
    }
    rule.erase("temporal_scope");

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
        if (has_voice || has_voices || has_component) {
            throw std::runtime_error(
                "rule '" + rule_id + "' is metric-targeted implicitly and must not specify voice/component targets");
        }

        rule["target_engine"] = num_voices * 2;
        rule["engine_type"] = "metric";
        return;
    }

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
                if (voice >= 0 && voice < num_voices) {
                    many.push_back(voice);
                }
            }
            if (!many.empty()) {
                rule["target_voices"] = std::move(many);
                has_target_voices = true;
            }
        } else if (has_voice_variable) {
            nlohmann::json all = nlohmann::json::array();
            for (int voice = 0; voice < num_voices; ++voice) {
                all.push_back(voice);
            }
            if (!all.empty()) {
                rule["target_voices"] = std::move(all);
                has_target_voices = true;
            }
        }
    }

    if (!has_target_voices && rule.contains("constraint") && rule["constraint"].is_object()) {
        std::set<int> voices_in_ast;
        std::function<void(const nlohmann::json&)> collect_voices = [&](const nlohmann::json& node) {
            if (node.is_object()) {
                if (node.contains("type") && node["type"].is_string() && node["type"].get<std::string>() == "voice_access" &&
                    node.contains("voice") && node["voice"].is_number_integer()) {
                    voices_in_ast.insert(node["voice"].get<int>());
                }
                for (auto it = node.begin(); it != node.end(); ++it) {
                    collect_voices(it.value());
                }
            } else if (node.is_array()) {
                for (const auto& child : node) {
                    collect_voices(child);
                }
            }
        };
        collect_voices(rule["constraint"]);

        if (!voices_in_ast.empty()) {
            nlohmann::json many = nlohmann::json::array();
            for (int voice : voices_in_ast) {
                if (voice >= 0 && voice < num_voices) {
                    many.push_back(voice);
                }
            }
            if (!many.empty()) {
                rule["target_voices"] = std::move(many);
                has_target_voices = true;
            }
        }
    }

    if (!has_target_voices && rule.contains("scope") && rule["scope"].is_string()) {
        const std::string scope = rule["scope"].get<std::string>();
        if (scope == "each_voice" || scope == "cross_voices" || scope == "all_voices") {
            nlohmann::json all = nlohmann::json::array();
            for (int voice = 0; voice < num_voices; ++voice) {
                all.push_back(voice);
            }
            if (!all.empty()) {
                rule["target_voices"] = std::move(all);
                has_target_voices = true;
            }
        }
    }

    if (!has_target_voices &&
        rule.value("rule_type", std::string("")) == "wildcard_constraint" &&
        rule.value("wildcard_type", std::string("")) == "for_all_voices") {
        nlohmann::json all = nlohmann::json::array();
        for (int voice = 0; voice < num_voices; ++voice) {
            all.push_back(voice);
        }
        if (!all.empty()) {
            rule["target_voices"] = std::move(all);
            has_target_voices = true;
        }
    }

    if (!has_target_voices &&
        rule.value("rule_type", std::string("")) == "wildcard_constraint") {
        nlohmann::json all = nlohmann::json::array();
        for (int voice = 0; voice < num_voices; ++voice) {
            all.push_back(voice);
        }
        if (!all.empty()) {
            rule["target_voices"] = std::move(all);
            has_target_voices = true;
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
    if (target_component.empty()) {
        target_component = "pitch";
    }

    if (!rule.contains("engine_type") && rule_type != "r-time-signature") {
        rule["engine_type"] = target_component;
    }

    nlohmann::json target_engines = nlohmann::json::array();
    for (const auto& voice_json : rule["target_voices"]) {
        const int voice = voice_json.get<int>();
        target_engines.push_back(engine_index_for_voice_component(voice, target_component, num_voices));
    }
    rule["target_engines"] = std::move(target_engines);

    if (rule.contains("constraint_function") && rule["constraint_function"].is_object() &&
        rule["constraint_function"].value("function", std::string("")) == "no_unisons_between_engines") {
        rule["constraint_function"]["parameters"] = rule["target_engines"];
    }
}

void normalize_rules_to_engine_targets(nlohmann::json& cfg, int num_voices) {
    if (!cfg.contains("rules") || (!cfg["rules"].is_array() && !cfg["rules"].is_object())) {
        return;
    }

    if (cfg["rules"].is_array()) {
        for (auto& rule : cfg["rules"]) {
            normalize_rule_targeting(rule, num_voices);
        }
        return;
    }

    for (auto it = cfg["rules"].begin(); it != cfg["rules"].end(); ++it) {
        normalize_rule_targeting(it.value(), num_voices);
    }
}

void normalize_dynamic_rule_syntax(nlohmann::json& cfg) {
    if (!cfg.contains("dynamic_rules") || !cfg["dynamic_rules"].is_array()) {
        return;
    }

    for (auto& rule : cfg["dynamic_rules"]) {
        if (!rule.is_object()) continue;

        // Shorthand alias:
        // "constraint": "voice[0].pitch[i+1] == voice[0].pitch[i] + 2"
        // -> "expression": "..."
        if (!rule.contains("expression") && rule.contains("constraint")) {
            rule["expression"] = rule["constraint"];
        }
    }
}

std::string numeric_duration_to_fraction_string(double value) {
    if (!std::isfinite(value) || std::abs(value) < 1e-12) {
        throw std::runtime_error("Invalid numeric duration value");
    }

    const bool is_rest = value < 0.0;
    const double abs_value = std::abs(value);

    int best_num = 0;
    int best_den = 1;
    double best_err = std::numeric_limits<double>::infinity();

    // Find a rational approximation suitable for musical fractions.
    for (int den = 1; den <= 1024; ++den) {
        const int num = static_cast<int>(std::round(abs_value * den));
        if (num <= 0) continue;
        const double approx = static_cast<double>(num) / static_cast<double>(den);
        const double err = std::abs(approx - abs_value);
        if (err < best_err) {
            best_err = err;
            best_num = num;
            best_den = den;
            if (err < 1e-9) break;
        }
    }

    if (best_num <= 0 || best_den <= 0) {
        throw std::runtime_error("Could not convert numeric duration to fraction");
    }

    const int g = gcd_int(best_num, best_den);
    best_num /= g;
    best_den /= g;

    std::ostringstream oss;
    if (is_rest) oss << "-";
    oss << best_num << "/" << best_den;
    return oss.str();
}

std::string rhythm_ticks_to_fraction_string(int tick_value, int rhythm_base) {
    if (rhythm_base <= 0) {
        return std::to_string(tick_value);
    }

    if (tick_value == 0) {
        return "0";
    }

    const bool is_rest = tick_value < 0;
    const int abs_tick = is_rest ? -tick_value : tick_value;
    const int g = gcd_int(abs_tick, rhythm_base);
    const int num = abs_tick / g;
    const int den = rhythm_base / g;

    std::ostringstream oss;
    if (is_rest) oss << "-";
    oss << num << "/" << den;
    return oss.str();
}

std::string normalize_list_string(std::string s) {
    for (char& c : s) {
        if (c == '[' || c == ']' || c == ',' || c == ';') {
            c = ' ';
        }
    }
    return s;
}

std::vector<int> parse_int_list_from_string(const std::string& input) {
    std::vector<int> out;
    std::istringstream iss(normalize_list_string(input));
    std::string token;
    while (iss >> token) {
        // Trim wrapping quotes if present.
        if (token.size() >= 2 &&
            ((token.front() == '"' && token.back() == '"') ||
             (token.front() == '\'' && token.back() == '\''))) {
            token = token.substr(1, token.size() - 2);
        }
        try {
            size_t idx = 0;
            const long v = std::stol(token, &idx, 10);
            if (idx == token.size()) {
                out.push_back(static_cast<int>(v));
            }
        } catch (...) {
            // Ignore non-integer tokens.
        }
    }
    return out;
}

std::vector<std::string> parse_duration_list_from_string(const std::string& input) {
    std::vector<std::string> out;
    std::istringstream iss(normalize_list_string(input));
    std::string token;
    while (iss >> token) {
        if (token.size() >= 2 &&
            ((token.front() == '"' && token.back() == '"') ||
             (token.front() == '\'' && token.back() == '\''))) {
            token = token.substr(1, token.size() - 2);
        }
        if (!token.empty()) {
            out.push_back(token);
        }
    }
    return out;
}

MusicalConstraintSolver::SolverConfig::SearchEngine parse_search_engine(const nlohmann::json& cfg) {
    if (!cfg.contains("search_options") || !cfg["search_options"].is_object()) {
        return MusicalConstraintSolver::SolverConfig::SearchEngine::DFS;
    }
    const std::string engine = cfg["search_options"].value("engine", std::string("dfs"));
    if (engine == "dfs") {
        return MusicalConstraintSolver::SolverConfig::SearchEngine::DFS;
    }
    throw std::runtime_error("search_options.engine: unsupported value '" + engine + "'");
}

GecodeClusterIntegration::VariableBranchingStrategy parse_variable_branching(const nlohmann::json& cfg) {
    if (!cfg.contains("search_options") || !cfg["search_options"].is_object()) {
        return GecodeClusterIntegration::VariableBranchingStrategy::FIRST_FAIL;
    }
    const std::string branching = cfg["search_options"].value("branching", std::string("first_fail"));
    if (branching == "first_fail") {
        return GecodeClusterIntegration::VariableBranchingStrategy::FIRST_FAIL;
    }
    if (branching == "input_order" || branching == "sequential") {
        return GecodeClusterIntegration::VariableBranchingStrategy::INPUT_ORDER;
    }
    throw std::runtime_error("search_options.branching: unsupported value '" + branching + "'");
}

GecodeClusterIntegration::ValueSelectionStrategy parse_value_selection(const nlohmann::json& cfg) {
    if (cfg.contains("search_options") && cfg["search_options"].is_object() &&
        cfg["search_options"].contains("value_order")) {
        const std::string value_order = cfg["search_options"].value("value_order", std::string("min"));
        if (value_order == "min") {
            return GecodeClusterIntegration::ValueSelectionStrategy::MIN;
        }
        if (value_order == "random") {
            return GecodeClusterIntegration::ValueSelectionStrategy::RANDOM;
        }
        if (value_order == "heuristic") {
            return GecodeClusterIntegration::ValueSelectionStrategy::HEURISTIC;
        }
        throw std::runtime_error("search_options.value_order: unsupported value '" + value_order + "'");
    }
    return GecodeClusterIntegration::ValueSelectionStrategy::MIN;
}

MusicalConstraintSolver::SolverConfig::RestartPolicy parse_restart_policy(const nlohmann::json& cfg) {
    if (!cfg.contains("search_options") || !cfg["search_options"].is_object()) {
        return MusicalConstraintSolver::SolverConfig::RestartPolicy::NONE;
    }
    const std::string restart_policy = cfg["search_options"].value("restart_policy", std::string("none"));
    if (restart_policy == "none") {
        return MusicalConstraintSolver::SolverConfig::RestartPolicy::NONE;
    }
    throw std::runtime_error("search_options.restart_policy: unsupported value '" + restart_policy + "'");
}

std::string status_to_string(SolveStatus status) {
    switch (status) {
        case SolveStatus::UNCONFIGURED: return "unconfigured";
        case SolveStatus::IDLE: return "idle";
        case SolveStatus::RUNNING: return "running";
        case SolveStatus::SUCCESS: return "success";
        case SolveStatus::FAILED: return "failed";
        case SolveStatus::CANCELLED: return "cancelled";
    }
    return "unknown";
}

bool json_value_to_bool(const nlohmann::json& value, bool default_value) {
    if (value.is_boolean()) {
        return value.get<bool>();
    }
    if (value.is_number_integer()) {
        return value.get<long long>() != 0;
    }
    if (value.is_number_unsigned()) {
        return value.get<unsigned long long>() != 0;
    }
    if (value.is_number_float()) {
        return std::abs(value.get<double>()) > std::numeric_limits<double>::epsilon();
    }
    if (value.is_string()) {
        const std::string text = value.get<std::string>();
        if (text == "true" || text == "1") return true;
        if (text == "false" || text == "0") return false;
    }
    return default_value;
}

std::vector<std::string> split_path_tokens(const std::string& key) {
    std::vector<std::string> tokens;
    std::string current;
    for (size_t i = 0; i < key.size(); ++i) {
        if (i + 1 < key.size() && key[i] == ':' && key[i + 1] == ':') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            ++i;
            continue;
        }
        if (key[i] == '[') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }

            std::string index;
            ++i;
            while (i < key.size() && key[i] != ']') {
                index.push_back(key[i]);
                ++i;
            }
            if (!index.empty()) {
                tokens.push_back(index);
            }
            continue;
        }
        if (key[i] == '.') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(key[i]);
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

void set_nested_json_path(nlohmann::json& root,
                          const std::vector<std::string>& tokens,
                          const nlohmann::json& value) {
    if (tokens.empty()) return;

    auto token_is_index = [](const std::string& s) {
        if (s.empty()) return false;
        for (char c : s) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                return false;
            }
        }
        return true;
    };

    nlohmann::json* cur = &root;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];
        const bool last = (i + 1 == tokens.size());

        if (token_is_index(t)) {
            const std::size_t index = static_cast<std::size_t>(std::stoul(t));
            if (!cur->is_array()) {
                *cur = nlohmann::json::array();
            }
            while (cur->size() <= index) {
                cur->push_back(nullptr);
            }

            if (last) {
                (*cur)[index] = value;
                continue;
            }

            const bool next_is_index = token_is_index(tokens[i + 1]);
            nlohmann::json& child = (*cur)[index];
            if (next_is_index) {
                if (!child.is_array()) {
                    child = nlohmann::json::array();
                }
            } else {
                if (!child.is_object()) {
                    child = nlohmann::json::object();
                }
            }
            cur = &child;
            continue;
        }

        if (!cur->is_object()) {
            *cur = nlohmann::json::object();
        }

        if (last) {
            (*cur)[t] = value;
            continue;
        }

        const bool next_is_index = token_is_index(tokens[i + 1]);
        if (!(*cur).contains(t)) {
            (*cur)[t] = next_is_index ? nlohmann::json::array() : nlohmann::json::object();
        } else if (next_is_index && !(*cur)[t].is_array()) {
            (*cur)[t] = nlohmann::json::array();
        } else if (!next_is_index && !(*cur)[t].is_object()) {
            (*cur)[t] = nlohmann::json::object();
        }
        cur = &(*cur)[t];
    }
}

bool is_non_negative_integer_string(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

nlohmann::json normalize_numeric_key_objects_to_arrays(const nlohmann::json& value) {
    if (value.is_array()) {
        nlohmann::json normalized = nlohmann::json::array();
        for (const auto& item : value) {
            normalized.push_back(normalize_numeric_key_objects_to_arrays(item));
        }
        return normalized;
    }

    if (!value.is_object()) {
        return value;
    }

    bool all_numeric_keys = !value.empty();
    size_t max_index = 0;
    for (auto it = value.begin(); it != value.end(); ++it) {
        if (!is_non_negative_integer_string(it.key())) {
            all_numeric_keys = false;
            break;
        }
        max_index = std::max(max_index, static_cast<size_t>(std::stoul(it.key())));
    }

    if (all_numeric_keys) {
        nlohmann::json normalized = nlohmann::json::array();
        for (size_t index = 0; index <= max_index; ++index) {
            const std::string key = std::to_string(index);
            if (value.contains(key)) {
                normalized.push_back(normalize_numeric_key_objects_to_arrays(value.at(key)));
            } else {
                normalized.push_back(nullptr);
            }
        }
        return normalized;
    }

    nlohmann::json normalized = nlohmann::json::object();
    for (auto it = value.begin(); it != value.end(); ++it) {
        normalized[it.key()] = normalize_numeric_key_objects_to_arrays(it.value());
    }
    return normalized;
}

void reconstruct_flattened_max_dict_paths(nlohmann::json& cfg) {
    if (!cfg.is_object()) return;

    nlohmann::json reconstructed = cfg;
    bool changed = false;

    for (auto it = cfg.begin(); it != cfg.end(); ++it) {
        const std::string key = it.key();
        if (key.find("::") == std::string::npos && key.find('.') == std::string::npos) {
            continue;
        }

        const std::vector<std::string> tokens = split_path_tokens(key);
        if (tokens.size() < 2) {
            continue;
        }

        set_nested_json_path(reconstructed, tokens, it.value());
        reconstructed.erase(key);
        changed = true;
    }

    cfg = normalize_numeric_key_objects_to_arrays(changed ? reconstructed : cfg);
}

void coerce_scalar_array_schema_fields(nlohmann::json& value) {
    static const std::set<std::string> array_keys = {
        "dynamic_rules", "rules", "voices", "duration_values", "midi_values", "time_signatures",
        "tuplets", "beat_divisions", "indices", "target_voices", "timepoints", "parameters"
    };

    if (value.is_object()) {
        for (auto it = value.begin(); it != value.end(); ++it) {
            const std::string key = it.key();
            if (array_keys.count(key) > 0 && !it.value().is_array() && !it.value().is_null()) {
                nlohmann::json wrapped = nlohmann::json::array();
                wrapped.push_back(it.value());
                it.value() = std::move(wrapped);
            }
            coerce_scalar_array_schema_fields(it.value());
        }
    } else if (value.is_array()) {
        for (auto& item : value) {
            coerce_scalar_array_schema_fields(item);
        }
    }
}

std::vector<int> project_metric_denominators_from_canonical(
    const MusicalConstraintSolver::MusicalSolution::SolvedScore& score,
    int sequence_length) {
    std::vector<int> projected;
    if (sequence_length <= 0) return projected;
    projected.assign(sequence_length, 4);

    std::vector<int> onset_by_index(sequence_length, std::numeric_limits<int>::max());
    for (const auto& measure : score.measures) {
        for (const auto& ev : measure.events) {
            if (ev.index >= 0 && ev.index < sequence_length) {
                onset_by_index[ev.index] = std::min(onset_by_index[ev.index], ev.onset_ticks);
            }
        }
    }

    bool used_tick_projection = false;
    for (int i = 0; i < sequence_length; ++i) {
        if (onset_by_index[i] == std::numeric_limits<int>::max()) {
            continue;
        }
        used_tick_projection = true;
        for (size_t seg_idx = 0; seg_idx < score.metric_timeline.size(); ++seg_idx) {
            const auto& seg = score.metric_timeline[seg_idx];
            const bool in_last = (seg_idx + 1 == score.metric_timeline.size()) &&
                onset_by_index[i] >= seg.start_tick;
            if ((onset_by_index[i] >= seg.start_tick && onset_by_index[i] < seg.end_tick) || in_last) {
                projected[i] = (seg.denominator > 0) ? seg.denominator : 4;
                break;
            }
        }
    }

    if (!used_tick_projection) {
        for (const auto& seg : score.metric_timeline) {
            const int start = std::max(0, seg.start_index);
            const int end = std::min(sequence_length - 1, seg.end_index);
            for (int i = start; i <= end; ++i) {
                projected[i] = (seg.denominator > 0) ? seg.denominator : 4;
            }
        }
    } else {
        for (int i = 1; i < sequence_length; ++i) {
            if (onset_by_index[i] == std::numeric_limits<int>::max()) {
                projected[i] = projected[i - 1];
            }
        }
    }

    return projected;
}

} // namespace

AsyncSolverWrapper::AsyncSolverWrapper()
    : solver_(), configured_(false), cancel_requested_(false), result_ready_(false),
    status_(SolveStatus::UNCONFIGURED), rhythm_base_(1) {}

AsyncSolverWrapper::~AsyncSolverWrapper() {
    request_cancel();
    join_worker_if_needed();
}

void AsyncSolverWrapper::join_worker_if_needed() {
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

bool AsyncSolverWrapper::configure_from_json(const std::string& config_json, std::string& error_message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ == SolveStatus::RUNNING) {
        error_message = "Cannot reconfigure while solve is running";
        return false;
    }

    join_worker_if_needed();

    if (!apply_config_json(config_json, error_message)) {
        configured_ = false;
        status_ = SolveStatus::UNCONFIGURED;
        return false;
    }

    configured_ = true;
    status_ = SolveStatus::IDLE;
    cancel_requested_.store(false);
    result_ready_.store(false);
    last_config_json_ = config_json;

    last_result_ = SolveResult{};
    last_result_.status = SolveStatus::IDLE;
    last_result_.message = "Configured successfully";
    last_result_.rhythm_base = rhythm_base_;

    return true;
}

bool AsyncSolverWrapper::solve_async(std::string& error_message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!configured_) {
        error_message = "Wrapper is not configured";
        return false;
    }

    if (status_ == SolveStatus::RUNNING) {
        error_message = "Solve already running";
        return false;
    }

    join_worker_if_needed();

    cancel_requested_.store(false);
    result_ready_.store(false);
    status_ = SolveStatus::RUNNING;

    worker_thread_ = std::thread(&AsyncSolverWrapper::run_solve_job, this);
    return true;
}

void AsyncSolverWrapper::request_cancel() {
    cancel_requested_.store(true);
}

SolveStatus AsyncSolverWrapper::get_status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

std::string AsyncSolverWrapper::get_status_string() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_to_string(status_);
}

bool AsyncSolverWrapper::is_running() const {
    return get_status() == SolveStatus::RUNNING;
}

bool AsyncSolverWrapper::take_result(SolveResult& out_result) {
    if (!result_ready_.load()) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    if (!result_ready_.load()) return false;

    out_result = last_result_;
    result_ready_.store(false);
    return true;
}

std::string AsyncSolverWrapper::get_status_json() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j;
    j["status"] = status_to_string(status_);
    return j.dump();
}

void AsyncSolverWrapper::run_solve_job() {
    SolveResult local_result;

    if (cancel_requested_.load()) {
        local_result.status = SolveStatus::CANCELLED;
        local_result.message = "Solve cancelled before start";
    } else {
        try {
            // Use solve_multiple() to handle both single and multiple solutions
            auto solutions = solver_.solve_multiple(max_solutions_, static_cast<int>(timeout_ms_));
            const int rules_ok = solver_.get_dynamic_rule_post_success_count();
            const int rules_failed = solver_.get_dynamic_rule_post_failed_count();
            local_result.dynamic_rules_posted_ok = rules_ok;
            local_result.dynamic_rules_post_failed = rules_failed;
            const std::string rule_compile_summary =
                (rules_failed == 0)
                    ? ("dynamic rules compile: OK (" + std::to_string(rules_ok) + " posted)")
                    : ("dynamic rules compile: FAILED (" + std::to_string(rules_ok) +
                       " posted, " + std::to_string(rules_failed) + " failed)");

            if (cancel_requested_.load()) {
                local_result.status = SolveStatus::CANCELLED;
                local_result.message = "Solve cancelled | " + rule_compile_summary;
            } else if (solutions.empty()) {
                local_result.status = SolveStatus::FAILED;
                local_result.found_solution = false;
                local_result.message = "No solutions found | " + rule_compile_summary;
            } else {
                // Handle first solution (for backward compatibility)
                const auto& solution = solutions[0];
                if (!solution.found_solution) {
                    local_result.status = SolveStatus::FAILED;
                    local_result.found_solution = false;
                    local_result.message = !solution.failure_reason.empty()
                        ? solution.failure_reason
                        : "No solutions found";
                    local_result.message += " | " + rule_compile_summary;
                } else {
                    local_result.status = SolveStatus::SUCCESS;
                    local_result.found_solution = true;
                    local_result.message = "Found " + std::to_string(solutions.size()) +
                        " solution(s) | " + rule_compile_summary;

                    local_result.voice_solutions = solution.voice_solutions;
                    local_result.voice_rhythms = solution.voice_rhythms;
                    local_result.solve_time_ms = solution.solve_time_ms;
                    local_result.backjumps_performed = solution.backjumps_performed;
                    local_result.total_rules_checked = solution.total_rules_checked;
                    local_result.rhythm_base = rhythm_base_;

                    nlohmann::json j;
                    j["found_solution"] = true;
                    j["num_solutions"] = solutions.size();
                    j["solve_time_ms"] = solution.solve_time_ms;
                    j["backjumps_performed"] = solution.backjumps_performed;
                    j["total_rules_checked"] = solution.total_rules_checked;
                    j["dynamic_rules_posted_ok"] = rules_ok;
                    j["dynamic_rules_post_failed"] = rules_failed;
                    j["dynamic_rules_compile_ok"] = (rules_failed == 0);
                    
                    // Return all solutions
                    nlohmann::json all_solutions = nlohmann::json::array();
                    for (const auto& sol : solutions) {
                        nlohmann::json sol_j;
                        sol_j["voice_solutions"] = sol.voice_solutions;
                        
                        // Convert rhythms to fractions
                        nlohmann::json voice_rhythm_fractions = nlohmann::json::array();
                        for (const auto& voice : sol.voice_rhythms) {
                            nlohmann::json v = nlohmann::json::array();
                            for (const int tick : voice) {
                                v.push_back(rhythm_ticks_to_fraction_string(tick, rhythm_base_));
                            }
                            voice_rhythm_fractions.push_back(v);
                        }
                        sol_j["voice_rhythms"] = voice_rhythm_fractions;
                        sol_j["voice_rhythm_ticks"] = sol.voice_rhythms;
                        
                        nlohmann::json metric_signature = nlohmann::json::array();
                        if (sol.has_canonical_score && !sol.canonical_score.metric_timeline.empty()) {
                            const auto den_by_index = project_metric_denominators_from_canonical(
                                sol.canonical_score, static_cast<int>(sol.metric_signature.size()));
                            for (size_t i = 0; i < sol.metric_signature.size(); ++i) {
                                std::ostringstream ts;
                                ts << sol.metric_signature[i] << "/" << den_by_index[i];
                                metric_signature.push_back(ts.str());
                            }
                        } else {
                            for (const int numerator : sol.metric_signature) {
                                std::ostringstream ts;
                                ts << numerator << "/4";
                                metric_signature.push_back(ts.str());
                            }
                        }
                        sol_j["metric_signature"] = metric_signature;
                        sol_j["metric_signature_numerators"] = sol.metric_signature;

                        if (sol.has_canonical_score) {
                            nlohmann::json score_j;
                            score_j["rhythm_base"] = sol.canonical_score.rhythm_base;

                            nlohmann::json metric_timeline = nlohmann::json::array();
                            for (const auto& seg : sol.canonical_score.metric_timeline) {
                                nlohmann::json seg_j;
                                seg_j["start_index"] = seg.start_index;
                                seg_j["end_index"] = seg.end_index;
                                seg_j["start_tick"] = seg.start_tick;
                                seg_j["end_tick"] = seg.end_tick;
                                seg_j["numerator"] = seg.numerator;
                                seg_j["denominator"] = seg.denominator;
                                metric_timeline.push_back(seg_j);
                            }
                            score_j["metric_timeline"] = metric_timeline;

                            nlohmann::json measures = nlohmann::json::array();
                            for (const auto& measure : sol.canonical_score.measures) {
                                nlohmann::json m_j;
                                m_j["measure_index"] = measure.measure_index;
                                m_j["start_ticks"] = measure.start_ticks;
                                m_j["end_ticks"] = measure.end_ticks;
                                m_j["numerator"] = measure.numerator;
                                m_j["denominator"] = measure.denominator;

                                nlohmann::json events = nlohmann::json::array();
                                for (const auto& ev : measure.events) {
                                    nlohmann::json e_j;
                                    e_j["voice"] = ev.voice;
                                    e_j["index"] = ev.index;
                                    e_j["pitch"] = ev.pitch;
                                    e_j["rhythm_ticks"] = ev.rhythm;
                                    e_j["onset_ticks"] = ev.onset_ticks;
                                    e_j["duration_ticks"] = ev.duration_ticks;
                                    e_j["is_rest"] = ev.is_rest;
                                    events.push_back(e_j);
                                }
                                m_j["events"] = events;
                                measures.push_back(m_j);
                            }
                            score_j["measures"] = measures;

                            sol_j["score"] = score_j;
                        }

                        all_solutions.push_back(sol_j);
                    }
                    j["solutions"] = all_solutions;
                    
                    local_result.result_json = j.dump();

                    if (export_json_ || export_txt_ || export_xml_ || export_png_ || export_midi_) {
                        try {
                            std::filesystem::path out_dir = export_path_.empty()
                                ? std::filesystem::path(".")
                                : std::filesystem::path(export_path_);
                            std::error_code mk_err;
                            std::filesystem::create_directories(out_dir, mk_err);
                            if (mk_err) {
                                throw std::runtime_error("Could not create export directory: " + out_dir.string());
                            }

                            const std::string base = export_filename_.empty() ? "max_solver" : export_filename_;
                            const std::filesystem::path base_path = out_dir / base;
                            std::vector<std::string> written_files;

                            if (export_json_) {
                                std::ofstream json_out(base_path.string() + ".json");
                                if (!json_out.is_open()) {
                                    throw std::runtime_error("Could not open JSON output file");
                                }
                                json_out << local_result.result_json;
                                written_files.push_back(base_path.string() + ".json");
                            }
                            if (export_txt_) {
                                std::ofstream txt_out(base_path.string() + ".txt");
                                if (!txt_out.is_open()) {
                                    throw std::runtime_error("Could not open TXT output file");
                                }
                                solution.print_solution(txt_out);
                                written_files.push_back(base_path.string() + ".txt");
                            }
                            if (export_xml_) {
                                solution.export_to_xml(base_path.string() + ".xml");
                                written_files.push_back(base_path.string() + ".xml");
                            }
                            if (export_png_) {
                                solution.export_to_png(base_path.string() + ".png");
                                written_files.push_back(base_path.string() + ".png");
                            }
                            if (export_midi_) {
                                solution.export_to_midi(base_path.string() + ".mid");
                                written_files.push_back(base_path.string() + ".mid");
                            }

                            if (!written_files.empty()) {
                                j["export_path_resolved"] = std::filesystem::absolute(out_dir).string();
                                j["exported_files"] = written_files;
                                local_result.result_json = j.dump();
                            }
                        } catch (const std::exception& ex) {
                            local_result.message += std::string(" (export warning: ") + ex.what() + ")";
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            local_result.status = SolveStatus::FAILED;
            local_result.found_solution = false;
            local_result.message = std::string("Exception during solve: ") + e.what();
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_result_ = local_result;
        status_ = local_result.status;
    }
    result_ready_.store(true);
}

bool AsyncSolverWrapper::apply_config_json(const std::string& config_json, std::string& error_message) {
    try {
        nlohmann::json cfg = nlohmann::json::parse(config_json);
        reconstruct_flattened_max_dict_paths(cfg);
        coerce_scalar_array_schema_fields(cfg);
        preprocess_legacy_config(cfg);

        if (cfg.contains("engine_domains")) {
            throw std::runtime_error(
                "'engine_domains' is deprecated in frontend configs. Use top-level 'voices' with per-voice pitch/rhythm domains.");
        }

        export_path_ = ".";
        export_filename_.clear();
        export_json_ = false;
        export_txt_ = false;
        export_xml_ = false;
        export_png_ = false;
        export_midi_ = false;

        MusicalConstraintSolver::SolverConfig sc;
        sc.sequence_length = cfg.value("solution_length", 12);
        sc.num_voices = cfg.value("num_voices", 2);
        sc.search_engine = parse_search_engine(cfg);
        sc.variable_branching = parse_variable_branching(cfg);
        sc.value_selection = parse_value_selection(cfg);
        sc.restart_policy = parse_restart_policy(cfg);
        sc.verbose_output = false;

        if (cfg.contains("output_options") && cfg["output_options"].is_object()) {
            const auto& oo = cfg["output_options"];
            if (oo.contains("export_path") && oo["export_path"].is_string()) {
                const std::string p = oo["export_path"].get<std::string>();
                if (!p.empty()) export_path_ = p;
            }
            if (oo.contains("export_filename") && oo["export_filename"].is_string()) {
                export_filename_ = oo["export_filename"].get<std::string>();
            }
            if (oo.contains("export_json")) export_json_ = json_value_to_bool_with_default(oo["export_json"], false);
            if (oo.contains("export_txt")) export_txt_ = json_value_to_bool_with_default(oo["export_txt"], false);
            if (oo.contains("export_xml")) export_xml_ = json_value_to_bool_with_default(oo["export_xml"], false);
            if (oo.contains("export_png")) export_png_ = json_value_to_bool_with_default(oo["export_png"], false);
            if (oo.contains("export_midi")) export_midi_ = json_value_to_bool_with_default(oo["export_midi"], false);
        }

        if (cfg.contains("voices")) {
            nlohmann::json mapped;
            int normalized_voice_count = 0;
            if (normalize_voices_to_engine_domains(cfg, mapped, normalized_voice_count)) {
                cfg["engine_domains"] = std::move(mapped);
                if (cfg.contains("num_voices") && cfg["num_voices"].is_number_integer() &&
                    cfg["num_voices"].get<int>() != normalized_voice_count) {
                    throw std::runtime_error(
                        "num_voices does not match voices definition; expected " +
                        std::to_string(normalized_voice_count) + " but got " +
                        std::to_string(cfg["num_voices"].get<int>()));
                }
                sc.num_voices = normalized_voice_count;
            }
        }

        normalize_rules_to_engine_targets(cfg, sc.num_voices);
        normalize_dynamic_rule_syntax(cfg);

        if (cfg.contains("meter") || cfg.contains("metric")) {
            const auto metric_from_meter = parse_metric_domain_from_meter(cfg);
            if (!metric_from_meter.empty()) {
                sc.metric_domain = metric_from_meter;
            }
        }

        if ((!cfg.contains("engine_domains") || !cfg["engine_domains"].is_object()) &&
            cfg.contains("domains") && (cfg["domains"].is_array() || cfg["domains"].is_object())) {
            nlohmann::json mapped = nlohmann::json::object();
            std::vector<nlohmann::json> domain_entries;
            if (cfg["domains"].is_array()) {
                for (const auto& d : cfg["domains"]) {
                    domain_entries.push_back(d);
                }
            } else {
                for (auto it = cfg["domains"].begin(); it != cfg["domains"].end(); ++it) {
                    domain_entries.push_back(it.value());
                }
            }

            for (const auto& d : domain_entries) {
                if (!d.is_object()) continue;
                int voice = d.value("voice", -1);
                if (voice < 0) {
                    const int engine_id = d.value("engine_id", -1);
                    if (engine_id >= 0) {
                        voice = ((engine_id % 2) == 0) ? (engine_id / 2) : ((engine_id - 1) / 2);
                    }
                }
                if (voice < 0) continue;

                const std::string type = d.value("type", std::string(""));
                std::string key;
                if (type == "pitch") {
                    key = "engine_" + std::to_string(voice * 2 + 1);
                } else if (type == "rhythm") {
                    key = "engine_" + std::to_string(voice * 2);
                } else {
                    continue;
                }

                mapped[key] = nlohmann::json::object();
                mapped[key]["type"] = type;
                mapped[key]["voice"] = voice;
                if (d.contains("values")) {
                    if (type == "pitch") {
                        mapped[key]["midi_values"] = d["values"];
                    } else if (type == "rhythm") {
                        mapped[key]["duration_values"] = d["values"];
                    }
                }
            }
            cfg["engine_domains"] = mapped;
        }

        if (cfg.contains("search_options") && cfg["search_options"].is_object()) {
            const auto& so = cfg["search_options"];
            if (so.contains("random_seed") && so["random_seed"].is_number_unsigned()) {
                sc.random_seed = so["random_seed"].get<unsigned int>();
            } else if (so.contains("random_seed") && so["random_seed"].is_number_integer()) {
                const int s = so["random_seed"].get<int>();
                sc.random_seed = s <= 0 ? 0u : static_cast<unsigned int>(s);
            }
            if (so.contains("timeout_ms") && so["timeout_ms"].is_number()) {
                sc.timeout_seconds = so["timeout_ms"].get<double>() / 1000.0;
                timeout_ms_ = so["timeout_ms"].get<double>();
            }
            if (so.contains("max_solutions") && so["max_solutions"].is_number_integer()) {
                max_solutions_ = so["max_solutions"].get<int>();
                sc.max_solutions = max_solutions_;
            }
            if (so.contains("heuristic_top_k") && so["heuristic_top_k"].is_number_integer()) {
                sc.heuristic_top_k = std::max(0, so["heuristic_top_k"].get<int>());
            }
            if (so.contains("heuristic_trace") && so["heuristic_trace"].is_boolean()) {
                sc.heuristic_trace = so["heuristic_trace"].get<bool>();
            }
        }

        if (cfg.contains("timeout_ms") && cfg["timeout_ms"].is_number()) {
            sc.timeout_seconds = cfg["timeout_ms"].get<double>() / 1000.0;
        }

        if (!cfg.contains("engine_domains") || !cfg["engine_domains"].is_object()) {
            throw std::runtime_error("Missing required voices-derived domains after normalization");
        }

        sc.voice_domains.assign(sc.num_voices, {});
        std::vector<std::vector<std::string>> rhythm_strings(sc.num_voices);

        const auto& engine_domains = cfg["engine_domains"];
        for (auto it = engine_domains.begin(); it != engine_domains.end(); ++it) {
            const auto& domain = it.value();
            if (!domain.is_object()) continue;

            const std::string type = domain.value("type", std::string(""));
            const int voice = domain.value("voice", -1);
            if (voice < 0 || voice >= sc.num_voices) continue;

            if (type == "pitch" && domain.contains("midi_values")) {
                const auto& midi = domain["midi_values"];
                if (midi.is_array()) {
                    for (const auto& v : midi) {
                        if (v.is_number_integer()) {
                            sc.voice_domains[voice].push_back(v.get<int>());
                        } else if (v.is_number_float()) {
                            sc.voice_domains[voice].push_back(static_cast<int>(std::lround(v.get<double>())));
                        } else if (v.is_string()) {
                            const auto parsed = parse_int_list_from_string(v.get<std::string>());
                            sc.voice_domains[voice].insert(sc.voice_domains[voice].end(), parsed.begin(), parsed.end());
                        }
                    }
                } else if (midi.is_number_integer()) {
                    sc.voice_domains[voice].push_back(midi.get<int>());
                } else if (midi.is_number_float()) {
                    sc.voice_domains[voice].push_back(static_cast<int>(std::lround(midi.get<double>())));
                } else if (midi.is_string()) {
                    const auto parsed = parse_int_list_from_string(midi.get<std::string>());
                    sc.voice_domains[voice].insert(sc.voice_domains[voice].end(), parsed.begin(), parsed.end());
                }
            } else if (type == "rhythm" && domain.contains("duration_values")) {
                const auto& durations = domain["duration_values"];
                if (durations.is_array()) {
                    for (const auto& dv : durations) {
                        if (dv.is_string()) {
                            const auto parsed = parse_duration_list_from_string(dv.get<std::string>());
                            rhythm_strings[voice].insert(rhythm_strings[voice].end(), parsed.begin(), parsed.end());
                        } else if (dv.is_number()) {
                            rhythm_strings[voice].push_back(numeric_duration_to_fraction_string(dv.get<double>()));
                        }
                    }
                } else if (durations.is_string()) {
                    const auto parsed = parse_duration_list_from_string(durations.get<std::string>());
                    rhythm_strings[voice].insert(rhythm_strings[voice].end(), parsed.begin(), parsed.end());
                } else if (durations.is_number()) {
                    rhythm_strings[voice].push_back(numeric_duration_to_fraction_string(durations.get<double>()));
                }
            }
        }

        for (int v = 0; v < sc.num_voices; ++v) {
            auto& d = sc.voice_domains[v];
            if (d.empty()) {
                throw std::runtime_error("Missing pitch domain for voice " + std::to_string(v));
            }
            std::sort(d.begin(), d.end());
            d.erase(std::unique(d.begin(), d.end()), d.end());
        }

        int rhythm_base = 1;
        for (int v = 0; v < sc.num_voices; ++v) {
            if (rhythm_strings[v].empty()) {
                rhythm_strings[v].push_back("1/4");
            }
            for (const auto& s : rhythm_strings[v]) {
                rhythm_base = lcm_int(rhythm_base, parse_duration_denominator(s));
            }
        }
        sc.rhythm_base = rhythm_base;
        rhythm_base_ = rhythm_base;
        sc.voice_rhythm_domains.assign(sc.num_voices, {});
        for (int v = 0; v < sc.num_voices; ++v) {
            for (const auto& s : rhythm_strings[v]) {
                sc.voice_rhythm_domains[v].push_back(parse_duration_to_ticks(s, rhythm_base));
            }
            auto& rd = sc.voice_rhythm_domains[v];
            std::sort(rd.begin(), rd.end());
            rd.erase(std::unique(rd.begin(), rd.end()), rd.end());
        }

        if (cfg.contains("score_length") && cfg["score_length"].is_string()) {
            sc.score_length_ticks = parse_score_time_to_ticks(
                cfg["score_length"].get<std::string>(), rhythm_base, "score_length");
        }
        if (cfg.contains("search_options") && cfg["search_options"].is_object() &&
            cfg["search_options"].contains("require_exact_score_length")) {
            sc.require_exact_score_length = json_value_to_bool_with_default(
                cfg["search_options"]["require_exact_score_length"], false);
        } else if (cfg.contains("require_exact_score_length")) {
            sc.require_exact_score_length = json_value_to_bool_with_default(
                cfg["require_exact_score_length"], false);
        }

        sc.enable_metric_engine = false;
        if (cfg.contains("search_options") && cfg["search_options"].is_object() &&
            cfg["search_options"].contains("enable_metric_engine")) {
            sc.enable_metric_engine = json_value_to_bool_with_default(
                cfg["search_options"]["enable_metric_engine"], false);
        } else if (cfg.contains("enable_metric_engine")) {
            sc.enable_metric_engine = json_value_to_bool_with_default(cfg["enable_metric_engine"], false);
        }

        solver_.configure(sc);
        solver_.clear_rules();
        solver_.clear_dynamic_rules();

        if (cfg.contains("rules") && (cfg["rules"].is_array() || cfg["rules"].is_object())) {
            std::vector<nlohmann::json> rule_entries;
            if (cfg["rules"].is_array()) {
                for (const auto& r : cfg["rules"]) {
                    rule_entries.push_back(r);
                }
            } else {
                for (auto it = cfg["rules"].begin(); it != cfg["rules"].end(); ++it) {
                    rule_entries.push_back(it.value());
                }
            }

            for (const auto& r : rule_entries) {
                if (!r.is_object()) continue;
                if (r.contains("enabled") && !json_value_to_bool(r["enabled"], true)) continue;

                const std::string rule_type = r.value("rule_type", std::string(""));
                std::string function;
                if (r.contains("constraint_function") && r["constraint_function"].is_object()) {
                    function = r["constraint_function"].value("function", std::string(""));
                }

                if (function.empty()) {
                    if (rule_type == "r-twelve-tone-voice1") {
                        function = "all_different";
                    } else if (rule_type == "r-palindrome-voice2" || rule_type == "r-palindrome-voice") {
                        function = "palindrome_of_engine";
                    }
                }

                std::vector<int> indices;
                if (r.contains("indices") && r["indices"].is_array()) {
                    for (const auto& idx : r["indices"]) {
                        if (idx.is_number_integer()) indices.push_back(idx.get<int>());
                    }
                }

                std::vector<std::string> timepoints;
                if (r.contains("timepoints") && r["timepoints"].is_array()) {
                    for (const auto& tp : r["timepoints"]) {
                        if (tp.is_string()) timepoints.push_back(tp.get<std::string>());
                    }
                }

                int target_engine = r.value("target_engine", -1);
                std::vector<int> target_engines;
                if (r.contains("target_engines") && r["target_engines"].is_array()) {
                    for (const auto& te : r["target_engines"]) {
                        if (te.is_number_integer()) target_engines.push_back(te.get<int>());
                    }
                }

                const bool has_target_engine = target_engine >= 0;
                const bool has_target_engines = !target_engines.empty();
                if (!rule_type.empty() && !has_target_engine && !has_target_engines) {
                    throw std::runtime_error("rule '" + r.value("id", rule_type) + "' is missing target_voice/target_voices after normalization");
                }

                std::string engine_type = r.value("engine_type", std::string(""));
                std::string description = r.value("description", std::string(""));
                std::vector<double> parameters;
                std::vector<std::string> parameter_strings;
                if (r.contains("constraint_function") &&
                    r["constraint_function"].is_object() &&
                    r["constraint_function"].contains("parameters")) {
                    const auto& params_node = r["constraint_function"]["parameters"];
                    if (params_node.is_array()) {
                        for (const auto& p : params_node) {
                            if (p.is_number()) {
                                parameters.push_back(p.get<double>());
                            } else if (p.is_string()) {
                                parameter_strings.push_back(p.get<std::string>());
                            }
                        }
                    } else if (params_node.is_number()) {
                        parameters.push_back(params_node.get<double>());
                    } else if (params_node.is_string()) {
                        parameter_strings.push_back(params_node.get<std::string>());
                    }
                }

                // bar_pattern fields (r-time-signature)
                std::string bar_pattern_type = r.value("bar_pattern_type", std::string(""));
                std::vector<std::string> bar_pattern;
                if (r.contains("bar_pattern") && r["bar_pattern"].is_array()) {
                    for (const auto& bp : r["bar_pattern"]) {
                        if (bp.is_string()) bar_pattern.push_back(bp.get<std::string>());
                    }
                }
                int bar_pattern_count = r.value("bar_pattern_count", 0);
                int bar_pattern_repetitions = r.value("bar_pattern_repetitions", 0);
                bool allow_cross_barline = false;
                if (r.contains("allow_cross_barline")) {
                    allow_cross_barline = json_value_to_bool(r["allow_cross_barline"], false);
                }
                std::map<std::string, double> bar_pattern_distribution;
                if (r.contains("bar_pattern_distribution") && r["bar_pattern_distribution"].is_object()) {
                    for (auto dit = r["bar_pattern_distribution"].begin(); dit != r["bar_pattern_distribution"].end(); ++dit) {
                        if (dit.value().is_number()) bar_pattern_distribution[dit.key()] = dit.value().get<double>();
                    }
                }

                solver_.add_rule_config(rule_type, function, indices, target_engine,
                                        target_engines, engine_type, description,
                                        parameters, parameter_strings, timepoints,
                                        bar_pattern_type, bar_pattern,
                                        bar_pattern_count, bar_pattern_repetitions,
                                        bar_pattern_distribution,
                                        allow_cross_barline);
            }
        }

        if (cfg.contains("dynamic_rules") && cfg["dynamic_rules"].is_array()) {
            std::vector<nlohmann::json> dynamic_rules;
            for (const auto& dr : cfg["dynamic_rules"]) {
                if (dr.is_null()) continue;
                if (dr.is_object() && dr.contains("enabled") && !json_value_to_bool(dr["enabled"], true)) {
                    continue;
                }
                dynamic_rules.push_back(dr);
            }
            solver_.load_dynamic_rules(dynamic_rules);
        }

        if (cfg.contains("rules") && (cfg["rules"].is_array() || cfg["rules"].is_object())) {
            std::vector<nlohmann::json> enhanced_rules;
            if (cfg["rules"].is_array()) {
                for (const auto& r : cfg["rules"]) {
                    enhanced_rules.push_back(r);
                }
            } else {
                for (auto it = cfg["rules"].begin(); it != cfg["rules"].end(); ++it) {
                    enhanced_rules.push_back(it.value());
                }
            }

            for (const auto& rule_json : enhanced_rules) {
                try {
                    if (!rule_json.is_object()) continue;
                    if (rule_json.contains("enabled") && !json_value_to_bool(rule_json["enabled"], true)) continue;

                    const std::string rule_type = rule_json.value("rule_type", "");
                    const std::string type_field = rule_json.value("type", "");
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
                        bool fixed_rhythm = !sc.voice_rhythm_domains.empty();
                        for (const auto& rd : sc.voice_rhythm_domains) {
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
                                for (int i = 0; i < sc.sequence_length; ++i) {
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
                                            tick = parse_score_time_to_ticks(
                                                tp.get<std::string>(), sc.rhythm_base,
                                                "r-timepoint-relationship timepoint");
                                        } else if (tp.is_number_integer()) {
                                            tick = tp.get<int>();
                                        }
                                        if (tick < 0 || (tick % fixed_step_ticks) != 0) {
                                            throw std::runtime_error(
                                                "r-timepoint-relationship fixed-rhythm: timepoint not aligned with rhythmic step");
                                        }
                                        const int idx = tick / fixed_step_ticks;
                                        if (idx < 0 || idx >= sc.sequence_length) {
                                            throw std::runtime_error("r-timepoint-relationship timepoint maps to out-of-range index");
                                        }
                                        selected_indices.push_back(idx);
                                    }
                                } else {
                                    // Variable-rhythm case: use onset-aware mapping
                                    auto voice_onsets = compute_voice_onsets(sc.voice_rhythm_domains, sc.sequence_length);
                                    for (const auto& tp : rule_json["timepoints"]) {
                                        int tick = -1;
                                        if (tp.is_string()) {
                                            tick = parse_score_time_to_ticks(
                                                tp.get<std::string>(), sc.rhythm_base,
                                                "r-timepoint-relationship timepoint");
                                        } else if (tp.is_number_integer()) {
                                            tick = tp.get<int>();
                                        }
                                        if (tick < 0) {
                                            throw std::runtime_error("r-timepoint-relationship timepoint is negative");
                                        }
                                        // Find first index at or after this tick for each voice
                                        for (int v = 0; v < static_cast<int>(voice_onsets.size()); ++v) {
                                            for (int i = 0; i < sc.sequence_length; ++i) {
                                                if (voice_onsets[v][i] >= tick) {
                                                    selected_indices.push_back(i);
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                            } else if (time_scope == "on_beats") {
                                if (sc.metric_domain.empty()) {
                                    throw std::runtime_error("r-timepoint-relationship on_beats requires metric_domain");
                                }
                                const auto& ts = sc.metric_domain.front();
                                const int numerator = ts.numerator;
                                const int denominator = ts.denominator;
                                if (numerator <= 0 || denominator <= 0 ||
                                    (sc.rhythm_base % denominator) != 0) {
                                    throw std::runtime_error(
                                        "r-timepoint-relationship on_beats has incompatible metric_domain/rhythm_base");
                                }
                                const int beat_tick = sc.rhythm_base / denominator;

                                std::set<int> beat_filter;
                                if (rule_json.contains("beat_numbers") && rule_json["beat_numbers"].is_array()) {
                                    for (const auto& b : rule_json["beat_numbers"]) {
                                        if (b.is_number_integer()) beat_filter.insert(b.get<int>());
                                    }
                                }

                                if (fixed_rhythm && fixed_step_ticks > 0) {
                                    // Fixed-rhythm: compute beat position from tick = i * fixed_step_ticks
                                    for (int i = 0; i < sc.sequence_length; ++i) {
                                        const int tick = i * fixed_step_ticks;
                                        if ((tick % beat_tick) != 0) continue;
                                        const int beat = ((tick / beat_tick) % numerator) + 1;
                                        if (beat_filter.empty() || beat_filter.count(beat) > 0) {
                                            selected_indices.push_back(i);
                                        }
                                    }
                                } else {
                                    // Variable-rhythm: find indices where voice onsets fall on beat boundaries
                                    auto voice_onsets = compute_voice_onsets(sc.voice_rhythm_domains, sc.sequence_length);
                                    for (int i = 0; i < sc.sequence_length; ++i) {
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
                                return idx < 0 || idx >= sc.sequence_length;
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
                                    solver_.apply_compiled_constraint(std::move(compiled));
                                }
                            }
                        }
                    } else if (rule_type == "wildcard_constraint") {
                        auto compiled = DynamicRules::WildcardRuleCompiler::compile_wildcard_from_json(rule_json);
                        if (compiled) {
                            solver_.apply_compiled_constraint(std::move(compiled));
                        }
                    } else if (type_field == "index") {
                        const std::string rule_id = rule_json.value("id", "index_rule");
                        if (!rule_json.contains("voice") || !rule_json["voice"].is_number_integer()) {
                            throw std::runtime_error("index rule '" + rule_id + "': missing or invalid 'voice' field");
                        }
                        if (!rule_json.contains("events") || !rule_json["events"].is_array()) {
                            throw std::runtime_error("index rule '" + rule_id + "': missing or invalid 'events' array");
                        }

                        struct IndexEvent {
                            int pos;
                            int rhythm_ticks;
                            int pitch;
                        };

                        const int voice = rule_json["voice"].get<int>();
                        const int rhythm_base_local = sc.rhythm_base;
                        std::vector<IndexEvent> events;

                        for (size_t ei = 0; ei < rule_json["events"].size(); ++ei) {
                            const auto& ev = rule_json["events"][ei];
                            if (!ev.is_array() || ev.size() != 3) {
                                throw std::runtime_error("index rule '" + rule_id + "': event[" + std::to_string(ei) + "] must be [pos, rhythm, pitch_or_null]");
                            }

                            const int pos = ev[0].get<int>();
                            const std::string rhythm_str = ev[1].get<std::string>();
                            const bool pitch_is_null = ev[2].is_null();
                            const int pitch_midi = pitch_is_null
                                ? GecodeClusterIntegration::IntegratedMusicalSpace::REST_PITCH_SENTINEL
                                : ev[2].get<int>();

                            const int rhythm_ticks = parse_duration_to_ticks(rhythm_str, rhythm_base_local);
                            const bool is_rest = (rhythm_ticks < 0);
                            if (is_rest && !pitch_is_null) {
                                throw std::runtime_error("index rule '" + rule_id + "' event[" + std::to_string(ei) + "]: rest rhythm requires null pitch");
                            }
                            if (!is_rest && pitch_is_null) {
                                throw std::runtime_error("index rule '" + rule_id + "' event[" + std::to_string(ei) + "]: note rhythm requires non-null pitch");
                            }

                            events.push_back({pos, rhythm_ticks, pitch_midi});
                        }

                        auto compiled = std::make_unique<DynamicRules::CompiledConstraint>(
                            rule_id,
                            "index rule: pin voice " + std::to_string(voice));

                        compiled->post_constraint = [voice, events, rule_id](DynamicRules::ConstraintContext& ctx) {
                            if (voice < 0 || voice >= ctx.num_voices) {
                                throw std::runtime_error("index rule '" + rule_id + "': voice out of range");
                            }

                            for (const auto& ev : events) {
                                if (ev.pos < 0 || ev.pos >= ctx.sequence_length) {
                                    throw std::runtime_error("index rule '" + rule_id + "': position out of range");
                                }

                                const int idx = voice * ctx.sequence_length + ev.pos;
                                if (ctx.rhythm_vars && idx < static_cast<int>(ctx.rhythm_vars->size())) {
                                    Gecode::dom(*ctx.space, (*ctx.rhythm_vars)[idx], Gecode::IntSet(ev.rhythm_ticks, ev.rhythm_ticks));
                                }
                                if (ev.pitch != GecodeClusterIntegration::IntegratedMusicalSpace::REST_PITCH_SENTINEL) {
                                    if (ctx.pitch_vars && idx < static_cast<int>(ctx.pitch_vars->size())) {
                                        Gecode::dom(*ctx.space, (*ctx.pitch_vars)[idx], Gecode::IntSet(ev.pitch, ev.pitch));
                                    }
                                }
                            }
                        };

                        solver_.apply_compiled_constraint(std::move(compiled));
                    } else if (rule_json.contains("id") && rule_json.contains("expression")) {
                        std::vector<nlohmann::json> single_rule = {rule_json};
                        solver_.load_dynamic_rules(single_rule);
                    }
                } catch (...) {
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        error_message = e.what();
        return false;
    }
}

} // namespace MaxMSPWrapper
