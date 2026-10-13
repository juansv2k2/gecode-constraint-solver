/**
 * @file repetition_rules.hh
 * @brief r-repetition rule type: relates two equal-length position ranges within a voice.
 *
 * Covers sentence-style repetition, harmonic sequence, antecedent/consequent
 * parallelism, and "same tune, different harmony" — per the design doc, all
 * of these are one relation applied between a source range and a target
 * range, just with a different `relation` mode:
 *
 *   exact               target[i] == source[i]
 *   transposed          target[i] == source[i] + transpose_semitones
 *   contour_preserving  each consecutive step in target moves the same
 *                       direction (up/down/same) as the matching step in source
 *   rhythm_preserving   target's *rhythm* matches source's rhythm (pitch free)
 *   diatonic_sequence   target[i] is source[i] shifted by sequence_step
 *                       diatonic scale steps within the declared key (a real
 *                       harmonic sequence, e.g. descending 5-6) — requires a
 *                       key at both the source and target positions
 *
 * Same field-naming rule as r-cadence: flat top-level fields, nothing named
 * "parameters" as a JSON object (the legacy line-scanner special-cases that
 * name as an array and will hang on an object value).
 *
 * v1 scope note: diatonic_sequence is hard-only. Scoring it per-candidate
 * would need the same table built once per candidate value, which is
 * possible but not implemented yet — flagged here rather than silently
 * omitted from the heuristic path.
 */

#ifndef REPETITION_RULES_HH
#define REPETITION_RULES_HH

#include "dynamic_rule_compiler.hh"
#include "musical_constraint_solver.hh"
#include "cadence_rules.hh"  // reuses HarmonicEntry lookup + degree helpers
#include <gecode/int.hh>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace RepetitionRules {

using HarmonicConfig = MusicalConstraintSolver::SolverConfig::HarmonicConfig;
using HarmonicEntry  = MusicalConstraintSolver::SolverConfig::HarmonicEntry;

struct RepetitionParams {
    std::string id;
    std::string relation;  // exact, transposed, contour_preserving, rhythm_preserving, diatonic_sequence
    std::vector<int> source_positions;
    std::vector<int> target_positions;
    std::vector<int> target_voices;  // relation applied independently per voice
    int transpose_semitones = 0;     // required for "transposed"
    int sequence_step = 0;           // required for "diatonic_sequence"
    bool heuristic = false;
};

inline RepetitionParams resolve_params(const nlohmann::json& rule_json) {
    RepetitionParams p;
    p.id = rule_json.value("id", std::string("r-repetition"));
    p.relation = rule_json.value("relation", std::string(""));

    static const std::vector<std::string> VALID_RELATIONS = {
        "exact", "transposed", "contour_preserving", "rhythm_preserving", "diatonic_sequence"
    };
    if (std::find(VALID_RELATIONS.begin(), VALID_RELATIONS.end(), p.relation) == VALID_RELATIONS.end())
        throw std::runtime_error(
            "r-repetition '" + p.id + "': unknown relation '" + p.relation +
            "' (expected exact, transposed, contour_preserving, rhythm_preserving, diatonic_sequence)");

    if (rule_json.contains("source_positions") && rule_json["source_positions"].is_array())
        for (const auto& v : rule_json["source_positions"])
            if (v.is_number_integer()) p.source_positions.push_back(v.get<int>());
    if (rule_json.contains("target_positions") && rule_json["target_positions"].is_array())
        for (const auto& v : rule_json["target_positions"])
            if (v.is_number_integer()) p.target_positions.push_back(v.get<int>());
    if (p.source_positions.empty() || p.source_positions.size() != p.target_positions.size())
        throw std::runtime_error(
            "r-repetition '" + p.id + "': source_positions and target_positions must be "
            "non-empty and the same length");

    if (rule_json.contains("target_voices") && rule_json["target_voices"].is_array())
        for (const auto& v : rule_json["target_voices"])
            if (v.is_number_integer()) p.target_voices.push_back(v.get<int>());
    if (p.target_voices.empty())
        throw std::runtime_error("r-repetition '" + p.id + "': target_voices is required");

    if (p.relation == "transposed") {
        if (!rule_json.contains("transpose_semitones"))
            throw std::runtime_error(
                "r-repetition '" + p.id + "': relation 'transposed' requires transpose_semitones");
        p.transpose_semitones = rule_json.value("transpose_semitones", 0);
    }
    if (p.relation == "contour_preserving" && p.source_positions.size() < 2)
        throw std::runtime_error(
            "r-repetition '" + p.id + "': relation 'contour_preserving' needs at least 2 "
            "positions per range to compare consecutive steps");
    if (p.relation == "diatonic_sequence") {
        if (!rule_json.contains("sequence_step"))
            throw std::runtime_error(
                "r-repetition '" + p.id + "': relation 'diatonic_sequence' requires sequence_step");
        p.sequence_step = rule_json.value("sequence_step", 0);
    }

    if (rule_json.contains("heuristic")) {
        const auto& value = rule_json["heuristic"];
        if (!value.is_boolean() && !value.is_number_integer())
            throw std::runtime_error("r-repetition '" + p.id + "': heuristic must be boolean or integer");
        p.heuristic = value.is_boolean() ? value.get<bool>() : value.get<int>() != 0;
    }
    if (p.heuristic && p.relation == "diatonic_sequence")
        throw std::runtime_error(
            "r-repetition '" + p.id + "': relation 'diatonic_sequence' does not support "
            "heuristic mode in v1 — hard constraint only");

    return p;
}

