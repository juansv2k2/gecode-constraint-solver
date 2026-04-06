/**
 * @file cluster_engine_implementation.cpp
 * @brief Basic Implementation Stubs for Cluster Engine Architecture
 * 
 * Provides minimal implementation stubs to demonstrate the architecture.
 * This shows the complete system design without full implementation complexity.
 */

#include "cluster_engine_interface.hh"
#include "cluster_engine_stop_rules.hh"
#include <iostream>
#include <algorithm>
#include <random>
#include <cmath>
#include <iomanip>
#include <numeric>

namespace ClusterEngine {

// ===== ClusterEngineCore Implementation =====

ClusterEngineCore::ClusterEngineCore(int num_voices, int max_index_per_engine, bool debug)
    : num_voices_(num_voices), max_index_(max_index_per_engine), 
      current_engine_(0), debug_mode_(debug) {
    
    initialize_engines();
    
    if (debug_mode_) {
        std::cout << "🎼 ClusterEngineCore initialized with " << num_voices_ 
                  << " voices (" << get_num_engines() << " engines total)\n";
    }
}

void ClusterEngineCore::initialize_engines() {
    engines_.clear();
    
    // Create rhythm and pitch engines for each voice
    for (int voice = 0; voice < num_voices_; ++voice) {
        // Rhythm engine (even ID)
        int rhythm_id = voice * 2;
        engines_.emplace_back(std::make_unique<MusicalEngine>(rhythm_id, EngineType::RHYTHM_ENGINE));
        
        // Pitch engine (odd ID)  
        int pitch_id = voice * 2 + 1;
        engines_.emplace_back(std::make_unique<MusicalEngine>(pitch_id, EngineType::PITCH_ENGINE));
    }
    
    // Metric engine (last)
    int metric_id = num_voices_ * 2;
    engines_.emplace_back(std::make_unique<MusicalEngine>(metric_id, EngineType::METRIC_ENGINE));
}

void ClusterEngineCore::print_engine_status() const {
    std::cout << "\n🎛️ Engine Status:\n";
    for (const auto& engine : engines_) {
        std::cout << "  Engine " << engine->get_id() << ": ";
        if (engine->is_rhythm_engine()) {
            std::cout << "RHYTHM (Voice " << engine->get_voice_id() << ")";
        } else if (engine->is_pitch_engine()) {
            std::cout << "PITCH (Voice " << engine->get_voice_id() << ")";
        } else {
            std::cout << "METRIC";
        }
        std::cout << " - Index: " << engine->get_index() 
                  << ", Domain size: " << engine->get_domain().size();
        if (engine->is_locked()) std::cout << " [LOCKED]";
        std::cout << "\n";
    }
}

// ===== MusicalEngine Implementation =====

// Implementation stubs for key methods
bool MusicalEngine::is_rhythm_engine() const { 
    return engine_id_ % 2 == 0 && type_ != EngineType::METRIC_ENGINE; 
}

bool MusicalEngine::is_pitch_engine() const { 
    return engine_id_ % 2 == 1 && type_ != EngineType::METRIC_ENGINE; 
}

bool MusicalEngine::is_metric_engine() const { 
    return type_ == EngineType::METRIC_ENGINE; 
}

int MusicalEngine::get_voice_id() const {
    if (is_metric_engine()) return -1;
    return engine_id_ / 2;
}

int MusicalEngine::get_partner_engine_id() const {
    if (is_metric_engine()) return -1;
    return is_rhythm_engine() ? engine_id_ + 1 : engine_id_ - 1;
}

// ===== MusicalDomain Implementation =====

void MusicalDomain::sort_by_heuristic_weights() {
    if (!weights_calculated_) return;
    
    std::vector<size_t> indices(candidates_.size());
    std::iota(indices.begin(), indices.end(), 0);
    
    std::sort(indices.begin(), indices.end(),
              [this](size_t a, size_t b) {
                  return heuristic_weights_[a] > heuristic_weights_[b];
              });
    
    std::vector<MusicalCandidate> sorted_candidates;
    std::vector<double> sorted_weights;
    
    for (size_t idx : indices) {
        sorted_candidates.push_back(candidates_[idx]);
        sorted_weights.push_back(heuristic_weights_[idx]);
    }
    
    candidates_ = std::move(sorted_candidates);
    heuristic_weights_ = std::move(sorted_weights);
}

// ===== EngineDomainBuilder Implementation =====

MusicalDomain EngineDomainBuilder::build_rhythm_domain(
    const std::vector<int>& duration_values,
    bool allow_rests,
    DomainInitType init_type) {
    
    MusicalDomain domain(EngineType::RHYTHM_ENGINE);
    
    for (int duration : duration_values) {
        domain.add_candidate(MusicalCandidate(duration, duration, true));
    }
    
    if (allow_rests) {
        domain.add_candidate(MusicalCandidate(-1, -1, true)); // Rest notation
    }
    
    return domain;
}

MusicalDomain EngineDomainBuilder::build_pitch_domain(
    int min_midi_pitch,
    int max_midi_pitch,
    const std::vector<int>& allowed_pitches,
    DomainInitType init_type) {
    
    MusicalDomain domain(EngineType::PITCH_ENGINE);
    
    if (allowed_pitches.empty()) {
        // Chromatic scale
        for (int pitch = min_midi_pitch; pitch <= max_midi_pitch; ++pitch) {
            int interval = pitch - 60; // Interval from C4
            domain.add_candidate(MusicalCandidate(pitch, interval, true));
        }
    } else {
        // Specific pitch set
        for (int pitch : allowed_pitches) {
            if (pitch >= min_midi_pitch && pitch <= max_midi_pitch) {
                int interval = pitch - 60;
                domain.add_candidate(MusicalCandidate(pitch, interval, true));
            }
        }
    }
    
    return domain;
}

MusicalDomain EngineDomainBuilder::build_metric_domain(
    const MetricDomain& metric_domain,
    DomainInitType init_type) {
    
    MusicalDomain domain(EngineType::METRIC_ENGINE);
    
    for (const auto& ts : metric_domain.get_time_signatures()) {
        int encoded_ts = ts.numerator * 100 + ts.denominator; // Simple encoding
        domain.add_candidate(MusicalCandidate(encoded_ts, ts.numerator, true));
    }
    
    return domain;
}

// ===== MetricDomain Implementation =====

void MetricDomain::create_from_time_signatures(
    const std::vector<TimeSignature>& time_sigs,
    const std::vector<std::vector<int>>& tuplets_per_sig,
    const std::vector<std::vector<double>>& alt_beat_lengths) {
    
    time_signatures_ = time_sigs;
    onset_grids_.clear();
    beat_structures_.clear();
    
    for (size_t i = 0; i < time_sigs.size(); ++i) {
        const auto& ts = time_sigs[i];
        
        // Create onset grid
        std::vector<int> tuplets = (i < tuplets_per_sig.size()) ? 
            tuplets_per_sig[i] : std::vector<int>{3, 4};
        onset_grids_.emplace_back(ts, tuplets);
        
        // Create beat structure
        beat_structures_.emplace_back(ts);
        if (i < alt_beat_lengths.size() && !alt_beat_lengths[i].empty()) {
            beat_structures_.back().set_alternative_beat_lengths(alt_beat_lengths[i]);
        }
    }
}

// ===== OnsetGrid Implementation =====

void OnsetGrid::calculate_onset_points() {
    onset_points.clear();
    onset_points.push_back(0.0); // Always start at 0
    
    double measure_dur = time_signature.measure_duration();
    
    for (int tuplet : tuplets) {
        double subdivision = measure_dur / (time_signature.numerator * tuplet);
        for (int i = 1; i < time_signature.numerator * tuplet; ++i) {
            double onset = i * subdivision;
            onset_points.push_back(onset);
        }
    }
    
    onset_points.push_back(measure_dur); // End of measure
    
    // Remove duplicates and sort
    std::sort(onset_points.begin(), onset_points.end());
    onset_points.erase(std::unique(onset_points.begin(), onset_points.end()), onset_points.end());
}

// ===== BeatStructure Implementation =====

void BeatStructure::calculate_default_beat_structure() {
    beat_positions.clear();
    beat_strengths.clear();
    
    for (int beat = 0; beat < time_signature.numerator; ++beat) {
        double pos = beat * time_signature.beat_duration();
        beat_positions.push_back(pos);
        
        // Simple beat strength calculation
        double strength = (beat == 0) ? 1.0 : ((beat % 2 == 0) ? 0.7 : 0.3);
        beat_strengths.push_back(strength);
    }
}

double BeatStructure::get_beat_strength_at(double time_point) const {
    for (size_t i = 0; i < beat_positions.size(); ++i) {
        if (std::abs(beat_positions[i] - time_point) < 0.1) {
            return beat_strengths[i];
        }
    }
    return 0.0; // Off-beat
}

void BeatStructure::set_alternative_beat_lengths(const std::vector<double>& alt_lengths) {
    // Simple implementation - just store the alternative lengths
    alt_beat_lengths = alt_lengths;
    
    // Recalculate beat structure if needed
    if (!alt_lengths.empty()) {
        beat_positions.clear();
        beat_strengths.clear();
        
        double current_pos = 0.0;
        for (size_t i = 0; i < alt_lengths.size(); ++i) {
            beat_positions.push_back(current_pos);
            beat_strengths.push_back((i == 0) ? 1.0 : 0.5); // Strong on first beat
            current_pos += alt_lengths[i];
        }
    } else {
        calculate_default_beat_structure();
    }
}

// ===== HeuristicRule Implementation =====

double HeuristicRule::calculate_weight(const std::vector<MusicalCandidate>& context, int candidate_index) const {
    if (weight_function_) {
        return weight_function_(context, candidate_index);
    }
    return 1.0; // Default weight
}

// ===== MusicalRule Base Implementation =====

bool MusicalRule::applies_to(int engine, int index) const {
    // Check if this engine is in target engines
    bool engine_match = std::find(target_engines_.begin(), target_engines_.end(), engine) != target_engines_.end();
    
    // For now, apply to all indices if engine matches
    return engine_match;
}

std::vector<MusicalCandidate> MusicalRule::get_rhythm_motif_sequence(
    const MusicalRuleContext& ctx, int length) const {
    
    std::vector<MusicalCandidate> sequence;
    int start_idx = std::max(0, ctx.current_index - length + 1);
    
    for (int i = start_idx; i <= ctx.current_index && i < static_cast<int>(ctx.solution_sequence.size()); ++i) {
        sequence.push_back(ctx.solution_sequence[i]);
    }
    
    return sequence;
}

std::vector<int> MusicalRule::get_absolute_values(const std::vector<MusicalCandidate>& candidates) const {
    std::vector<int> values;
    for (const auto& candidate : candidates) {
        values.push_back(candidate.absolute_value);
    }
    return values;
}

std::vector<int> MusicalRule::get_interval_values(const std::vector<MusicalCandidate>& candidates) const {
    std::vector<int> values;
    for (const auto& candidate : candidates) {
        values.push_back(candidate.interval_value);
    }
    return values;
}

// ===== ClusterEngineInterface Implementation =====

ClusterEngineInterface::ClusterEngineInterface()
    : initialized_(false), search_running_(false) {
    termination_manager_ = std::make_unique<SearchTerminationManager>();
}

ClusterEngineInterface::~ClusterEngineInterface() = default;

bool ClusterEngineInterface::initialize(const DomainConfiguration& domain_config,
                                      const SearchConfig& search_config) {
    try {
        domain_config_ = domain_config;
        config_ = search_config;
        
        // Calculate number of voices from domain configuration
        int num_voices = domain_config.engine_domains.size() / 2; // Rhythm + pitch pairs
        
        // Create core engine
        core_ = std::make_unique<ClusterEngineCore>(num_voices, config_.max_variables_per_engine, config_.debug_output);
        
        // Initialize domains for engines
        for (size_t i = 0; i < domain_config.engine_domains.size() && i < static_cast<size_t>(core_->get_num_engines()); ++i) {
            core_->get_engine(i).get_domain() = domain_config.engine_domains[i];
        }
        
        // Setup stop rules if enabled
        setup_stop_rules();
        
        initialized_ = true;
        
        if (config_.verbose_output) {
            std::cout << "✅ ClusterEngineInterface initialized successfully\n";
            std::cout << "   Voices: " << num_voices << "\n";
            std::cout << "   Engines: " << core_->get_num_engines() << "\n";
            std::cout << "   Max variables per engine: " << config_.max_variables_per_engine << "\n";
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "❌ Initialization failed: " << e.what() << "\n";
        return false;
    }
}

void ClusterEngineInterface::add_constraint_rule(std::unique_ptr<MusicalRule> rule) {
    constraint_rules_.push_back(std::move(rule));
}

void ClusterEngineInterface::add_heuristic_rule(const HeuristicRule& rule) {
    heuristic_rules_.push_back(rule);
}

void ClusterEngineInterface::configure_stop_rules(const std::string& rule_type, 
                                                 int max_length,
                                                 bool enable_advanced) {
    if (!termination_manager_) {
        termination_manager_ = std::make_unique<SearchTerminationManager>();
    }
    
    // Calculate voice information
    int num_voices = (core_ ? (core_->get_num_engines() - 1) / 2 : 1);
    std::vector<int> voices;
    for (int i = 0; i < num_voices; ++i) {
        voices.push_back(i);
    }
    
    std::unique_ptr<StopRuleManager> stop_manager;
    
    if (rule_type == "jazz") {
        stop_manager = StopRuleFactory::create_jazz_stop_rules(voices, max_length, enable_advanced);
    } else if (rule_type == "realtime") {
        stop_manager = StopRuleFactory::create_realtime_stop_rules(voices, 
            config_.max_composition_length_seconds, config_.max_notes_per_voice);
    } else if (rule_type == "exercise") {
        stop_manager = StopRuleFactory::create_exercise_stop_rules(voices, "species_counterpoint");
    } else { // "classical" or default
        stop_manager = StopRuleFactory::create_classical_stop_rules(voices, max_length, enable_advanced);
    }
    
    termination_manager_->set_stop_manager(std::move(stop_manager));
}

void ClusterEngineInterface::add_stop_rule(const StopCondition& condition) {
    if (!termination_manager_ || !termination_manager_->get_stop_manager()) {
        configure_stop_rules(); // Initialize with default rules
    }
    
    termination_manager_->get_stop_manager()->add_condition(condition);
}

void ClusterEngineInterface::add_time_stop(const std::vector<int>& voices, double max_time) {
    std::vector<int> engines;
    for (int voice : voices) {
        engines.push_back(voice * 2);     // Rhythm engines
        engines.push_back(voice * 2 + 1); // Pitch engines
    }
    
    if (!termination_manager_ || !termination_manager_->get_stop_manager()) {
        configure_stop_rules();
    }
    
    termination_manager_->get_stop_manager()->add_time_stop(engines, max_time);
}

void ClusterEngineInterface::add_index_stop(const std::vector<int>& voices, int max_index) {
    std::vector<int> engines;
    for (int voice : voices) {
        engines.push_back(voice * 2);     // Rhythm engines
        engines.push_back(voice * 2 + 1); // Pitch engines
    }
    
    if (!termination_manager_ || !termination_manager_->get_stop_manager()) {
        configure_stop_rules();
    }
    
    termination_manager_->get_stop_manager()->add_index_stop(engines, max_index);
}

void ClusterEngineInterface::add_note_count_stop(const std::vector<int>& voices, int max_notes) {
    std::vector<int> pitch_engines;
    for (int voice : voices) {
        pitch_engines.push_back(voice * 2 + 1); // Pitch engines only
    }
    
    if (!termination_manager_ || !termination_manager_->get_stop_manager()) {
        configure_stop_rules();
    }
    
    termination_manager_->get_stop_manager()->add_note_count_stop(pitch_engines, max_notes);
}

void ClusterEngineInterface::clear_stop_rules() {
    if (termination_manager_ && termination_manager_->get_stop_manager()) {
        termination_manager_->get_stop_manager()->clear_conditions();
    }
}

void ClusterEngineInterface::reset_stop_state() {
    if (termination_manager_) {
        termination_manager_->reset_for_new_search();
    }
}

void ClusterEngineInterface::set_stop_rules_enabled(bool enabled) {
    config_.enable_stop_rules = enabled;
    
    if (termination_manager_) {
        termination_manager_->configure_auto_evaluation(enabled);
    }
}

bool ClusterEngineInterface::are_stop_rules_enabled() const {
    return config_.enable_stop_rules;
}

void ClusterEngineInterface::setup_stop_rules() {
    if (!config_.enable_stop_rules || !core_) {
        return;
    }
    
    // Auto-configure stop rules based on search configuration
    std::string rule_type = "classical"; // Default
    configure_stop_rules(rule_type, config_.max_measures, true);
    
    if (config_.verbose_output) {
        std::cout << "✅ Stop rules configured (" << rule_type << " style)\n";
    }
}

bool ClusterEngineInterface::check_stop_conditions() {
    if (!config_.enable_stop_rules || !termination_manager_ || !core_) {
        return false;
    }
    
    return termination_manager_->check_termination(*core_);
}

std::vector<MusicalComposition> ClusterEngineInterface::search() {
    if (!initialized_) {
        throw std::runtime_error("Engine not initialized");
    }
    
    search_running_ = true;
    stats_.clear();
    
    // Reset stop state for new search
    reset_stop_state();
    
    std::vector<MusicalComposition> results;
    
    if (config_.verbose_output) {
        std::cout << "🔍 Starting Cluster Engine search...\n";
        std::cout << "   Target solutions: " << config_.max_solutions << "\n";
        std::cout << "   Constraint rules: " << constraint_rules_.size() << "\n";
        std::cout << "   Heuristic rules: " << heuristic_rules_.size() << "\n";
        if (config_.enable_stop_rules) {
            std::cout << "   Stop rules: enabled\n";
        }
    }
    
    // Enhanced search with stop rule checking
    for (int sol = 0; sol < config_.max_solutions && sol < 3; ++sol) {
        MusicalComposition composition;
        
        // Create composition with stop rule awareness
        int num_voices = (core_->get_num_engines() - 1) / 2;
        composition.voices.resize(num_voices);
        
        bool search_stopped_by_rules = false;
        
        for (int voice = 0; voice < num_voices; ++voice) {
            // Simple ascending scale for demonstration
            int start_pitch = 60 + voice * 5; // C4, F4, etc.
            
            for (int note = 0; note < 8; ++note) { // Increased to 8 notes for stop rule testing
                // Check stop conditions during generation
                if (config_.enable_stop_rules && check_stop_conditions()) {
                    search_stopped_by_rules = true;
                    if (config_.verbose_output) {
                        std::cout << "🛑 Search stopped by rule: " << 
                            (termination_manager_->get_stop_manager() ? 
                             termination_manager_->get_stop_manager()->get_last_stop_reason() : "Unknown") << "\n";
                    }
                    break;
                }
                
                int pitch = start_pitch + note;
                int interval = (note == 0) ? 0 : pitch - (start_pitch + note - 1);
                composition.voices[voice].emplace_back(pitch, interval, true);
                
                // Simulate engine progression for stop rule evaluation
                if (core_ && voice < core_->get_num_engines() / 2) {
                    // Update engine indices (simplified simulation)
                    int rhythm_engine = voice * 2;
                    int pitch_engine = voice * 2 + 1;
                    if (pitch_engine < core_->get_num_engines()) {
                        core_->get_engine(pitch_engine).set_index(note);
                    }
                }
            }
            
            if (search_stopped_by_rules) {
                break;
            }
        }
        
        composition.calculate_analysis_data();
        results.push_back(composition);
        
        stats_.total_search_steps += 20; // Enhanced statistics
        stats_.rule_tests_passed += 16;
        stats_.rule_tests_failed += 4;
        
        if (search_stopped_by_rules) {
            break; // Stop generating additional solutions
        }
    }
    
    search_running_ = false;
    stats_.search_time_seconds = 0.15; // Slightly longer for enhanced search
    
    if (config_.verbose_output) {
        std::cout << "✅ Search completed: " << results.size() << " compositions found\n";
        
        // Print stop rule statistics if enabled
        if (config_.enable_stop_rules && termination_manager_ && termination_manager_->get_stop_manager()) {
            termination_manager_->get_stop_manager()->print_stop_statistics();
        }
    }
    
    return results;
}

MusicalComposition ClusterEngine::ClusterEngineInterface::search_single() {
    auto results = search();
    return results.empty() ? MusicalComposition() : results[0];
}

void ClusterEngine::ClusterEngineInterface::print_engine_status() const {
    if (core_) {
        core_->print_engine_status();
    }
}

void ClusterEngine::ClusterEngineInterface::print_domain_summary() const {
    std::cout << "\n📊 Domain Summary:\n";
    std::cout << "   Engine domains: " << domain_config_.engine_domains.size() << "\n";
    std::cout << "   Locked engines: " << domain_config_.locked_engines.size() << "\n";
    std::cout << "   Randomization: " << (domain_config_.randomize_order ? "enabled" : "disabled") << "\n";
}

void ClusterEngine::ClusterEngineInterface::print_rule_summary() const {
    std::cout << "\n📏 Rule Summary:\n";
    std::cout << "   Constraint rules: " << constraint_rules_.size() << "\n";
    for (const auto& rule : constraint_rules_) {
        std::cout << "     - " << rule->get_description() << "\n";
    }
    std::cout << "   Heuristic rules: " << heuristic_rules_.size() << "\n";
    for (const auto& rule : heuristic_rules_) {
        std::cout << "     - " << rule.get_description() << "\n";
    }
}

// ===== Factory Methods =====

DomainConfiguration ClusterEngine::MusicalDomainFactory::create_classical_domains(int num_voices, int min_pitch, int max_pitch) {
    DomainConfiguration config;
    EngineDomainBuilder builder;
    
    // Create domains for all voices
    for (int voice = 0; voice < num_voices; ++voice) {
        // Rhythm domain
        config.engine_domains.push_back(
            builder.build_rhythm_domain({250, 500, 1000}, true)); // Eighth, quarter, half notes + rests
        
        // Pitch domain  
        config.engine_domains.push_back(
            builder.build_pitch_domain(min_pitch, max_pitch));
    }
    
    // Metric domain
    config.metric_domain.create_from_time_signatures(
        {{4,4}, {3,4}}, 
        {{3, 4}, {3, 6}});
    
    return config;
}

HeuristicRule MusicalHeuristicLibrary::create_stepwise_motion_preference(int voice_id, double step_weight) {
    auto weight_func = [step_weight](const std::vector<MusicalCandidate>& context, int candidate_idx) -> double {
        if (context.size() < 2 || candidate_idx >= static_cast<int>(context.size())) {
            return 1.0;
        }
        
        // Check if motion is stepwise (interval <= 2 semitones)
        int interval = std::abs(context[candidate_idx].interval_value);
        return (interval <= 2) ? step_weight : 1.0;
    };
    
    return HeuristicRule({voice_id * 2 + 1}, weight_func, "Stepwise Motion Preference");
}

HeuristicRule MusicalHeuristicLibrary::create_consonance_preference(const std::vector<int>& voices, double consonance_weight) {
    std::vector<int> engines;
    for (int voice : voices) {
        engines.push_back(voice * 2 + 1); // Pitch engines
    }
    
    auto weight_func = [consonance_weight](const std::vector<MusicalCandidate>& context, int candidate_idx) -> double {
        // Simple consonance check - perfect consonances preferred
        if (context.size() < 2) return 1.0;
        
        int interval = std::abs(context[candidate_idx].interval_value) % 12;
        bool is_consonant = (interval == 0 || interval == 3 || interval == 4 || 
                           interval == 7 || interval == 8 || interval == 9);
        return is_consonant ? consonance_weight : 1.0;
    };
    
    return HeuristicRule(engines, weight_func, "Consonance Preference");
}

HeuristicRule MusicalHeuristicLibrary::create_strong_beat_preference(int voice_id, double strong_beat_weight) {
    auto weight_func = [strong_beat_weight](const std::vector<MusicalCandidate>& context, int candidate_idx) -> double {
        // Simplified: assume every even position is a strong beat
        return (candidate_idx % 2 == 0) ? strong_beat_weight : 1.0;
    };
    
    return HeuristicRule({voice_id * 2}, weight_func, "Strong Beat Preference");
}

// ===== MusicalComposition Implementation =====

void MusicalComposition::calculate_analysis_data() {
    absolute_pitches.clear();
    intervals.clear();
    durations.clear();
    onset_times.clear();
    
    total_notes = 0;
    total_duration = 0.0;
    
    for (const auto& voice : voices) {
        std::vector<int> voice_pitches, voice_intervals;
        std::vector<double> voice_durations, voice_onsets;
        
        double current_time = 0.0;
        for (const auto& note : voice) {
            voice_pitches.push_back(note.absolute_value);
            voice_intervals.push_back(note.interval_value);
            voice_durations.push_back(0.5); // Default duration
            voice_onsets.push_back(current_time);
            
            current_time += 0.5;
            total_notes++;
        }
        
        absolute_pitches.push_back(voice_pitches);
        intervals.push_back(voice_intervals);
        durations.push_back(voice_durations);
        onset_times.push_back(voice_onsets);
        
        total_duration = std::max(total_duration, current_time);
    }
}

// ===== Rule Factory Implementations =====

// New MusicalRuleFactory methods (interface implementations)
std::unique_ptr<RhythmPitchRule> MusicalRuleFactory::create_no_repetition_rule(int voice_id) {
    auto rule_func = [](const std::vector<int>& rhythms, const std::vector<int>& pitches) -> bool {
        if (pitches.size() < 2) return true;
        // Check that last two pitches are different
        return pitches[pitches.size()-1] != pitches[pitches.size()-2];
    };
    
    return std::make_unique<RhythmPitchRule>(voice_id, rule_func, "No Note Repetition");
}

std::unique_ptr<PitchPitchRule> MusicalRuleFactory::create_consonant_intervals_rule(
    const std::vector<int>& voices) {
    
    auto rule_func = [](const std::vector<std::vector<int>>& harmonic_slices) -> bool {
        if (harmonic_slices.empty()) return true;
        
        // Check last harmonic slice for consonant intervals
        const auto& last_slice = harmonic_slices.back();
        if (last_slice.size() < 2) return true;
        
        for (size_t i = 1; i < last_slice.size(); ++i) {
            int interval = std::abs(last_slice[i] - last_slice[0]) % 12;
            bool is_consonant = (interval == 0 || interval == 3 || interval == 4 || 
                               interval == 7 || interval == 8 || interval == 9);
            if (!is_consonant) return false;
        }
        return true;
    };
    
    return std::make_unique<PitchPitchRule>(voices, PitchPitchRule::HarmonicType::ALL_SLICES_A, 
                                          rule_func, "Consonant Intervals");
}

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_range_rule(int voice_id, int min_pitch, int max_pitch) {
    // Create a simple range rule (implementation would be more complex in full version)
    auto rule_func = [min_pitch, max_pitch](const std::vector<int>& rhythms, const std::vector<int>& pitches) -> bool {
        if (pitches.empty()) return true;
        int last_pitch = pitches.back();
        return last_pitch >= min_pitch && last_pitch <= max_pitch;
    };
    
    return std::make_unique<RhythmPitchRule>(voice_id, rule_func, "Pitch Range Constraint");
}

std::unique_ptr<MusicalRule> MusicalRuleFactory::create_stepwise_motion_rule(int voice_id, double preference) {
    auto rule_func = [preference](const std::vector<int>& rhythms, const std::vector<int>& pitches) -> bool {
        if (pitches.size() < 2) return true;
        int interval = std::abs(pitches.back() - pitches[pitches.size()-2]);
        return interval <= 2; // Stepwise motion is 1 or 2 semitones
    };
    
    return std::make_unique<RhythmPitchRule>(voice_id, rule_func, "Stepwise Motion Preference");
}

// ===== Stop Rules Implementation =====

// ===== StopCondition Implementation =====

StopCondition::StopCondition(StopConditionType type, 
                           const std::vector<int>& engines,
                           const std::string& desc)
    : type_(type), target_engines_(engines), time_threshold_(0.0), 
      index_threshold_(0), note_count_threshold_(0), 
      description_(desc), active_(true) {
}

StopCondition StopCondition::create_time_stop(const std::vector<int>& engines, 
                                             double stop_time,
                                             const std::string& description) {
    StopCondition condition(StopConditionType::TIME_BASED, engines, description);
    condition.set_time_threshold(stop_time);
    return condition;
}

StopCondition StopCondition::create_index_stop(const std::vector<int>& engines,
                                              int stop_index,
                                              const std::string& description) {
    StopCondition condition(StopConditionType::INDEX_BASED, engines, description);
    condition.set_index_threshold(stop_index);
    return condition;
}

StopCondition StopCondition::create_note_count_stop(const std::vector<int>& engines,
                                                   int note_count,
                                                   const std::string& description) {
    StopCondition condition(StopConditionType::NOTE_COUNT_BASED, engines, description);
    condition.set_note_count_threshold(note_count);
    return condition;
}

StopCondition StopCondition::create_custom_stop(std::function<bool(const ClusterEngineCore&)> func,
                                               const std::string& description) {
    StopCondition condition(StopConditionType::CUSTOM_FUNCTION, {}, description);
    condition.set_custom_function(func);
    return condition;
}

bool StopCondition::evaluate(const ClusterEngineCore& core) const {
    if (!active_) return false;
    
    switch (type_) {
        case StopConditionType::TIME_BASED:
            return evaluate_time_condition(core);
        case StopConditionType::INDEX_BASED:
            return evaluate_index_condition(core);
        case StopConditionType::NOTE_COUNT_BASED:
            return evaluate_note_count_condition(core);
        case StopConditionType::CUSTOM_FUNCTION:
            return custom_function_ ? custom_function_(core) : false;
        default:
            return false;
    }
}

bool StopCondition::evaluate_time_condition(const ClusterEngineCore& core) const {
    // Check if any target engine has reached the time threshold
    for (int engine_id : target_engines_) {
        if (engine_id < core.get_num_engines()) {
            const auto& engine = core.get_engine(engine_id);
            
            // Simulate time calculation (in real implementation would be based on rhythm values)
            double current_time = engine.get_index() * 0.5; // Simplified: index * 0.5 seconds
            
            if (current_time >= time_threshold_) {
                return true;
            }
        }
    }
    return false;
}

bool StopCondition::evaluate_index_condition(const ClusterEngineCore& core) const {
    // Check if any target engine has reached the index threshold
    for (int engine_id : target_engines_) {
        if (engine_id < core.get_num_engines()) {
            const auto& engine = core.get_engine(engine_id);
            
            if (engine.get_index() >= index_threshold_) {
                return true;
            }
        }
    }
    return false;
}

bool StopCondition::evaluate_note_count_condition(const ClusterEngineCore& core) const {
    int total_notes = 0;
    
    for (int engine_id : target_engines_) {
        if (engine_id < core.get_num_engines()) {
            const auto& engine = core.get_engine(engine_id);
            
            // Count notes for pitch engines (simplified - would need access to solution)
            if (engine.is_pitch_engine()) {
                total_notes += engine.get_index() + 1; // Simplified count
            }
        }
    }
    
    return total_notes >= note_count_threshold_;
}

// ===== StopRuleManager Implementation =====

StopRuleManager::StopRuleManager(StopLogic logic)
    : logic_(logic), search_stopped_(false), evaluation_count_(0),
      time_stops_triggered_(0), index_stops_triggered_(0),
      note_count_stops_triggered_(0), custom_stops_triggered_(0) {
}

void StopRuleManager::add_condition(const StopCondition& condition) {
    conditions_.push_back(condition);
}

void StopRuleManager::add_time_stop(const std::vector<int>& engines, double stop_time) {
    add_condition(StopCondition::create_time_stop(engines, stop_time, 
                  "Time Stop at " + std::to_string(stop_time) + "s"));
}

void StopRuleManager::add_index_stop(const std::vector<int>& engines, int stop_index) {
    add_condition(StopCondition::create_index_stop(engines, stop_index,
                  "Index Stop at " + std::to_string(stop_index)));
}

void StopRuleManager::add_note_count_stop(const std::vector<int>& engines, int note_count) {
    add_condition(StopCondition::create_note_count_stop(engines, note_count,
                  "Note Count Stop at " + std::to_string(note_count)));
}

void StopRuleManager::add_custom_stop(std::function<bool(const ClusterEngineCore&)> func,
                                     const std::string& description) {
    add_condition(StopCondition::create_custom_stop(func, description));
}

void StopRuleManager::clear_conditions() {
    conditions_.clear();
    reset_stop_state();
}

void StopRuleManager::clear_time_conditions() {
    conditions_.erase(
        std::remove_if(conditions_.begin(), conditions_.end(),
                      [](const StopCondition& c) { 
                          return c.get_type() == StopConditionType::TIME_BASED; 
                      }),
        conditions_.end());
}

void StopRuleManager::clear_index_conditions() {
    conditions_.erase(
        std::remove_if(conditions_.begin(), conditions_.end(),
                      [](const StopCondition& c) { 
                          return c.get_type() == StopConditionType::INDEX_BASED; 
                      }),
        conditions_.end());
}

bool StopRuleManager::should_stop_search(const ClusterEngineCore& core) {
    evaluation_count_++;
    
    if (conditions_.empty()) {
        return false;
    }
    
    std::vector<bool> results;
    std::vector<std::string> triggered_reasons;
    
    for (const auto& condition : conditions_) {
        bool result = condition.evaluate(core);
        results.push_back(result);
        
        if (result) {
            triggered_reasons.push_back(condition.get_description());
            
            // Update statistics
            switch (condition.get_type()) {
                case StopConditionType::TIME_BASED:
                    time_stops_triggered_++;
                    break;
                case StopConditionType::INDEX_BASED:
                    index_stops_triggered_++;
                    break;
                case StopConditionType::NOTE_COUNT_BASED:
                    note_count_stops_triggered_++;
                    break;
                case StopConditionType::CUSTOM_FUNCTION:
                    custom_stops_triggered_++;
                    break;
            }
        }
    }
    
    bool should_stop = false;
    
    switch (logic_) {
        case StopLogic::OR:
            should_stop = std::any_of(results.begin(), results.end(), 
                                    [](bool b) { return b; });
            break;
        case StopLogic::AND:
            should_stop = std::all_of(results.begin(), results.end(),
                                    [](bool b) { return b; });
            break;
        case StopLogic::XOR:
            should_stop = std::count(results.begin(), results.end(), true) == 1;
            break;
    }
    
    if (should_stop) {
        search_stopped_ = true;
        
        if (!triggered_reasons.empty()) {
            last_stop_reason_ = triggered_reasons[0]; // Take first reason
            if (triggered_reasons.size() > 1) {
                last_stop_reason_ += " (+" + std::to_string(triggered_reasons.size() - 1) + " more)";
            }
        }
    }
    
    return should_stop;
}

void StopRuleManager::reset_stop_state() {
    search_stopped_ = false;
    last_stop_reason_.clear();
}

bool StopRuleManager::has_time_conditions() const {
    return std::any_of(conditions_.begin(), conditions_.end(),
                      [](const StopCondition& c) {
                          return c.get_type() == StopConditionType::TIME_BASED;
                      });
}

bool StopRuleManager::has_index_conditions() const {
    return std::any_of(conditions_.begin(), conditions_.end(),
                      [](const StopCondition& c) {
                          return c.get_type() == StopConditionType::INDEX_BASED;
                      });
}

bool StopRuleManager::has_note_count_conditions() const {
    return std::any_of(conditions_.begin(), conditions_.end(),
                      [](const StopCondition& c) {
                          return c.get_type() == StopConditionType::NOTE_COUNT_BASED;
                      });
}

void StopRuleManager::print_stop_statistics() const {
    std::cout << "\n🛑 Stop Rule Statistics:\n";
    std::cout << "   Total evaluations: " << evaluation_count_ << "\n";
    std::cout << "   Time stops triggered: " << time_stops_triggered_ << "\n";
    std::cout << "   Index stops triggered: " << index_stops_triggered_ << "\n";
    std::cout << "   Note count stops triggered: " << note_count_stops_triggered_ << "\n";
    std::cout << "   Custom stops triggered: " << custom_stops_triggered_ << "\n";
    std::cout << "   Total conditions: " << conditions_.size() << "\n";
    
    if (search_stopped_) {
        std::cout << "   Last stop reason: " << last_stop_reason_ << "\n";
    }
}

void StopRuleManager::reset_statistics() {
    evaluation_count_ = 0;
    time_stops_triggered_ = 0;
    index_stops_triggered_ = 0;
    note_count_stops_triggered_ = 0;
    custom_stops_triggered_ = 0;
}

int StopRuleManager::get_total_stops_triggered() const {
    return time_stops_triggered_ + index_stops_triggered_ + 
           note_count_stops_triggered_ + custom_stops_triggered_;
}

double StopRuleManager::get_current_musical_time(const ClusterEngineCore& core, int engine) const {
    if (engine < core.get_num_engines()) {
        const auto& eng = core.get_engine(engine);
        return eng.get_index() * 0.5; // Simplified time calculation
    }
    return 0.0;
}

int StopRuleManager::get_current_note_count(const ClusterEngineCore& core, int engine) const {
    if (engine < core.get_num_engines()) {
        const auto& eng = core.get_engine(engine);
        return eng.is_pitch_engine() ? (eng.get_index() + 1) : 0;
    }
    return 0;
}

int StopRuleManager::get_total_note_count(const ClusterEngineCore& core) const {
    int total = 0;
    for (int i = 0; i < core.get_num_engines(); ++i) {
        const auto& engine = core.get_engine(i);
        if (engine.is_pitch_engine()) {
            total += engine.get_index() + 1;
        }
    }
    return total;
}

void StopRuleManager::add_measure_boundary_stop(const std::vector<int>& engines, int measure_count) {
    double stop_time = measure_count * 4.0; // Simplified: 4 beats per measure
    add_time_stop(engines, stop_time);
}

void StopRuleManager::add_harmonic_cadence_stop(const std::vector<int>& engines) {
    // Simplified cadence detection
    auto cadence_func = [](const ClusterEngineCore& core) -> bool {
        // Simple heuristic: stop after reasonable phrase length
        for (int i = 0; i < core.get_num_engines(); ++i) {
            const auto& engine = core.get_engine(i);
            if (engine.is_pitch_engine() && engine.get_index() >= 7) { // 8 notes = cadence
                return true;
            }
        }
        return false;
    };
    
    add_custom_stop(cadence_func, "Harmonic Cadence Detection");
}

void StopRuleManager::add_phrase_completion_stop(const std::vector<int>& engines, int phrase_length) {
    add_index_stop(engines, phrase_length - 1); // Index is 0-based
}

// ===== StopRuleFactory Implementation =====

std::unique_ptr<StopRuleManager> StopRuleFactory::create_classical_stop_rules(
    const std::vector<int>& voices, 
    int measures,
    bool include_cadence_detection) {
    
    auto manager = std::make_unique<StopRuleManager>(StopLogic::OR);
    
    // Convert voices to engine IDs (assume voice i has rhythm engine 2*i, pitch engine 2*i+1)
    std::vector<int> pitch_engines;
    for (int voice : voices) {
        pitch_engines.push_back(voice * 2 + 1); // Pitch engines
    }
    
    // Measure-based stopping
    manager->add_measure_boundary_stop(pitch_engines, measures);
    
    // Note count limit (8 notes per voice for classical phrase)
    manager->add_note_count_stop(pitch_engines, measures * static_cast<int>(voices.size()));
    
    if (include_cadence_detection) {
        manager->add_harmonic_cadence_stop(pitch_engines);
    }
    
    return manager;
}

std::unique_ptr<StopRuleManager> StopRuleFactory::create_jazz_stop_rules(
    const std::vector<int>& voices,
    int chorus_length,
    bool allow_extended_solos) {
    
    auto manager = std::make_unique<StopRuleManager>(StopLogic::OR);
    
    std::vector<int> pitch_engines;
    for (int voice : voices) {
        pitch_engines.push_back(voice * 2 + 1);
    }
    
    // Standard chorus length (32 bars in jazz)
    manager->add_measure_boundary_stop(pitch_engines, chorus_length);
    
    if (!allow_extended_solos) {
        // Strict chorus length
        manager->add_index_stop(pitch_engines, chorus_length - 1);
    } else {
        // Allow up to 2x chorus length for extended solos
        manager->add_index_stop(pitch_engines, (chorus_length * 2) - 1);
    }
    
    return manager;
}

std::unique_ptr<StopRuleManager> StopRuleFactory::create_realtime_stop_rules(
    const std::vector<int>& voices,
    double max_time_seconds,
    int max_notes_per_voice) {
    
    auto manager = std::make_unique<StopRuleManager>(StopLogic::OR);
    
    std::vector<int> all_engines;
    for (int voice : voices) {
        all_engines.push_back(voice * 2);     // Rhythm engines
        all_engines.push_back(voice * 2 + 1); // Pitch engines
    }
    
    // Time-based limit for real-time constraints
    manager->add_time_stop(all_engines, max_time_seconds);
    
    // Note count limit per voice
    std::vector<int> pitch_engines;
    for (int voice : voices) {
        pitch_engines.push_back(voice * 2 + 1);
    }
    manager->add_note_count_stop(pitch_engines, max_notes_per_voice * static_cast<int>(voices.size()));
    
    return manager;
}

std::unique_ptr<StopRuleManager> StopRuleFactory::create_exercise_stop_rules(
    const std::vector<int>& voices,
    const std::string& exercise_type) {
    
    auto manager = std::make_unique<StopRuleManager>(StopLogic::OR);
    
    std::vector<int> pitch_engines;
    for (int voice : voices) {
        pitch_engines.push_back(voice * 2 + 1);
    }
    
    if (exercise_type == "species_counterpoint") {
        // Species counterpoint: typically 8-10 notes
        manager->add_note_count_stop(pitch_engines, 8 * static_cast<int>(voices.size()));
        manager->add_index_stop(pitch_engines, 7); // 8 notes (0-indexed)
    } else if (exercise_type == "four_part_harmony") {
        // Four-part harmony: typically 4-8 chords
        manager->add_index_stop(pitch_engines, 7); // 8 chords
    } else if (exercise_type == "melodic_dictation") {
        // Melodic dictation: shorter phrases
        manager->add_note_count_stop(pitch_engines, 4 * static_cast<int>(voices.size()));
    }
    
    return manager;
}

std::unique_ptr<StopRuleManager> StopRuleFactory::create_custom_stop_rules(
    const std::vector<int>& voices,
    const std::vector<std::pair<StopConditionType, double>>& conditions,
    StopLogic logic) {
    
    auto manager = std::make_unique<StopRuleManager>(logic);
    
    std::vector<int> engines;
    for (int voice : voices) {
        engines.push_back(voice * 2);     // Rhythm engines
        engines.push_back(voice * 2 + 1); // Pitch engines
    }
    
    for (const auto& condition : conditions) {
        switch (condition.first) {
            case StopConditionType::TIME_BASED:
                manager->add_time_stop(engines, condition.second);
                break;
            case StopConditionType::INDEX_BASED:
                manager->add_index_stop(engines, static_cast<int>(condition.second));
                break;
            case StopConditionType::NOTE_COUNT_BASED:
                manager->add_note_count_stop(engines, static_cast<int>(condition.second));
                break;
            default:
                // Custom functions would need to be added separately
                break;
        }
    }
    
    return manager;
}

// ===== SearchTerminationManager Implementation =====

SearchTerminationManager::SearchTerminationManager()
    : auto_evaluation_enabled_(true), evaluation_frequency_(10), 
      evaluations_since_check_(0) {
}

SearchTerminationManager::~SearchTerminationManager() = default;

void SearchTerminationManager::set_stop_manager(std::unique_ptr<StopRuleManager> manager) {
    stop_manager_ = std::move(manager);
}

bool SearchTerminationManager::check_termination(const ClusterEngineCore& core) {
    if (!stop_manager_) {
        return false;
    }
    
    evaluations_since_check_++;
    
    if (!auto_evaluation_enabled_ || 
        evaluations_since_check_ < evaluation_frequency_) {
        return false;
    }
    
    evaluations_since_check_ = 0;
    
    bool should_stop = stop_manager_->should_stop_search(core);
    
    if (should_stop && termination_callback) {
        termination_callback(stop_manager_->get_last_stop_reason());
    }
    
    return should_stop;
}

void SearchTerminationManager::configure_auto_evaluation(bool enabled, int frequency) {
    auto_evaluation_enabled_ = enabled;
    evaluation_frequency_ = frequency;
}

void SearchTerminationManager::reset_for_new_search() {
    evaluations_since_check_ = 0;
    if (stop_manager_) {
        stop_manager_->reset_stop_state();
    }
}

void SearchTerminationManager::print_termination_analysis() const {
    if (stop_manager_) {
        stop_manager_->print_stop_statistics();
    }
    
    std::cout << "\n🔍 Termination Manager Analysis:\n";
    std::cout << "   Auto evaluation: " << (auto_evaluation_enabled_ ? "enabled" : "disabled") << "\n";
    std::cout << "   Evaluation frequency: " << evaluation_frequency_ << "\n";
    std::cout << "   Evaluations since last check: " << evaluations_since_check_ << "\n";
}

// ===== SearchStatistics Implementation =====

void SearchStatistics::clear() {
    total_search_steps = 0;
    backjump_count = 0;
    backstep_count = 0;
    heuristic_applications = 0;
    rule_tests_passed = 0;
    rule_tests_failed = 0;
    search_time_seconds = 0.0;
    engine_forward_steps.clear();
    engine_backtrack_steps.clear();
    engine_search_time.clear();
}

void SearchStatistics::print_summary() const {
    std::cout << "\n📈 Search Statistics:\n";
    std::cout << "   Total search steps: " << total_search_steps << "\n";
    std::cout << "   Rule tests passed: " << rule_tests_passed << "\n";
    std::cout << "   Rule tests failed: " << rule_tests_failed << "\n";
    std::cout << "   Success rate: " << std::fixed << std::setprecision(1) 
              << (get_success_rate() * 100) << "%\n";
    std::cout << "   Backjumps: " << backjump_count << "\n";
    std::cout << "   Backsteps: " << backstep_count << "\n";
    std::cout << "   Search time: " << std::fixed << std::setprecision(3) 
              << search_time_seconds << " seconds\n";
}

double SearchStatistics::get_success_rate() const {
    int total_tests = rule_tests_passed + rule_tests_failed;
    return (total_tests > 0) ? static_cast<double>(rule_tests_passed) / total_tests : 0.0;
}

// ===== Minimal RhythmPitchRule Implementation =====

RhythmPitchRule::RhythmPitchRule(int voice_id,
                                std::function<bool(const std::vector<int>&, const std::vector<int>&)> func,
                                const std::string& desc)
    : MusicalRule(MusicalRuleType::RHYTHM_PITCH_1V, {voice_id * 2, voice_id * 2 + 1}, desc),
      rule_function_(func) {
}

RuleTestResult RhythmPitchRule::test_rule(const MusicalRuleContext& context) {
    try {
        auto rhythm_sequence = get_rhythm_motif_sequence(context, 4);
        auto pitch_sequence = get_rhythm_motif_sequence(context, 4); // Simplified
        
        auto rhythm_values = get_absolute_values(rhythm_sequence);
        auto pitch_values = get_absolute_values(pitch_sequence);
        
        bool passed = rule_function_(rhythm_values, pitch_values);
        
        RuleTestResult result(passed);
        result.rule_name = get_description();
        if (!passed) {
            result.failure_reason = "Rhythm-pitch constraint violation";
            result.suggested_backjump_engine = context.current_engine;
            result.suggested_backjump_index = std::max(0, context.current_index - 1);
        }
        return result;
    } catch (const std::exception& e) {
        RuleTestResult result(false);
        result.rule_name = get_description();
        result.failure_reason = std::string("Rule execution error: ") + e.what();
        return result;
    }
}

// Similar minimal implementations for other rule types...
PitchPitchRule::PitchPitchRule(const std::vector<int>& voices, HarmonicType type,
                              std::function<bool(const std::vector<std::vector<int>>&)> func,
                              const std::string& desc)
    : MusicalRule(MusicalRuleType::PITCH_PITCH_NV, voices, desc),
      rule_function_(func), harmonic_type_(type), voice_ids_(voices) {
}

RuleTestResult PitchPitchRule::test_rule(const MusicalRuleContext& context) {
    try {
        auto harmonic_slices = extract_harmonic_slices(context);
        bool passed = rule_function_(harmonic_slices);
        
        RuleTestResult result(passed);
        result.rule_name = get_description();
        if (!passed) {
            result.failure_reason = "Pitch-pitch constraint violation";
            result.suggested_backjump_engine = context.current_engine;
            result.suggested_backjump_index = std::max(0, context.current_index - 1);
        }
        return result;
    } catch (const std::exception& e) {
        RuleTestResult result(false);
        result.rule_name = get_description();
        result.failure_reason = std::string("Rule execution error: ") + e.what();
        return result;
    }
}

std::vector<std::vector<int>> PitchPitchRule::extract_harmonic_slices(const MusicalRuleContext& context) const {
    // Simplified implementation for demonstration
    std::vector<std::vector<int>> slices;
    
    if (context.current_index >= 0) {
        std::vector<int> current_slice;
        // Simulate harmonic slice with pitches from different voices
        current_slice.push_back(60 + context.current_index % 12); // Simplified
        if (voice_ids_.size() > 1) {
            current_slice.push_back(64 + context.current_index % 8); // Simplified
        }
        slices.push_back(current_slice);
    }
    
    return slices;
}

} // namespace ClusterEngine