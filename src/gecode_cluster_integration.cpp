/**
 * @file gecode_cluster_integration_clean.cpp  
 * @brief Clean Gecode Integration Implementation without duplicates
 */

#include "gecode_cluster_integration.hh"
#include <algorithm>
#include <iomanip>
#include <functional>
#include <map>
#include <cmath>
#include <numeric>
#include <sstream>

namespace GecodeClusterIntegration {

namespace {

HeuristicValueScorer g_pitch_heuristic_scorer;
bool g_pitch_heuristic_enabled = false;
unsigned int g_pitch_tie_break_seed = 0;
int g_pitch_top_k = 0;
bool g_pitch_trace = false;

int select_value_by_heuristic(const Space& home, IntVar x, int i);

template <typename Vars>
void branch_with_search_strategy(
    IntegratedMusicalSpace& space,
    Vars vars,
    VariableBranchingStrategy variable_branching,
    ValueSelectionStrategy value_selection,
    unsigned int random_seed,
    bool allow_heuristic_value_selection) {
    const bool use_input_order =
        (variable_branching == VariableBranchingStrategy::INPUT_ORDER);
    const bool use_random_values =
        (value_selection == ValueSelectionStrategy::RANDOM && random_seed != 0);
    const bool use_heuristic_values =
        allow_heuristic_value_selection &&
        value_selection == ValueSelectionStrategy::HEURISTIC &&
        g_pitch_heuristic_enabled;

    if (use_heuristic_values) {
        if (use_input_order) {
            branch(space, vars, INT_VAR_NONE(), INT_VAL(select_value_by_heuristic));
        } else {
            branch(space, vars, INT_VAR_SIZE_MIN(), INT_VAL(select_value_by_heuristic));
        }
        return;
    }

    if (use_random_values) {
        Gecode::Rnd rng(random_seed);
        if (use_input_order) {
            branch(space, vars, INT_VAR_NONE(), INT_VAL_RND(rng));
        } else {
            branch(space, vars, INT_VAR_SIZE_MIN(), INT_VAL_RND(rng));
        }
        return;
    }

    if (use_input_order) {
        branch(space, vars, INT_VAR_NONE(), INT_VAL_MIN());
    } else {
        branch(space, vars, INT_VAR_SIZE_MIN(), INT_VAL_MIN());
    }
}

unsigned int tie_break_hash(unsigned int seed, int var_index, int candidate_value) {
    unsigned int x = seed;
    x ^= static_cast<unsigned int>(var_index) + 0x9e3779b9u + (x << 6) + (x >> 2);
    x ^= static_cast<unsigned int>(candidate_value) + 0x9e3779b9u + (x << 6) + (x >> 2);
    return x;
}

double bucket_value(const HeuristicValueScoreBuckets& buckets, int priority) {
    for (const auto& kv : buckets) {
        if (kv.first == priority) {
            return kv.second;
        }
    }
    return 0.0;
}

std::string buckets_to_string(const HeuristicValueScoreBuckets& buckets) {
    if (buckets.empty()) {
        return "{}";
    }

    std::ostringstream oss;
    oss << "{";
    for (size_t i = 0; i < buckets.size(); ++i) {
        if (i > 0) {
            oss << ", ";
        }
        oss << "p" << buckets[i].first << ":" << buckets[i].second;
    }
    oss << "}";
    return oss.str();
}

bool candidate_is_better(
    const HeuristicValueScoreBuckets& candidate,
    const HeuristicValueScoreBuckets& best,
    int var_index,
    int candidate_value,
    int best_value,
    unsigned int tie_seed) {

    std::vector<int> priorities;
    priorities.reserve(candidate.size() + best.size());
    for (const auto& kv : candidate) priorities.push_back(kv.first);
    for (const auto& kv : best) priorities.push_back(kv.first);
    std::sort(priorities.begin(), priorities.end(), std::greater<int>());
    priorities.erase(std::unique(priorities.begin(), priorities.end()), priorities.end());

    for (int prio : priorities) {
        double c = bucket_value(candidate, prio);
        double b = bucket_value(best, prio);
        if (c > b) return true;
        if (c < b) return false;
    }

    if (tie_seed != 0) {
        unsigned int c_rank = tie_break_hash(tie_seed, var_index, candidate_value);
        unsigned int b_rank = tie_break_hash(tie_seed, var_index, best_value);
        return c_rank < b_rank;
    }

    return candidate_value < best_value;
}

int select_value_by_heuristic(const Space& home, IntVar x, int i) {
    if (!g_pitch_heuristic_enabled || !g_pitch_heuristic_scorer) {
        return x.min();
    }

    const auto& space = static_cast<const IntegratedMusicalSpace&>(home);
    const IntVarArray& pitch_vars = space.get_absolute_vars();
    const int var_index = i;
    if (var_index < 0 || var_index >= static_cast<int>(pitch_vars.size())) {
        return x.min();
    }

    const int seq_len = space.get_sequence_length();
    if (seq_len <= 0) {
        return x.min();
    }

    const int voice = var_index / seq_len;
    const int position = var_index % seq_len;

    IntVarValues values(x);
    if (!values()) {
        return x.min();
    }

    int best_value = values.val();
    HeuristicValueScoreBuckets best_buckets =
        g_pitch_heuristic_scorer(space, voice, position, best_value);

    if (g_pitch_trace) {
        std::cout << "🎯 candidate voice=" << voice
                  << " pos=" << position
                  << " value=" << best_value
                  << " buckets=" << buckets_to_string(best_buckets) << std::endl;
    }

    int evaluated = 1;
    const bool top_k_enabled = g_pitch_top_k > 0;

    for (++values; values(); ++values) {
        if (top_k_enabled && evaluated >= g_pitch_top_k) {
            break;
        }
        int candidate = values.val();
        HeuristicValueScoreBuckets candidate_buckets =
            g_pitch_heuristic_scorer(space, voice, position, candidate);
        ++evaluated;

        if (g_pitch_trace) {
            std::cout << "🎯 candidate voice=" << voice
                      << " pos=" << position
                      << " value=" << candidate
                      << " buckets=" << buckets_to_string(candidate_buckets) << std::endl;
        }

        if (candidate_is_better(
                candidate_buckets,
                best_buckets,
                var_index,
                candidate,
                best_value,
                g_pitch_tie_break_seed)) {
            best_value = candidate;
            best_buckets = std::move(candidate_buckets);
        }
    }

    if (g_pitch_trace) {
        std::cout << "🎯 selector voice=" << voice
                  << " pos=" << position
                  << " evaluated=" << evaluated
                  << " chose=" << best_value;
        if (top_k_enabled) {
            std::cout << " (top_k=" << g_pitch_top_k << ")";
        }
        std::cout << " buckets=" << buckets_to_string(best_buckets);
        std::cout << std::endl;
    }

    return best_value;
}

} // namespace

void configure_pitch_heuristic_value_ordering(
    HeuristicValueScorer scorer,
    unsigned int tie_break_seed,
    int top_k,
    bool trace) {
    g_pitch_heuristic_scorer = std::move(scorer);
    g_pitch_heuristic_enabled = static_cast<bool>(g_pitch_heuristic_scorer);
    g_pitch_tie_break_seed = tie_break_seed;
    g_pitch_top_k = std::max(0, top_k);
    g_pitch_trace = trace;
}

void clear_pitch_heuristic_value_ordering() {
    g_pitch_heuristic_scorer = nullptr;
    g_pitch_heuristic_enabled = false;
    g_pitch_tie_break_seed = 0;
    g_pitch_top_k = 0;
    g_pitch_trace = false;
}

// ===============================
// Implementation of IntegratedMusicalSpace
// ===============================

IntegratedMusicalSpace::IntegratedMusicalSpace(int length, int voices, 
                                              VariableBranchingStrategy variable_branching,
                                              ValueSelectionStrategy value_selection,
                                              const std::vector<int>& note_domain,
                                              unsigned int random_seed)
    : Space(), 
      sequence_length_(length), 
      absolute_vars_(*this, length * voices, 
                     note_domain.empty() ? IntSet(48, 96) : IntSet(note_domain.data(), note_domain.size())),  // Domain from config
      interval_vars_(*this, (length * voices) - voices, -12, 12),  // Interval range
      rhythm_vars_(*this, 0, 0, 0),  // No rhythm vars in single-domain constructor
    metric_vars_(*this, 0, 0, 0),  // No metric vars in scaffold constructor
        num_voices_(voices),
            variable_branching_(variable_branching),
            value_selection_(value_selection),
      solution_storage_(std::make_unique<MusicalConstraints::DualSolutionStorage>(length)),
      vocal_space_configured_(false) {
    
    // Setup interval constraints between consecutive notes within each voice
    for (int voice = 0; voice < voices; ++voice) {
        for (int i = 1; i < length; ++i) {
            int abs_idx = voice * length + i;
            int prev_abs_idx = voice * length + (i - 1);
            int int_idx = voice * (length - 1) + (i - 1);
            
            // interval[i] = absolute[i] - absolute[i-1]
            rel(*this, interval_vars_[int_idx] == absolute_vars_[abs_idx] - absolute_vars_[prev_abs_idx]);
        }
    }
    
    // Setup basic musical constraints
    post_musical_constraints();
    
    branch_with_search_strategy(*this, absolute_vars_, variable_branching_, value_selection_, random_seed, true);
}

// Per-voice domain constructor: each voice gets its own pitch domain
IntegratedMusicalSpace::IntegratedMusicalSpace(int length, int voices,
                                              VariableBranchingStrategy variable_branching,
                                              ValueSelectionStrategy value_selection,
                                              const std::vector<std::vector<int>>& voice_domains,
                                              unsigned int random_seed)
    : Space(),
      sequence_length_(length),
      absolute_vars_(*this, length * voices, IntSet(0, 127)),  // Wide domain, narrowed below
      interval_vars_(*this, (length * voices) - voices, -12, 12),
      rhythm_vars_(*this, 0, 0, 0),  // No rhythm vars in pitch-only constructor
    metric_vars_(*this, 0, 0, 0),  // No metric vars in scaffold constructor
        num_voices_(voices),
            variable_branching_(variable_branching),
            value_selection_(value_selection),
      solution_storage_(std::make_unique<MusicalConstraints::DualSolutionStorage>(length)),
      vocal_space_configured_(false) {

    // Narrow each voice's variables to its specific domain
    for (int v = 0; v < voices; ++v) {
        if (v < (int)voice_domains.size() && !voice_domains[v].empty()) {
            IntSet voice_dom(voice_domains[v].data(), voice_domains[v].size());
            for (int i = 0; i < length; ++i) {
                dom(*this, absolute_vars_[v * length + i], voice_dom);
            }
        }
    }

    // Setup interval constraints
    for (int voice = 0; voice < voices; ++voice) {
        for (int i = 1; i < length; ++i) {
            int abs_idx = voice * length + i;
            int prev_abs_idx = voice * length + (i - 1);
            int int_idx = voice * (length - 1) + (i - 1);
            rel(*this, interval_vars_[int_idx] == absolute_vars_[abs_idx] - absolute_vars_[prev_abs_idx]);
        }
    }

    post_musical_constraints();
    branch_with_search_strategy(*this, absolute_vars_, variable_branching_, value_selection_, random_seed, true);
}

// Per-voice pitch AND rhythm domain constructor
IntegratedMusicalSpace::IntegratedMusicalSpace(int length, int voices,
                                              VariableBranchingStrategy variable_branching,
                                              ValueSelectionStrategy value_selection,
                                              const std::vector<std::vector<int>>& voice_domains,
                                              const std::vector<std::vector<int>>& voice_rhythm_domains,
                                              unsigned int random_seed)
    : Space(),
      sequence_length_(length),
      absolute_vars_(*this, length * voices, IntSet(0, 127)),
      interval_vars_(*this, (length * voices) - voices, -12, 12),
      rhythm_vars_(*this, length * voices, -100000, 100000),  // Wide range including negatives for rests; narrowed by dom() below
    metric_vars_(*this, 0, 0, 0),  // No metric vars until metric engine activation
        num_voices_(voices),
            variable_branching_(variable_branching),
            value_selection_(value_selection),
      solution_storage_(std::make_unique<MusicalConstraints::DualSolutionStorage>(length)),
      vocal_space_configured_(false) {

    // Narrow each voice's pitch variables
    for (int v = 0; v < voices; ++v) {
        if (v < (int)voice_domains.size() && !voice_domains[v].empty()) {
            IntSet voice_dom(voice_domains[v].data(), voice_domains[v].size());
            for (int i = 0; i < length; ++i) {
                dom(*this, absolute_vars_[v * length + i], voice_dom);
            }
        }
    }

    // Narrow each voice's rhythm variables to allowed duration values and exclude 0.
    for (int v = 0; v < voices; ++v) {
        if (v < (int)voice_rhythm_domains.size() && !voice_rhythm_domains[v].empty()) {
            IntSet rhythm_dom(voice_rhythm_domains[v].data(), voice_rhythm_domains[v].size());
            for (int i = 0; i < length; ++i) {
                dom(*this, rhythm_vars_[v * length + i], rhythm_dom);
            }
        } else {
            // No explicit domain: exclude 0 (0 is not a valid duration)
            for (int i = 0; i < length; ++i)
                rel(*this, rhythm_vars_[v * length + i], IRT_NQ, 0);
        }
    }

    // Interval constraints: only enforce when BOTH consecutive positions are notes (rhythm > 0).
    // Uses implication reification: if both_notes then interval == diff.
    for (int voice = 0; voice < voices; ++voice) {
        for (int i = 1; i < length; ++i) {
            int abs_idx      = voice * length + i;
            int prev_abs_idx = voice * length + (i - 1);
            int int_idx      = voice * (length - 1) + (i - 1);
            int r_cur        = voice * length + i;
            int r_prev       = voice * length + (i - 1);
            BoolVar cur_note  = expr(*this, rhythm_vars_[r_cur]  > 0);
            BoolVar prev_note = expr(*this, rhythm_vars_[r_prev] > 0);
            BoolVar both_notes = expr(*this, cur_note && prev_note);
            IntVar diff = expr(*this, absolute_vars_[abs_idx] - absolute_vars_[prev_abs_idx]);
            rel(*this, interval_vars_[int_idx], IRT_EQ, diff, Reify(both_notes, RM_IMP));
        }
    }

    post_musical_constraints();
    branch_with_search_strategy(*this, absolute_vars_, variable_branching_, value_selection_, random_seed, true);
    branch_with_search_strategy(*this, rhythm_vars_, variable_branching_, value_selection_, random_seed, false);
}

// Per-voice pitch/rhythm domains + metric domain constructor
IntegratedMusicalSpace::IntegratedMusicalSpace(int length, int voices,
                                              VariableBranchingStrategy variable_branching,
                                              ValueSelectionStrategy value_selection,
                                              const std::vector<std::vector<int>>& voice_domains,
                                              const std::vector<std::vector<int>>& voice_rhythm_domains,
                                              const std::vector<int>& metric_domain_numerators,
                                              unsigned int random_seed)
    : Space(),
      sequence_length_(length),
      absolute_vars_(*this, length * voices, IntSet(0, 127)),
      interval_vars_(*this, (length * voices) - voices, -12, 12),
      rhythm_vars_(*this, length * voices, -100000, 100000),
      metric_vars_(*this, length, -100000, 100000),
    num_voices_(voices),
    variable_branching_(variable_branching),
    value_selection_(value_selection),
      solution_storage_(std::make_unique<MusicalConstraints::DualSolutionStorage>(length)),
      vocal_space_configured_(false) {

    // Narrow each voice's pitch variables
    for (int v = 0; v < voices; ++v) {
        if (v < (int)voice_domains.size() && !voice_domains[v].empty()) {
            IntSet voice_dom(voice_domains[v].data(), voice_domains[v].size());
            for (int i = 0; i < length; ++i) {
                dom(*this, absolute_vars_[v * length + i], voice_dom);
            }
        }
    }

    // Narrow each voice's rhythm variables to allowed duration values and exclude 0.
    for (int v = 0; v < voices; ++v) {
        if (v < (int)voice_rhythm_domains.size() && !voice_rhythm_domains[v].empty()) {
            IntSet rhythm_dom(voice_rhythm_domains[v].data(), voice_rhythm_domains[v].size());
            for (int i = 0; i < length; ++i) {
                dom(*this, rhythm_vars_[v * length + i], rhythm_dom);
            }
        } else {
            for (int i = 0; i < length; ++i)
                rel(*this, rhythm_vars_[v * length + i], IRT_NQ, 0);
        }
    }

    if (metric_domain_numerators.empty()) {
        throw std::runtime_error("Metric constructor requires non-empty metric domain");
    }
    std::vector<int> metric_values = metric_domain_numerators;
    std::sort(metric_values.begin(), metric_values.end());
    metric_values.erase(std::unique(metric_values.begin(), metric_values.end()), metric_values.end());
    IntSet metric_dom(metric_values.data(), metric_values.size());
    for (int i = 0; i < length; ++i) {
        dom(*this, metric_vars_[i], metric_dom);
    }

    // Interval constraints: only enforce when BOTH consecutive positions are notes (rhythm > 0).
    for (int voice = 0; voice < voices; ++voice) {
        for (int i = 1; i < length; ++i) {
            int abs_idx      = voice * length + i;
            int prev_abs_idx = voice * length + (i - 1);
            int int_idx      = voice * (length - 1) + (i - 1);
            int r_cur        = voice * length + i;
            int r_prev       = voice * length + (i - 1);
            BoolVar cur_note  = expr(*this, rhythm_vars_[r_cur]  > 0);
            BoolVar prev_note = expr(*this, rhythm_vars_[r_prev] > 0);
            BoolVar both_notes = expr(*this, cur_note && prev_note);
            IntVar diff = expr(*this, absolute_vars_[abs_idx] - absolute_vars_[prev_abs_idx]);
            rel(*this, interval_vars_[int_idx], IRT_EQ, diff, Reify(both_notes, RM_IMP));
        }
    }

    post_musical_constraints();
    branch_with_search_strategy(*this, absolute_vars_, variable_branching_, value_selection_, random_seed, true);
    branch_with_search_strategy(*this, rhythm_vars_, variable_branching_, value_selection_, random_seed, false);
    branch_with_search_strategy(*this, metric_vars_, variable_branching_, value_selection_, random_seed, false);
}

IntegratedMusicalSpace::IntegratedMusicalSpace(IntegratedMusicalSpace& space)
    : Space(space),
      sequence_length_(space.sequence_length_),
      num_voices_(space.num_voices_),
    variable_branching_(space.variable_branching_),
    value_selection_(space.value_selection_),
      solution_storage_(std::make_unique<MusicalConstraints::DualSolutionStorage>(space.sequence_length_)),
      vocal_space_configured_(space.vocal_space_configured_) {
    
    absolute_vars_.update(*this, space.absolute_vars_);
    interval_vars_.update(*this, space.interval_vars_);
    rhythm_vars_.update(*this, space.rhythm_vars_);
    metric_vars_.update(*this, space.metric_vars_);
    
    // Copy musical rules
    for (const auto& rule : space.musical_rules_) {
        musical_rules_.push_back(rule);
    }
    
    // Note: compiled_rules_ contains unique_ptrs that cannot be copied
    // They will need to be re-added if needed
}

Space* IntegratedMusicalSpace::copy() {
    return new IntegratedMusicalSpace(*this);
}

void IntegratedMusicalSpace::constrain(const Space& best) {
    // Implement optimization constraint if needed
}

void IntegratedMusicalSpace::add_musical_rule(std::shared_ptr<MusicalConstraints::MusicalRule> rule) {
    musical_rules_.push_back(rule);
    
    // Post the rule as Gecode constraints based on rule type
    std::string rule_type = rule->rule_type();
    
    if (rule_type == "AllDifferentPitchRule") {
        // 12-tone row: all pitch classes in voice 0 must be different
        IntVarArgs voice0_pitches;
        for (int i = 0; i < sequence_length_; ++i) {
            voice0_pitches << absolute_vars_[i];  // Voice 0 pitch variables
        }
        distinct(*this, voice0_pitches);
        std::cout << "Posted AllDifferent constraint for 12-tone row" << std::endl;
        
    } else if (rule_type == "PerfectFifthIntervalRule") {
        // Perfect fifth intervals: consecutive notes must differ by 7 semitones
        for (int i = 1; i < sequence_length_; ++i) {
            // Force a specific interval of +7 semitones (perfect fifth up)
            rel(*this, absolute_vars_[i] == absolute_vars_[i-1] + 7);
        }
        std::cout << "Posted Perfect Fifth interval constraints" << std::endl;
        
    } else if (rule_type == "PalindromeRule") {
        // Voice 1 must be palindrome of Voice 0
        for (int i = 0; i < sequence_length_; ++i) {
            int voice1_idx = sequence_length_ + i;           // Voice 1 at position i  
            int voice0_retro_idx = (sequence_length_ - 1) - i;  // Voice 0 at retrograde position
            
            // voice1[i] = voice0[length-1-i] (palindrome/retrograde)
            rel(*this, absolute_vars_[voice1_idx], IRT_EQ, absolute_vars_[voice0_retro_idx]);
        }
        std::cout << "Posted Palindrome constraint between voices" << std::endl;
        
    } else if (rule_type == "FixedValueRule") {
        // Fixed value constraints (for rhythms = 4, metric = 4, etc.)
        std::cout << "Posted FixedValue constraint" << std::endl;
        
    } else {
        std::cout << "Adding musical rule (no specific constraint): " << rule_type << std::endl;
    }
}

void IntegratedMusicalSpace::add_compiled_musical_rule(std::unique_ptr<GecodeClusterIntegration::MusicalRule> rule) {
    std::cout << "Adding compiled musical rule: " << rule->get_name() << std::endl;
    compiled_rules_.push_back(std::move(rule));
}

void IntegratedMusicalSpace::set_search_strategies(VariableBranchingStrategy variable_branching,
                                                   ValueSelectionStrategy value_selection) {
    variable_branching_ = variable_branching;
    value_selection_ = value_selection;
}

void IntegratedMusicalSpace::constrain_note_range(int min_note, int max_note) {
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        rel(*this, absolute_vars_[i], IRT_GQ, min_note);
        rel(*this, absolute_vars_[i], IRT_LQ, max_note);
    }
}

