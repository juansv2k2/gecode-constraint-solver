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
    std::vector<int> target_voices;
};

inline TendencyParams resolve_params(const nlohmann::json& rule_json) {
    TendencyParams p;
    p.id = rule_json.value("id", std::string("r-tendency-tone"));
    p.tendency = rule_json.value("tendency", std::string(""));
    if (p.tendency != "chordal_seventh" && p.tendency != "leading_tone")
        throw std::runtime_error(
            "r-tendency-tone '" + p.id + "': tendency must be 'chordal_seventh' or 'leading_tone', got '" +
            p.tendency + "'");

    if (rule_json.contains("positions") && rule_json["positions"].is_array())
        for (const auto& v : rule_json["positions"])
            if (v.is_number_integer()) p.positions.push_back(v.get<int>());
    if (p.positions.size() != 2)
        throw std::runtime_error(
            "r-tendency-tone '" + p.id + "': 'positions' must have exactly 2 entries "
            "[tone position, resolution position]");

    if (rule_json.contains("target_voices") && rule_json["target_voices"].is_array())
        for (const auto& v : rule_json["target_voices"])
            if (v.is_number_integer()) p.target_voices.push_back(v.get<int>());
    if (p.target_voices.empty())
        throw std::runtime_error("r-tendency-tone '" + p.id + "': target_voices is required");

    return p;
}

inline void post_tendency_constraint(DynamicRules::ConstraintContext& ctx,
                                      const TendencyParams& p,
                                      const HarmonicConfig& harmonic_domain) {
    if (!ctx.pitch_vars) return;

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
