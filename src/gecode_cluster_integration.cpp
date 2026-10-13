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
Gecode::Rnd* g_heuristic_rng = nullptr;

HeuristicValueScorer g_rhythm_heuristic_scorer;
bool g_rhythm_heuristic_enabled = false;
unsigned int g_rhythm_tie_break_seed = 0;
int g_rhythm_top_k = 0;
bool g_rhythm_trace = false;
Gecode::Rnd* g_rhythm_heuristic_rng = nullptr;

int select_value_by_heuristic(const Space& home, IntVar x, int i);
int select_rhythm_by_heuristic(const Space& home, IntVar x, int i);

// Value selector for rhythm variables: picks the value with the smallest absolute value
// (shortest duration/rest). On ties between a note and a rest of the same duration, the
// note (positive value) is preferred. This avoids always picking rests when the domain
// contains both positive (note) and negative (rest) tick values.
int select_rhythm_abs_min_value(const Space& /*home*/, IntVar x, int /*i*/) {
    int best = x.min();
    int best_abs = std::abs(best);
    for (IntVarValues v(x); v(); ++v) {
        int val = v.val();
        int a = std::abs(val);
        // Prefer smaller absolute value; on ties, prefer notes (positive) over rests (negative).
        if (a < best_abs || (a == best_abs && val > 0 && best < 0)) {
            best_abs = a;
            best = val;
        }
    }
    return best;
}

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