void IntegratedMusicalSpace::add_retrograde_inversion_constraint(int inversion_center) {
    if (num_voices_ < 2) return;
    
    // Voice 2 is retrograde inversion of Voice 1
    // Voice2[i] = 2 * center - Voice1[length-1-i]
    for (int i = 0; i < sequence_length_; ++i) {
        // Voice 2 notes at position i
        int voice2_idx = sequence_length_ + i;
        int voice1_retro_idx = sequence_length_ - 1 - i;  // Retrograde index in voice 1
        
        // voice2[i] + voice1[retro_idx] = 2 * center
        rel(*this, absolute_vars_[voice2_idx] + absolute_vars_[voice1_retro_idx] == 2 * inversion_center);
    }
}

void IntegratedMusicalSpace::post_twelve_tone_row_constraint() {
    // 12-tone row: all pitch classes in voice 0 must be different
    IntVarArgs voice0_pitches;
    for (int i = 0; i < sequence_length_; ++i) {
        voice0_pitches << absolute_vars_[i];  // Voice 0 pitch variables
    }
    distinct(*this, voice0_pitches);
}

void IntegratedMusicalSpace::post_perfect_fifth_intervals_constraint() {
    // Perfect fifth intervals: consecutive notes must differ by 7 semitones
    for (int i = 1; i < sequence_length_; ++i) {
        // Force a specific interval of +7 semitones (perfect fifth up)
        rel(*this, absolute_vars_[i] == absolute_vars_[i-1] + 7);
    }
}

