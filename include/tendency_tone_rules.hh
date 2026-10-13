/**
 * @file tendency_tone_rules.hh
 * @brief r-tendency-tone: enforces that a specific chord tone, wherever it
 * happens to land in the texture, resolves the way tonal harmony requires.
 *
 * This is the piece r-cadence explicitly deferred (see its "known v1
 * limitations" note): leading-tone and chordal-7th resolution require
 * tracking WHICH VOICE holds the tone, not just soprano/bass. A tendency
 * tone can appear in any voice, so the rule must say "whichever voice has
 * this pitch class at position0 must resolve it correctly at position1" —
 * a conditional (reified) constraint, not a fixed per-voice one.
 *
 * Two tendencies, both textbook-standard and effectively inviolable in
 * common-practice four-part writing:
 *   chordal_seventh  the 7th of a dominant-function chord resolves DOWN
 *                     by a diatonic step (e.g. V7's 7th -> 3rd of I)
 *   leading_tone      scale degree 7 resolves UP by a diatonic step to
 *                     the tonic (scale degree 1)
 *
 * Implementation reuses RepetitionRules::diatonic_shift (same "one scale
 * step" arithmetic as r-repetition's diatonic_sequence) and is posted as a
 * Gecode::extensional table per voice: for every value the voice's pos0
 * pitch could take, if its pitch class matches the tendency tone, the ONLY
 * legal pos1 value is the resolved pitch; otherwise pos1 is unconstrained
 * by this rule (any of its own domain values remains legal). This is what
 * makes it correctly conditional — a voice not holding the tendency tone at
 * position0 is not forced anywhere.
 *
 * v1 limitation: leading_tone uses the natural (not raised) 7th in minor
 * keys, matching the existing r-cadence/harmonic_domain simplification of
 * natural-minor scale steps. Harmonic-minor's raised leading tone is not
 * yet modeled.
 */

#ifndef TENDENCY_TONE_RULES_HH
#define TENDENCY_TONE_RULES_HH

#include "dynamic_rule_compiler.hh"
#include "musical_constraint_solver.hh"
#include "cadence_rules.hh"
#include "repetition_rules.hh"
#include <gecode/int.hh>
#include <nlohmann/json.hpp>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace TendencyToneRules {

using HarmonicConfig = MusicalConstraintSolver::SolverConfig::HarmonicConfig;
using HarmonicEntry  = MusicalConstraintSolver::SolverConfig::HarmonicEntry;

struct TendencyParams {
    std::string id;
    std::string tendency;  // "chordal_seventh" or "leading_tone"
    std::vector<int> positions;  // exactly 2: [tone position, resolution position]
    std::vector<int> beat_positions;
    std::vector<int> target_voices;
};

inline int selector_integer(const nlohmann::json& value) {
    if (!value.is_number_integer() || (value.is_number_unsigned() &&
        value.get<unsigned long long>()>static_cast<unsigned long long>(std::numeric_limits<int>::max())))
        throw std::runtime_error("Temporal selectors require representable nonnegative integers");
    const long long number=value.get<long long>();
    if (number<0 || number>std::numeric_limits<int>::max())
        throw std::runtime_error("Temporal selector outside integer range");
    return static_cast<int>(number);
}

inline TendencyParams resolve_params(const nlohmann::json& rule_json) {
    TendencyParams p;
    p.id = rule_json.value("id", std::string("r-tendency-tone"));
    p.tendency = rule_json.value("tendency", std::string(""));
    if (p.tendency != "chordal_seventh" && p.tendency != "leading_tone")
        throw std::runtime_error(
            "r-tendency-tone '" + p.id + "': tendency must be 'chordal_seventh' or 'leading_tone', got '" +
            p.tendency + "'");

    for (const char* name : {"positions", "beat_positions"}) {
        if (!rule_json.contains(name)) continue;
        if (!rule_json[name].is_array() || rule_json[name].size() != 2)
            throw std::runtime_error("Tendency rule requires exactly two positions or beat_positions");
        auto& values = std::string(name) == "positions" ? p.positions : p.beat_positions;
        for (const auto& value : rule_json[name]) {
            values.push_back(selector_integer(value));
        }
    }
    if ((!p.positions.empty() && !p.beat_positions.empty()) || (p.positions.size() != 2 && p.beat_positions.size() != 2))
        throw std::runtime_error(
            "r-tendency-tone '" + p.id + "': provide positions or beat_positions, not both");

    if (rule_json.contains("target_voices") && rule_json["target_voices"].is_array())
        for (const auto& v : rule_json["target_voices"]) {
            p.target_voices.push_back(selector_integer(v));
        }
    if (p.target_voices.empty())
        throw std::runtime_error("r-tendency-tone '" + p.id + "': target_voices is required");

    return p;
}

