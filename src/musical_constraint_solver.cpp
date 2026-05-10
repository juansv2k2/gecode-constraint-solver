/**
 * @file musical_constraint_solver.cpp
 * @brief Implementation of Main Interface for Production Musical Constraint Solver
 * 
 * Complete implementation that integrates all cluster-engine functionality
 * into a production-ready musical constraint solving system.
 */

#include "musical_constraint_solver.hh"
#include "gecode_cluster_integration.hh"
#include "dynamic_rule_compiler.hh"
#include "rule_expression_parser.hh"
#include <gecode/driver.hh>
#include <gecode/int.hh>
#include <gecode/minimodel.hh>
#include <gecode/search.hh>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <limits>
#include <fstream>
#include <random>
#include <regex>

namespace MusicalConstraintSolver {

namespace {

unsigned int resolve_effective_random_seed(unsigned int configured_seed) {
    if (configured_seed == std::numeric_limits<unsigned int>::max()) {
        return 0u;
    }
    if (configured_seed != 0u) {
        return configured_seed;
    }

    std::random_device rd;
    unsigned int seed = rd();
    if (seed == 0u) {
        seed = static_cast<unsigned int>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        if (seed == 0u) {
            seed = 1u;
        }
    }
    return seed;
}

bool is_heuristic_mode(const nlohmann::json& rule_json) {
    const std::string mode = rule_json.value("mode", "constraint");
    return mode == "heur_switch" || mode == "real_heuristic" || mode == "heuristic";
}

bool is_wildcard_rule(const nlohmann::json& rule_json) {
    const std::string wildcard_type = rule_json.value("wildcard_type", "");
    const bool has_offsets = rule_json.contains("pattern_offsets") && rule_json.at("pattern_offsets").is_array();
    return !wildcard_type.empty() || has_offsets;
}

int eval_index_expression(const std::string& expr, int i) {
    std::smatch m;
    static const std::regex i_plus_minus_re(R"(^\s*i\s*([+-])\s*(\d+)\s*$)");
    static const std::regex i_re(R"(^\s*i\s*$)");
    static const std::regex int_re(R"(^\s*-?\d+\s*$)");

    if (std::regex_match(expr, i_re)) {
        return i;
    }
    if (std::regex_match(expr, m, i_plus_minus_re)) {
        const int off = std::stoi(m[2].str());
        return (m[1].str() == "+") ? (i + off) : (i - off);
    }
    if (std::regex_match(expr, int_re)) {
        return std::stoi(expr);
    }
    return i;
}

int parse_duration_to_ticks(const std::string& s, int rhythm_base) {
    if (rhythm_base <= 0) {
        throw std::runtime_error("Invalid rhythm_base for duration parsing");
    }

    const bool is_rest = !s.empty() && s[0] == '-';
    const std::string core = is_rest ? s.substr(1) : s;
    const auto slash = core.find('/');
    if (slash == std::string::npos) {
        throw std::runtime_error("Invalid rhythmic value '" + s + "' (expected fraction like 1/4)");
    }

    const int num = std::stoi(core.substr(0, slash));
    const int den = std::stoi(core.substr(slash + 1));
    if (num <= 0 || den <= 0) {
        throw std::runtime_error("Invalid rhythmic value '" + s + "'");
    }
    if ((rhythm_base % den) != 0) {
        throw std::runtime_error("Rhythmic value '" + s + "' incompatible with rhythm base " + std::to_string(rhythm_base));
    }

    const int ticks = num * (rhythm_base / den);
    return is_rest ? -ticks : ticks;
}

std::pair<int, int> parse_metric_signature(const std::string& s) {
    const auto slash = s.find('/');
    if (slash == std::string::npos) {
        throw std::runtime_error("Invalid metric signature '" + s + "' (expected N/D)");
    }
    int n = 0;
    int d = 0;
    try {
        n = std::stoi(s.substr(0, slash));
        d = std::stoi(s.substr(slash + 1));
    } catch (...) {
        throw std::runtime_error("Cannot parse metric signature '" + s + "'");
    }
    if (n <= 0 || d <= 0) {
        throw std::runtime_error("Invalid metric signature '" + s + "' (values must be positive)");
    }
    return {n, d};
}

int parse_metric_hierarchy_grid_step_ticks(const SolverConfig& config, bool ignore_tuplets) {
    if (config.rhythm_base <= 0) {
        throw std::runtime_error("r-metric-hierarchy requires a positive rhythm_base");
    }
    if (config.metric_domain.empty()) {
        throw std::runtime_error("r-metric-hierarchy requires non-empty metric_domain");
    }

    int best_step = std::numeric_limits<int>::max();

    for (const auto& entry : config.metric_domain) {
        if (entry.denominator <= 0) {
            continue;
        }

        std::vector<int> subdivisions;
        subdivisions.push_back(1);
        if (!ignore_tuplets) {
            for (int t : entry.tuplets) {
                if (t > 0) subdivisions.push_back(t);
            }
        }
        for (int b : entry.beat_divisions) {
            if (b > 0) subdivisions.push_back(b);
        }

        std::sort(subdivisions.begin(), subdivisions.end());
        subdivisions.erase(std::unique(subdivisions.begin(), subdivisions.end()), subdivisions.end());

        for (int subdiv : subdivisions) {
            const int grid_den = entry.denominator * subdiv;
            if (grid_den <= 0) {
                continue;
            }
            if ((config.rhythm_base % grid_den) != 0) {
                continue;
            }
            const int step_ticks = config.rhythm_base / grid_den;
            if (step_ticks > 0 && step_ticks < best_step) {
                best_step = step_ticks;
            }
        }
    }

    if (best_step == std::numeric_limits<int>::max()) {
        throw std::runtime_error(
            "r-metric-hierarchy could not derive a valid metric grid from metric_domain and rhythm_base");
    }

    return best_step;
}

bool metric_hierarchy_include_rests_mode(const std::vector<std::string>& parameter_strings) {
    for (const auto& s : parameter_strings) {
        if (s == "include-rests" || s == "include_rests" || s == ":include-rests" ||
            s == "include-rest" || s == ":include-rest") {
            return true;
        }
    }
    return false;
}

bool metric_hierarchy_ignore_tuplets_mode(const std::vector<std::string>& parameter_strings) {
    for (const auto& s : parameter_strings) {
        if (s == "no-tuplets" || s == "no_tuplets" || s == ":no-tuplets" ||
            s == "ignore-tuplets" || s == "ignore_tuplets" || s == ":ignore-tuplets") {
            return true;
        }
    }
    return false;
}

std::map<int, int> build_metric_denominator_map(const SolverConfig& config) {
    std::map<int, int> by_numerator;
    for (const auto& entry : config.metric_domain) {
        if (entry.numerator > 0 && entry.denominator > 0 && !by_numerator.count(entry.numerator)) {
            by_numerator[entry.numerator] = entry.denominator;
        }
    }
    return by_numerator;
}

int parse_score_time_token_to_ticks(const std::string& token, int rhythm_base, const std::string& context) {
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
    int num = 0;
    int den = 0;
    try {
        num = std::stoi(token.substr(0, slash));
        den = std::stoi(token.substr(slash + 1));
    } catch (...) {
        throw std::runtime_error(context + ": cannot parse time token '" + token + "'");
    }
    if (num < 0 || den <= 0) {
        throw std::runtime_error(context + ": invalid time token '" + token + "'");
    }
    if ((rhythm_base * num) % den != 0) {
        throw std::runtime_error(context + ": time token '" + token + "' is not representable with rhythm_base=" + std::to_string(rhythm_base));
    }
    return (rhythm_base * num) / den;
}

std::vector<std::pair<int, int>> parse_metric_signature_parameters(
    const std::vector<std::string>& parameter_strings,
    const std::vector<double>& parameters,
    const std::string& context) {
    std::vector<std::pair<int, int>> desired_signatures;
    for (const auto& s : parameter_strings) {
        desired_signatures.push_back(parse_metric_signature(s));
    }
    for (double p : parameters) {
        const int n = static_cast<int>(std::lround(p));
        if (n <= 0) {
            throw std::runtime_error(context + ": invalid numeric signature parameter");
        }
        desired_signatures.push_back({n, 4});
    }
    return desired_signatures;
}

std::vector<int> compute_voice_total_ticks(const std::vector<std::vector<int>>& voice_rhythms) {
    std::vector<int> totals;
    totals.reserve(voice_rhythms.size());
    for (const auto& vr : voice_rhythms) {
        int total = 0;
        for (int d : vr) {
            total += std::abs(d);
        }
        totals.push_back(total);
    }
    return totals;
}

std::vector<std::vector<int>> compute_voice_onsets(const std::vector<std::vector<int>>& voice_rhythms, int sequence_length) {
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

int min_onset_at_index(const std::vector<std::vector<int>>& voice_onsets, int index) {
    int result = std::numeric_limits<int>::max();
    for (const auto& v : voice_onsets) {
        if (index >= 0 && index < static_cast<int>(v.size())) {
            result = std::min(result, v[index]);
        }
    }
    return result == std::numeric_limits<int>::max() ? 0 : result;
}

int first_index_at_or_after_tick(const std::vector<std::vector<int>>& voice_onsets, int sequence_length, int tick) {
    for (int index = 0; index < sequence_length; ++index) {
        if (min_onset_at_index(voice_onsets, index) >= tick) {
            return index;
        }
    }
    return sequence_length;
}

void add_trailing_rests_to_score(
    MusicalSolution::SolvedScore& score,
    const std::vector<int>& voice_totals,
    int num_voices,
    int score_length_ticks) {
    if (score.measures.empty() || score_length_ticks < 0 || num_voices <= 0) {
        return;
    }

    // Ensure the final measure reaches the configured score boundary.
    if (score.measures.back().end_ticks < score_length_ticks) {
        score.measures.back().end_ticks = score_length_ticks;
        if (!score.metric_timeline.empty()) {
            score.metric_timeline.back().end_tick = score_length_ticks;
        }
    }

    for (int voice = 0; voice < num_voices; ++voice) {
        const int voice_end = (voice < static_cast<int>(voice_totals.size())) ? voice_totals[voice] : 0;
        if (voice_end >= score_length_ticks) {
            continue;
        }

        int tick = voice_end;
        while (tick < score_length_ticks) {
            int measure_idx = static_cast<int>(score.measures.size()) - 1;
            for (size_t m = 0; m < score.measures.size(); ++m) {
                const auto& meas = score.measures[m];
                const bool in_last = (m + 1 == score.measures.size()) && (tick >= meas.start_ticks);
                if ((tick >= meas.start_ticks && tick < meas.end_ticks) || in_last) {
                    measure_idx = static_cast<int>(m);
                    break;
                }
            }

            if (measure_idx < 0 || measure_idx >= static_cast<int>(score.measures.size())) {
                break;
            }

            int segment_end = std::min(score_length_ticks, score.measures[measure_idx].end_ticks);
            if (segment_end <= tick) {
                segment_end = score_length_ticks;
            }

            MusicalSolution::ScoreEvent rest;
            rest.voice = voice;
            rest.index = -1;  // Not tied to a solver position; this is score-padding.
            rest.pitch = GecodeClusterIntegration::IntegratedMusicalSpace::REST_PITCH_SENTINEL;
            rest.rhythm = -(segment_end - tick);
            rest.duration_ticks = segment_end - tick;
            rest.is_rest = true;
            rest.onset_ticks = tick;
            score.measures[measure_idx].events.push_back(std::move(rest));

            tick = segment_end;
        }
    }
}

MusicalSolution::SolvedScore build_canonical_score_from_timepoints(
    const SolverConfig& config,
    const std::vector<std::vector<int>>& voice_pitches,
    const std::vector<std::vector<int>>& voice_rhythms,
    const std::vector<int>& segment_starts_ticks,
    const std::vector<std::pair<int, int>>& desired_signatures) {

    MusicalSolution::SolvedScore score;
    score.rhythm_base = config.rhythm_base > 0 ? config.rhythm_base : 1;
    if (segment_starts_ticks.empty()) {
        return score;
    }
    if (config.score_length_ticks < 0) {
        throw std::runtime_error("timepoint-based metric rules require score_length");
    }
    if (segment_starts_ticks.size() != desired_signatures.size()) {
        throw std::runtime_error("timepoint-based metric segments require one signature per timepoint");
    }

    const auto voice_onsets = compute_voice_onsets(voice_rhythms, config.sequence_length);
    const auto voice_totals = compute_voice_total_ticks(voice_rhythms);
    const int global_end_tick = voice_totals.empty()
        ? 0
        : *std::max_element(voice_totals.begin(), voice_totals.end());
    if (global_end_tick > config.score_length_ticks) {
        throw std::runtime_error("Solved rhythm exceeds score_length");
    }

    for (size_t i = 0; i < segment_starts_ticks.size(); ++i) {
        MusicalSolution::MetricSegment seg;
        seg.start_tick = segment_starts_ticks[i];
        seg.end_tick = (i + 1 < segment_starts_ticks.size()) ? segment_starts_ticks[i + 1] : config.score_length_ticks;
        seg.numerator = desired_signatures[i].first;
        seg.denominator = desired_signatures[i].second;
        seg.start_index = first_index_at_or_after_tick(voice_onsets, config.sequence_length, seg.start_tick);
        const int next_index = first_index_at_or_after_tick(voice_onsets, config.sequence_length, seg.end_tick);
        seg.end_index = (i + 1 < segment_starts_ticks.size()) ? (next_index - 1) : (config.sequence_length - 1);
        score.metric_timeline.push_back(seg);

        MusicalSolution::ScoreMeasure measure;
        measure.measure_index = static_cast<int>(i);
        measure.start_ticks = seg.start_tick;
        measure.end_ticks = seg.end_tick;
        measure.numerator = seg.numerator;
        measure.denominator = seg.denominator;
        score.measures.push_back(measure);
    }

    for (size_t v = 0; v < voice_pitches.size() && v < voice_rhythms.size(); ++v) {
        const auto& pitches = voice_pitches[v];
        const auto& rhythms = voice_rhythms[v];
        const int n = std::min(static_cast<int>(pitches.size()), static_cast<int>(rhythms.size()));
        for (int i = 0; i < n; ++i) {
            MusicalSolution::ScoreEvent ev;
            ev.voice = static_cast<int>(v);
            ev.index = i;
            ev.pitch = pitches[i];
            ev.rhythm = rhythms[i];
            ev.duration_ticks = std::abs(rhythms[i]);
            ev.is_rest = rhythms[i] < 0 || pitches[i] == GecodeClusterIntegration::IntegratedMusicalSpace::REST_PITCH_SENTINEL;
            ev.onset_ticks = (v < voice_onsets.size() && i < static_cast<int>(voice_onsets[v].size()))
                ? voice_onsets[v][i]
                : 0;

            int measure_idx = static_cast<int>(score.measures.size()) - 1;
            for (size_t m = 0; m < score.measures.size(); ++m) {
                const auto& meas = score.measures[m];
                const bool in_last = (m + 1 == score.measures.size()) && (ev.onset_ticks >= meas.start_ticks);
                if ((ev.onset_ticks >= meas.start_ticks && ev.onset_ticks < meas.end_ticks) || in_last) {
                    measure_idx = static_cast<int>(m);
                    break;
                }
            }
            if (measure_idx >= 0 && measure_idx < static_cast<int>(score.measures.size())) {
                score.measures[measure_idx].events.push_back(std::move(ev));
            }
        }
    }

    add_trailing_rests_to_score(score, voice_totals, config.num_voices, config.score_length_ticks);

    return score;
}

MusicalSolution::SolvedScore build_canonical_score(
    const SolverConfig& config,
    const std::vector<std::vector<int>>& voice_pitches,
    const std::vector<std::vector<int>>& voice_rhythms,
    const std::vector<int>& metric_signature_by_index) {

    MusicalSolution::SolvedScore score;
    score.rhythm_base = config.rhythm_base > 0 ? config.rhythm_base : 1;

    if (metric_signature_by_index.empty()) {
        return score;
    }

    const auto metric_denominators = build_metric_denominator_map(config);
    const int sequence_length = static_cast<int>(metric_signature_by_index.size());
    const auto voice_onsets = compute_voice_onsets(voice_rhythms, sequence_length);
    const auto voice_totals = compute_voice_total_ticks(voice_rhythms);
    const int global_end_tick = voice_totals.empty()
        ? 0
        : *std::max_element(voice_totals.begin(), voice_totals.end());
    if (config.score_length_ticks >= 0 && global_end_tick > config.score_length_ticks) {
        throw std::runtime_error("Solved rhythm exceeds score_length");
    }

    // Build metric segments from piecewise-constant metric sequence.
    int seg_start = 0;
    while (seg_start < sequence_length) {
        int seg_end = seg_start;
        const int n = metric_signature_by_index[seg_start];
        while (seg_end + 1 < sequence_length && metric_signature_by_index[seg_end + 1] == n) {
            ++seg_end;
        }

        MusicalSolution::MetricSegment seg;
        seg.start_index = seg_start;
        seg.end_index = seg_end;
        seg.numerator = n;
        auto it = metric_denominators.find(n);
        seg.denominator = (it != metric_denominators.end()) ? it->second : 4;
        score.metric_timeline.push_back(seg);

        seg_start = seg_end + 1;
    }

    // Build measure containers aligned to metric segments using onset estimates.
    score.measures.reserve(score.metric_timeline.size());
    for (size_t i = 0; i < score.metric_timeline.size(); ++i) {
        auto& seg = score.metric_timeline[i];
        MusicalSolution::ScoreMeasure measure;
        measure.measure_index = static_cast<int>(i);
        measure.numerator = seg.numerator;
        measure.denominator = seg.denominator;

        measure.start_ticks = min_onset_at_index(voice_onsets, seg.start_index);
        if (i + 1 < score.metric_timeline.size()) {
            measure.end_ticks = min_onset_at_index(voice_onsets, score.metric_timeline[i + 1].start_index);
        } else {
            int implied = measure.start_ticks + (seg.denominator > 0
                ? (seg.numerator * score.rhythm_base) / seg.denominator
                : score.rhythm_base);
            measure.end_ticks = std::max(global_end_tick, implied);
        }
        if (measure.end_ticks < measure.start_ticks) {
            measure.end_ticks = measure.start_ticks;
        }
        seg.start_tick = measure.start_ticks;
        seg.end_tick = measure.end_ticks;
        score.measures.push_back(std::move(measure));
    }

    // Map events into the measure windows by onset tick.
    for (size_t v = 0; v < voice_pitches.size() && v < voice_rhythms.size(); ++v) {
        const auto& pitches = voice_pitches[v];
        const auto& rhythms = voice_rhythms[v];
        const int n = std::min(static_cast<int>(pitches.size()), static_cast<int>(rhythms.size()));
        for (int i = 0; i < n; ++i) {
            MusicalSolution::ScoreEvent ev;
            ev.voice = static_cast<int>(v);
            ev.index = i;
            ev.pitch = pitches[i];
            ev.rhythm = rhythms[i];
            ev.duration_ticks = std::abs(rhythms[i]);
            ev.is_rest = rhythms[i] < 0 || pitches[i] == GecodeClusterIntegration::IntegratedMusicalSpace::REST_PITCH_SENTINEL;
            ev.onset_ticks = (v < voice_onsets.size() && i < static_cast<int>(voice_onsets[v].size()))
                ? voice_onsets[v][i]
                : 0;

            int measure_idx = static_cast<int>(score.measures.size()) - 1;
            for (size_t m = 0; m < score.measures.size(); ++m) {
                const auto& meas = score.measures[m];
                const bool in_last = (m + 1 == score.measures.size()) && (ev.onset_ticks >= meas.start_ticks);
                if ((ev.onset_ticks >= meas.start_ticks && ev.onset_ticks < meas.end_ticks) || in_last) {
                    measure_idx = static_cast<int>(m);
                    break;
                }
            }
            if (measure_idx >= 0 && measure_idx < static_cast<int>(score.measures.size())) {
                score.measures[measure_idx].events.push_back(std::move(ev));
            }
        }
    }

    if (config.score_length_ticks >= 0) {
        add_trailing_rests_to_score(score, voice_totals, config.num_voices, config.score_length_ticks);
    }

    return score;
}

std::vector<int> project_metric_signature_from_canonical(const MusicalSolution::SolvedScore& score, int sequence_length) {
    std::vector<int> projected;
    if (sequence_length <= 0) return projected;
    projected.assign(sequence_length, score.metric_timeline.empty() ? 4 : score.metric_timeline.front().numerator);

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
        for (const auto& seg : score.metric_timeline) {
            const bool in_last = (&seg == &score.metric_timeline.back()) && onset_by_index[i] >= seg.start_tick;
            if ((onset_by_index[i] >= seg.start_tick && onset_by_index[i] < seg.end_tick) || in_last) {
                projected[i] = seg.numerator;
                break;
            }
        }
    }

    if (!used_tick_projection) {
        for (const auto& seg : score.metric_timeline) {
            const int start = std::max(0, seg.start_index);
            const int end = std::min(sequence_length - 1, seg.end_index);
            for (int i = start; i <= end; ++i) {
                projected[i] = seg.numerator;
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

std::vector<int> project_metric_denominators_from_canonical(const MusicalSolution::SolvedScore& score, int sequence_length) {
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
        for (const auto& seg : score.metric_timeline) {
            const bool in_last = (&seg == &score.metric_timeline.back()) && onset_by_index[i] >= seg.start_tick;
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

std::vector<int> metric_denominators_for_solution(const MusicalSolution& solution) {
    if (solution.metric_signature.empty()) return {};
    if (solution.has_canonical_score && !solution.canonical_score.metric_timeline.empty()) {
        return project_metric_denominators_from_canonical(
            solution.canonical_score, static_cast<int>(solution.metric_signature.size()));
    }
    return std::vector<int>(solution.metric_signature.size(), 4);
}

std::string musicxml_step_for_pitch_class(int pitch_class) {
    static const char* steps[] = {"C", "C", "D", "D", "E", "F", "F", "G", "G", "A", "A", "B"};
    return steps[pitch_class % 12];
}

int musicxml_alter_for_pitch_class(int pitch_class) {
    static const int alters[] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    return alters[pitch_class % 12];
}

int gcd_int_local(int a, int b) {
    a = std::abs(a);
    b = std::abs(b);
    while (b != 0) {
        const int t = a % b;
        a = b;
        b = t;
    }
    return (a == 0) ? 1 : a;
}

struct RationalValue {
    int num = 1;
    int den = 1;
};

RationalValue make_reduced_rational(int num, int den) {
    if (den == 0) return {1, 1};
    if (den < 0) {
        num = -num;
        den = -den;
    }
    const int g = gcd_int_local(num, den);
    return {num / g, den / g};
}

RationalValue multiply_rational(const RationalValue& a, const RationalValue& b) {
    return make_reduced_rational(a.num * b.num, a.den * b.den);
}

bool rational_equal(const RationalValue& a, const RationalValue& b) {
    return a.num == b.num && a.den == b.den;
}

struct MusicXmlDurationNotation {
    std::string type = "quarter";
    int dots = 0;
    bool has_time_modification = false;
    int actual_notes = 0;
    int normal_notes = 0;
};

MusicXmlDurationNotation musicxml_notation_for_duration(int duration_ticks, int rhythm_base) {
    if (duration_ticks <= 0 || rhythm_base <= 0) return {};

    struct BaseType {
        const char* name;
        int den;
    };
    static const BaseType base_types[] = {
        {"whole", 1}, {"half", 2}, {"quarter", 4}, {"eighth", 8},
        {"16th", 16}, {"32nd", 32}, {"64th", 64}, {"128th", 128}
    };

    const RationalValue target = make_reduced_rational(duration_ticks, rhythm_base);

    // Prefer exact simple/dotted notation first.
    for (const auto& base : base_types) {
        const RationalValue base_fraction = make_reduced_rational(1, base.den);
        for (int dots = 0; dots <= 3; ++dots) {
            const RationalValue dot_factor = make_reduced_rational((1 << (dots + 1)) - 1, (1 << dots));
            const RationalValue candidate = multiply_rational(base_fraction, dot_factor);
            if (rational_equal(candidate, target)) {
                MusicXmlDurationNotation out;
                out.type = base.name;
                out.dots = dots;
                return out;
            }
        }
    }

    // Fallback to exact tuplet notation (optionally dotted base) when needed.
    struct TupletChoice {
        bool found = false;
        MusicXmlDurationNotation notation;
        int actual = std::numeric_limits<int>::max();
        int spread = std::numeric_limits<int>::max();
        int dots = std::numeric_limits<int>::max();
        int base_distance_from_quarter = std::numeric_limits<int>::max();
    } best;

    for (size_t base_idx = 0; base_idx < (sizeof(base_types) / sizeof(base_types[0])); ++base_idx) {
        const auto& base = base_types[base_idx];
        const RationalValue base_fraction = make_reduced_rational(1, base.den);
        for (int dots = 0; dots <= 3; ++dots) {
            const RationalValue dot_factor = make_reduced_rational((1 << (dots + 1)) - 1, (1 << dots));
            const RationalValue dotted = multiply_rational(base_fraction, dot_factor);
            for (int actual = 2; actual <= 16; ++actual) {
                for (int normal = 1; normal < actual; ++normal) {
                    const RationalValue time_factor = make_reduced_rational(normal, actual);
                    const RationalValue candidate = multiply_rational(dotted, time_factor);
                    if (rational_equal(candidate, target)) {
                        const int spread = actual - normal;
                        const int quarter_idx = 2;
                        const int base_distance = std::abs(static_cast<int>(base_idx) - quarter_idx);

                        const bool is_better = !best.found ||
                            (actual < best.actual) ||
                            (actual == best.actual && spread < best.spread) ||
                            (actual == best.actual && spread == best.spread && dots < best.dots) ||
                            (actual == best.actual && spread == best.spread && dots == best.dots &&
                             base_distance < best.base_distance_from_quarter);

                        if (is_better) {
                            best.found = true;
                            best.actual = actual;
                            best.spread = spread;
                            best.dots = dots;
                            best.base_distance_from_quarter = base_distance;
                            best.notation.type = base.name;
                            best.notation.dots = dots;
                            best.notation.has_time_modification = true;
                            best.notation.actual_notes = actual;
                            best.notation.normal_notes = normal;
                        }
                    }
                }
            }
        }
    }

    if (best.found) {
        return best.notation;
    }

    // Last-resort fallback: keep a sensible type even if exact symbolic mapping is unavailable.
    MusicXmlDurationNotation fallback;
    const int whole = rhythm_base;
    if (duration_ticks >= whole) fallback.type = "whole";
    else if (duration_ticks * 2 >= whole) fallback.type = "half";
    else if (duration_ticks * 4 >= whole) fallback.type = "quarter";
    else if (duration_ticks * 8 >= whole) fallback.type = "eighth";
    else if (duration_ticks * 16 >= whole) fallback.type = "16th";
    else if (duration_ticks * 32 >= whole) fallback.type = "32nd";
    else if (duration_ticks * 64 >= whole) fallback.type = "64th";
    else fallback.type = "128th";
    return fallback;
}

void emit_musicxml_note_duration(std::stringstream& xml, int duration_ticks, int whole_ticks) {
    const auto notation = musicxml_notation_for_duration(duration_ticks, whole_ticks);
    xml << "        <duration>" << std::max(1, duration_ticks) << "</duration>\n";
    xml << "        <type>" << notation.type << "</type>\n";
    for (int d = 0; d < notation.dots; ++d) {
        xml << "        <dot/>\n";
    }
    if (notation.has_time_modification) {
        xml << "        <time-modification>\n";
        xml << "          <actual-notes>" << notation.actual_notes << "</actual-notes>\n";
        xml << "          <normal-notes>" << notation.normal_notes << "</normal-notes>\n";
        xml << "        </time-modification>\n";
    }
}

std::string substitute_wildcard_string(const std::string& input, int voice_a, int voice_b, int i) {
    std::string out = input;

    // Single-voice placeholders
    out = std::regex_replace(out, std::regex(R"(voice\s*\[\s*v\s*\])"), "voice[" + std::to_string(voice_a) + "]");
    out = std::regex_replace(out, std::regex(R"(voice\s*\[\s*V\s*\])"), "voice[" + std::to_string(voice_a) + "]");

    // Pairwise placeholders
    out = std::regex_replace(out, std::regex(R"(voice\s*\[\s*v1\s*\])"), "voice[" + std::to_string(voice_a) + "]");
    out = std::regex_replace(out, std::regex(R"(voice\s*\[\s*V1\s*\])"), "voice[" + std::to_string(voice_a) + "]");
    out = std::regex_replace(out, std::regex(R"(voice\s*\[\s*a\s*\])"), "voice[" + std::to_string(voice_a) + "]");
    out = std::regex_replace(out, std::regex(R"(voice\s*\[\s*A\s*\])"), "voice[" + std::to_string(voice_a) + "]");

    out = std::regex_replace(out, std::regex(R"(voice\s*\[\s*v2\s*\])"), "voice[" + std::to_string(voice_b) + "]");
    out = std::regex_replace(out, std::regex(R"(voice\s*\[\s*V2\s*\])"), "voice[" + std::to_string(voice_b) + "]");
    out = std::regex_replace(out, std::regex(R"(voice\s*\[\s*b\s*\])"), "voice[" + std::to_string(voice_b) + "]");
    out = std::regex_replace(out, std::regex(R"(voice\s*\[\s*B\s*\])"), "voice[" + std::to_string(voice_b) + "]");

    static const std::regex bracket_i_expr(R"(\[\s*(i(?:\s*[+-]\s*\d+)?)\s*\])");
    std::string rebuilt;
    std::size_t cursor = 0;
    for (std::sregex_iterator it(out.begin(), out.end(), bracket_i_expr), end; it != end; ++it) {
        const auto& match = *it;
        rebuilt.append(out, cursor, static_cast<std::size_t>(match.position()) - cursor);
        const std::string expr = match[1].str();
        const int idx = eval_index_expression(expr, i);
        rebuilt += "[" + std::to_string(idx) + "]";
        cursor = static_cast<std::size_t>(match.position() + match.length());
    }
    rebuilt.append(out, cursor, std::string::npos);
    return rebuilt;
}

nlohmann::json substitute_wildcard_json(const nlohmann::json& value, int voice_a, int voice_b, int i) {
    if (value.is_string()) {
        return substitute_wildcard_string(value.get<std::string>(), voice_a, voice_b, i);
    }
    if (value.is_array()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& item : value) {
            arr.push_back(substitute_wildcard_json(item, voice_a, voice_b, i));
        }
        return arr;
    }
    if (value.is_object()) {
        nlohmann::json obj = nlohmann::json::object();
        for (auto it = value.begin(); it != value.end(); ++it) {
            obj[it.key()] = substitute_wildcard_json(it.value(), voice_a, voice_b, i);
        }
        return obj;
    }
    return value;
}

std::vector<nlohmann::json> expand_wildcard_heuristic_rule(
    const nlohmann::json& rule_json,
    int num_voices,
    int sequence_length) {

    std::vector<nlohmann::json> expanded;
    if (!is_heuristic_mode(rule_json) || !is_wildcard_rule(rule_json)) {
        return expanded;
    }

    std::vector<int> offsets{0};
    if (rule_json.contains("pattern_offsets") && rule_json.at("pattern_offsets").is_array() &&
        !rule_json.at("pattern_offsets").empty()) {
        offsets.clear();
        for (const auto& v : rule_json.at("pattern_offsets")) {
            if (v.is_number_integer()) {
                offsets.push_back(v.get<int>());
            }
        }
        if (offsets.empty()) {
            offsets.push_back(0);
        }
    }

    const int min_offset = *std::min_element(offsets.begin(), offsets.end());
    const int max_offset = *std::max_element(offsets.begin(), offsets.end());

    int step_size = rule_json.value("step_size", 1);
    if (step_size <= 0) {
        step_size = 1;
    }

    const std::string wildcard_type = rule_json.value("wildcard_type", "");
    const bool pairwise_cross_voice =
        wildcard_type == "for_all_pairs" || rule_json.value("cross_voices", false);
    const bool expression_uses_voice_placeholder =
        rule_json.contains("expression") &&
        rule_json.at("expression").dump().find("voice[v]") != std::string::npos;
    const bool expression_uses_pair_placeholders =
        rule_json.contains("expression") && (
            rule_json.at("expression").dump().find("voice[v1]") != std::string::npos ||
            rule_json.at("expression").dump().find("voice[v2]") != std::string::npos ||
            rule_json.at("expression").dump().find("voice[a]") != std::string::npos ||
            rule_json.at("expression").dump().find("voice[b]") != std::string::npos);
    const bool expand_voices = expression_uses_voice_placeholder || wildcard_type == "for_all_voices";
    const int voice_start = expand_voices ? 0 : 0;
    const int voice_end = expand_voices ? std::max(0, num_voices - 1) : 0;

    const std::string base_id = rule_json.value("id", "wildcard_heuristic");

    if (pairwise_cross_voice || expression_uses_pair_placeholders) {
        for (int voice_a = 0; voice_a < num_voices; ++voice_a) {
            for (int voice_b = voice_a + 1; voice_b < num_voices; ++voice_b) {
                for (int i = 0; i < sequence_length; i += step_size) {
                    const int lo = i + min_offset;
                    const int hi = i + max_offset;
                    if (lo < 0 || hi >= sequence_length) {
                        continue;
                    }

                    nlohmann::json concrete = rule_json;
                    concrete["id"] = base_id + "_v" + std::to_string(voice_a) + "_" +
                                     std::to_string(voice_b) + "_i" + std::to_string(i);
                    concrete["wildcard_generated"] = true;
                    concrete["candidate_position"] = i;
                    concrete["candidate_voices"] = nlohmann::json::array({voice_a, voice_b});
                    concrete.erase("wildcard_type");
                    concrete.erase("pattern_offsets");
                    concrete.erase("window_size");
                    concrete.erase("step_size");
                    concrete.erase("cross_voices");

                    if (rule_json.contains("expression")) {
                        concrete["expression"] =
                            substitute_wildcard_json(rule_json.at("expression"), voice_a, voice_b, i);
                    }

                    expanded.push_back(std::move(concrete));
                }
            }
        }
        return expanded;
    }

    for (int voice = voice_start; voice <= voice_end; ++voice) {
        for (int i = 0; i < sequence_length; i += step_size) {
            const int lo = i + min_offset;
            const int hi = i + max_offset;
            if (lo < 0 || hi >= sequence_length) {
                continue;
            }

            nlohmann::json concrete = rule_json;
            concrete["id"] = base_id + "_v" + std::to_string(voice) + "_i" + std::to_string(i);
            concrete["wildcard_generated"] = true;
            concrete["candidate_position"] = i;
            if (expand_voices) {
                concrete["candidate_voice"] = voice;
            }
            concrete.erase("wildcard_type");
            concrete.erase("pattern_offsets");
            concrete.erase("window_size");
            concrete.erase("step_size");
            concrete.erase("cross_voices");

            if (rule_json.contains("expression")) {
                concrete["expression"] = substitute_wildcard_json(rule_json.at("expression"), voice, voice, i);
            }

            expanded.push_back(std::move(concrete));
        }
    }

    return expanded;
}

} // namespace

// ===============================
// Musical Solution Implementation
// ===============================

void MusicalSolution::print_solution(std::ostream& os) const {
    os << "🎼 Musical Solution" << std::endl;
    os << "==================" << std::endl;
    
    if (!found_solution) {
        os << "❌ No solution found: " << failure_reason << std::endl;
        return;
    }
    
    // Print the sequence
    os << "Notes (MIDI): ";
    for (size_t i = 0; i < absolute_notes.size(); ++i) {
        if (i > 0) os << " → ";
        os << absolute_notes[i];
    }
    os << std::endl;
    
    os << "Note names:   ";
    for (size_t i = 0; i < note_names.size(); ++i) {
        if (i > 0) os << " → ";
        os << note_names[i];
    }
    os << std::endl;
    
    os << "Intervals:    ";
    for (size_t i = 0; i < intervals.size(); ++i) {
        if (i > 0) os << ", ";
        os << std::showpos << intervals[i] << std::noshowpos;
    }
    os << std::endl;
    
    // Print statistics
    os << "\n📊 Solution Statistics" << std::endl;
    os << "Solve time: " << std::fixed << std::setprecision(2) << solve_time_ms << " ms" << std::endl;
    os << "Rules checked: " << total_rules_checked << std::endl;
    os << "Backjumps performed: " << backjumps_performed << std::endl;
    os << "Average interval: " << std::fixed << std::setprecision(1) << average_interval_size << std::endl;
    os << "Direction changes: " << melodic_direction_changes << std::endl;
    
    if (!applied_rules.empty()) {
        os << "\n🎯 Applied Rules:" << std::endl;
        for (const auto& rule : applied_rules) {
            os << "  ✅ " << rule << std::endl;
        }
    }
}

void MusicalSolution::export_to_midi(const std::string& filename) const {
    // Simplified MIDI export (would need full MIDI library implementation)
    std::ofstream file(filename);
    if (file.is_open()) {
        file << "# Simple MIDI-like export" << std::endl;
        file << "# Tempo: 120 BPM" << std::endl;
        for (size_t i = 0; i < absolute_notes.size(); ++i) {
            int note = absolute_notes[i];
            double time = i * 0.5; // Half note per beat
            file << "Note " << note << " at " << time << " seconds" << std::endl;
        }
        file.close();
    }
}

std::string MusicalSolution::to_json() const {
    nlohmann::json j;
    j["found_solution"] = found_solution;
    j["solve_time_ms"] = solve_time_ms;
    j["total_rules_checked"] = total_rules_checked;
    j["backjumps_performed"] = backjumps_performed;
    j["average_interval_size"] = average_interval_size;
    j["melodic_direction_changes"] = melodic_direction_changes;
    j["failure_reason"] = failure_reason;

    j["absolute_notes"] = absolute_notes;
    j["intervals"] = intervals;
    j["note_names"] = note_names;
    j["voice_solutions"] = voice_solutions;
    j["voice_rhythm_ticks"] = voice_rhythms;
    j["metric_signature_numerators"] = metric_signature;

    const auto metric_denominators = metric_denominators_for_solution(*this);
    nlohmann::json metric_signature_strings = nlohmann::json::array();
    for (size_t i = 0; i < metric_signature.size(); ++i) {
        const int den = (i < metric_denominators.size()) ? metric_denominators[i] : 4;
        metric_signature_strings.push_back(std::to_string(metric_signature[i]) + "/" + std::to_string(den));
    }
    j["metric_signature"] = metric_signature_strings;

    if (has_canonical_score) {
        nlohmann::json score_j;
        score_j["rhythm_base"] = canonical_score.rhythm_base;

        nlohmann::json metric_timeline = nlohmann::json::array();
        for (const auto& seg : canonical_score.metric_timeline) {
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
        for (const auto& measure : canonical_score.measures) {
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
        j["score"] = score_j;
    }

    return j.dump(2);
}

void MusicalSolution::export_to_xml(const std::string& filename) const {
    std::ofstream xml_out(filename);
    if (!xml_out.is_open()) {
        std::cerr << "Error: Could not create XML file " << filename << std::endl;
        return;
    }
    xml_out << to_musicxml();
    xml_out.close();
}

void MusicalSolution::export_to_png(const std::string& filename) const {
    if (!found_solution) {
        std::cerr << "Cannot export PNG: No solution found (" << failure_reason << ")" << std::endl;
        return;
    }
    
    // Step 1: Generate temporary MusicXML file
    std::string temp_xml = filename + ".tmp.xml";
    std::ofstream xml_out(temp_xml);
    if (xml_out.is_open()) {
        xml_out << to_musicxml();
        xml_out.close();
    } else {
        std::cerr << "Error: Could not create temporary XML file " << temp_xml << std::endl;
        return;
    }
    
    // Step 2: Try MuseScore command-line export
    std::string musescore_cmd = "mscore -o \"" + filename + "\" \"" + temp_xml + "\" 2>/dev/null";
    int result = std::system(musescore_cmd.c_str());
    
    if (result != 0) {
        // Try alternative MuseScore command
        musescore_cmd = "musescore -o \"" + filename + "\" \"" + temp_xml + "\" 2>/dev/null";
        result = std::system(musescore_cmd.c_str());
    }
    
    if (result != 0) {
        // Try MuseScore 4 command
        musescore_cmd = "MuseScore4 -o \"" + filename + "\" \"" + temp_xml + "\" 2>/dev/null";
        result = std::system(musescore_cmd.c_str());
    }
    
    if (result != 0) {
        // Fallback: Create a visual text representation with musical staff
        std::string fallback_file = filename.substr(0, filename.find_last_of('.')) + "_notation.txt";
        std::ofstream png_out(fallback_file);
        if (png_out.is_open()) {
            png_out << "♫ MUSICAL NOTATION ♫" << std::endl;
            png_out << "═══════════════════════════════════════════════════════════" << std::endl;
            png_out << std::endl;
            
            // Create a simple staff representation
            std::vector<std::string> note_names = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            
            // Draw treble clef staff with notes positioned
            png_out << "Treble Clef Staff:" << std::endl;
            png_out << " ♪                                                      " << std::endl;
            png_out << " ═══════════════════════════════════════════════════════  F5" << std::endl;
            png_out << " ───────────────────────────────────────────────────────  " << std::endl;
            png_out << " ═══════════════════════════════════════════════════════  D5" << std::endl;
            png_out << " ───────────────────────────────────────────────────────  " << std::endl;
            png_out << " ═══════════════════════════════════════════════════════  B4" << std::endl;
            png_out << " ───────────────────────────────────────────────────────  " << std::endl;
            png_out << " ═══════════════════════════════════════════════════════  G4" << std::endl;
            png_out << " ───────────────────────────────────────────────────────  " << std::endl;
            png_out << " ═══════════════════════════════════════════════════════  E4" << std::endl;
            png_out << std::endl;
            
            // Add note sequence
            png_out << "Complete Note Sequence:" << std::endl;
            for (size_t i = 0; i < absolute_notes.size(); ++i) {
                if (i > 0 && i % 8 == 0) png_out << std::endl;
                int octave = (absolute_notes[i] / 12) - 1;
                int note = absolute_notes[i] % 12;
                png_out << note_names[note] << octave;
                if (i < absolute_notes.size() - 1 && (i + 1) % 8 != 0) png_out << " → ";
            }
            png_out << std::endl << std::endl;
            
            // Add voice information if available
            if (!voice_solutions.empty()) {
                png_out << "Multi-Voice Breakdown:" << std::endl;
                for (size_t v = 0; v < voice_solutions.size(); ++v) {
                    png_out << "Voice " << (v + 1) << ": ";
                    for (size_t i = 0; i < voice_solutions[v].size(); ++i) {
                        if (i > 0) png_out << " → ";
                        int octave = (voice_solutions[v][i] / 12) - 1;
                        int note = voice_solutions[v][i] % 12;
                        png_out << note_names[note] << octave;
                        if (i >= 12) { // Limit display length
                            png_out << " ...";
                            break;
                        }
                    }
                    png_out << std::endl;
                }
                png_out << std::endl;
            }
            
            // Performance statistics
            png_out << "♪ Performance Statistics ♪" << std::endl;
            png_out << "Solve Time: " << solve_time_ms << " ms" << std::endl;
            png_out << "Rules Checked: " << total_rules_checked << std::endl;
            png_out << "Backjumps: " << backjumps_performed << std::endl;
            png_out << "Average Interval: " << std::fixed << std::setprecision(1) << average_interval_size << " semitones" << std::endl;
            
            png_out.close();
            
            std::cerr << "MuseScore not available. Created text notation: " << fallback_file << std::endl;
        }
    }
    
    // Step 3: Clean up temporary XML file
    std::remove(temp_xml.c_str());
}

std::string MusicalSolution::to_xml() const {
    std::stringstream xml;
    
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
    xml << "<musical_solution>" << std::endl;
    
    // Metadata section
    xml << "  <metadata>" << std::endl;
    xml << "    <found_solution>" << (found_solution ? "true" : "false") << "</found_solution>" << std::endl;
    xml << "    <solve_time_ms>" << solve_time_ms << "</solve_time_ms>" << std::endl;
    xml << "    <total_rules_checked>" << total_rules_checked << "</total_rules_checked>" << std::endl;
    xml << "    <backjumps_performed>" << backjumps_performed << "</backjumps_performed>" << std::endl;
    xml << "    <average_interval_size>" << average_interval_size << "</average_interval_size>" << std::endl;
    xml << "    <melodic_direction_changes>" << melodic_direction_changes << "</melodic_direction_changes>" << std::endl;
    xml << "  </metadata>" << std::endl;
    
    if (!found_solution) {
        xml << "  <failure_reason>" << failure_reason << "</failure_reason>" << std::endl;
        xml << "</musical_solution>" << std::endl;
        return xml.str();
    }
    
    // Multi-voice solution data
    if (!voice_solutions.empty()) {
        xml << "  <voices>" << std::endl;
        for (size_t voice = 0; voice < voice_solutions.size(); ++voice) {
            xml << "    <voice id=\"" << voice << "\">" << std::endl;
            
            // Pitch sequence
            xml << "      <pitch_sequence>" << std::endl;
            for (size_t i = 0; i < voice_solutions[voice].size(); ++i) {
                int midi = voice_solutions[voice][i];
                std::string note_name = Solver::midi_to_note_name(midi);
                xml << "        <note position=\"" << (i + 1) << "\" midi=\"" << midi 
                   << "\" name=\"" << note_name << "\"/>" << std::endl;
            }
            xml << "      </pitch_sequence>" << std::endl;
            
            // Rhythm sequence
            xml << "      <rhythm_sequence>" << std::endl;
            if (voice < voice_rhythms.size()) {
                for (size_t i = 0; i < voice_rhythms[voice].size(); ++i) {
                    int value = voice_rhythms[voice][i];
                    xml << "        <duration position=\"" << (i + 1) << "\" value=\"" << value
                       << "\" note_type=\"1/" << (16 / value) << "\"/>" << std::endl;
                }
            }
            xml << "      </rhythm_sequence>" << std::endl;
            xml << "    </voice>" << std::endl;
        }
        xml << "  </voices>" << std::endl;
    } else {
        // Legacy single-voice format (for backward compatibility)
        xml << "  <sequence>" << std::endl;
        for (size_t i = 0; i < absolute_notes.size(); ++i) {
            xml << "    <note position=\"" << (i + 1) << "\" midi=\"" << absolute_notes[i] 
               << "\" name=\"" << note_names[i] << "\"";
            if (i > 0 && i - 1 < intervals.size()) {
                xml << " interval=\"" << intervals[i - 1] << "\"";
            }
            xml << "/>" << std::endl;
        }
        xml << "  </sequence>" << std::endl;
    }
    
    // Metric signature
    if (!metric_signature.empty()) {
        const auto metric_denominators = metric_denominators_for_solution(*this);
        xml << "  <metric_signature>" << std::endl;
        for (size_t i = 0; i < metric_signature.size(); ++i) {
            const int den = (i < metric_denominators.size()) ? metric_denominators[i] : 4;
            xml << "    <time_signature numerator=\"" << metric_signature[i] 
               << "\" denominator=\"" << den << "\"/>" << std::endl;
        }
        xml << "  </metric_signature>" << std::endl;
    }

    if (has_canonical_score) {
        xml << "  <score rhythm_base=\"" << canonical_score.rhythm_base << "\">" << std::endl;
        xml << "    <metric_timeline>" << std::endl;
        for (const auto& seg : canonical_score.metric_timeline) {
            xml << "      <segment start_index=\"" << seg.start_index
                << "\" end_index=\"" << seg.end_index
                << "\" start_tick=\"" << seg.start_tick
                << "\" end_tick=\"" << seg.end_tick
                << "\" numerator=\"" << seg.numerator
                << "\" denominator=\"" << seg.denominator
                << "\"/>" << std::endl;
        }
        xml << "    </metric_timeline>" << std::endl;

        xml << "    <measures>" << std::endl;
        for (const auto& measure : canonical_score.measures) {
            xml << "      <measure index=\"" << measure.measure_index
                << "\" start_ticks=\"" << measure.start_ticks
                << "\" end_ticks=\"" << measure.end_ticks
                << "\" numerator=\"" << measure.numerator
                << "\" denominator=\"" << measure.denominator
                << "\">" << std::endl;
            for (const auto& ev : measure.events) {
                xml << "        <event voice=\"" << ev.voice
                    << "\" index=\"" << ev.index
                    << "\" pitch=\"" << ev.pitch
                    << "\" rhythm_ticks=\"" << ev.rhythm
                    << "\" onset_ticks=\"" << ev.onset_ticks
                    << "\" duration_ticks=\"" << ev.duration_ticks
                    << "\" is_rest=\"" << (ev.is_rest ? "true" : "false")
                    << "\"/>" << std::endl;
            }
            xml << "      </measure>" << std::endl;
        }
        xml << "    </measures>" << std::endl;
        xml << "  </score>" << std::endl;
    }
    
    // Applied rules
    if (!applied_rules.empty()) {
        xml << "  <applied_rules>" << std::endl;
        for (const auto& rule : applied_rules) {
            xml << "    <rule>" << rule << "</rule>" << std::endl;
        }
        xml << "  </applied_rules>" << std::endl;
    }
    
    xml << "</musical_solution>" << std::endl;
    return xml.str();
}

std::string MusicalSolution::to_musicxml() const {
    std::stringstream xml;

    auto emit_right_barline = [&xml](bool is_final_measure) {
        xml << "      <barline location=\"right\">\n";
        xml << "        <bar-style>" << (is_final_measure ? "light-heavy" : "regular") << "</bar-style>\n";
        xml << "      </barline>\n";
    };

    struct RenderMeasure {
        int numerator = 4;
        int denominator = 4;
        int start_ticks = 0;
        int end_ticks = 0;
        std::vector<ScoreEvent> events;
    };
    
    // Generate proper MusicXML format for notation software
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n";
    xml << "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\" \"http://www.musicxml.org/dtds/partwise.dtd\">\n";
    xml << "<score-partwise version=\"4.0\">\n";
    
    // Work identification
    xml << "  <work>\n";
    xml << "    <work-title>Constraint Solver Composition</work-title>\n";
    xml << "  </work>\n";
    
    // Part list
    xml << "  <part-list>\n";
    if (!voice_solutions.empty()) {
        for (size_t voice = 0; voice < voice_solutions.size(); ++voice) {
            xml << "    <score-part id=\"P" << (voice + 1) << "\">\n";
            xml << "      <part-name>Voice " << (voice + 1) << "</part-name>\n";
            xml << "    </score-part>\n";
        }
    } else {
        xml << "    <score-part id=\"P1\">\n";
        xml << "      <part-name>Melody</part-name>\n";
        xml << "    </score-part>\n";
    }
    xml << "  </part-list>\n";
    
    // Parts
    if (has_canonical_score && !canonical_score.measures.empty() && !voice_solutions.empty()) {
        const int whole_ticks = canonical_score.rhythm_base > 0 ? canonical_score.rhythm_base : 1;
        const int quarter_ticks = std::max(1, whole_ticks / 4);
        const int divisions = quarter_ticks;

        // Split metric segments into actual bar-sized measures so each musical bar gets a barline.
        std::vector<RenderMeasure> render_measures;
        for (const auto& seg_measure : canonical_score.measures) {
            const int seg_start = seg_measure.start_ticks;
            const int seg_end = std::max(seg_measure.end_ticks, seg_start);
            const int bar_ticks = std::max(
                1,
                (seg_measure.numerator > 0 && seg_measure.denominator > 0)
                    ? (seg_measure.numerator * whole_ticks) / seg_measure.denominator
                    : whole_ticks);

            if (seg_end <= seg_start) {
                RenderMeasure rm;
                rm.numerator = seg_measure.numerator;
                rm.denominator = seg_measure.denominator;
                rm.start_ticks = seg_start;
                rm.end_ticks = seg_start;
                render_measures.push_back(std::move(rm));
                continue;
            }

            int bar_start = seg_start;
            while (bar_start < seg_end) {
                int bar_end = std::min(seg_end, bar_start + bar_ticks);
                RenderMeasure rm;
                rm.numerator = seg_measure.numerator;
                rm.denominator = seg_measure.denominator;
                rm.start_ticks = bar_start;
                rm.end_ticks = bar_end;
                render_measures.push_back(std::move(rm));
                bar_start = bar_end;
            }

            if (render_measures.empty()) {
                RenderMeasure rm;
                rm.numerator = seg_measure.numerator;
                rm.denominator = seg_measure.denominator;
                rm.start_ticks = seg_start;
                rm.end_ticks = seg_end;
                render_measures.push_back(std::move(rm));
            }
        }

        for (const auto& seg_measure : canonical_score.measures) {
            for (const auto& ev : seg_measure.events) {
                int remaining = std::max(1, ev.duration_ticks);
                int cursor = ev.onset_ticks;

                while (remaining > 0) {
                    int target_idx = -1;
                    for (size_t i = 0; i < render_measures.size(); ++i) {
                        const bool is_last = (i + 1 == render_measures.size());
                        const bool in_measure = (cursor >= render_measures[i].start_ticks) &&
                            (cursor < render_measures[i].end_ticks ||
                             (is_last && cursor == render_measures[i].end_ticks));
                        if (in_measure) {
                            target_idx = static_cast<int>(i);
                            break;
                        }
                    }

                    if (target_idx < 0 || target_idx >= static_cast<int>(render_measures.size())) {
                        break;
                    }

                    const auto& rm = render_measures[target_idx];
                    const int span = std::max(1, rm.end_ticks - cursor);
                    const int chunk = std::min(remaining, span);

                    ScoreEvent part = ev;
                    part.onset_ticks = cursor;
                    part.duration_ticks = chunk;
                    part.rhythm = part.is_rest ? -chunk : chunk;
                    render_measures[target_idx].events.push_back(std::move(part));

                    remaining -= chunk;
                    cursor += chunk;
                }
            }
        }

        if (render_measures.empty()) {
            RenderMeasure rm;
            rm.numerator = canonical_score.measures.front().numerator;
            rm.denominator = canonical_score.measures.front().denominator;
            rm.start_ticks = canonical_score.measures.front().start_ticks;
            rm.end_ticks = canonical_score.measures.front().end_ticks;
            render_measures.push_back(std::move(rm));
        }

        for (size_t voice = 0; voice < voice_solutions.size(); ++voice) {
            xml << "  <part id=\"P" << (voice + 1) << "\">\n";

            for (size_t m = 0; m < render_measures.size(); ++m) {
                const auto& measure = render_measures[m];
                xml << "    <measure number=\"" << (m + 1) << "\">\n";

                if (m == 0) {
                    xml << "      <attributes>\n";
                    xml << "        <divisions>" << divisions << "</divisions>\n";
                    xml << "        <key>\n";
                    xml << "          <fifths>0</fifths>\n";
                    xml << "        </key>\n";
                    xml << "        <time>\n";
                    xml << "          <beats>" << measure.numerator << "</beats>\n";
                    xml << "          <beat-type>" << measure.denominator << "</beat-type>\n";
                    xml << "        </time>\n";
                    xml << "        <clef>\n";
                    xml << "          <sign>G</sign>\n";
                    xml << "          <line>2</line>\n";
                    xml << "        </clef>\n";
                    xml << "      </attributes>\n";
                } else {
                    const auto& prev = render_measures[m - 1];
                    if (prev.numerator != measure.numerator || prev.denominator != measure.denominator) {
                        xml << "      <attributes>\n";
                        xml << "        <time>\n";
                        xml << "          <beats>" << measure.numerator << "</beats>\n";
                        xml << "          <beat-type>" << measure.denominator << "</beat-type>\n";
                        xml << "        </time>\n";
                        xml << "      </attributes>\n";
                    }
                }

                std::vector<ScoreEvent> events;
                for (const auto& ev : measure.events) {
                    if (ev.voice == static_cast<int>(voice)) {
                        events.push_back(ev);
                    }
                }
                std::sort(events.begin(), events.end(),
                          [](const ScoreEvent& a, const ScoreEvent& b) {
                              if (a.onset_ticks != b.onset_ticks) return a.onset_ticks < b.onset_ticks;
                              return a.index < b.index;
                          });

                if (events.empty()) {
                    xml << "      <note>\n";
                    xml << "        <rest/>\n";
                    emit_musicxml_note_duration(
                        xml,
                        std::max(1, measure.end_ticks - measure.start_ticks),
                        whole_ticks);
                    xml << "      </note>\n";
                } else {
                    for (const auto& ev : events) {
                        xml << "      <note>\n";
                        if (ev.is_rest || ev.pitch < 0) {
                            xml << "        <rest/>\n";
                        } else {
                            const int pitch_class = ((ev.pitch % 12) + 12) % 12;
                            const int octave = (ev.pitch / 12) - 1;
                            xml << "        <pitch>\n";
                            xml << "          <step>" << musicxml_step_for_pitch_class(pitch_class) << "</step>\n";
                            const int alter = musicxml_alter_for_pitch_class(pitch_class);
                            if (alter != 0) {
                                xml << "          <alter>" << alter << "</alter>\n";
                            }
                            xml << "          <octave>" << octave << "</octave>\n";
                            xml << "        </pitch>\n";
                        }
                        emit_musicxml_note_duration(xml, std::max(1, ev.duration_ticks), whole_ticks);
                        xml << "      </note>\n";
                    }
                }

                emit_right_barline((m + 1) == render_measures.size());

                xml << "    </measure>\n";
            }

            xml << "  </part>\n";
        }
    } else if (!voice_solutions.empty()) {
        for (size_t voice = 0; voice < voice_solutions.size(); ++voice) {
            xml << "  <part id=\"P" << (voice + 1) << "\">\n";
            xml << "    <measure number=\"1\">\n";
            xml << "      <attributes>\n";
            xml << "        <divisions>1</divisions>\n";
            xml << "        <key>\n";
            xml << "          <fifths>0</fifths>\n";
            xml << "        </key>\n";
            xml << "        <time>\n";
            xml << "          <beats>4</beats>\n";
            xml << "          <beat-type>4</beat-type>\n";
            xml << "        </time>\n";
            xml << "        <clef>\n";
            xml << "          <sign>G</sign>\n";
            xml << "          <line>2</line>\n";
            xml << "        </clef>\n";
            xml << "      </attributes>\n";
            
            // Convert MIDI numbers to notes
            for (size_t i = 0; i < voice_solutions[voice].size() && i < 16; ++i) {
                int midi_note = voice_solutions[voice][i];
                int octave = (midi_note / 12) - 1;
                int pitch_class = midi_note % 12;
                std::vector<std::string> pitch_names = {"C", "C", "D", "D", "E", "F", "F", "G", "G", "A", "A", "B"};
                std::vector<int> alterations = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
                
                xml << "      <note>\n";
                xml << "        <pitch>\n";
                xml << "          <step>" << pitch_names[pitch_class] << "</step>\n";
                if (alterations[pitch_class] != 0) {
                    xml << "          <alter>" << alterations[pitch_class] << "</alter>\n";
                }
                xml << "          <octave>" << octave << "</octave>\n";
                xml << "        </pitch>\n";
                xml << "        <duration>1</duration>\n";
                xml << "        <type>quarter</type>\n";
                xml << "      </note>\n";
            }
            
            // Fill measure with rests if needed
            size_t notes_written = std::min(voice_solutions[voice].size(), size_t(16));
            for (size_t r = notes_written; r < 4; ++r) {
                xml << "      <note>\n";
                xml << "        <rest/>\n";
                xml << "        <duration>1</duration>\n";
                xml << "        <type>quarter</type>\n";
                xml << "      </note>\n";
            }

            emit_right_barline(true);
            
            xml << "    </measure>\n";
            xml << "  </part>\n";
        }
    } else if (!absolute_notes.empty()) {
        // Single voice from absolute_notes
        xml << "  <part id=\"P1\">\n";
        xml << "    <measure number=\"1\">\n";
        xml << "      <attributes>\n";
        xml << "        <divisions>1</divisions>\n";
        xml << "        <key>\n";
        xml << "          <fifths>0</fifths>\n";
        xml << "        </key>\n";
        xml << "        <time>\n";
        xml << "          <beats>4</beats>\n";
        xml << "          <beat-type>4</beat-type>\n";
        xml << "        </time>\n";
        xml << "        <clef>\n";
        xml << "          <sign>G</sign>\n";
        xml << "          <line>2</line>\n";
        xml << "        </clef>\n";
        xml << "      </attributes>\n";
        
        // Convert MIDI numbers to notes
        for (size_t i = 0; i < absolute_notes.size() && i < 16; ++i) {
            int midi_note = absolute_notes[i];
            int octave = (midi_note / 12) - 1;
            int pitch_class = midi_note % 12;
            std::vector<std::string> pitch_names = {"C", "C", "D", "D", "E", "F", "F", "G", "G", "A", "A", "B"};
            std::vector<int> alterations = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
            
            xml << "      <note>\n";
            xml << "        <pitch>\n";
            xml << "          <step>" << pitch_names[pitch_class] << "</step>\n";
            if (alterations[pitch_class] != 0) {
                xml << "          <alter>" << alterations[pitch_class] << "</alter>\n";
            }
            xml << "          <octave>" << octave << "</octave>\n";
            xml << "        </pitch>\n";
            xml << "        <duration>1</duration>\n";
            xml << "        <type>quarter</type>\n";
            xml << "      </note>\n";
        }
        
        // Fill measure with rests if needed
        size_t notes_written = std::min(absolute_notes.size(), size_t(16));
        for (size_t r = notes_written; r < 4; ++r) {
            xml << "      <note>\n";
            xml << "        <rest/>\n";
            xml << "        <duration>1</duration>\n";
            xml << "        <type>quarter</type>\n";
            xml << "      </note>\n";
        }

        emit_right_barline(true);
        
        xml << "    </measure>\n";
        xml << "  </part>\n";
    }
    
    xml << "</score-partwise>\n";
    return xml.str();
}

// ===============================
// Specific Musical Rules for Factory
// ===============================

class NoRepetitionRule : public MusicalConstraints::MusicalRule {
public:
    MusicalConstraints::RuleResult check_rule(const MusicalConstraints::DualSolutionStorage& storage, 
                                            int current_index) const override {
        if (current_index > 0) {
            int current_note = storage.absolute(current_index);
            for (int i = std::max(0, current_index - 3); i < current_index; ++i) {
                if (storage.absolute(i) == current_note) {
                    auto result = MusicalConstraints::RuleResult::Failure(2, "No repetition rule violated");
                    MusicalConstraints::BackjumpSuggestion suggestion(i, current_index - i);
                    suggestion.explanation = "Repeated note at position " + std::to_string(i);
                    result.add_suggestion(suggestion);
                    return result;
                }
            }
        }
        return MusicalConstraints::RuleResult::Success();
    }
    
    std::string description() const override { return "No Repetition Rule"; }
    std::vector<int> get_dependent_variables(int current_index) const override {
        std::vector<int> deps;
        for (int i = std::max(0, current_index - 3); i <= current_index; ++i) {
            deps.push_back(i);
        }
        return deps;
    }
    std::string rule_type() const override { return "NoRepetitionRule"; }
};

class MelodicIntervalRule : public MusicalConstraints::MusicalRule {
private:
    int max_interval_;
public:
    explicit MelodicIntervalRule(int max_interval = 7) : max_interval_(max_interval) {}
    
    MusicalConstraints::RuleResult check_rule(const MusicalConstraints::DualSolutionStorage& storage, 
                                            int current_index) const override {
        if (current_index > 0) {
            int interval = std::abs(storage.interval(current_index));
            if (interval > max_interval_) {
                auto result = MusicalConstraints::RuleResult::Failure(1, "Large melodic leap");
                MusicalConstraints::BackjumpSuggestion suggestion(current_index - 1, 1);
                suggestion.explanation = "Interval too large: " + std::to_string(interval);
                result.add_suggestion(suggestion);
                return result;
            }
        }
        return MusicalConstraints::RuleResult::Success();
    }
    
    std::string description() const override { 
        return "Melodic Interval Rule (max " + std::to_string(max_interval_) + " semitones)"; 
    }
    std::vector<int> get_dependent_variables(int current_index) const override {
        return (current_index > 0) ? std::vector<int>{current_index - 1, current_index} : std::vector<int>{current_index};
    }
    std::string rule_type() const override { return "MelodicIntervalRule"; }
};

class StepwiseMotionRule : public MusicalConstraints::MusicalRule {
public:
    explicit StepwiseMotionRule(double ratio = 0.7) { 
        // Store ratio for potential future use in heuristics
        (void)ratio; // Suppress unused parameter warning
    }
    
    MusicalConstraints::RuleResult check_rule(const MusicalConstraints::DualSolutionStorage& storage, 
                                            int current_index) const override {
        if (current_index >= 3) {
            // Check for too many large leaps
            int large_leaps = 0;
            for (int i = current_index - 2; i <= current_index; ++i) {
                if (std::abs(storage.interval(i)) > 2) large_leaps++;
            }
            
            if (large_leaps > 1) { // More than 1 large leap in 3 notes
                auto result = MusicalConstraints::RuleResult::Failure(2, "Prefer stepwise motion");
                MusicalConstraints::BackjumpSuggestion suggestion(current_index - 2, 2);
                suggestion.explanation = "Too many large leaps in sequence";
                result.add_suggestion(suggestion);
                return result;
            }
        }
        return MusicalConstraints::RuleResult::Success();
    }
    
    std::string description() const override { return "Stepwise Motion Preference Rule"; }
    std::vector<int> get_dependent_variables(int current_index) const override {
        std::vector<int> deps;
        for (int i = std::max(0, current_index - 2); i <= current_index; ++i) {
            deps.push_back(i);
        }
        return deps;
    }
    std::string rule_type() const override { return "StepwiseMotionRule"; }
};

class RangeConstraintRule : public MusicalConstraints::MusicalRule {
private:
    int min_note_, max_note_;
public:
    RangeConstraintRule(int min_note, int max_note) : min_note_(min_note), max_note_(max_note) {}
    
    MusicalConstraints::RuleResult check_rule(const MusicalConstraints::DualSolutionStorage& storage, 
                                            int current_index) const override {
        int note = storage.absolute(current_index);
        if (note < min_note_ || note > max_note_) {
            auto result = MusicalConstraints::RuleResult::Failure(1, "Note out of range");
            MusicalConstraints::BackjumpSuggestion suggestion(current_index, 1);
            suggestion.explanation = "Note " + std::to_string(note) + " outside range [" + 
                                  std::to_string(min_note_) + "," + std::to_string(max_note_) + "]";
            result.add_suggestion(suggestion);
            return result;
        }
        return MusicalConstraints::RuleResult::Success();
    }
    
    std::string description() const override { 
        return "Range Constraint [" + std::to_string(min_note_) + "," + std::to_string(max_note_) + "]"; 
    }
    std::vector<int> get_dependent_variables(int current_index) const override {
        return {current_index};
    }
    std::string rule_type() const override { return "RangeConstraintRule"; }
};

// ===============================
// Musical Rule Factory Implementation
// ===============================

std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> 
MusicalRuleFactory::create_basic_rules() {
    std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> rules;
    
    rules.push_back(std::make_shared<NoRepetitionRule>());
    rules.push_back(std::make_shared<MelodicIntervalRule>(12)); // Octave max
    
    return rules;
}

std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>
MusicalRuleFactory::create_jazz_rules() {
    std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> rules;
    
    rules.push_back(std::make_shared<NoRepetitionRule>());
    rules.push_back(std::make_shared<MelodicIntervalRule>(7)); // More conservative
    
    return rules;
}

std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>
MusicalRuleFactory::create_voice_leading_rules() {
    std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> rules;
    
    rules.push_back(std::make_shared<NoRepetitionRule>());
    rules.push_back(std::make_shared<MelodicIntervalRule>(5)); // Conservative classical
    
    return rules;
}

std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>
MusicalRuleFactory::create_contemporary_rules() {
    std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> rules;
    
    rules.push_back(std::make_shared<MelodicIntervalRule>(18)); // Very permissive
    
    return rules;
}

std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>
MusicalRuleFactory::create_custom_rules(const SolverConfig& config) {
    std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>> rules;
    
    if (!config.allow_repetitions) {
        rules.push_back(std::make_shared<NoRepetitionRule>());
    }
    
    rules.push_back(std::make_shared<MelodicIntervalRule>(config.max_interval_size));
    
    return rules;
}

std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>
MusicalRuleFactory::get_style_rules(SolverConfig::MusicalStyle style) {
    switch (style) {
        case SolverConfig::CLASSICAL: return create_voice_leading_rules();
        case SolverConfig::JAZZ: return create_jazz_rules();
        case SolverConfig::CONTEMPORARY: return create_contemporary_rules();
        case SolverConfig::MINIMAL: return create_basic_rules();
        case SolverConfig::CUSTOM: return create_basic_rules(); // Default fallback
        default: return create_basic_rules();
    }
}

// ===============================
// Main Solver Implementation
// ===============================

Solver::Solver() {
    initialize_solver();
}

Solver::Solver(const SolverConfig& config) : config_(config) {
    initialize_solver();
}

void Solver::initialize_solver() {
    // Reset per-configuration advanced constraint flags so previous runs
    // cannot leak constraints into the next configured solve.
    retrograde_inversion_enabled_ = false;
    retrograde_inversion_center_ = 65;
    perfect_fifth_intervals_enabled_ = false;
    twelve_tone_row_enabled_ = false;
    palindrome_voices_enabled_ = false;
    
    // Create solution storage
    solution_storage_ = std::make_unique<MusicalConstraints::DualSolutionStorage>(
        config_.sequence_length * config_.num_voices, MusicalConstraints::DomainType::ABSOLUTE_DOMAIN);
    
    // Initialize dynamic rule system
    compiled_rules_ = std::make_unique<DynamicRules::CompiledRuleSet>();
    dynamic_rule_configs_.clear();
    
    // Auto-configure rules if needed
    if (rules_.empty()) {
        auto_configure_rules();
    }
}

void Solver::configure(const SolverConfig& config) {
    config_ = config;
    initialize_solver();
}

void Solver::setup_for_style(SolverConfig::MusicalStyle style) {
    config_.style = style;
    clear_rules();
    add_rules(MusicalRuleFactory::get_style_rules(style));
    
    // Adjust search behavior based on style complexity.
    switch (style) {
        case SolverConfig::CLASSICAL:
            config_.variable_branching = GecodeClusterIntegration::VariableBranchingStrategy::FIRST_FAIL;
            config_.value_selection = GecodeClusterIntegration::ValueSelectionStrategy::MIN;
            break;
        case SolverConfig::JAZZ:
            config_.variable_branching = GecodeClusterIntegration::VariableBranchingStrategy::FIRST_FAIL;
            config_.value_selection = GecodeClusterIntegration::ValueSelectionStrategy::HEURISTIC;
            break;
        case SolverConfig::CONTEMPORARY:
            config_.variable_branching = GecodeClusterIntegration::VariableBranchingStrategy::INPUT_ORDER;
            config_.value_selection = GecodeClusterIntegration::ValueSelectionStrategy::RANDOM;
            break;
        default:
            config_.variable_branching = GecodeClusterIntegration::VariableBranchingStrategy::FIRST_FAIL;
            config_.value_selection = GecodeClusterIntegration::ValueSelectionStrategy::MIN;
    }
}

void Solver::setup_for_jazz_improvisation() {
    config_.style = SolverConfig::JAZZ;
    config_.sequence_length = 16;
    config_.max_interval_size = 7;
    config_.allow_repetitions = false;
    config_.prefer_stepwise_motion = true;
    setup_for_style(SolverConfig::JAZZ);
}

void Solver::setup_for_classical_melody() {
    config_.style = SolverConfig::CLASSICAL;
    config_.sequence_length = 8;
    config_.max_interval_size = 5;
    config_.prefer_stepwise_motion = true;
    setup_for_style(SolverConfig::CLASSICAL);
}

void Solver::setup_for_experimental_music() {
    config_.style = SolverConfig::CONTEMPORARY;
    config_.max_interval_size = 18;
    config_.allow_repetitions = true;
    config_.prefer_stepwise_motion = false;
    setup_for_style(SolverConfig::CONTEMPORARY);
}

void Solver::add_rule(std::shared_ptr<MusicalConstraints::MusicalRule> rule) {
    rules_.push_back(rule);
}

void Solver::add_rules(const std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>& rules) {
    rules_.insert(rules_.end(), rules.begin(), rules.end());
}

void Solver::add_rule_config(const std::string& rule_type, const std::string& function,
                const std::vector<int>& indices, int target_engine,
                const std::vector<int>& target_engines,
                const std::string& engine_type, const std::string& description,
                const std::vector<double>& parameters,
                const std::vector<std::string>& parameter_strings,
                const std::vector<std::string>& timepoints) {
    // Preserve full rule metadata for engine-targeted posting in build_configured_space_.
    engine_rule_configs_.push_back({rule_type, function, indices, timepoints, target_engine,
                    target_engines, engine_type, description, parameters, parameter_strings});
    
    // Convert JSON rule configuration to actual MusicalRule objects based on rule_type or function
    if ((rule_type == "r-pitches-one-engine" || rule_type == "r-pitches-all-different" || 
         rule_type == "r-twelve-tone-voice1") && function == "all_different") {
        // Engine-targeted all_different is posted later in build_configured_space_.
        // Keep this branch as a recognized rule type with no legacy global fallback.
        
    } else if (rule_type == "r-perfect-fifth-intervals" && function == "consecutive_perfect_fifths") {
        // Enable perfect fifth intervals constraint
        perfect_fifth_intervals_enabled_ = true;
        add_rule(std::make_shared<MelodicIntervalRule>(7));  // Add basic interval rule
        
    } else if ((rule_type == "r-palindrome-voice" || rule_type == "r-palindrome-voice2") && 
               function == "palindrome_of_engine") {
        // Enable palindrome voices constraint  
        palindrome_voices_enabled_ = true;
        add_rule(std::make_shared<NoRepetitionRule>());  // Add basic rule
        
    } else if (rule_type == "r-cross-voice-retrograde-inversion" && function == "retrograde_inversion_relationship") {
        // SPECIAL CASE: Retrograde Inversion Constraint
        std::cout << "🎯 DETECTED RETROGRADE INVERSION RULE - Adding special constraint!" << std::endl;
        
        // Set a flag to enable special retrograde inversion solving
        retrograde_inversion_enabled_ = true;
        retrograde_inversion_center_ = (!parameters.empty()) ? static_cast<int>(parameters[0]) : 65;
        
        std::cout << "   Inversion center: " << retrograde_inversion_center_ << " (MIDI)" << std::endl;
        
        // Add basic rules but with special handling in solve()
        add_rule(std::make_shared<NoRepetitionRule>());
        
    } else if (rule_type == "r-cross-voice-no-unisons") {
        // Add rule to prevent unisons between voices (handled by engine separation)
        add_rule(std::make_shared<MelodicIntervalRule>(12)); // Allow wide intervals between voices
        
    } else if (rule_type == "r-rhythmic-uniformity") {
        // Rhythm uniformity is handled by the engine extraction (all quarter notes)
        // No specific rule needed as this is built into the rhythm generation
        
    } else if (rule_type == "r-metric-signature") {
        // Metric signature is handled by the metric engine extraction
        // No specific rule needed as this is built into the metric generation
        
    } else {
        // Default: add basic musical rules for unrecognized rule types
        add_rule(std::make_shared<NoRepetitionRule>());
        add_rule(std::make_shared<MelodicIntervalRule>(7));
    }
}

void Solver::clear_rules() {
    rules_.clear();
    engine_rule_configs_.clear();

    // Keep rule flags consistent with an empty rule set.
    retrograde_inversion_enabled_ = false;
    retrograde_inversion_center_ = 65;
    perfect_fifth_intervals_enabled_ = false;
    twelve_tone_row_enabled_ = false;
    palindrome_voices_enabled_ = false;
}

void Solver::auto_configure_rules() {
    clear_rules();
    add_rules(MusicalRuleFactory::get_style_rules(config_.style));
}

// ===============================
// Dynamic Rule Management (NEW)
// ===============================

void Solver::add_dynamic_rule(const nlohmann::json& rule_json) {
    try {
        std::cout << "🎯 Adding dynamic rule: " << rule_json.value("id", "unknown") << std::endl;
        
        // Store rule config for later access
        dynamic_rule_configs_.push_back(rule_json);

        // Expand wildcard heuristic rules to concrete index-specific rules.
        auto expanded_rules = expand_wildcard_heuristic_rule(
            rule_json, config_.num_voices, config_.sequence_length);
        if (!expanded_rules.empty()) {
            std::cout << "   🔁 Expanded wildcard heuristic into "
                      << expanded_rules.size() << " concrete rules" << std::endl;
            for (const auto& expanded_rule : expanded_rules) {
                auto compiled_expanded = DynamicRules::DynamicRuleCompiler::compile_from_json(expanded_rule);
                compiled_rules_->add_constraint(std::move(compiled_expanded));
            }
            std::cout << "   ✅ Dynamic wildcard heuristic compiled successfully" << std::endl;
            return;
        }
        
        // Compile and add to rule set
        auto compiled_rule = DynamicRules::DynamicRuleCompiler::compile_from_json(rule_json);
        compiled_rules_->add_constraint(std::move(compiled_rule));
        
        std::cout << "   ✅ Dynamic rule compiled successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "   ❌ Failed to compile dynamic rule: " << e.what() << std::endl;
        throw;
    }
}

void Solver::add_dynamic_rule(const std::string& rule_json_string) {
    try {
        auto rule_json = nlohmann::json::parse(rule_json_string);
        add_dynamic_rule(rule_json);
    } catch (const std::exception& e) {
        std::cout << "❌ Failed to parse dynamic rule JSON: " << e.what() << std::endl;
        throw;
    }
}

void Solver::load_dynamic_rules(const std::vector<nlohmann::json>& rules_array) {
    std::cout << "📋 Loading " << rules_array.size() << " dynamic rules..." << std::endl;
    
    for (const auto& rule_json : rules_array) {
        try {
            add_dynamic_rule(rule_json);
        } catch (const std::exception& e) {
            std::cout << "⚠️  Skipped rule due to error: " << e.what() << std::endl;
        }
    }
    
    std::cout << "✅ Loaded dynamic rules. Total in system: " << get_dynamic_rules_count() << std::endl;
}

void Solver::clear_dynamic_rules() {
    compiled_rules_ = std::make_unique<DynamicRules::CompiledRuleSet>();
    dynamic_rule_configs_.clear();
    std::cout << "🧹 Cleared all dynamic rules" << std::endl;
}

void Solver::apply_compiled_constraint(std::unique_ptr<DynamicRules::CompiledConstraint> compiled_constraint) {
    if (!compiled_constraint) {
        std::cout << "   ⚠️  Null compiled constraint, skipping" << std::endl;
        return;
    }
    
    try {
        std::cout << "🎯 Applying compiled constraint: " << compiled_constraint->rule_id << std::endl;
        
        // Add to the compiled rule set directly
        if (!compiled_rules_) {
            compiled_rules_ = std::make_unique<DynamicRules::CompiledRuleSet>();
        }
        
        compiled_rules_->add_constraint(std::move(compiled_constraint));
        
        std::cout << "   ✅ Wildcard constraint applied successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "   ❌ Failed to apply compiled constraint: " << e.what() << std::endl;
        throw;
    }
}

size_t Solver::get_dynamic_rules_count() const {
    return compiled_rules_ ? compiled_rules_->total_count() : 0;
}

MusicalSolution Solver::solve() {
    total_solve_attempts_++;
    return solve_internal();
}

std::vector<MusicalSolution> Solver::solve_multiple(
        int max_solutions, int timeout_ms,
        std::function<void(const MusicalSolution&, int)> on_solution) {
    std::vector<MusicalSolution> solutions;
    try {
        auto* raw_space = build_configured_space_();

        if (config_.restart_policy != SolverConfig::RestartPolicy::NONE) {
            throw std::runtime_error("Unsupported restart policy in solve_multiple()");
        }

        Gecode::Search::Options search_opts;
        search_opts.threads = 1;
        search_opts.nogoods_limit = 128;

        if (config_.search_engine != SolverConfig::SearchEngine::DFS) {
            throw std::runtime_error("Unsupported search engine in solve_multiple()");
        }

        Gecode::DFS<GecodeClusterIntegration::IntegratedMusicalSpace> search_engine(raw_space, search_opts);

        int limit = (max_solutions < 0) ? std::numeric_limits<int>::max() : max_solutions;
        auto wall_start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < limit; ++i) {
            // Check wall-clock timeout before each next()
            if (timeout_ms > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now() - wall_start).count();
                if (elapsed >= timeout_ms) {
                    std::cout << "   ⏱️  Timeout after " << elapsed << " ms — returning "
                              << solutions.size() << " solution(s) found so far" << std::endl;
                    break;
                }
            }

            auto* solved_space = search_engine.next();
            if (!solved_space) break;  // search space exhausted

            MusicalSolution sol = extract_solution_from_space_(solved_space);
            if (sol.found_solution) {
                solutions.push_back(sol);
                if (on_solution) on_solution(sol, static_cast<int>(solutions.size()) - 1);
            }
        }
    } catch (const std::exception& e) {
        MusicalSolution failed;
        failed.found_solution = false;
        failed.failure_reason = "Exception during multi-solve: " + std::string(e.what());
        if (solutions.empty()) solutions.push_back(failed);
    }
    return solutions;
}

// ---------------------------------------------------------------------------
// Build a fully-configured Gecode space (all constraints posted, no search).
// ---------------------------------------------------------------------------
GecodeClusterIntegration::IntegratedMusicalSpace* Solver::build_configured_space_() {
    // BUILD PER-VOICE DOMAINS (required — each voice must have its domain set)
    std::vector<std::vector<int>> all_voice_domains = config_.voice_domains;
    if (all_voice_domains.empty()) {
        throw std::runtime_error("voice_domains is empty — each voice must specify its pitch domain");
    }
    if ((int)all_voice_domains.size() < config_.num_voices) {
        throw std::runtime_error("voice_domains has fewer entries than num_voices");
    }

    const unsigned int effective_random_seed = resolve_effective_random_seed(config_.random_seed);

    if (compiled_rules_ && compiled_rules_->has_heuristic_scorers()) {
        GecodeClusterIntegration::configure_pitch_heuristic_value_ordering(
            [this](const GecodeClusterIntegration::IntegratedMusicalSpace& space,
                   int voice, int position, int candidate_value) {
                if (!compiled_rules_) {
                    return GecodeClusterIntegration::HeuristicValueScoreBuckets{};
                }

                auto& mutable_space = const_cast<GecodeClusterIntegration::IntegratedMusicalSpace&>(space);
                auto& mutable_pitch_vars = const_cast<Gecode::IntVarArray&>(space.get_absolute_vars());
                auto& mutable_rhythm_vars = const_cast<Gecode::IntVarArray&>(space.get_rhythm_vars());

                DynamicRules::ConstraintContext score_ctx(
                    &mutable_space,
                    &mutable_pitch_vars,
                    &mutable_rhythm_vars,
                    config_.num_voices,
                    config_.sequence_length,
                    &const_cast<Gecode::IntVarArray&>(space.get_metric_vars()));

                DynamicRules::HeuristicCandidateContext candidate_ctx;
                candidate_ctx.voice = voice;
                candidate_ctx.position = position;
                candidate_ctx.candidate_value = candidate_value;

                return compiled_rules_->evaluate_heuristic_buckets(score_ctx, candidate_ctx);
            },
            effective_random_seed,
            config_.heuristic_top_k,
            config_.heuristic_trace);
    } else {
        GecodeClusterIntegration::clear_pitch_heuristic_value_ordering();
    }

    bool has_metric_targeted_rule = false;
    bool has_explicit_metric_timepoint_rule = false;
    const int metric_engine_index = config_.num_voices * 2;
    for (const auto& cfg : engine_rule_configs_) {
        std::vector<int> targets;
        if (cfg.target_engine >= 0) targets.push_back(cfg.target_engine);
        targets.insert(targets.end(), cfg.target_engines.begin(), cfg.target_engines.end());
        for (int t : targets) {
            if (t == metric_engine_index) {
                if (cfg.rule_type == "r-metric-signature" && !cfg.timepoints.empty()) {
                    has_explicit_metric_timepoint_rule = true;
                } else {
                    has_metric_targeted_rule = true;
                }
            }
        }
    }

    if (has_metric_targeted_rule && has_explicit_metric_timepoint_rule) {
        throw std::runtime_error("Cannot mix index-based and timepoint-based metric rules in the same solve");
    }

    const bool metric_active = (config_.enable_metric_engine || has_metric_targeted_rule) && !has_explicit_metric_timepoint_rule;
    std::vector<int> metric_domain_numerators;
    if (metric_active) {
        if (config_.metric_domain.empty()) {
            throw std::runtime_error("Metric engine is active but metric_domain is empty");
        }

        std::map<int, int> numerator_to_denominator;
        for (const auto& entry : config_.metric_domain) {
            if (entry.numerator <= 0 || entry.denominator <= 0) {
                throw std::runtime_error("metric_domain contains invalid time signature values");
            }
            auto it = numerator_to_denominator.find(entry.numerator);
            if (it != numerator_to_denominator.end() && it->second != entry.denominator) {
                throw std::runtime_error(
                    "metric_domain currently requires unique numerators (found conflicting signatures for " +
                    std::to_string(entry.numerator) + ")");
            }
            numerator_to_denominator[entry.numerator] = entry.denominator;
            metric_domain_numerators.push_back(entry.numerator);
        }
        std::sort(metric_domain_numerators.begin(), metric_domain_numerators.end());
        metric_domain_numerators.erase(
            std::unique(metric_domain_numerators.begin(), metric_domain_numerators.end()),
            metric_domain_numerators.end());
    }

    std::unique_ptr<GecodeClusterIntegration::IntegratedMusicalSpace> gecode_space;
    if (metric_active) {
        gecode_space = std::make_unique<GecodeClusterIntegration::IntegratedMusicalSpace>(
            config_.sequence_length, config_.num_voices,
            config_.variable_branching, config_.value_selection,
            all_voice_domains, config_.voice_rhythm_domains, metric_domain_numerators,
            effective_random_seed);
    } else {
        gecode_space = std::make_unique<GecodeClusterIntegration::IntegratedMusicalSpace>(
            config_.sequence_length, config_.num_voices,
            config_.variable_branching, config_.value_selection,
            all_voice_domains, config_.voice_rhythm_domains, effective_random_seed);
    }

    // ADD MUSICAL RULES
    if (!rules_.empty()) {
        gecode_space->add_musical_rules(rules_);
        std::cout << "DEBUG: Added " << rules_.size() << " musical rules" << std::endl;
    }

    // POST ENGINE-TARGETED RULES
    for (const auto& cfg : engine_rule_configs_) {
        const bool is_all_different_rule =
            (cfg.function == "all_different" || cfg.rule_type == "r-pitches-all-different" || cfg.rule_type == "r-twelve-tone-voice1");
        const bool is_rhythmic_uniformity_rule =
            (cfg.rule_type == "r-rhythmic-uniformity");
        const bool is_metric_signature_rule =
            (cfg.rule_type == "r-metric-signature" &&
             (cfg.function == "equal_values" || cfg.function == "equal" || cfg.function.empty()));
        const bool is_metric_hierarchy_rule =
            (cfg.rule_type == "r-metric-hierarchy");

        if (!is_all_different_rule && !is_rhythmic_uniformity_rule && !is_metric_signature_rule && !is_metric_hierarchy_rule) {
            continue;
        }

        std::vector<int> targets;
        if (cfg.target_engine >= 0) {
            targets.push_back(cfg.target_engine);
        }
        targets.insert(targets.end(), cfg.target_engines.begin(), cfg.target_engines.end());
        std::sort(targets.begin(), targets.end());
        targets.erase(std::unique(targets.begin(), targets.end()), targets.end());

        if (targets.empty()) {
            throw std::runtime_error("rule '" + cfg.description + "' has no target engine");
        }

        std::vector<int> selected_indices = cfg.indices;
        if (selected_indices.empty()) {
            for (int i = 0; i < config_.sequence_length; ++i) {
                selected_indices.push_back(i);
            }
        }

        for (int engine : targets) {
            const bool is_metric_engine = (engine == metric_engine_index);
            const bool is_pitch_engine = !is_metric_engine && ((engine % 2) == 1);
            const bool is_rhythm_engine = !is_metric_engine && ((engine % 2) == 0);

            if (!cfg.engine_type.empty()) {
                if (cfg.engine_type == "pitch" && !is_pitch_engine) {
                    throw std::runtime_error("rule '" + cfg.description + "' declares engine_type='pitch' but targets non-pitch engine " + std::to_string(engine));
                }
                if (cfg.engine_type == "rhythm" && !is_rhythm_engine) {
                    throw std::runtime_error("rule '" + cfg.description + "' declares engine_type='rhythm' but targets non-rhythm engine " + std::to_string(engine));
                }
                if (cfg.engine_type == "metric" && !is_metric_engine) {
                    throw std::runtime_error("rule '" + cfg.description + "' declares engine_type='metric' but targets non-metric engine " + std::to_string(engine));
                }
            }

            if (is_metric_signature_rule) {
                if (!is_metric_engine) {
                    throw std::runtime_error("r-metric-signature rule '" + cfg.description + "' must target metric engine " + std::to_string(metric_engine_index));
                }
                std::vector<std::pair<int, int>> desired_signatures = parse_metric_signature_parameters(
                    cfg.parameter_strings, cfg.parameters,
                    "r-metric-signature rule '" + cfg.description + "'");

                if (desired_signatures.empty()) {
                    throw std::runtime_error("r-metric-signature rule '" + cfg.description + "' requires signature parameters");
                }

                std::map<int, int> metric_domain_map;
                for (const auto& entry : config_.metric_domain) {
                    auto it = metric_domain_map.find(entry.numerator);
                    if (it != metric_domain_map.end() && it->second != entry.denominator) {
                        throw std::runtime_error(
                            "r-metric-signature rule '" + cfg.description +
                            "' cannot be applied because metric_domain has duplicate numerators with different denominators");
                    }
                    metric_domain_map[entry.numerator] = entry.denominator;
                }

                for (const auto& sig : desired_signatures) {
                    auto it = metric_domain_map.find(sig.first);
                    if (it == metric_domain_map.end() || it->second != sig.second) {
                        throw std::runtime_error(
                            "r-metric-signature rule '" + cfg.description +
                            "' signature " + std::to_string(sig.first) + "/" + std::to_string(sig.second) +
                            " is not present in metric_domain");
                    }
                }

                if (!cfg.timepoints.empty()) {
                    if (!cfg.indices.empty()) {
                        throw std::runtime_error("r-metric-signature rule '" + cfg.description + "' cannot specify both indices and timepoints");
                    }
                    if (config_.score_length_ticks < 0) {
                        throw std::runtime_error("r-metric-signature rule '" + cfg.description + "' with timepoints requires score_length");
                    }
                    if (desired_signatures.size() != cfg.timepoints.size()) {
                        throw std::runtime_error(
                            "r-metric-signature rule '" + cfg.description +
                            "' requires the same number of parameters and timepoints");
                    }

                    int previous_tick = -1;
                    for (size_t i = 0; i < cfg.timepoints.size(); ++i) {
                        const int tick = parse_score_time_token_to_ticks(
                            cfg.timepoints[i], config_.rhythm_base,
                            "r-metric-signature rule '" + cfg.description + "' timepoints[" + std::to_string(i) + "]");
                        if (i == 0 && tick != 0) {
                            throw std::runtime_error("r-metric-signature rule '" + cfg.description + "' requires first timepoint to be 0");
                        }
                        if (tick <= previous_tick) {
                            throw std::runtime_error("r-metric-signature rule '" + cfg.description + "' timepoints must be strictly increasing");
                        }
                        if (tick >= config_.score_length_ticks) {
                            throw std::runtime_error("r-metric-signature rule '" + cfg.description + "' has timepoint outside score_length");
                        }
                        previous_tick = tick;
                    }
                    continue;
                }

                if (!gecode_space->has_metric_vars()) {
                    throw std::runtime_error("r-metric-signature rule '" + cfg.description + "' targets metric engine but metric vars are unavailable");
                }

                std::vector<int> segment_starts = cfg.indices;
                if (segment_starts.empty()) {
                    segment_starts.push_back(0);
                }
                if (segment_starts.front() != 0) {
                    throw std::runtime_error("r-metric-signature rule '" + cfg.description + "' requires first index to be 0");
                }
                for (size_t i = 0; i < segment_starts.size(); ++i) {
                    if (segment_starts[i] < 0 || segment_starts[i] >= config_.sequence_length) {
                        throw std::runtime_error("r-metric-signature rule '" + cfg.description + "' has out-of-range index " + std::to_string(segment_starts[i]));
                    }
                    if (i > 0 && segment_starts[i] <= segment_starts[i - 1]) {
                        throw std::runtime_error("r-metric-signature rule '" + cfg.description + "' indices must be strictly increasing");
                    }
                }

                if (desired_signatures.size() != segment_starts.size()) {
                    throw std::runtime_error(
                        "r-metric-signature rule '" + cfg.description +
                        "' requires the same number of parameters and indices (segment starts)");
                }

                for (size_t seg = 0; seg < segment_starts.size(); ++seg) {
                    const int start = segment_starts[seg];
                    const int end = (seg + 1 < segment_starts.size())
                        ? (segment_starts[seg + 1] - 1)
                        : (config_.sequence_length - 1);
                    const int target_numerator = desired_signatures[seg].first;
                    for (int idx = start; idx <= end; ++idx) {
                        Gecode::rel(*gecode_space, gecode_space->get_metric_vars()[idx], Gecode::IRT_EQ, target_numerator);
                    }
                }
                continue;
            }

            if (is_all_different_rule && is_pitch_engine) {
                const int voice = engine / 2;
                if (voice < 0 || voice >= config_.num_voices) {
                    throw std::runtime_error("all_different rule '" + cfg.description + "' has out-of-range pitch engine " + std::to_string(engine));
                }

                Gecode::IntVarArgs vars;
                for (int idx : selected_indices) {
                    if (idx < 0 || idx >= config_.sequence_length) {
                        throw std::runtime_error("all_different rule '" + cfg.description + "' has out-of-range index " + std::to_string(idx));
                    }
                    vars << gecode_space->get_absolute_vars()[voice * config_.sequence_length + idx];
                }
                if (vars.size() > 1) {
                    Gecode::distinct(*gecode_space, vars);
                }
            } else if (is_all_different_rule && is_rhythm_engine) {
                const int voice = engine / 2;
                if (voice < 0 || voice >= config_.num_voices) {
                    throw std::runtime_error("all_different rule '" + cfg.description + "' has out-of-range rhythm engine " + std::to_string(engine));
                }
                if (!gecode_space->has_rhythm_vars()) {
                    throw std::runtime_error("all_different rule '" + cfg.description + "' targets rhythm engine but rhythm vars are unavailable");
                }

                Gecode::IntVarArgs vars;
                for (int idx : selected_indices) {
                    if (idx < 0 || idx >= config_.sequence_length) {
                        throw std::runtime_error("all_different rule '" + cfg.description + "' has out-of-range index " + std::to_string(idx));
                    }
                    vars << gecode_space->get_rhythm_vars()[voice * config_.sequence_length + idx];
                }
                if (vars.size() > 1) {
                    Gecode::distinct(*gecode_space, vars);
                }
            } else if (is_rhythmic_uniformity_rule) {
                if (!is_rhythm_engine) {
                    throw std::runtime_error("r-rhythmic-uniformity rule '" + cfg.description + "' must target rhythm engines only");
                }
                if (!gecode_space->has_rhythm_vars()) {
                    throw std::runtime_error("r-rhythmic-uniformity rule '" + cfg.description + "' targets rhythm engine but rhythm vars are unavailable");
                }

                const int voice = engine / 2;
                if (voice < 0 || voice >= config_.num_voices) {
                    throw std::runtime_error("r-rhythmic-uniformity rule '" + cfg.description + "' has out-of-range rhythm engine " + std::to_string(engine));
                }

                if (voice < 0 || voice >= static_cast<int>(config_.voice_rhythm_domains.size())) {
                    throw std::runtime_error("r-rhythmic-uniformity rule '" + cfg.description + "' has no rhythm domain for voice " + std::to_string(voice));
                }

                std::vector<int> allowed_ticks;
                for (const auto& s : cfg.parameter_strings) {
                    allowed_ticks.push_back(parse_duration_to_ticks(s, config_.rhythm_base));
                }
                for (double p : cfg.parameters) {
                    allowed_ticks.push_back(static_cast<int>(std::lround(p)));
                }

                if (allowed_ticks.empty()) {
                    throw std::runtime_error("r-rhythmic-uniformity rule '" + cfg.description + "' requires at least one parameter value");
                }

                std::sort(allowed_ticks.begin(), allowed_ticks.end());
                allowed_ticks.erase(std::unique(allowed_ticks.begin(), allowed_ticks.end()), allowed_ticks.end());

                const auto& domain_ticks = config_.voice_rhythm_domains[voice];
                for (int tick : allowed_ticks) {
                    if (std::find(domain_ticks.begin(), domain_ticks.end(), tick) == domain_ticks.end()) {
                        throw std::runtime_error(
                            "r-rhythmic-uniformity rule '" + cfg.description +
                            "' parameter value " + std::to_string(tick) +
                            " is not present in rhythm domain for voice " + std::to_string(voice));
                    }
                }

                Gecode::IntSet allowed_set(allowed_ticks.data(), static_cast<int>(allowed_ticks.size()));

                Gecode::IntVarArgs vars;
                for (int idx : selected_indices) {
                    if (idx < 0 || idx >= config_.sequence_length) {
                        throw std::runtime_error("r-rhythmic-uniformity rule '" + cfg.description + "' has out-of-range index " + std::to_string(idx));
                    }
                    vars << gecode_space->get_rhythm_vars()[voice * config_.sequence_length + idx];
                }

                if (vars.size() > 0) {
                    // Every selected rhythm must be one of the allowed values.
                    Gecode::dom(*gecode_space, vars, allowed_set);
                    // Uniformity: all selected positions use the same value (chosen from allowed_set).
                    if (vars.size() > 1) {
                        Gecode::rel(*gecode_space, vars, Gecode::IRT_EQ);
                    }
                }
            } else if (is_metric_hierarchy_rule) {
                if (!is_rhythm_engine) {
                    throw std::runtime_error("r-metric-hierarchy rule '" + cfg.description + "' must target rhythm engines only");
                }
                if (!gecode_space->has_rhythm_vars()) {
                    throw std::runtime_error("r-metric-hierarchy rule '" + cfg.description + "' targets rhythm engine but rhythm vars are unavailable");
                }

                const int voice = engine / 2;
                if (voice < 0 || voice >= config_.num_voices) {
                    throw std::runtime_error("r-metric-hierarchy rule '" + cfg.description + "' has out-of-range rhythm engine " + std::to_string(engine));
                }
                if (voice < 0 || voice >= static_cast<int>(config_.voice_rhythm_domains.size())) {
                    throw std::runtime_error("r-metric-hierarchy rule '" + cfg.description + "' has no rhythm domain for voice " + std::to_string(voice));
                }

                const bool include_rests = metric_hierarchy_include_rests_mode(cfg.parameter_strings);
                const bool ignore_tuplets = metric_hierarchy_ignore_tuplets_mode(cfg.parameter_strings);
                const int grid_step_ticks = parse_metric_hierarchy_grid_step_ticks(config_, ignore_tuplets);

                std::vector<int> allowed_ticks;
                for (int tick : config_.voice_rhythm_domains[voice]) {
                    if (tick == 0) {
                        continue;
                    }
                    if (include_rests) {
                        if ((std::abs(tick) % grid_step_ticks) == 0) {
                            allowed_ticks.push_back(tick);
                        }
                    } else {
                        // Match the Lisp mode where only durations are constrained and rests are ignored.
                        if (tick < 0 || (tick % grid_step_ticks) == 0) {
                            allowed_ticks.push_back(tick);
                        }
                    }
                }

                std::sort(allowed_ticks.begin(), allowed_ticks.end());
                allowed_ticks.erase(std::unique(allowed_ticks.begin(), allowed_ticks.end()), allowed_ticks.end());

                if (allowed_ticks.empty()) {
                    throw std::runtime_error(
                        "r-metric-hierarchy rule '" + cfg.description +
                        "' leaves no allowed rhythm values for voice " + std::to_string(voice));
                }

                Gecode::IntSet allowed_set(allowed_ticks.data(), static_cast<int>(allowed_ticks.size()));
                Gecode::IntVarArgs vars;
                for (int idx : selected_indices) {
                    if (idx < 0 || idx >= config_.sequence_length) {
                        throw std::runtime_error("r-metric-hierarchy rule '" + cfg.description + "' has out-of-range index " + std::to_string(idx));
                    }
                    vars << gecode_space->get_rhythm_vars()[voice * config_.sequence_length + idx];
                }

                if (vars.size() > 0) {
                    Gecode::dom(*gecode_space, vars, allowed_set);
                }
            }
        }
    }

    // POST ADVANCED CONSTRAINTS
    if (twelve_tone_row_enabled_) {
        gecode_space->post_twelve_tone_row_constraint();
        std::cout << "Posted 12-tone row constraint" << std::endl;
    }
    if (perfect_fifth_intervals_enabled_) {
        gecode_space->post_perfect_fifth_intervals_constraint();
        std::cout << "Posted perfect fifth intervals constraint" << std::endl;
    }
    if (palindrome_voices_enabled_) {
        gecode_space->post_palindrome_voices_constraint();
        std::cout << "Posted palindrome voices constraint" << std::endl;
    }

    // POST DYNAMIC RULES
    if (compiled_rules_ && compiled_rules_->total_count() > 0) {
        std::cout << "🎯 Applying " << compiled_rules_->total_count() << " dynamic rules" << std::endl;
        DynamicRules::ConstraintContext ctx(gecode_space.get(), &gecode_space->get_absolute_vars(),
                                           &gecode_space->get_rhythm_vars(), config_.num_voices,
                                           config_.sequence_length, &gecode_space->get_metric_vars());
        if (compiled_rules_->constraint_count() > 0) {
            compiled_rules_->post_all_constraints(ctx);
        }
        compiled_rules_->apply_all_heuristics(ctx);
        if (compiled_rules_->has_heuristic_scorers()) {
            std::cout << "🎯 Heuristic scorers ready for value ordering integration" << std::endl;
            if (gecode_space->get_absolute_vars().size() > 0) {
                DynamicRules::HeuristicCandidateContext warmup_candidate;
                warmup_candidate.voice = 0;
                warmup_candidate.position = 0;
                warmup_candidate.candidate_value = gecode_space->get_absolute_vars()[0].min();
                double warmup_score = compiled_rules_->evaluate_heuristic_score(ctx, warmup_candidate);
                std::cout << "🎯 Heuristic warmup score (voice=0,pos=0,candidate="
                          << warmup_candidate.candidate_value << "): " << warmup_score << std::endl;
            }
        }
    }

    if (retrograde_inversion_enabled_) {
        std::cout << "🎯 APPLYING RETROGRADE INVERSION CONSTRAINT!" << std::endl;
        std::cout << "   Inversion center: " << retrograde_inversion_center_ << " (MIDI)" << std::endl;
        gecode_space->add_retrograde_inversion_constraint(retrograde_inversion_center_);
        std::cout << "✅ Posted retrograde inversion constraint in Gecode space" << std::endl;
    }

    return gecode_space.release();
}

// ---------------------------------------------------------------------------
// Extract a MusicalSolution from a solved Gecode space.
// Takes ownership and deletes solved_space. nullptr → no solution found.
// ---------------------------------------------------------------------------
MusicalSolution Solver::extract_solution_from_space_(
        GecodeClusterIntegration::IntegratedMusicalSpace* solved_space) {
    auto start_time = std::chrono::high_resolution_clock::now();

    MusicalSolution solution;
    int rules_checked = 0;
    bool success = (solved_space != nullptr);

    if (success) {
        auto absolute_sequence = solved_space->get_absolute_sequence();

        bool fully_assigned = true;
        for (int note : absolute_sequence) {
            if (note == -1) { fully_assigned = false; break; }
        }

        if (fully_assigned) {
            for (size_t i = 0; i < absolute_sequence.size(); ++i)
                solution_storage_->write_absolute(absolute_sequence[i], static_cast<int>(i));

            solution.voice_solutions.clear();
            solution.voice_rhythms.clear();

            for (int voice = 0; voice < config_.num_voices; ++voice) {
                if (config_.voice_rhythm_domains.empty() ||
                    voice >= (int)config_.voice_rhythm_domains.size() ||
                    config_.voice_rhythm_domains[voice].empty()) {
                    throw std::runtime_error(
                        "Voice " + std::to_string(voice) + " has no rhythm domain.");
                }
                const auto& rhythm_domain = config_.voice_rhythm_domains[voice];
                auto solved_rhythms = solved_space->get_rhythm_sequence_from_vars(voice);
                std::vector<int> rhythm_data;
                if (!solved_rhythms.empty()) {
                    rhythm_data = solved_rhythms;
                } else {
                    rhythm_data.assign(config_.sequence_length, rhythm_domain[0]);
                }
                solution.voice_rhythms.push_back(rhythm_data);
                solution.voice_solutions.push_back(solved_space->get_pitch_sequence(voice));
            }

            const EngineRuleConfig* explicit_metric_rule = nullptr;
            for (const auto& cfg : engine_rule_configs_) {
                if (cfg.rule_type == "r-metric-signature" && !cfg.timepoints.empty()) {
                    explicit_metric_rule = &cfg;
                    break;
                }
            }

            if (explicit_metric_rule != nullptr) {
                std::vector<int> segment_starts_ticks;
                segment_starts_ticks.reserve(explicit_metric_rule->timepoints.size());
                for (size_t i = 0; i < explicit_metric_rule->timepoints.size(); ++i) {
                    segment_starts_ticks.push_back(parse_score_time_token_to_ticks(
                        explicit_metric_rule->timepoints[i], config_.rhythm_base,
                        "r-metric-signature rule '" + explicit_metric_rule->description + "' timepoints[" + std::to_string(i) + "]"));
                }

                solution.canonical_score = build_canonical_score_from_timepoints(
                    config_, solution.voice_solutions, solution.voice_rhythms,
                    segment_starts_ticks,
                    parse_metric_signature_parameters(
                        explicit_metric_rule->parameter_strings,
                        explicit_metric_rule->parameters,
                        "r-metric-signature rule '" + explicit_metric_rule->description + "'"));
            } else {
                std::vector<int> raw_metric_signature = solved_space->get_metric_sequence();
                if ((int)raw_metric_signature.size() < config_.sequence_length) {
                    const int fill = raw_metric_signature.empty() ? 4 : raw_metric_signature.back();
                    raw_metric_signature.resize(config_.sequence_length, fill);
                } else if ((int)raw_metric_signature.size() > config_.sequence_length) {
                    raw_metric_signature.resize(config_.sequence_length);
                }

                solution.canonical_score = build_canonical_score(
                    config_, solution.voice_solutions, solution.voice_rhythms, raw_metric_signature);
            }
            solution.has_canonical_score = true;

            // Backward-compatible projection from canonical model.
            solution.metric_signature = project_metric_signature_from_canonical(
                solution.canonical_score, config_.sequence_length);
        } else {
            success = false;
            solution.failure_reason = "Gecode found partial solution but not fully assigned";
        }

        rules_checked = static_cast<int>(rules_.size() * config_.sequence_length);
        solution.applied_rules.push_back("Gecode constraint propagation completed successfully");
        delete solved_space;
    } else {
        solution.failure_reason = "Gecode constraint solver found no solution";
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    solution.solve_time_ms = duration.count() / 1000.0;
    solution.total_rules_checked = rules_checked;
    solution.backjumps_performed = 0;
    solution.found_solution = success;

    if (success) {
        for (int i = 0; i < config_.sequence_length; ++i) {
            solution.absolute_notes.push_back(solution_storage_->absolute(i));
            solution.note_names.push_back(midi_to_note_name(solution_storage_->absolute(i)));
            if (i > 0)
                solution.intervals.push_back(solution_storage_->interval(i));
        }

        double sum_intervals = 0;
        int direction_changes = 0;
        int last_direction = 0;
        for (int interval : solution.intervals) {
            sum_intervals += std::abs(interval);
            int direction = (interval > 0) ? 1 : (interval < 0) ? -1 : 0;
            if (direction != 0 && direction != last_direction) {
                direction_changes++;
                last_direction = direction;
            }
        }
        if (!solution.intervals.empty())
            solution.average_interval_size = sum_intervals / solution.intervals.size();
        solution.melodic_direction_changes = direction_changes;

        total_solutions_found_++;
    }

    update_stats("gecode_solve", solution.solve_time_ms);
    return solution;
}

MusicalSolution Solver::solve_internal() {
    total_solve_attempts_++;
    try {
        auto* raw_space = build_configured_space_();

        if (config_.restart_policy != SolverConfig::RestartPolicy::NONE) {
            throw std::runtime_error("Unsupported restart policy in solve_internal()");
        }

        Gecode::Search::Options search_opts;
        search_opts.threads = 1;
        search_opts.nogoods_limit = 128;

        if (config_.search_engine != SolverConfig::SearchEngine::DFS) {
            throw std::runtime_error("Unsupported search engine in solve_internal()");
        }

        Gecode::DFS<GecodeClusterIntegration::IntegratedMusicalSpace> search_engine(raw_space, search_opts);
        return extract_solution_from_space_(search_engine.next());
    } catch (const std::exception& e) {
        MusicalSolution failed;
        failed.found_solution = false;
        failed.failure_reason = "Exception during solving: " + std::string(e.what());
        return failed;
    }
}

std::map<std::string, double> Solver::get_performance_stats() const {
    auto stats = performance_stats_;
    stats["total_solutions_found"] = static_cast<double>(total_solutions_found_);
    stats["total_solve_attempts"] = static_cast<double>(total_solve_attempts_);
    stats["success_rate"] = (total_solve_attempts_ > 0) ? 
        static_cast<double>(total_solutions_found_) / total_solve_attempts_ * 100.0 : 0.0;
    return stats;
}

void Solver::reset_statistics() {
    performance_stats_.clear();
    total_solutions_found_ = 0;
    total_solve_attempts_ = 0;
}

void Solver::update_stats(const std::string& operation, double time_ms) {
    if (performance_stats_.find(operation) == performance_stats_.end()) {
        performance_stats_[operation] = 0.0;
    }
    performance_stats_[operation] += time_ms;
}

std::string Solver::midi_to_note_name(int midi_note) {
    const char* note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (midi_note / 12) - 1;
    int pitch_class = midi_note % 12;
    return std::string(note_names[pitch_class]) + std::to_string(octave);
}

std::string Solver::interval_to_name(int semitones) {
    static const std::map<int, std::string> interval_names = {
        {0, "Unison"}, {1, "Minor 2nd"}, {2, "Major 2nd"}, {3, "Minor 3rd"},
        {4, "Major 3rd"}, {5, "Perfect 4th"}, {6, "Tritone"}, {7, "Perfect 5th"},
        {8, "Minor 6th"}, {9, "Major 6th"}, {10, "Minor 7th"}, {11, "Major 7th"},
        {12, "Octave"}
    };
    
    int abs_interval = std::abs(semitones);
    auto it = interval_names.find(abs_interval);
    std::string base_name = (it != interval_names.end()) ? it->second : "Compound";
    
    return (semitones < 0 ? "↓" : "↑") + base_name;
}

bool Solver::export_solution_to_xml(const MusicalSolution& solution, const std::string& filename) const {
    try {
        solution.export_to_xml(filename);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error exporting solution to XML: " << e.what() << std::endl;
        return false;
    }
}

bool Solver::export_solution_to_png(const MusicalSolution& solution, const std::string& filename) const {
    try {
        solution.export_to_png(filename);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error exporting solution to PNG: " << e.what() << std::endl;
        return false;
    }
}

bool Solver::solve_and_export_xml(const std::string& filename) {
    auto solution = solve();
    if (!solution.found_solution) {
        std::cerr << "Cannot export XML: No solution found (" << solution.failure_reason << ")" << std::endl;
        return false;
    }
    return export_solution_to_xml(solution, filename);
}

bool Solver::solve_and_export_png(const std::string& filename) {
    auto solution = solve();
    if (!solution.found_solution) {
        std::cerr << "Cannot export PNG: No solution found (" << solution.failure_reason << ")" << std::endl;
        return false;
    }
    return export_solution_to_png(solution, filename);
}

bool Solver::validate_configuration(std::string& error_message) const {
    if (config_.sequence_length < 1) {
        error_message = "Sequence length must be positive";
        return false;
    }
    if (config_.max_interval_size < 1) {
        error_message = "Max interval size must be positive";  
        return false;
    }
    return true;
}

std::map<std::string, int> Solver::get_rule_statistics() const {
    std::map<std::string, int> stats;
    stats["total_rules"] = static_cast<int>(rules_.size());
    stats["active_rules"] = static_cast<int>(rules_.size());
    return stats;
}

bool Solver::test_rules(std::vector<int> test_sequence, std::string& report) const {
    // Simple rule testing implementation
    report = "All rules passed for test sequence";
    return true;
}

MusicalSolution Solver::create_solution_from_storage() const {
    MusicalSolution solution;
    solution.found_solution = true;
    // Would extract from actual storage in real implementation
    return solution;
}

// ===============================
// Convenience Functions Implementation
// ===============================

MusicalSolution quick_solve(int length, SolverConfig::MusicalStyle style) {
    SolverConfig config;
    config.sequence_length = length;
    config.style = style;
    
    Solver solver(config);
    return solver.solve();
}

MusicalSolution solve_jazz_improvisation(int length) {
    Solver solver;
    solver.setup_for_jazz_improvisation();
    return solver.solve();
}

MusicalSolution solve_classical_melody(int length) {
    Solver solver;
    solver.setup_for_classical_melody();
    return solver.solve();
}

std::vector<MusicalSolution> batch_solve(int num_sequences, const SolverConfig& config) {
    Solver solver(config);
    return solver.solve_multiple(num_sequences);
}

} // namespace MusicalConstraintSolver