void IntegratedMusicalSpace::post_palindrome_voices_constraint() {
    // Voice 1 must be palindrome of Voice 0
    for (int i = 0; i < sequence_length_; ++i) {
        int voice1_idx = sequence_length_ + i;           // Voice 1 at position i  
        int voice0_retro_idx = (sequence_length_ - 1) - i;  // Voice 0 at retrograde position
        
        // voice1[i] = voice0[length-1-i] (palindrome/retrograde)
        rel(*this, absolute_vars_[voice1_idx], IRT_EQ, absolute_vars_[voice0_retro_idx]);
    }
}

std::vector<int> IntegratedMusicalSpace::get_absolute_sequence() const {
    std::vector<int> sequence;
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        if (absolute_vars_[i].assigned()) {
            sequence.push_back(absolute_vars_[i].val());
        } else {
            sequence.push_back(60); // Default middle C
        }
    }
    return sequence;
}

std::vector<int> IntegratedMusicalSpace::get_interval_sequence() const {
    std::vector<int> sequence;
    for (int i = 0; i < interval_vars_.size(); ++i) {
        if (interval_vars_[i].assigned()) {
            sequence.push_back(interval_vars_[i].val());
        } else {
            sequence.push_back(0); // Default unison
        }
    }
    return sequence;
}

