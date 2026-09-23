/**
 * @file cadence_rules.hh
 * @brief r-cadence rule type: places a definitional cadence at a position pair.
 *
 * Deliberately thin, per the design discussion: this encodes only what makes
 * a cadence *that* cadence (bass motion of the underlying harmony, whether the
 * chords are in root position, where the soprano lands) — not general voice
 * leading (parallel fifths/octaves, spacing, etc., which already live in
 * r-pitch-pitch and apply everywhere, not just at cadences).
 *
 * Known v1 limitations (intentional, not oversights):
 *   - metric_strength is accepted and echoed in the compile log, but not yet
 *     enforced — that requires reaching into the metric engine, out of scope
 *     for this pass.
 *   - Leading-tone / chordal-7th resolution is not enforced — it requires
 *     knowing which voice holds those tones, not just soprano/bass.
 * Both are logged as explicit gaps at compile time rather than silently
 * ignored.
 *
 * Implemented once here (not duplicated per binary) — new rule types route
 * through this shared header and the JSON-based rule loop only, never through
 * the legacy line-based config scanner.
 */

#ifndef CADENCE_RULES_HH
#define CADENCE_RULES_HH

#include "dynamic_rule_compiler.hh"
#include "musical_constraint_solver.hh"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace CadenceRules {

using HarmonicConfig = MusicalConstraintSolver::SolverConfig::HarmonicConfig;
using HarmonicEntry  = MusicalConstraintSolver::SolverConfig::HarmonicEntry;

struct CadenceParams {
    std::string id;
    std::string cadence_type;      // "PAC","IAC","HC","half","deceptive","plagal"
    std::vector<int> positions;    // exactly 2: [pre-cadence, resolution]
    std::vector<int> target_voices;
    bool require_root_position = false;
    int  soprano_target_degree = 0;  // 1-7, 0 = not required
    int  end_degree_required   = 0;  // 1-7, 0 = not required (used by HC)
    int  bass_from_degree      = 0;  // 0 = not required
    int  bass_to_degree        = 0;
    std::string metric_strength;     // informational only in v1
};

// Forward-fill lookup mirroring the neural scorer's harmonic_state semantics:
// the entry active at `position` is the last entry whose beat_position <= position.
inline const HarmonicEntry* find_active_entry(const HarmonicConfig& hd, int position) {
    const HarmonicEntry* active = nullptr;
    for (const auto& e : hd.entries) {
        if (e.beat_position <= position &&
                (!active || e.beat_position > active->beat_position)) {
            active = &e;
        }
    }
    return active;
}

// Applies cadence_type defaults, then lets any explicit top-level field on the
// rule JSON override them. Fields are flat (not nested under "parameters") —
// deliberately, since "parameters" is special-cased by the legacy line-based
// config scanner as an array-only field and an object value there would hang it.
inline CadenceParams resolve_params(const nlohmann::json& rule_json) {
    CadenceParams p;
    p.id = rule_json.value("id", std::string("r-cadence"));
    p.cadence_type = rule_json.value("cadence_type", std::string("PAC"));

    if (p.cadence_type == "PAC") {
        p.bass_from_degree = 5; p.bass_to_degree = 1;
        p.require_root_position = true;
        p.soprano_target_degree = 1;
    } else if (p.cadence_type == "IAC") {
        p.bass_from_degree = 5; p.bass_to_degree = 1;
        p.require_root_position = false;
        p.soprano_target_degree = 0;
    } else if (p.cadence_type == "HC" || p.cadence_type == "half") {
        p.end_degree_required = 5;
        p.require_root_position = false;
        p.soprano_target_degree = 0;
    } else if (p.cadence_type == "deceptive") {
        p.bass_from_degree = 5; p.bass_to_degree = 6;
        p.require_root_position = false;
        p.soprano_target_degree = 0;
    } else if (p.cadence_type == "plagal") {
        p.bass_from_degree = 4; p.bass_to_degree = 1;
        p.require_root_position = false;
        p.soprano_target_degree = 0;
    } else {
        throw std::runtime_error(
            "r-cadence '" + p.id + "': unknown cadence_type '" + p.cadence_type +
            "' (expected PAC, IAC, HC, half, deceptive, plagal)");
    }

    // Explicit overrides
    if (rule_json.contains("require_root_position"))
        p.require_root_position = rule_json.value("require_root_position", p.require_root_position);
    if (rule_json.contains("soprano_target_degree"))
        p.soprano_target_degree = rule_json.value("soprano_target_degree", p.soprano_target_degree);
    if (rule_json.contains("end_degree_required"))
        p.end_degree_required = rule_json.value("end_degree_required", p.end_degree_required);
    if (rule_json.contains("bass_motion") && rule_json["bass_motion"].is_array() &&
            rule_json["bass_motion"].size() == 2) {
        p.bass_from_degree = rule_json["bass_motion"][0].get<int>();
        p.bass_to_degree   = rule_json["bass_motion"][1].get<int>();
    }
    p.metric_strength = rule_json.value("metric_strength", std::string());

    if (rule_json.contains("positions") && rule_json["positions"].is_array())
        for (const auto& v : rule_json["positions"])
            if (v.is_number_integer()) p.positions.push_back(v.get<int>());
    if (p.positions.size() != 2)
        throw std::runtime_error(
            "r-cadence '" + p.id + "': 'positions' must have exactly 2 entries "
            "[pre-cadence chord, resolution chord]");

    if (rule_json.contains("target_voices") && rule_json["target_voices"].is_array())
        for (const auto& v : rule_json["target_voices"])
            if (v.is_number_integer()) p.target_voices.push_back(v.get<int>());
    if (p.target_voices.empty())
        throw std::runtime_error("r-cadence '" + p.id + "': target_voices is required");

    return p;
}