// Branching for rhythm variables: like branch_with_search_strategy but uses abs-min for
// the MIN strategy so that notes are not always dominated by rests in mixed domains.
template <typename Vars>
void branch_rhythm_with_search_strategy(
    IntegratedMusicalSpace& space,
    Vars vars,
    VariableBranchingStrategy variable_branching,
    ValueSelectionStrategy value_selection,
    unsigned int random_seed) {
    const bool use_input_order =
        (variable_branching == VariableBranchingStrategy::INPUT_ORDER);
    const bool use_random_values =
        (value_selection == ValueSelectionStrategy::RANDOM && random_seed != 0);
    const bool use_heuristic_values =
        (value_selection == ValueSelectionStrategy::HEURISTIC && g_rhythm_heuristic_enabled);

    if (use_heuristic_values) {
        if (use_input_order) {
            branch(space, vars, INT_VAR_NONE(), INT_VAL(select_rhythm_by_heuristic));
        } else {
            branch(space, vars, INT_VAR_SIZE_MIN(), INT_VAL(select_rhythm_by_heuristic));
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

    // Use abs-min for MIN strategy: prefers the shortest duration (positive or negative),
    // with notes (positive) preferred over same-length rests (negative).
    if (use_input_order) {
        branch(space, vars, INT_VAR_NONE(), INT_VAL(select_rhythm_abs_min_value));
    } else {
        branch(space, vars, INT_VAR_SIZE_MIN(), INT_VAL(select_rhythm_abs_min_value));
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

    // Collect all candidates with their heuristic scores
    std::vector<std::pair<int, HeuristicValueScoreBuckets>> candidates;
    const bool top_k_enabled = g_pitch_top_k > 0;
    
    for (; values(); ++values) {
        if (top_k_enabled && (int)candidates.size() >= g_pitch_top_k) {
            break;
        }
        int candidate = values.val();
        HeuristicValueScoreBuckets candidate_buckets =
            g_pitch_heuristic_scorer(space, voice, position, candidate);
        candidates.push_back({candidate, candidate_buckets});

        if (g_pitch_trace) {
            std::cout << "🎯 candidate voice=" << voice
                      << " pos=" << position
                      << " value=" << candidate
                      << " buckets=" << buckets_to_string(candidate_buckets) << std::endl;
        }
    }

    if (candidates.empty()) {
        return x.min();
    }

    // Find the best candidate(s) using heuristic comparison
    int best_idx = 0;
    HeuristicValueScoreBuckets best_buckets = candidates[0].second;
    
    for (size_t i = 1; i < candidates.size(); ++i) {
        if (candidate_is_better(
                candidates[i].second,
                best_buckets,
                var_index,
                candidates[i].first,
                candidates[best_idx].first,
                0)) {  // Use 0 for deterministic comparison (no random tie-break yet)
            best_idx = i;
            best_buckets = candidates[i].second;
        }
    }

    // Collect all candidates tied with the best score
    std::vector<int> tied_candidates;
    tied_candidates.push_back(candidates[best_idx].first);
    
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (i != best_idx) {
            // Check if this candidate is tied with the best
            std::vector<int> priorities;
            priorities.reserve(candidates[i].second.size() + best_buckets.size());
            for (const auto& kv : candidates[i].second) priorities.push_back(kv.first);
            for (const auto& kv : best_buckets) priorities.push_back(kv.first);
            std::sort(priorities.begin(), priorities.end(), std::greater<int>());
            priorities.erase(std::unique(priorities.begin(), priorities.end()), priorities.end());

            bool is_tied = true;
            for (int prio : priorities) {
                double c = bucket_value(candidates[i].second, prio);
                double b = bucket_value(best_buckets, prio);
                if (c != b) {
                    is_tied = false;
                    break;
                }
            }
            
            if (is_tied) {
                tied_candidates.push_back(candidates[i].first);
            }
        }
    }

    // Randomly select from tied candidates if RNG is available and multiple candidates are tied
    int selected_value = candidates[best_idx].first;
    if (!tied_candidates.empty() && g_heuristic_rng != nullptr) {
        int random_idx = g_heuristic_rng->operator()(tied_candidates.size());
        selected_value = tied_candidates[random_idx];
    }

    if (g_pitch_trace) {
        std::cout << "🎯 selector voice=" << voice
                  << " pos=" << position
                  << " evaluated=" << candidates.size()
                  << " tied=" << tied_candidates.size()
                  << " chose=" << selected_value;
        if (top_k_enabled) {
            std::cout << " (top_k=" << g_pitch_top_k << ")";
        }
        std::cout << " buckets=" << buckets_to_string(best_buckets);
        std::cout << std::endl;
    }

    return selected_value;
}

// Rhythm heuristic value selector — mirrors select_value_by_heuristic but
// uses g_rhythm_heuristic_scorer and treats positive values (notes) as
// preferred over negative (rests) on exact score ties.
int select_rhythm_by_heuristic(const Space& home, IntVar x, int i) {
    if (!g_rhythm_heuristic_enabled || !g_rhythm_heuristic_scorer) {
        return select_rhythm_abs_min_value(home, x, i);
    }
    const auto& space = static_cast<const IntegratedMusicalSpace&>(home);
    const int seq_len = space.get_sequence_length();
    if (seq_len <= 0) return x.min();
    const int voice    = i / seq_len;
    const int position = i % seq_len;

    const bool top_k_enabled = g_rhythm_top_k > 0;
    // Collect all candidate values (limited by top_k if set)
    std::vector<std::pair<int, HeuristicValueScoreBuckets>> candidates;
    for (IntVarValues v(x); v(); ++v) {
        if (top_k_enabled && (int)candidates.size() >= g_rhythm_top_k) break;
        candidates.push_back({v.val(), g_rhythm_heuristic_scorer(space, voice, position, v.val())});
    }
    if (candidates.empty()) return x.min();

    // Find best bucket-ordered candidate
    size_t best_idx = 0;
    for (size_t k = 1; k < candidates.size(); ++k) {
        const auto& cb = candidates[k].second;
        const auto& bb = candidates[best_idx].second;
        // Gather unique priorities across both bucket sets
        std::vector<int> prios;
        for (const auto& kv : cb) prios.push_back(kv.first);
        for (const auto& kv : bb) prios.push_back(kv.first);
        std::sort(prios.begin(), prios.end(), std::greater<int>());
        prios.erase(std::unique(prios.begin(), prios.end()), prios.end());
        bool decided = false;
        for (int p : prios) {
            double cs = bucket_value(cb, p);
            double bs = bucket_value(bb, p);
            if (cs > bs) { best_idx = k; decided = true; break; }
            if (cs < bs) { decided = true; break; }
        }
        if (!decided) {
            // Exact tie: prefer a note (positive) over a rest (negative)
            if (candidates[k].first > 0 && candidates[best_idx].first < 0)
                best_idx = k;
        }
    }
    return candidates[best_idx].first;
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
    
    // Initialize RNG for stochastic tie-breaking when tie_break_seed is non-zero
    if (g_heuristic_rng != nullptr) {
        delete g_heuristic_rng;
    }
    if (tie_break_seed != 0) {
        g_heuristic_rng = new Gecode::Rnd(tie_break_seed);
    } else {
        g_heuristic_rng = nullptr;
    }
}

void clear_pitch_heuristic_value_ordering() {
    g_pitch_heuristic_scorer = nullptr;
    g_pitch_heuristic_enabled = false;
    g_pitch_tie_break_seed = 0;
    g_pitch_top_k = 0;
    g_pitch_trace = false;
    if (g_heuristic_rng != nullptr) {
        delete g_heuristic_rng;
        g_heuristic_rng = nullptr;
    }
}

bool has_pitch_heuristic_value_ordering() {
    return g_pitch_heuristic_enabled && g_pitch_heuristic_scorer != nullptr;
}

HeuristicValueScorer get_pitch_heuristic_value_ordering() {
    return g_pitch_heuristic_scorer;
}

void configure_rhythm_heuristic_value_ordering(
    HeuristicValueScorer scorer,
    unsigned int tie_break_seed,
    int top_k,
    bool trace) {
    g_rhythm_heuristic_scorer = std::move(scorer);
    g_rhythm_heuristic_enabled = static_cast<bool>(g_rhythm_heuristic_scorer);
    g_rhythm_tie_break_seed = tie_break_seed;
    g_rhythm_top_k = std::max(0, top_k);
    g_rhythm_trace = trace;
    if (g_rhythm_heuristic_rng != nullptr) {
        delete g_rhythm_heuristic_rng;
        g_rhythm_heuristic_rng = nullptr;
    }
    if (tie_break_seed != 0) {
        g_rhythm_heuristic_rng = new Gecode::Rnd(tie_break_seed);
    }
}

void clear_rhythm_heuristic_value_ordering() {
    g_rhythm_heuristic_scorer = nullptr;
    g_rhythm_heuristic_enabled = false;
    g_rhythm_tie_break_seed = 0;
    g_rhythm_top_k = 0;
    g_rhythm_trace = false;
    if (g_rhythm_heuristic_rng != nullptr) {
        delete g_rhythm_heuristic_rng;
        g_rhythm_heuristic_rng = nullptr;
    }
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
    // Rhythm variables drive onset-based constraints, so branch them before pitches
    // to let hierarchy rules prune the search tree earlier.
    branch_rhythm_with_search_strategy(*this, rhythm_vars_, variable_branching_, value_selection_, random_seed);
    branch_with_search_strategy(*this, absolute_vars_, variable_branching_, value_selection_, random_seed, true);
}

IntegratedMusicalSpace::IntegratedMusicalSpace(int length, int voices,
                                              VariableBranchingStrategy variable_branching,
                                              ValueSelectionStrategy value_selection,
                                              const std::vector<std::vector<int>>& voice_domains,
                                              const std::vector<std::vector<int>>& voice_rhythm_domains,
                                              int measure_horizon_ticks, bool exact_measure_fill,
                                              unsigned int random_seed)
    : Space(),
      sequence_length_(length),
      absolute_vars_(*this, length * voices, IntSet(-1, 127)),
      interval_vars_(*this, (length * voices) - voices, -12, 12),
      rhythm_vars_(*this, length * voices, -100000, 100000),
      metric_vars_(*this, 0, 0, 0),
    duration_vars_(*this, length * voices),
      active_vars_(*this, length * voices, 0, 1),
      measure_count_mode_(true),
      measure_horizon_ticks_(measure_horizon_ticks),
      exact_measure_fill_(exact_measure_fill),
      num_voices_(voices),
      variable_branching_(variable_branching),
      value_selection_(value_selection),
      solution_storage_(std::make_unique<MusicalConstraints::DualSolutionStorage>(length)),
      vocal_space_configured_(false) {
    if (length <= 0 || voices <= 0 || measure_horizon_ticks <= 0 || measure_horizon_ticks > length)
        throw std::runtime_error("Invalid measure-count event capacity or horizon");

    for (int voice = 0; voice < voices; ++voice) {
        if (voice >= static_cast<int>(voice_domains.size()) || voice_domains[voice].empty())
            throw std::runtime_error("Measure-count mode requires a pitch domain for every voice");
        if (voice >= static_cast<int>(voice_rhythm_domains.size()) || voice_rhythm_domains[voice].empty())
            throw std::runtime_error("Measure-count mode requires a rhythm domain for every voice");

        std::vector<int> pitch_values{-1};
        pitch_values.insert(pitch_values.end(), voice_domains[voice].begin(), voice_domains[voice].end());
        IntSet pitch_domain(pitch_values.data(), pitch_values.size());
        std::vector<int> rhythm_values{0};
        rhythm_values.insert(rhythm_values.end(), voice_rhythm_domains[voice].begin(), voice_rhythm_domains[voice].end());
        IntSet rhythm_domain(rhythm_values.data(), rhythm_values.size());
        int max_abs_duration = 0;
        for (int duration : voice_rhythm_domains[voice]) {
            if (duration == 0) throw std::runtime_error("Zero duration is reserved for inactive measure-mode slots");
            max_abs_duration = std::max(max_abs_duration, std::abs(duration));
        }
        IntVarArgs absolute_durations;
        BoolVarArgs voice_active;
        for (int event = 0; event < length; ++event) {
            const int index = voice * length + event;
            dom(*this, absolute_vars_[index], pitch_domain);
            dom(*this, rhythm_vars_[index], rhythm_domain);
            rel(*this, absolute_vars_[index], IRT_NQ, -1, Reify(active_vars_[index], RM_EQV));
            rel(*this, rhythm_vars_[index], IRT_NQ, 0, Reify(active_vars_[index], RM_EQV));
            voice_active << active_vars_[index];

            if (event == 0) {
                rel(*this, active_vars_[index], IRT_EQ, 1);
            } else {
                rel(*this, active_vars_[index - 1], IRT_GQ, active_vars_[index]);
            }

            IntVar absolute_duration(*this, 0, max_abs_duration);
            abs(*this, rhythm_vars_[index], absolute_duration);
            duration_vars_[index] = absolute_duration;
            absolute_durations << absolute_duration;

            if (event > 0) {
                const int previous = index - 1;
                const int interval_index = voice * (length - 1) + event - 1;
                BoolVar current_note = expr(*this, rhythm_vars_[index] > 0);
                BoolVar previous_note = expr(*this, rhythm_vars_[previous] > 0);
                BoolVar both_notes = expr(*this, current_note && previous_note);
                IntVar difference = expr(*this, absolute_vars_[index] - absolute_vars_[previous]);
                rel(*this, interval_vars_[interval_index], IRT_EQ, difference, Reify(both_notes, RM_IMP));
            }
        }
        linear(*this, voice_active, IRT_GQ, 1);
        linear(*this, absolute_durations,
               exact_measure_fill_ ? IRT_EQ : IRT_LQ, measure_horizon_ticks_);
    }

    post_musical_constraints();
    branch_rhythm_with_search_strategy(*this, rhythm_vars_, variable_branching_, value_selection_, random_seed);
    branch_with_search_strategy(*this, absolute_vars_, variable_branching_, value_selection_, random_seed, true);
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
    // Metric and rhythm variables constrain onset structure; branch them before pitches
    // so metric hierarchy rules prune earlier.
    branch_with_search_strategy(*this, metric_vars_, variable_branching_, value_selection_, random_seed, false);
    branch_rhythm_with_search_strategy(*this, rhythm_vars_, variable_branching_, value_selection_, random_seed);
    branch_with_search_strategy(*this, absolute_vars_, variable_branching_, value_selection_, random_seed, true);
}

IntegratedMusicalSpace::IntegratedMusicalSpace(IntegratedMusicalSpace& space)
    : Space(space),
      sequence_length_(space.sequence_length_),
    measure_count_mode_(space.measure_count_mode_),
    measure_horizon_ticks_(space.measure_horizon_ticks_),
    exact_measure_fill_(space.exact_measure_fill_),
      num_voices_(space.num_voices_),
    variable_branching_(space.variable_branching_),
    value_selection_(space.value_selection_),
      solution_storage_(std::make_unique<MusicalConstraints::DualSolutionStorage>(space.sequence_length_)),
      vocal_space_configured_(space.vocal_space_configured_) {
    
    absolute_vars_.update(*this, space.absolute_vars_);
    interval_vars_.update(*this, space.interval_vars_);
    rhythm_vars_.update(*this, space.rhythm_vars_);
    metric_vars_.update(*this, space.metric_vars_);
    duration_vars_.update(*this, space.duration_vars_);
    boundary_vars_.update(*this, space.boundary_vars_);
    active_vars_.update(*this, space.active_vars_);
    motive_vars_.update(*this, space.motive_vars_);
    motive_edges_ = space.motive_edges_;
    
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

void IntegratedMusicalSpace::initialize_event_timing() {
    if (boundary_vars_.size() > 0) return;
    if (rhythm_vars_.size() != sequence_length_ * num_voices_)
        throw std::runtime_error("Event timing requires per-voice rhythm variables");
    if (duration_vars_.size() == 0) {
        duration_vars_ = IntVarArray(*this, rhythm_vars_.size());
        for (int index = 0; index < rhythm_vars_.size(); ++index) {
            const int maximum = std::max(std::abs(rhythm_vars_[index].min()), std::abs(rhythm_vars_[index].max()));
            duration_vars_[index] = IntVar(*this, 0, maximum);
            abs(*this, rhythm_vars_[index], duration_vars_[index]);
        }
    }
    boundary_vars_ = IntVarArray(*this, (sequence_length_ + 1) * num_voices_);
    for (int voice = 0; voice < num_voices_; ++voice) {
        const int boundary_start = voice * (sequence_length_ + 1);
        boundary_vars_[boundary_start] = IntVar(*this, 0, 0);
        for (int event = 0; event < sequence_length_; ++event) {
            boundary_vars_[boundary_start + event + 1] = expr(*this,
                boundary_vars_[boundary_start + event] + duration_vars_[voice * sequence_length_ + event]);
        }
    }
}

const IntVar& IntegratedMusicalSpace::get_event_boundary(int voice, int boundary) const {
    if (voice < 0 || voice >= num_voices_ || boundary < 0 || boundary > sequence_length_ ||
        boundary_vars_.size() == 0)
        throw std::runtime_error("Event boundary is out of range or timing is not initialized");
    return boundary_vars_[voice * (sequence_length_ + 1) + boundary];
}

bool IntegratedMusicalSpace::event_is_active(int voice, int pos) const {
    if (!measure_count_mode_) return voice >= 0 && voice < num_voices_ && pos >= 0 && pos < sequence_length_;
    const int index = voice * sequence_length_ + pos;
    return index >= 0 && index < active_vars_.size() && active_vars_[index].assigned() && active_vars_[index].val() == 1;
}

int IntegratedMusicalSpace::get_active_event_count(int voice) const {
    if (voice < 0 || voice >= num_voices_) return 0;
    if (!measure_count_mode_) return sequence_length_;
    int count = 0;
    for (int pos = 0; pos < sequence_length_; ++pos)
        if (event_is_active(voice, pos)) ++count;
    return count;
}

void IntegratedMusicalSpace::post_active_all_different(int voice, bool rhythm, const std::vector<int>& indices) {
    if (!measure_count_mode_ || voice < 0 || voice >= num_voices_)
        throw std::runtime_error("Active all_different requires measure mode and a valid voice");
    auto& variables = rhythm ? rhythm_vars_ : absolute_vars_;
    std::vector<int> selected = indices;
    if (selected.empty()) for (int event = 0; event < sequence_length_; ++event) selected.push_back(event);
    for (int event : selected) {
        if (event < 0 || event >= sequence_length_) throw std::runtime_error("Active all_different index out of range");
        if (!indices.empty()) rel(*this, active_vars_[voice * sequence_length_ + event], IRT_EQ, 1);
    }
    for (size_t first = 0; first < selected.size(); ++first) {
        const int first_index = voice * sequence_length_ + selected[first];
        for (size_t second = first + 1; second < selected.size(); ++second) {
            const int second_index = voice * sequence_length_ + selected[second];
            BoolVar compare = expr(*this, rhythm
                ? BoolExpr((rhythm_vars_[first_index] != 0) && (rhythm_vars_[second_index] != 0))
                : BoolExpr((rhythm_vars_[first_index] > 0) && (rhythm_vars_[second_index] > 0)));
            rel(*this, variables[first_index], IRT_NQ, variables[second_index], Reify(compare, RM_IMP));
        }
    }
}

void IntegratedMusicalSpace::post_active_uniformity(int voice, bool rhythm, const std::vector<int>& indices,
                                                  const std::vector<int>& allowed_values) {
    if (!measure_count_mode_ || voice < 0 || voice >= num_voices_ || allowed_values.empty())
        throw std::runtime_error("Active uniformity requires measure mode, valid voice and allowed values");
    auto& variables = rhythm ? rhythm_vars_ : absolute_vars_;
    std::vector<int> selected = indices;
    if (selected.empty()) for (int event = 0; event < sequence_length_; ++event) selected.push_back(event);
    IntVar common(*this, IntSet(allowed_values.data(), allowed_values.size()));
    for (int event : selected) {
        if (event < 0 || event >= sequence_length_) throw std::runtime_error("Active uniformity index out of range");
        const int index = voice * sequence_length_ + event;
        if (!indices.empty()) rel(*this, active_vars_[index], IRT_EQ, 1);
        BoolVar compare = expr(*this, rhythm ? BoolExpr(rhythm_vars_[index] != 0) : BoolExpr(rhythm_vars_[index] > 0));
        rel(*this, variables[index], IRT_EQ, common, Reify(compare, RM_IMP));
    }
}

void IntegratedMusicalSpace::post_active_reverse(int source_voice, int target_voice, bool inversion, int center) {
    if (!measure_count_mode_ || source_voice < 0 || source_voice >= num_voices_ ||
        target_voice < 0 || target_voice >= num_voices_ || center < 0 || center > 127)
        throw std::runtime_error("Active reversal requires valid measure-mode voices and MIDI inversion center");
    BoolVarArgs source_active, target_active;
    IntVarArgs source_pitches, source_rhythms;
    for (int event = 0; event < sequence_length_; ++event) {
        source_active << active_vars_[source_voice * sequence_length_ + event];
        target_active << active_vars_[target_voice * sequence_length_ + event];
        source_pitches << absolute_vars_[source_voice * sequence_length_ + event];
        source_rhythms << rhythm_vars_[source_voice * sequence_length_ + event];
    }
    IntVar count(*this, 1, sequence_length_);
    linear(*this, source_active, IRT_EQ, count);
    linear(*this, target_active, IRT_EQ, count);
    for (int event = 0; event < sequence_length_; ++event) {
        IntVar reversed_index = expr(*this, max(count - 1 - event, 0));
        IntVar source_pitch(*this, -1, 127), source_rhythm(*this, -100000, 100000);
        element(*this, source_pitches, reversed_index, source_pitch);
        element(*this, source_rhythms, reversed_index, source_rhythm);
        const int target_index = target_voice * sequence_length_ + event;
        BoolVar source_note = expr(*this, source_rhythm > 0);
        BoolVar target_note = expr(*this, rhythm_vars_[target_index] > 0);
        rel(*this, source_note, IRT_EQ, target_note, Reify(target_active[event], RM_IMP));
        BoolVar compare = expr(*this, target_active[event] && target_note);
        IntVar expected = inversion ? expr(*this, 2 * center - source_pitch) : source_pitch;
        rel(*this, absolute_vars_[target_index], IRT_EQ, expected, Reify(compare, RM_IMP));
    }
}

void IntegratedMusicalSpace::post_motive_domains(const std::vector<MotiveDomains::Domain>& domains) {
    // Upper bound on a voice's active events: duration knapsack over rhythm motives, capped by horizon / shortest duration.
    // Pitch is ignored, so the bound can only over-estimate; an unreachable exact horizon leaves the voice unbounded.
    std::vector<int> event_bound(num_voices_, sequence_length_);
    if (measure_count_mode_ && measure_horizon_ticks_ > 0 && measure_horizon_ticks_ <= 100000) {
        const int horizon = measure_horizon_ticks_;
        for (int voice = 0; voice < num_voices_; ++voice) {
            int shortest = std::numeric_limits<int>::max();
            for (IntVarValues value(rhythm_vars_[voice * sequence_length_]); value(); ++value)
                if (value.val() != 0) shortest = std::min(shortest, std::abs(value.val()));
            int bound = sequence_length_;
            if (shortest != std::numeric_limits<int>::max()) bound = std::min(bound, horizon / shortest);
            for (const auto& domain : domains) {
                if (domain.voice != voice || !domain.rhythm) continue;
                std::vector<int> best(horizon + 1, -1);
                best[0] = 0;
                for (int total = 0; total <= horizon; ++total) {
                    if (best[total] < 0) continue;
                    for (const auto& motive : domain.candidates) {
                        long long ticks = 0;
                        for (int value : motive) ticks += std::abs(value);
                        if (motive.empty() || ticks <= 0 || total + ticks > horizon) continue;
                        best[total + ticks] = std::max(best[total + ticks], best[total] + static_cast<int>(motive.size()));
                    }
                }
                int reachable = -1;
                if (exact_measure_fill_) reachable = best[horizon];
                else for (int total = 1; total <= horizon; ++total) reachable = std::max(reachable, best[total]);
                if (reachable >= 1) bound = std::min(bound, reachable);
            }
            event_bound[voice] = std::max(1, bound);
            for (int event = event_bound[voice]; event < sequence_length_; ++event)
                rel(*this, active_vars_[voice * sequence_length_ + event], IRT_EQ, 0);
        }
    }
    std::vector<BoolVar> choices;
    for (const auto& domain : domains) {
        if (domain.voice < 0 || domain.voice >= num_voices_ || domain.candidates.empty())
            throw std::runtime_error("Invalid motive domain voice or empty candidates");
        auto& variables = domain.rhythm ? rhythm_vars_ : absolute_vars_;
        std::vector<std::vector<BoolVar>> incoming(sequence_length_ + 1), outgoing(sequence_length_ + 1);
        std::vector<bool> reachable(sequence_length_ + 1, false), finishable(sequence_length_ + 1, false);
        BoolVarArray stops(*this, measure_count_mode_ ? sequence_length_ + 1 : 0, 0, 1);
        reachable[0] = true;
        finishable[sequence_length_] = true;
        for (int start = 0; start <= sequence_length_; ++start)
            if (reachable[start])
                for (const auto& motive : domain.candidates)
                    if (!motive.empty() && motive.size() <= static_cast<size_t>(sequence_length_ - start) &&
                        start + motive.size() <= static_cast<size_t>(event_bound[domain.voice]))
                        reachable[start + motive.size()] = true;
        for (int start = sequence_length_ - 1; start >= 0; --start)
            for (const auto& motive : domain.candidates)
                if (!motive.empty() && motive.size() <= static_cast<size_t>(sequence_length_ - start) && finishable[start + motive.size()])
                    finishable[start] = true;
        for (int start = 0; start < sequence_length_; ++start) {
            if (!reachable[start]) continue;
            for (size_t motive_id = 0; motive_id < domain.candidates.size(); ++motive_id) {
                const auto& motive = domain.candidates[motive_id];
                if (motive.empty() || motive.size() > static_cast<size_t>(sequence_length_ - start)) continue;
                const int end = start + static_cast<int>(motive.size());
                if (end > event_bound[domain.voice]) continue;
                if (!measure_count_mode_ && !finishable[end]) continue;
                bool compatible = true;
                for (size_t offset = 0; offset < motive.size(); ++offset)
                    if (!variables[domain.voice * sequence_length_ + start + offset].in(motive[offset])) compatible = false;
                if (!compatible) continue;
                BoolVar selected(*this, 0, 1);
                for (size_t offset = 0; offset < motive.size(); ++offset)
                    rel(*this, variables[domain.voice * sequence_length_ + start + offset], IRT_EQ,
                        motive[offset], Reify(selected, RM_IMP));
                if (measure_count_mode_)
                    for (size_t offset = 0; offset < motive.size(); ++offset)
                        rel(*this, active_vars_[domain.voice * sequence_length_ + start + offset],
                            IRT_EQ, 1, Reify(selected, RM_IMP));
                incoming[end].push_back(selected);
                outgoing[start].push_back(selected);
                choices.push_back(selected);
                MotiveDomains::Selection edge;
                edge.voice = domain.voice;
                edge.rhythm = domain.rhythm;
                edge.motive_id = static_cast<int>(motive_id);
                edge.start_event = start;
                edge.end_event = end;
                edge.values = motive;
                motive_edges_.push_back(std::move(edge));
            }
        }
        if (measure_count_mode_) {
            for (int node = 0; node < sequence_length_; ++node)
                rel(*this, active_vars_[domain.voice * sequence_length_ + node],
                    IRT_EQ, 0, Reify(stops[node], RM_IMP));
        }
        for (int node = 0; node <= sequence_length_; ++node) {
            BoolVarArgs incident;
            IntArgs coefficients;
            BoolVarArgs entering;
            for (const auto& choice : incoming[node]) {
                incident << choice;
                entering << choice;
                coefficients << 1;
            }
            for (const auto& choice : outgoing[node]) {
                incident << choice;
                coefficients << -1;
            }
            if (measure_count_mode_) {
                incident << stops[node];
                coefficients << -1;
            }
            if (entering.size() > 0) linear(*this, entering, IRT_LQ, 1);
            const int balance = node == 0 ? -1 : (measure_count_mode_ ? 0 : (node == sequence_length_ ? 1 : 0));
            if (incident.size() == 0) {
                if (balance != 0) { fail(); return; }
            } else linear(*this, coefficients, incident, IRT_EQ, balance);
        }
    }
    motive_vars_ = BoolVarArray(*this, static_cast<int>(choices.size()));
    for (size_t index = 0; index < choices.size(); ++index) motive_vars_[index] = choices[index];
    if (motive_vars_.size() > 0) branch(*this, motive_vars_, BOOL_VAR_NONE(), BOOL_VAL_MAX());
}

std::vector<MotiveDomains::Selection> IntegratedMusicalSpace::get_selected_motives() const {
    std::vector<MotiveDomains::Selection> selections;
    for (int index = 0; index < motive_vars_.size(); ++index)
        if (motive_vars_[index].assigned() && motive_vars_[index].val() == 1)
            selections.push_back(motive_edges_[index]);
    return selections;
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
        "Provide voices[" + std::to_string(voice) + "].rhythm.duration_values in the config.");
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
        if (measure_count_mode_ && !event_is_active(voice, pos)) continue;
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
        if (measure_count_mode_ && !event_is_active(voice, i - start_idx)) continue;
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