void IntegratedMusicalSpace::print_musical_solution(std::ostream& os) const {
    auto abs_sequence = get_absolute_sequence();
    auto int_sequence = get_interval_sequence();
    
    os << "Musical Solution:" << std::endl;
    
    for (int voice = 0; voice < num_voices_; ++voice) {
        os << "Voice " << (voice + 1) << ": ";
        for (int i = 0; i < sequence_length_; ++i) {
            int idx = voice * sequence_length_ + i;
            if (idx < abs_sequence.size()) {
                os << abs_sequence[idx] << " ";
            }
        }
        os << std::endl;
    }
    
    os << "Intervals: ";
    for (int interval : int_sequence) {
        os << interval << " ";
    }
    os << std::endl;
}

void IntegratedMusicalSpace::post_musical_constraints() {
    // Add basic musical constraints
    for (int voice = 0; voice < num_voices_; ++voice) {
        for (int i = 1; i < sequence_length_; ++i) {
            int int_idx = voice * (sequence_length_ - 1) + (i - 1);
            // Reasonable melodic intervals (within an octave)
            if (int_idx < interval_vars_.size()) {
                rel(*this, interval_vars_[int_idx], IRT_GQ, -12);
                rel(*this, interval_vars_[int_idx], IRT_LQ, 12);
            }
        }
    }
}

