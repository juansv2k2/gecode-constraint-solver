/**
 * @file harmonic_domain_parser.hh
 * @brief Shared "harmonic_domain" JSON parser, used by both the CLI and the
 * Max wrapper so r-cadence/r-repetition (which need HarmonicEntry's key and
 * scale_degree()) work identically in both binaries.
 *
 * Before this existed, the CLI parsed harmonic_domain into a full
 * HarmonicConfig (key/degree aware), while the Max wrapper had its own,
 * separate parser that only ever built a flat tick-indexed chord-class array
 * for the neural scorer — no key/degree information at all. r-cadence and
 * r-repetition silently did nothing in Max as a result: they were never
 * unrecognized/erroring, they simply had no harmony data to validate against.
 * This header is the single source of truth for both.
 */

#ifndef HARMONIC_DOMAIN_PARSER_HH
#define HARMONIC_DOMAIN_PARSER_HH

#include "musical_constraint_solver.hh"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace HarmonicDomainParser {

using HarmonicConfig = MusicalConstraintSolver::SolverConfig::HarmonicConfig;
using HarmonicEntry  = MusicalConstraintSolver::SolverConfig::HarmonicEntry;

// Parse a Roman-numeral degree token ("V7", "ii", "IV", "vii") into (scale
// degree 1-7, inferred quality 0/1/2). Case gives the default triad quality;
// a trailing '7' forces dom7. An explicit "quality" field on the entry always
// overrides this inferred value — see parse() below.
inline bool parse_roman_degree(const std::string& s, int& degree_out, int& quality_out) {
    if (s.empty()) return false;
    std::string body = s;
    bool has7 = (body.back() == '7');
    if (has7) body.pop_back();
    while (!body.empty() && !std::isalpha(static_cast<unsigned char>(body.back())))
        body.pop_back();
    if (body.empty()) return false;
    static const std::map<std::string, int> ROMAN = {
        {"i",1},{"ii",2},{"iii",3},{"iv",4},{"v",5},{"vi",6},{"vii",7}
    };
    const bool is_upper = std::isupper(static_cast<unsigned char>(body[0])) != 0;
    std::string lower = body;
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto it = ROMAN.find(lower);
    if (it == ROMAN.end()) return false;
    degree_out  = it->second;
    quality_out = has7 ? 2 : (is_upper ? 0 : 1);
    return true;
}

// Parses "harmonic_domain" out of `cfg` (the whole config JSON root) into `hd`,
// supporting both absolute (chord+quality) and functional (degree+key+mode)
// notation — the two are interchangeable inputs to one internal
// representation, not separate code paths downstream. "key"/"mode" forward-
// fill independently of "chord"/"degree", so a key stated once persists until
// a later entry changes it. Also builds the tick-indexed harmonic_state the
// neural scorer reads (rhythm_base/sequence_length must already be resolved).
// No-op (hd left default-constructed, hd.enabled stays false) if the config
// has no harmonic_domain key.
inline void parse(const nlohmann::json& cfg, HarmonicConfig& hd,
                   int sequence_length, int rhythm_base) {
    if (!cfg.contains("harmonic_domain") || !cfg["harmonic_domain"].is_array() ||
            cfg["harmonic_domain"].empty())
        return;

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
    static const std::vector<std::vector<int>> CHORD_TONES = {
        {0, 4, 7},        // major:  root, M3, P5
        {0, 3, 7},        // minor:  root, m3, P5
        {0, 4, 7, 10}     // dom7:   root, M3, P5, m7
    };
    static const int MAJOR_STEPS[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int MINOR_STEPS[7] = {0, 2, 3, 5, 7, 8, 10};

    hd.enabled = true;
    int current_key_tonic = -1;  // -1 = no key declared yet
    int current_key_mode  = 0;   // 0=major 1=minor

    for (const auto& entry : cfg["harmonic_domain"]) {
        HarmonicEntry he;
        he.beat_position = entry.value("beat", 0);

        if (entry.contains("key")) {
            auto kit = NOTE_MAP.find(entry.value("key", std::string("C")));
            current_key_tonic = (kit != NOTE_MAP.end()) ? kit->second : 0;
        }
        if (entry.contains("mode")) {
            std::string mode_name = entry.value("mode", std::string("major"));
            current_key_mode = (mode_name == "minor" || mode_name == "min") ? 1 : 0;
        }
        he.key_tonic = current_key_tonic;
        he.key_mode  = current_key_mode;

        if (entry.contains("degree")) {
            if (current_key_tonic < 0)
                throw std::runtime_error(
                    "harmonic_domain: entry uses 'degree' but no 'key' has been "
                    "declared yet (on this or an earlier entry)");
            int degree = 0, inferred_quality = 0;
            const std::string degree_str = entry.value("degree", std::string());
            if (!parse_roman_degree(degree_str, degree, inferred_quality))
                throw std::runtime_error(
                    "harmonic_domain: invalid 'degree' value '" + degree_str + "'");
            const int* steps = (current_key_mode == 0) ? MAJOR_STEPS : MINOR_STEPS;
            he.chord_root = (current_key_tonic + steps[degree - 1]) % 12;
            if (entry.contains("quality")) {
                auto qit = QUAL_MAP.find(entry.value("quality", std::string("major")));
                he.chord_quality = (qit != QUAL_MAP.end()) ? qit->second : inferred_quality;
            } else {
                he.chord_quality = inferred_quality;
            }
        } else {
            std::string chord_name = entry.value("chord", "C");
            auto nit = NOTE_MAP.find(chord_name);
            he.chord_root = (nit != NOTE_MAP.end()) ? nit->second : 0;

            std::string qual_name = entry.value("quality", "major");
            auto qit = QUAL_MAP.find(qual_name);
            he.chord_quality = (qit != QUAL_MAP.end()) ? qit->second : 0;
        }

        const auto& tones = CHORD_TONES[std::min(he.chord_quality, 2)];
        for (int t : tones)
            he.chord_tones.push_back((he.chord_root + t) % 12);

        hd.entries.push_back(he);
    }

    // Build tick-indexed harmonic_state so the neural scorer can resolve chord
    // by metric onset tick (computed live from rhythm_vars at score time).
    // beat_position is in quarter-note beats -> convert to ticks via rhythm_base.
    const int quarter_ticks = std::max(1, rhythm_base / 4);
    std::sort(hd.entries.begin(), hd.entries.end(),
              [](const auto& a, const auto& b) { return a.beat_position < b.beat_position; });
    const int last_beat_tick = hd.entries.empty() ? 0
                             : hd.entries.back().beat_position * quarter_ticks;
    const int total_ticks = last_beat_tick + sequence_length * quarter_ticks + 1;
    hd.harmonic_state.assign(total_ticks, -1);
    for (int i = 0; i < (int)hd.entries.size(); ++i) {
        const auto& e = hd.entries[i];
        const int start_t = e.beat_position * quarter_ticks;
        const int end_t = (i + 1 < (int)hd.entries.size())
                          ? hd.entries[i + 1].beat_position * quarter_ticks
                          : total_ticks;
        const int chord_cls = e.chord_root * 3 + std::min(e.chord_quality, 2);
        for (int t = start_t; t < end_t && t < total_ticks; ++t)
            hd.harmonic_state[t] = chord_cls;
    }
}

}  // namespace HarmonicDomainParser

#endif  // HARMONIC_DOMAIN_PARSER_HH