// Diatonic scale-step transposition of an absolute pitch: shifts by `step`
// scale degrees (not semitones) within (key_tonic, key_mode), wrapping octaves
// correctly in both directions. Returns -1 if `p`'s pitch class isn't diatonic
// in this key (chromatic pitches have no defined diatonic-sequence target).
inline int diatonic_shift(int p, int key_tonic, int key_mode, int step) {
    static const int MAJOR_STEPS[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int MINOR_STEPS[7] = {0, 2, 3, 5, 7, 8, 10};
    const int* steps = (key_mode == 0) ? MAJOR_STEPS : MINOR_STEPS;

    auto floor_div = [](long long a, long long b) { long long q = a / b, r = a % b; if (r != 0 && ((r < 0) != (b < 0))) --q; return q; };
    auto floor_mod = [](long long a, long long b) { long long r = a % b; if (r != 0 && ((r < 0) != (b < 0))) r += b; return r; };

    const int rel = p - key_tonic;
    const int octave = floor_div(rel, 12);
    const int pc_rel = floor_mod(rel, 12);

    int degree_index = -1;
    for (int d = 0; d < 7; ++d) if (steps[d] == pc_rel) { degree_index = d; break; }
    if (degree_index < 0) return -1;  // chromatic — no diatonic-sequence target defined

    const int scale_index     = octave * 7 + degree_index;
    const long long new_scale_index = static_cast<long long>(scale_index) + step;
    const long long new_octave      = floor_div(new_scale_index, 7);
    const int new_degree            = static_cast<int>(floor_mod(new_scale_index, 7));
    const long long shifted = key_tonic + new_octave * 12 + steps[new_degree];
    if (shifted < std::numeric_limits<int>::min() || shifted > std::numeric_limits<int>::max()) return -1;
    return static_cast<int>(shifted);
}

inline void validate_measure_addresses(const RepetitionParams& params, int voices, int capacity) {
    for (int voice : params.target_voices)
        if (voice < 0 || voice >= voices) throw std::runtime_error("Repetition target voice out of range");
    for (const auto& positions : {params.source_positions, params.target_positions})
        for (int position : positions)
            if (position < 0 || position >= capacity) throw std::runtime_error("Repetition index outside event capacity");
}

inline Gecode::IntVar key_at_event(DynamicRules::ConstraintContext& ctx, int voice, int event,
                                  Gecode::BoolVar sounded, const HarmonicConfig& harmony) {
    if (!harmony.enabled || harmony.entries.empty() || ctx.rhythm_base < 4 || ctx.rhythm_base % 4 != 0)
        throw std::runtime_error("Measure diatonic sequence requires keyed harmony and quarter-note tick alignment");
    ctx.space->initialize_event_timing();
    const auto onset = ctx.space->get_event_boundary(voice, event);
    Gecode::IntVar key(*ctx.space, 0, 23);
    Gecode::BoolVarArgs coverage;
    for (size_t segment = 0; segment < harmony.entries.size(); ++segment) {
        const auto& entry = harmony.entries[segment];
        const long long lower = static_cast<long long>(entry.beat_position) * (ctx.rhythm_base / 4);
        if (lower < 0 || lower > Gecode::Int::Limits::max) throw std::runtime_error("Diatonic harmony beat out of range");
        Gecode::BoolExpr applies = sounded && (onset >= static_cast<int>(lower));
        if (segment + 1 < harmony.entries.size()) {
            const long long upper = static_cast<long long>(harmony.entries[segment + 1].beat_position) * (ctx.rhythm_base / 4);
            if (upper <= lower || upper > Gecode::Int::Limits::max)
                throw std::runtime_error("Diatonic harmony beats must be strictly increasing");
            applies = applies && (onset < static_cast<int>(upper));
        }
        if (entry.key_tonic < 0) continue;
        Gecode::BoolVar selected = Gecode::expr(*ctx.space, applies);
        Gecode::rel(*ctx.space, key, Gecode::IRT_EQ, entry.key_tonic * 2 + entry.key_mode,
                    Gecode::Reify(selected, Gecode::RM_IMP));
        coverage << selected;
    }
    if (coverage.size() == 0) throw std::runtime_error("Diatonic sequence requires a declared key");
    Gecode::linear(*ctx.space, coverage, Gecode::IRT_EQ, 1, Gecode::Reify(sounded, Gecode::RM_IMP));
    return key;
}

inline void post_measure_repetition(DynamicRules::ConstraintContext& ctx, const RepetitionParams& params,
                                    const HarmonicConfig& harmony) {
    validate_measure_addresses(params, ctx.num_voices, ctx.sequence_length);
    if (!ctx.pitch_vars || !ctx.rhythm_vars) throw std::runtime_error("Measure repetition requires pitch and rhythm variables");
    for (int voice : params.target_voices) {
        if (voice < 0 || voice >= ctx.num_voices) throw std::runtime_error("Repetition target voice out of range");
        std::vector<Gecode::BoolVar> paired_notes;
        for (size_t pair = 0; pair < params.source_positions.size(); ++pair) {
            const int source = params.source_positions[pair], target = params.target_positions[pair];
            if (source < 0 || target < 0 || source >= ctx.sequence_length || target >= ctx.sequence_length)
                throw std::runtime_error("Repetition index outside event capacity");
            const int source_index = voice * ctx.sequence_length + source;
            const int target_index = voice * ctx.sequence_length + target;
            Gecode::rel(*ctx.space, ctx.space->get_active_vars()[source_index], Gecode::IRT_EQ, 1);
            Gecode::rel(*ctx.space, ctx.space->get_active_vars()[target_index], Gecode::IRT_EQ, 1);
            const auto source_rhythm = (*ctx.rhythm_vars)[source_index], target_rhythm = (*ctx.rhythm_vars)[target_index];
            Gecode::BoolVar source_note = Gecode::expr(*ctx.space, source_rhythm > 0);
            Gecode::BoolVar target_note = Gecode::expr(*ctx.space, target_rhythm > 0);
            if (params.relation != "rhythm_preserving") Gecode::rel(*ctx.space, source_note, Gecode::IRT_EQ, target_note);
            Gecode::BoolVar notes = Gecode::expr(*ctx.space, source_note && target_note);
            paired_notes.push_back(notes);
            const auto source_pitch = (*ctx.pitch_vars)[source_index], target_pitch = (*ctx.pitch_vars)[target_index];
            if (params.relation == "exact") {
                Gecode::rel(*ctx.space, target_pitch, Gecode::IRT_EQ, source_pitch, Gecode::Reify(notes, Gecode::RM_IMP));
            } else if (params.relation == "transposed") {
                Gecode::linear(*ctx.space, Gecode::IntArgs({1,-1}), Gecode::IntVarArgs({target_pitch,source_pitch}),
                    Gecode::IRT_EQ, params.transpose_semitones, Gecode::Reify(notes, Gecode::RM_IMP));
            } else if (params.relation == "rhythm_preserving") {
                Gecode::rel(*ctx.space, target_rhythm, Gecode::IRT_EQ, source_rhythm);
            } else if (params.relation == "diatonic_sequence") {
                const auto source_key = key_at_event(ctx, voice, source, notes, harmony);
                const auto target_key = key_at_event(ctx, voice, target, notes, harmony);
                Gecode::rel(*ctx.space, source_key, Gecode::IRT_EQ, target_key, Gecode::Reify(notes, Gecode::RM_IMP));
                Gecode::TupleSet tuples(3);
                std::set<int> keys;
                for (const auto& entry : harmony.entries) if (entry.key_tonic >= 0) keys.insert(entry.key_tonic * 2 + entry.key_mode);
                for (int key : keys) for (int pitch = 0; pitch <= 127; ++pitch) {
                    if (!source_pitch.in(pitch)) continue;
                    const int shifted = diatonic_shift(pitch, key / 2, key % 2, params.sequence_step);
                    if (shifted >= 0 && shifted <= 127 && target_pitch.in(shifted)) tuples.add(Gecode::IntArgs({key,pitch,shifted}));
                }
                tuples.finalize();
                Gecode::extensional(*ctx.space, Gecode::IntVarArgs({source_key,source_pitch,target_pitch}), tuples,
                                    Gecode::Reify(notes, Gecode::RM_IMP));
            }
        }
        if (params.relation == "contour_preserving") {
            for (size_t pair = 1; pair < paired_notes.size(); ++pair) {
                const auto source_previous = (*ctx.pitch_vars)[voice * ctx.sequence_length + params.source_positions[pair - 1]];
                const auto source_current = (*ctx.pitch_vars)[voice * ctx.sequence_length + params.source_positions[pair]];
                const auto target_previous = (*ctx.pitch_vars)[voice * ctx.sequence_length + params.target_positions[pair - 1]];
                const auto target_current = (*ctx.pitch_vars)[voice * ctx.sequence_length + params.target_positions[pair]];
                Gecode::BoolVar compare = Gecode::expr(*ctx.space, paired_notes[pair - 1] && paired_notes[pair]);
                Gecode::BoolVar source_up = Gecode::expr(*ctx.space, source_current > source_previous);
                Gecode::BoolVar target_up = Gecode::expr(*ctx.space, target_current > target_previous);
                Gecode::BoolVar source_down = Gecode::expr(*ctx.space, source_current < source_previous);
                Gecode::BoolVar target_down = Gecode::expr(*ctx.space, target_current < target_previous);
                Gecode::rel(*ctx.space, source_up, Gecode::IRT_EQ, target_up, Gecode::Reify(compare, Gecode::RM_IMP));
                Gecode::rel(*ctx.space, source_down, Gecode::IRT_EQ, target_down, Gecode::Reify(compare, Gecode::RM_IMP));
            }
        }
    }
}

inline void post_repetition_constraint(DynamicRules::ConstraintContext& ctx,
                                        const RepetitionParams& p,
                                        const HarmonicConfig& harmonic_domain) {
    if (ctx.space && ctx.space->is_measure_count_mode()) {
        post_measure_repetition(ctx, p, harmonic_domain);
        return;
    }
    const int n = (int)p.source_positions.size();

    for (int voice : p.target_voices) {
        if (voice < 0 || voice >= ctx.num_voices)
            throw std::runtime_error("r-repetition '" + p.id + "': voice out of range");

        auto pitch_at = [&](int pos) -> Gecode::IntVar& {
            const int idx = voice * ctx.sequence_length + pos;
            if (!ctx.pitch_vars || idx < 0 || idx >= (int)ctx.pitch_vars->size())
                throw std::runtime_error("r-repetition '" + p.id + "': position out of range");
            return (*ctx.pitch_vars)[idx];
        };
        auto rhythm_at = [&](int pos) -> Gecode::IntVar& {
            const int idx = voice * ctx.sequence_length + pos;
            if (!ctx.rhythm_vars || idx < 0 || idx >= (int)ctx.rhythm_vars->size())
                throw std::runtime_error(
                    "r-repetition '" + p.id + "': rhythm_preserving requires rhythm_vars, "
                    "none available at this position");
            return (*ctx.rhythm_vars)[idx];
        };

        if (p.relation == "exact") {
            for (int i = 0; i < n; ++i)
                Gecode::rel(*ctx.space, pitch_at(p.target_positions[i]), Gecode::IRT_EQ,
                            pitch_at(p.source_positions[i]));

        } else if (p.relation == "transposed") {
            for (int i = 0; i < n; ++i) {
                Gecode::IntVar shifted = Gecode::expr(*ctx.space,
                    pitch_at(p.source_positions[i]) + p.transpose_semitones);
                Gecode::rel(*ctx.space, pitch_at(p.target_positions[i]), Gecode::IRT_EQ, shifted);
            }

        } else if (p.relation == "rhythm_preserving") {
            for (int i = 0; i < n; ++i)
                Gecode::rel(*ctx.space, rhythm_at(p.target_positions[i]), Gecode::IRT_EQ,
                            rhythm_at(p.source_positions[i]));

        } else if (p.relation == "contour_preserving") {
            // sign(x) via pos/neg reification, matching the pattern used for
            // contrary_motion in r-pitch-pitch: sign = pos - neg gives -1/0/1.
            auto sign_of_step = [&](Gecode::IntVar& a, Gecode::IntVar& b) -> Gecode::IntVar {
                Gecode::IntVar diff = Gecode::expr(*ctx.space, b - a);
                Gecode::BoolVar pos(*ctx.space, 0, 1), neg(*ctx.space, 0, 1);
                Gecode::rel(*ctx.space, diff, Gecode::IRT_GR, 0, pos);
                Gecode::rel(*ctx.space, diff, Gecode::IRT_LE, -1, neg);
                return Gecode::expr(*ctx.space, pos - neg);
            };
            for (int i = 0; i + 1 < n; ++i) {
                Gecode::IntVar sign_s = sign_of_step(pitch_at(p.source_positions[i]),
                                                      pitch_at(p.source_positions[i + 1]));
                Gecode::IntVar sign_t = sign_of_step(pitch_at(p.target_positions[i]),
                                                      pitch_at(p.target_positions[i + 1]));
                Gecode::rel(*ctx.space, sign_s, Gecode::IRT_EQ, sign_t);
            }

        } else if (p.relation == "diatonic_sequence") {
            for (int i = 0; i < n; ++i) {
                Gecode::IntVar& src = pitch_at(p.source_positions[i]);
                Gecode::IntVar& tgt = pitch_at(p.target_positions[i]);
                const HarmonicEntry* es = CadenceRules::find_active_entry(harmonic_domain, p.source_positions[i]);
                const HarmonicEntry* et = CadenceRules::find_active_entry(harmonic_domain, p.target_positions[i]);
                if (!es || !et || es->key_tonic < 0 || et->key_tonic < 0)
                    throw std::runtime_error(
                        "r-repetition '" + p.id + "': diatonic_sequence requires a key declared "
                        "in harmonic_domain at both source and target positions");

                // Build the (source, target) tuple table by iterating the source
                // var's actual domain values, computing each one's diatonic-shifted
                // target, and keeping only tuples within the target var's bounds —
                // this is what makes the relation exact and general without needing
                // a sentinel value for chromatic/out-of-range cases.
                Gecode::TupleSet tuples(2);  // arity 2: (source, target) — default ctor is uninitialized
                for (Gecode::IntVarValues v(src); v(); ++v) {
                    const int sv = v.val();
                    const int tv = diatonic_shift(sv, es->key_tonic, es->key_mode, p.sequence_step);
                    if (tv < 0) continue;  // source value is chromatic in this key — no valid target
                    if (tv < tgt.min() || tv > tgt.max()) continue;  // outside target's declared bounds
                    tuples.add(Gecode::IntArgs({sv, tv}));
                }
                tuples.finalize();
                if (tuples.tuples() == 0)
                    throw std::runtime_error(
                        "r-repetition '" + p.id + "': diatonic_sequence at position " +
                        std::to_string(p.source_positions[i]) + " has no valid (source, target) "
                        "pairing — sequence_step likely moves outside the target voice's domain");
                Gecode::IntVarArgs pair(2);
                pair[0] = src; pair[1] = tgt;
                Gecode::extensional(*ctx.space, pair, tuples);
            }
        }
    }
}

// Heuristic scoring for exact/transposed/contour_preserving (pitch relations
// only in v1 — rhythm_preserving and diatonic_sequence are hard-only, see
// resolve_params()). Per the design doc's own guidance, phrase/motivic
// relations default to soft, so this is the primary path for r-repetition,
// not an afterthought bolted onto the hard-constraint version above.
// Returns 0.0 when the candidate isn't at one of this rule's paired target
// positions — i.e. this rule has no opinion on it.
inline double score_candidate(const RepetitionParams& p,
                               const DynamicRules::ConstraintContext& ctx,
                               const DynamicRules::HeuristicCandidateContext& cand) {
    if (std::find(p.target_voices.begin(), p.target_voices.end(), cand.voice) == p.target_voices.end())
        return 0.0;
    if (!ctx.pitch_vars) return 0.0;

    int idx = -1;
    for (size_t i = 0; i < p.target_positions.size(); ++i)
        if (p.target_positions[i] == cand.position) { idx = (int)i; break; }
    if (idx < 0) return 0.0;

    const int src_pos = p.source_positions[idx];
    const int src_idx = cand.voice * ctx.sequence_length + src_pos;
    if (src_idx < 0 || src_idx >= (int)ctx.pitch_vars->size()) return 0.0;
    const Gecode::IntVar& src_var = (*ctx.pitch_vars)[src_idx];
    if (!src_var.assigned()) return 0.0;  // nothing to compare against yet
    const int src_val = src_var.val();
    if (ctx.space && ctx.space->is_measure_count_mode()) {
        if (!ctx.rhythm_vars || cand.voice < 0 || cand.voice >= ctx.num_voices || src_pos < 0 ||
            src_pos >= ctx.sequence_length || cand.position < 0 || cand.position >= ctx.sequence_length || cand.candidate_value < 0) return 0.0;
        std::vector<int> events{src_pos,cand.position};
        if (p.relation == "contour_preserving" && idx > 0) {
            events.push_back(p.source_positions[idx - 1]); events.push_back(p.target_positions[idx - 1]);
        }
        for (int event : events) {
            if (event < 0 || event >= ctx.sequence_length) return 0.0;
            const auto& rhythm = (*ctx.rhythm_vars)[cand.voice * ctx.sequence_length + event];
            if (!rhythm.assigned() || rhythm.val() <= 0) return 0.0;
        }
    }

    if (p.relation == "exact")
        return (cand.candidate_value == src_val) ? 1.0 : -0.5;
    if (p.relation == "transposed")
        return (static_cast<long long>(cand.candidate_value) == static_cast<long long>(src_val) + p.transpose_semitones) ? 1.0 : -0.5;
    if (p.relation == "contour_preserving") {
        if (idx == 0) return 0.0;  // first position in the range has no prior step to compare
        const int prev_src_idx = cand.voice * ctx.sequence_length + p.source_positions[idx - 1];
        const int prev_tgt_idx = cand.voice * ctx.sequence_length + p.target_positions[idx - 1];
        if (prev_src_idx < 0 || prev_src_idx >= (int)ctx.pitch_vars->size() ||
            prev_tgt_idx < 0 || prev_tgt_idx >= (int)ctx.pitch_vars->size()) return 0.0;
        const Gecode::IntVar& prev_src_var = (*ctx.pitch_vars)[prev_src_idx];
        const Gecode::IntVar& prev_tgt_var = (*ctx.pitch_vars)[prev_tgt_idx];
        if (!prev_src_var.assigned() || !prev_tgt_var.assigned()) return 0.0;
        auto sign = [](int x) { return (x > 0) - (x < 0); };
        const int src_sign = sign(src_val - prev_src_var.val());
        const int tgt_sign = sign(cand.candidate_value - prev_tgt_var.val());
        return (src_sign == tgt_sign) ? 1.0 : -0.5;
    }
    return 0.0;
}

}  // namespace RepetitionRules

#endif  // REPETITION_RULES_HH