void IntegratedMusicalSpace::add_musical_rules(const std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>& rules) {
    for (const auto& rule : rules) {
        add_musical_rule(rule);
    }
}

MusicalConstraints::DualSolutionStorage IntegratedMusicalSpace::export_to_dual_storage() const {
    auto abs_sequence = get_absolute_sequence();
    auto int_sequence = get_interval_sequence();
    
    MusicalConstraints::DualSolutionStorage storage(sequence_length_);
    // Convert to dual storage format as needed
    return storage;
}

std::vector<int> IntegratedMusicalSpace::get_rhythm_sequence(int voice) const {
    throw std::runtime_error(
        "get_rhythm_sequence() has no fallback. "
        "Provide 'duration_values' for voice " + std::to_string(voice) +
        " rhythm engine (engine_" + std::to_string(voice * 2) + ") in engine_domains.");
}

std::vector<int> IntegratedMusicalSpace::get_pitch_sequence(int voice) const {
    std::vector<int> sequence;
    int start_idx = voice * sequence_length_;
    int end_idx = std::min(start_idx + sequence_length_, (int)absolute_vars_.size());

    // Check if rhythm vars are available to detect rests
    bool has_rhythm = (rhythm_vars_.size() > 0);

    for (int i = start_idx; i < end_idx; ++i) {
        int pos = i - start_idx;
        int r_idx = voice * sequence_length_ + pos;
        if (has_rhythm && r_idx < (int)rhythm_vars_.size() &&
                rhythm_vars_[r_idx].assigned() && rhythm_vars_[r_idx].val() < 0) {
            sequence.push_back(REST_PITCH_SENTINEL);
        } else if (absolute_vars_[i].assigned()) {
            sequence.push_back(absolute_vars_[i].val());
        } else {
            sequence.push_back(60); // Default middle C
        }
    }
    return sequence;
}