inline Gecode::IntVar sounding_pitch(DynamicRules::ConstraintContext& ctx, int voice, int address, bool beat) {
    if (voice < 0 || voice >= ctx.num_voices || !ctx.pitch_vars || !ctx.rhythm_vars)
        throw std::runtime_error("Tendency rule requires valid pitch/rhythm voice context");
    std::set<int> possible{-1};
    if (!beat && (address < 0 || address >= ctx.sequence_length))
        throw std::runtime_error("Tendency event index outside capacity");
    for (int event = 0; event < ctx.sequence_length; ++event) {
        if (!beat && event != address) continue;
        for (Gecode::IntVarValues value((*ctx.pitch_vars)[voice * ctx.sequence_length + event]); value(); ++value)
            possible.insert(value.val());
    }
    std::vector<int> values(possible.begin(), possible.end());
    Gecode::IntVar selected_pitch(*ctx.space, Gecode::IntSet(values.data(), values.size()));
    Gecode::BoolVar note(*ctx.space, 0, 1);
    Gecode::rel(*ctx.space, selected_pitch, Gecode::IRT_GQ, 0, Gecode::Reify(note, Gecode::RM_EQV));
    if (!beat) {
        const int index = voice * ctx.sequence_length + address;
        Gecode::rel(*ctx.space, ctx.space->get_active_vars()[index], Gecode::IRT_EQ, 1);
        Gecode::rel(*ctx.space, (*ctx.rhythm_vars)[index], Gecode::IRT_GR, 0, Gecode::Reify(note, Gecode::RM_EQV));
        Gecode::rel(*ctx.space, selected_pitch, Gecode::IRT_EQ, (*ctx.pitch_vars)[index], Gecode::Reify(note, Gecode::RM_IMP));
    } else {
        if (ctx.rhythm_base < 4 || ctx.rhythm_base % 4 != 0 || address < 0)
            throw std::runtime_error("Tendency beat selector requires aligned quarter-note ticks");
        const long long tick = static_cast<long long>(address) * (ctx.rhythm_base / 4);
        if (tick >= ctx.space->get_measure_horizon_ticks()) throw std::runtime_error("Tendency beat lies outside the bar plan");
        ctx.space->initialize_event_timing();
        Gecode::BoolVarArgs selectors;
        for (int event = 0; event < ctx.sequence_length; ++event) {
            const int index = voice * ctx.sequence_length + event;
            Gecode::BoolVar selected = Gecode::expr(*ctx.space, ((*ctx.rhythm_vars)[index] > 0) &&
                (ctx.space->get_event_boundary(voice,event) <= static_cast<int>(tick)) &&
                (ctx.space->get_event_boundary(voice,event+1) > static_cast<int>(tick)));
            Gecode::rel(*ctx.space, selected_pitch, Gecode::IRT_EQ, (*ctx.pitch_vars)[index], Gecode::Reify(selected, Gecode::RM_IMP));
            selectors << selected;
        }
        Gecode::IntVar count(*ctx.space, 0, 1);
        Gecode::channel(*ctx.space, note, count);
        Gecode::linear(*ctx.space, selectors, Gecode::IRT_EQ, count);
    }
    return selected_pitch;
}