// Config-time validation of the underlying harmony (no Gecode involved): does
// the chord progression declared in harmonic_domain actually match the
// requested cadence type? This is a fact about the harmonic plan, independent
// of which voice ends up on which pitch, so it's checked once here rather
// than posted as a solver constraint.
inline void validate_harmony(const CadenceParams& p, const HarmonicConfig& hd) {
    const HarmonicEntry* e0 = find_active_entry(hd, p.positions[0]);
    const HarmonicEntry* e1 = find_active_entry(hd, p.positions[1]);
    if (!e0 || !e1)
        throw std::runtime_error(
            "r-cadence '" + p.id + "': no harmonic_domain entry covers position " +
            std::to_string(!e0 ? p.positions[0] : p.positions[1]));
    if (e0->key_tonic < 0 || e1->key_tonic < 0)
        throw std::runtime_error(
            "r-cadence '" + p.id + "': harmonic_domain entries at the cadence "
            "positions have no key declared — r-cadence needs scale degrees, "
            "not just absolute chords");

    if (p.end_degree_required > 0 && e1->scale_degree() != p.end_degree_required)
        throw std::runtime_error(
            "r-cadence '" + p.id + "': expected the resolution chord (position " +
            std::to_string(p.positions[1]) + ") to be scale degree " +
            std::to_string(p.end_degree_required) + ", but harmonic_domain has degree " +
            std::to_string(e1->scale_degree()));

    if (p.bass_from_degree > 0 &&
            (e0->scale_degree() != p.bass_from_degree || e1->scale_degree() != p.bass_to_degree))
        throw std::runtime_error(
            "r-cadence '" + p.id + "': expected harmony to move degree " +
            std::to_string(p.bass_from_degree) + " -> " + std::to_string(p.bass_to_degree) +
            " (" + p.cadence_type + "), but harmonic_domain has degree " +
            std::to_string(e0->scale_degree()) + " -> " + std::to_string(e1->scale_degree()));
}

// Posts the Gecode-level constraints that ARE enforced in v1: root position
// (bass pitch-class == chord root) and soprano target degree. By convention
// (matching standard SATB voicing) target_voices.front() is soprano and
// target_voices.back() is bass.
inline void post_cadence_constraint(DynamicRules::ConstraintContext& ctx,
                                     const CadenceParams& p,
                                     const HarmonicConfig& hd) {
    if (!ctx.pitch_vars) return;
    validate_harmony(p, hd);

    const int soprano_voice = p.target_voices.front();
    const int bass_voice    = p.target_voices.back();

    auto pitch_var_at = [&](int voice, int pos) -> Gecode::IntVar& {
        const int idx = voice * ctx.sequence_length + pos;
        if (idx < 0 || idx >= (int)ctx.pitch_vars->size())
            throw std::runtime_error("r-cadence '" + p.id + "': voice/position out of range");
        return (*ctx.pitch_vars)[idx];
    };

    auto constrain_pitch_class = [&](Gecode::IntVar& var, int target_pc) {
        Gecode::IntVar twelve(*ctx.space, 12, 12);
        Gecode::IntVar pc(*ctx.space, 0, 11);
        Gecode::mod(*ctx.space, var, twelve, pc);
        Gecode::rel(*ctx.space, pc, Gecode::IRT_EQ, target_pc);
    };

    if (p.require_root_position) {
        const HarmonicEntry* e0 = find_active_entry(hd, p.positions[0]);
        const HarmonicEntry* e1 = find_active_entry(hd, p.positions[1]);
        constrain_pitch_class(pitch_var_at(bass_voice, p.positions[0]), e0->chord_root);
        constrain_pitch_class(pitch_var_at(bass_voice, p.positions[1]), e1->chord_root);
    }

    if (p.soprano_target_degree > 0) {
        const HarmonicEntry* e1 = find_active_entry(hd, p.positions[1]);
        const int target_pc = HarmonicEntry::degree_to_pitch_class(
            e1->key_tonic, e1->key_mode, p.soprano_target_degree);
        constrain_pitch_class(pitch_var_at(soprano_voice, p.positions[1]), target_pc);
    }
}

}  // namespace CadenceRules

#endif  // CADENCE_RULES_HH