std::vector<int> IntegratedMusicalSpace::get_rhythm_sequence_from_vars(int voice) const {
    std::vector<int> sequence;
    if (rhythm_vars_.size() == 0) {
        return sequence;  // No rhythm vars; caller handles fallback
    }
    int start_idx = voice * sequence_length_;
    int end_idx = std::min(start_idx + sequence_length_, (int)rhythm_vars_.size());

    for (int i = start_idx; i < end_idx; ++i) {
        if (rhythm_vars_[i].assigned()) {
            sequence.push_back(rhythm_vars_[i].val());
        } else {
            sequence.push_back(4); // Should not happen after successful solve
        }
    }
    return sequence;
}

std::vector<int> IntegratedMusicalSpace::get_metric_sequence() const {
    // Phase-1 scaffold: if metric vars exist and are assigned, surface them.
    // Fallback keeps current behavior until full metric engine wiring lands.
    if (metric_vars_.size() > 0) {
        std::vector<int> sequence;
        sequence.reserve(metric_vars_.size());
        for (int i = 0; i < metric_vars_.size(); ++i) {
            sequence.push_back(metric_vars_[i].assigned() ? metric_vars_[i].val() : 4);
        }
        return sequence;
    }
    return std::vector<int>{4};
}

} // namespace GecodeClusterIntegration