inline void post_measure_tendency(DynamicRules::ConstraintContext& ctx, const TendencyParams& params,
                                 const HarmonicConfig& harmony) {
    if (!harmony.enabled || harmony.entries.empty()) throw std::runtime_error("Tendency rule requires keyed harmonic_domain");
    const bool beat = !params.beat_positions.empty();
    const auto& addresses = beat ? params.beat_positions : params.positions;
    if (addresses[0] >= addresses[1]) throw std::runtime_error("Tendency source must precede resolution");
    if (ctx.rhythm_base < 4 || ctx.rhythm_base % 4 != 0)
        throw std::runtime_error("Tendency harmony lookup requires quarter-note tick alignment");
    for (int voice : params.target_voices) {
        Gecode::IntVar source = sounding_pitch(ctx, voice, addresses[0], beat);
        Gecode::IntVar target = sounding_pitch(ctx, voice, addresses[1], beat);
        ctx.space->initialize_event_timing();
        Gecode::IntVar source_tick = beat ? Gecode::IntVar(*ctx.space, addresses[0] * (ctx.rhythm_base / 4), addresses[0] * (ctx.rhythm_base / 4)) :
            ctx.space->get_event_boundary(voice,addresses[0]);
        if (beat) {
            const auto* entry = CadenceRules::find_active_entry(harmony, addresses[0]);
            if (!entry || entry->key_tonic < 0 || (params.tendency == "chordal_seventh" && entry->chord_quality != 2))
                throw std::runtime_error("Beat-addressed tendency requires a keyed source chord (dom7 for chordal_seventh)");
        }
        Gecode::BoolVar source_note = Gecode::expr(*ctx.space, source >= 0);
        Gecode::BoolVarArgs covered;
        for (size_t segment = 0; segment < harmony.entries.size(); ++segment) {
            const auto& entry = harmony.entries[segment];
            const long long start = static_cast<long long>(entry.beat_position) * (ctx.rhythm_base / 4);
            if (start < 0 || start > Gecode::Int::Limits::max) throw std::runtime_error("Tendency harmonic beat outside tick range");
            Gecode::BoolExpr applies = source_note && (source_tick >= static_cast<int>(start));
            if (segment + 1 < harmony.entries.size()) {
                const long long end = static_cast<long long>(harmony.entries[segment+1].beat_position) * (ctx.rhythm_base / 4);
                if (end <= start || end > Gecode::Int::Limits::max) throw std::runtime_error("Tendency harmonic beats must increase");
                applies = applies && (source_tick < static_cast<int>(end));
            }
            if (entry.key_tonic < 0 || (params.tendency == "chordal_seventh" && entry.chord_quality != 2)) continue;
            Gecode::BoolVar selected = Gecode::expr(*ctx.space, applies);
            covered << selected;
            const int carrier = params.tendency == "chordal_seventh" ? (entry.chord_root+10)%12 :
                HarmonicEntry::degree_to_pitch_class(entry.key_tonic,entry.key_mode,7);
            Gecode::TupleSet tuples(2);
            for (Gecode::IntVarValues value(source); value(); ++value) {
                const int pitch = value.val();
                if (pitch < 0 || pitch%12 != carrier) {
                    for (Gecode::IntVarValues resolution(target); resolution(); ++resolution)
                        tuples.add(Gecode::IntArgs({pitch,resolution.val()}));
                } else {
                    const int resolved = RepetitionRules::diatonic_shift(pitch,entry.key_tonic,entry.key_mode,
                        params.tendency == "chordal_seventh" ? -1 : 1);
                    if (resolved >= 0 && target.in(resolved)) tuples.add(Gecode::IntArgs({pitch,resolved}));
                }
            }
            tuples.finalize();
            Gecode::extensional(*ctx.space,Gecode::IntVarArgs({source,target}),tuples,Gecode::Reify(selected,Gecode::RM_IMP));
        }
        if (covered.size() == 0) throw std::runtime_error("No valid keyed source harmony for tendency rule");
        Gecode::linear(*ctx.space,covered,Gecode::IRT_EQ,1,Gecode::Reify(source_note,Gecode::RM_IMP));
    }
}

