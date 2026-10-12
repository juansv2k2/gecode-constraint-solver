#ifndef MOTIVE_DOMAIN_PARSER_HH
#define MOTIVE_DOMAIN_PARSER_HH

#include "motive_domains.hh"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <regex>
#include <stdexcept>

namespace MotiveDomains {

inline void restore_indexed_arrays(nlohmann::json& value) {
    if (value.is_object()) {
        std::map<int, nlohmann::json> entries;
        static const std::regex index_pattern("^(0|[1-9][0-9]*)$");
        for (auto entry = value.begin(); entry != value.end(); ++entry) {
            if (!std::regex_match(entry.key(), index_pattern))
                throw std::runtime_error("Motive dictionary arrays require numeric indexes");
            entries.emplace(std::stoi(entry.key()), entry.value());
        }
        value = nlohmann::json::array();
        for (auto& entry : entries) {
            if (entry.first != static_cast<int>(value.size()))
                throw std::runtime_error("Motive dictionary array has missing indexes");
            value.push_back(std::move(entry.second));
        }
    }
    if (value.is_array())
        for (auto& entry : value)
            if (entry.is_object() || entry.is_array()) restore_indexed_arrays(entry);
}

inline std::pair<int, int> duration_fraction(const nlohmann::json& value) {
    if (value.is_number_integer()) {
        const long long numerator = value.get<long long>();
        if (numerator == 0 || numerator <= -std::numeric_limits<int>::max() ||
            numerator > std::numeric_limits<int>::max())
            throw std::runtime_error("Rhythm motive numeric duration must be nonzero and representable: " + value.dump());
        return {static_cast<int>(numerator), 1};
    }
    if (value.is_number_float()) {
        const double duration = value.get<double>();
        if (!std::isfinite(duration) || std::abs(duration) < 1e-12)
            throw std::runtime_error("Invalid numeric rhythm motive duration: " + value.dump());
        int best_numerator = 0;
        int best_denominator = 1;
        double best_error = std::numeric_limits<double>::infinity();
        for (int denominator = 1; denominator <= 1024; ++denominator) {
            const double scaled = std::abs(duration) * denominator;
            if (scaled > std::numeric_limits<int>::max()) break;
            const int numerator = static_cast<int>(std::round(scaled));
            if (numerator <= 0) continue;
            const double error = std::abs(static_cast<double>(numerator) / denominator - std::abs(duration));
            if (error < best_error) {
                best_error = error;
                best_numerator = numerator;
                best_denominator = denominator;
                if (error < 1e-9) break;
            }
        }
        if (best_numerator == 0)
            throw std::runtime_error("Numeric rhythm motive duration is not representable: " + value.dump());
        const int divisor = std::gcd(best_numerator, best_denominator);
        const int sign = duration < 0.0 ? -1 : 1;
        return {sign * (best_numerator / divisor), best_denominator / divisor};
    }
    if (!value.is_string())
        throw std::runtime_error("Rhythm motive values must be fraction strings (e.g. 1/8, -1/4) or numbers (e.g. 0.25); got " + value.dump());
    const std::string text = value.get<std::string>();
    static const std::regex fraction("^(-?[0-9]+)/([0-9]+)$");
    if (!std::regex_match(text, fraction))
        throw std::runtime_error("Invalid rhythm motive duration: " + text);
    const auto slash = text.find('/');
    const long long numerator = std::stoll(text.substr(0, slash));
    const long long denominator = std::stoll(text.substr(slash + 1));
    if (numerator == 0 || denominator <= 0 ||
        numerator <= -std::numeric_limits<int>::max() || numerator > std::numeric_limits<int>::max() ||
        denominator > std::numeric_limits<int>::max())
        throw std::runtime_error("Rhythm motive duration must be nonzero and representable: " + text);
    return {static_cast<int>(numerator), static_cast<int>(denominator)};
}

inline void validate_motives(const nlohmann::json& motives, bool rhythm, int event_count) {
    if (!motives.is_array() || motives.empty())
        throw std::runtime_error("motives must be a nonempty array of nonempty arrays");
    if (event_count <= 0) throw std::runtime_error("Motive event count must be positive");
    std::vector<bool> reachable(event_count + 1, false);
    reachable[0] = true;
    for (const auto& motive : motives) {
        if (!motive.is_array() || motive.empty())
            throw std::runtime_error("Each motive must be a nonempty array; flat lists are not motives");
        for (const auto& value : motive) {
            if (rhythm) duration_fraction(value);
            else if (!value.is_number_integer() || value.get<long long>() < 0 || value.get<long long>() > 127)
                throw std::runtime_error("Pitch motive values must be MIDI integers 0..127");
        }
    }
    for (int start = 0; start <= event_count; ++start) {
        if (!reachable[start]) continue;
        for (const auto& motive : motives) {
            const size_t end = static_cast<size_t>(start) + motive.size();
            if (end <= static_cast<size_t>(event_count)) reachable[end] = true;
        }
    }
    if (!reachable[event_count])
        throw std::runtime_error("Complete motives cannot fill solution_length=" + std::to_string(event_count));
}

inline void normalize_domains(nlohmann::json& cfg) {
    const int event_count = cfg.value("solution_length", 12);
    auto normalize_voice = [&](nlohmann::json& voice) {
        if (!voice.is_object()) return;
        for (const std::string component : {"pitch", "rhythm"}) {
            if (!voice.contains(component) || !voice[component].is_object()) continue;
            auto& domain = voice[component];
            if (!domain.contains("motives")) continue;
            const bool rhythm = component == "rhythm";
            const std::string scalar_key = rhythm ? "duration_values" : "midi_values";
            if (domain.contains(scalar_key))
                throw std::runtime_error(component + " must specify motives OR " + scalar_key + ", not both");
            restore_indexed_arrays(domain["motives"]);
            if (rhythm && domain["motives"].is_array()) {
                for (auto& motive : domain["motives"])
                    if (motive.is_array())
                        for (auto& value : motive)
                            if (value.is_number()) {
                                const auto fraction = duration_fraction(value);
                                value = std::to_string(fraction.first) + "/" + std::to_string(fraction.second);
                            }
            }
            validate_motives(domain["motives"], rhythm, event_count);
            auto values = nlohmann::json::array();
            for (const auto& motive : domain["motives"])
                for (const auto& value : motive)
                    if (std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
            domain[scalar_key] = std::move(values);
        }
    };
    if (cfg.contains("global_domain")) normalize_voice(cfg["global_domain"]);
    if (cfg.contains("voices") && (cfg["voices"].is_array() || cfg["voices"].is_object()))
        for (auto& voice : cfg["voices"]) normalize_voice(voice);
}

inline const nlohmann::json* component_for_voice(const nlohmann::json& cfg, int voice,
                                                const std::string& component) {
    if (cfg.contains("voices") && cfg["voices"].is_array()) {
        const auto& voices = cfg["voices"];
        for (const auto& entry : voices)
            if (entry.is_object() && entry.contains("voice") && entry["voice"] == voice && entry.contains(component))
                return &entry[component];
        if (voice < static_cast<int>(voices.size()) && voices[voice].is_object() &&
            (!voices[voice].contains("voice") || voices[voice]["voice"] == voice) && voices[voice].contains(component))
            return &voices[voice][component];
    } else if (cfg.contains("voices") && cfg["voices"].is_object()) {
        const auto index = std::to_string(voice);
        if (cfg["voices"].contains(index) && cfg["voices"][index].contains(component))
            return &cfg["voices"][index][component];
    }
    if (cfg.contains("global_domain") && cfg["global_domain"].contains(component))
        return &cfg["global_domain"][component];
    return nullptr;
}

inline std::vector<Domain> parse_domains(const nlohmann::json& cfg, int voices,
                                        int event_count, int rhythm_base) {
    std::vector<Domain> result;
    for (int voice = 0; voice < voices; ++voice) {
        for (const std::string component : {"pitch", "rhythm"}) {
            const auto* domain = component_for_voice(cfg, voice, component);
            if (!domain || !domain->is_object() || !domain->contains("motives")) continue;
            Domain parsed;
            parsed.voice = voice;
            parsed.rhythm = component == "rhythm";
            validate_motives((*domain)["motives"], parsed.rhythm, event_count);
            for (const auto& motive : (*domain)["motives"]) {
                std::vector<int> values;
                for (const auto& value : motive) {
                    if (!parsed.rhythm) values.push_back(value.get<int>());
                    else {
                        const auto fraction = duration_fraction(value);
                        const long long ticks = static_cast<long long>(rhythm_base) * fraction.first;
                        if (rhythm_base <= 0 || ticks % fraction.second != 0 ||
                            ticks / fraction.second <= -std::numeric_limits<int>::max() ||
                            ticks / fraction.second > std::numeric_limits<int>::max())
                            throw std::runtime_error("Rhythm motive is not representable in rhythm_base");
                        values.push_back(static_cast<int>(ticks / fraction.second));
                    }
                }
                parsed.candidates.push_back(std::move(values));
            }
            result.push_back(std::move(parsed));
        }
    }
    return result;
}

}
#endif