inline void post_tendency_constraint(DynamicRules::ConstraintContext& ctx,
                                      const TendencyParams& p,
                                      const HarmonicConfig& harmonic_domain) {
    if (!ctx.pitch_vars) return;
    if (ctx.space && ctx.space->is_measure_count_mode()) {
        post_measure_tendency(ctx,p,harmonic_domain);
        return;
    }
    if (!p.beat_positions.empty()) throw std::runtime_error("Tendency beat_positions currently requires measure mode");

    const HarmonicEntry* e0 = CadenceRules::find_active_entry(harmonic_domain, p.positions[0]);
    if (!e0 || e0->key_tonic < 0)
        throw std::runtime_error(
            "r-tendency-tone '" + p.id + "': no keyed harmonic_domain entry covers position " +
            std::to_string(p.positions[0]));

    // Source pitch class and resolution direction (in diatonic scale steps).
    int source_pc;
    int direction;  // +1 = up, -1 = down
    if (p.tendency == "chordal_seventh") {
        if (e0->chord_quality != 2)  // 0=major 1=minor 2=dom7
            throw std::runtime_error(
                "r-tendency-tone '" + p.id + "': chordal_seventh requires the chord at position " +
                std::to_string(p.positions[0]) + " to be a dominant seventh (harmonic_domain quality=dom7)");
        source_pc = (e0->chord_root + 10) % 12;  // minor 7th above the root
        direction = -1;
    } else {  // leading_tone
        source_pc = HarmonicEntry::degree_to_pitch_class(e0->key_tonic, e0->key_mode, 7);
        direction = +1;
    }

    for (int voice : p.target_voices) {
        if (voice < 0 || voice >= ctx.num_voices)
            throw std::runtime_error("r-tendency-tone '" + p.id + "': voice out of range");

        const int idx0 = voice * ctx.sequence_length + p.positions[0];
        const int idx1 = voice * ctx.sequence_length + p.positions[1];
        if (idx0 < 0 || idx0 >= (int)ctx.pitch_vars->size() ||
            idx1 < 0 || idx1 >= (int)ctx.pitch_vars->size())
            throw std::runtime_error("r-tendency-tone '" + p.id + "': position out of range");
        Gecode::IntVar& src = (*ctx.pitch_vars)[idx0];
        Gecode::IntVar& tgt = (*ctx.pitch_vars)[idx1];

        // Conditional table: a candidate source value that does NOT carry the
        // tendency-tone pitch class is paired with every legal target value
        // (this rule has no opinion on that voice); a candidate that DOES
        // carry it is paired with exactly its resolved value. This is what
        // makes the constraint apply only to whichever voice actually holds
        // the tone, without fixing every other voice to anything.
        Gecode::TupleSet tuples(2);
        for (Gecode::IntVarValues sv(src); sv(); ++sv) {
            const int s = sv.val();
            if (s % 12 != source_pc) {
                for (Gecode::IntVarValues tv(tgt); tv(); ++tv)
                    tuples.add(Gecode::IntArgs({s, tv.val()}));
            } else {
                const int resolved = RepetitionRules::diatonic_shift(s, e0->key_tonic, e0->key_mode, direction);
                if (resolved >= 0 && resolved >= tgt.min() && resolved <= tgt.max())
                    tuples.add(Gecode::IntArgs({s, resolved}));
                // If resolved falls outside the target voice's declared domain,
                // no tuple is added for this source value — the solver will
                // correctly treat holding this tone in this voice as infeasible
                // rather than silently allowing an unresolved tendency tone.
            }
        }
        tuples.finalize();
        if (tuples.tuples() == 0)
            throw std::runtime_error(
                "r-tendency-tone '" + p.id + "': no valid (source, target) pairing for voice " +
                std::to_string(voice) + " — likely the resolved pitch falls outside its domain");
        Gecode::IntVarArgs pair(2);
        pair[0] = src; pair[1] = tgt;
        Gecode::extensional(*ctx.space, pair, tuples);
    }
}

}  // namespace TendencyToneRules

#endif  // TENDENCY_TONE_RULES_HH
