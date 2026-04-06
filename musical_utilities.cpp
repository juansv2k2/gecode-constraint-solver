#include "musical_utilities.hh"
#include "cluster_engine_core.hh"
#include <numeric>
#include <set>
#include <cassert>

using namespace ClusterEngine;

// ===== CORE SOLUTION ACCESS (Direct ClusterEngine Translation) =====

MusicalCandidate MusicalUtilities::get_current_rhythm_value(const ClusterEngineCore& core, int engine_id) {
    if (engine_id < 0 || engine_id >= core.get_num_engines()) {
        return MusicalCandidate(-1, 0); // Invalid engine, return rest equivalent
    }
    
    // Translation of cluster's: (caar (aref (aref vsolution engine) (aref vindex engine)))
    const auto& engine = core.get_engine(engine_id);
    
    if (!engine.is_rhythm_engine()) {
        return MusicalCandidate(-1, 0); // Not a rhythm engine
    }
    
    if (engine.has_solution()) {
        return engine.get_current_candidate();
    }
    
    return MusicalCandidate(0, 0); // Default rhythm value
}

MusicalCandidate MusicalUtilities::get_current_pitch_value(const ClusterEngineCore& core, int engine_id) {
    if (engine_id < 0 || engine_id >= core.get_num_engines()) {
        return MusicalCandidate(-1, 0); // Invalid engine, return rest equivalent
    }
    
    // Translation of cluster's get-pitch-motif-at-current-index
    const auto& engine = core.get_engine(engine_id);
    
    if (!engine.is_pitch_engine()) {
        return MusicalCandidate(60, 60); // Default C4 if not a pitch engine
    }
    
    if (engine.has_solution()) {
        return engine.get_current_candidate();
    }
    
    return MusicalCandidate(60, 60); // Default MIDI note C4
}

MusicalCandidate MusicalUtilities::get_candidate_rhythm_value(const ClusterEngineCore& core, int engine_id, int candidate_index) {
    // Translation of cluster's get-last-cell-at-current-index-nth for heuristic rules
    if (engine_id < 0 || engine_id >= core.get_num_engines()) {
        return MusicalCandidate(-1, 0);
    }
    
    const auto& engine = core.get_engine(engine_id);
    const auto& domain = engine.get_domain();
    const auto& candidates = domain.get_candidates();
    
    if (candidate_index >= 0 && candidate_index < static_cast<int>(candidates.size())) {
        return candidates[candidate_index];
    }
    
    return MusicalCandidate(0, 0);
}

// ===== TEMPORAL COORDINATION (Time-based Functions) =====

double MusicalUtilities::get_current_starttime(const ClusterEngineCore& core, int engine_id) {
    // Translation of cluster's get-current-index-starttime
    const auto& engine = core.get_engine(engine_id);
    
    if (!engine.is_rhythm_engine() || engine.get_index() < 0) {
        return 1.0; // Minimum time as in cluster-engine
    }
    
    // Calculate start time based on accumulated rhythm values
    double start_time = 1.0; // Starting time point
    const auto& solution = engine.get_solution();
    
    for (int i = 0; i < engine.get_index(); ++i) {
        if (i < static_cast<int>(solution.size())) {
            int duration = convert_cluster_rhythm_to_duration(solution[i]);
            if (duration > 0) {
                start_time += duration;
            }
        }
    }
    
    return start_time;
}

double MusicalUtilities::get_current_endtime(const ClusterEngineCore& core, int engine_id) {
    // Translation of cluster's get-current-index-endtime
    double start_time = get_current_starttime(core, engine_id);
    MusicalCandidate rhythm_value = get_current_rhythm_value(core, engine_id);
    
    int duration = convert_cluster_rhythm_to_duration(rhythm_value);
    if (duration > 0) {
        return start_time + duration;
    }
    return start_time + 1.0; // Default duration
}

double MusicalUtilities::get_previous_endtime(const ClusterEngineCore& core, int engine_id) {
    // Translation of cluster's get-previous-index-endtime
    const auto& engine = core.get_engine(engine_id);
    
    if (engine.get_index() <= 0) {
        return 1.0;
    }
    
    // Calculate end time for previous index
    double prev_start = 1.0;
    const auto& solution = engine.get_solution();
    
    for (int i = 0; i < engine.get_index() - 1; ++i) {
        if (i < static_cast<int>(solution.size())) {
            prev_start += convert_cluster_rhythm_to_duration(solution[i]);
        }
    }
    
    if (engine.get_index() - 1 < static_cast<int>(solution.size())) {
        prev_start += convert_cluster_rhythm_to_duration(solution[engine.get_index() - 1]);
    }
    
    return prev_start;
}

// ===== SEQUENCE EXTRACTION (Motif and Pattern Functions) =====

std::vector<MusicalCandidate> MusicalUtilities::get_rhythm_sequence(const ClusterEngineCore& core, int engine_id, 
                                                      int start_index, int end_index) {
    // Translation of cluster's get-rhythm-motifs-from-index-to-current-index
    std::vector<MusicalCandidate> sequence;
    const auto& engine = core.get_engine(engine_id);
    const auto& solution = engine.get_solution();
    
    for (int idx = start_index; idx <= end_index && idx < static_cast<int>(solution.size()); ++idx) {
        sequence.push_back(solution[idx]);
    }
    
    return sequence;
}

std::vector<MusicalCandidate> MusicalUtilities::get_pitch_sequence(const ClusterEngineCore& core, int engine_id,
                                                     int start_index, int end_index) {
    // Translation equivalent for pitch motifs
    std::vector<MusicalCandidate> sequence;
    const auto& engine = core.get_engine(engine_id);
    const auto& solution = engine.get_solution();
    
    for (int idx = start_index; idx <= end_index && idx < static_cast<int>(solution.size()); ++idx) {
        sequence.push_back(solution[idx]);
    }
    
    return sequence;
}

std::vector<int> MusicalUtilities::get_interval_motif(const ClusterEngineCore& core, int engine_id,
                                                     int start_index, int end_index) {
    // Translation of cluster's get-m-motif functionality
    std::vector<MusicalCandidate> pitches = get_pitch_sequence(core, engine_id, start_index, end_index);
    return to_melodic_intervals(pitches);
}

// ===== COUNTING AND ANALYSIS (Musical Intelligence) =====

int MusicalUtilities::count_notes_at_current_index(const ClusterEngineCore& core, int engine_id) {
    // Translation of cluster's count-notes-last-cell-at-current-index
    MusicalCandidate rhythm_value = get_current_rhythm_value(core, engine_id);
    
    // In cluster-engine, negative values are rests
    if (is_rest(rhythm_value.absolute_value)) {
        return 0;
    }
    
    // For now, assume each position has one note
    // This can be extended for chord support (lists of pitches)
    return 1;
}

int MusicalUtilities::count_main_notes_at_current_index(const ClusterEngineCore& core, int engine_id) {
    // Translation of cluster's count-notes-exclude-gracenotes-last-cell-at-current-index
    MusicalCandidate rhythm_value = get_current_rhythm_value(core, engine_id);
    
    if (is_rest(rhythm_value.absolute_value) || is_grace_note(rhythm_value.absolute_value)) {
        return 0;
    }
    
    return 1; // Main note count
}

int MusicalUtilities::count_events_at_current_index(const ClusterEngineCore& core, int engine_id) {
    // Translation of cluster's count-events-last-cell-at-current-index
    // Counts both notes and rests
    const auto& engine = core.get_engine(engine_id);
    if (engine.has_solution()) {
        return 1; // Each current position is one event in our model
    }
    return 0;
}

// ===== TIMEPOINT COORDINATION (Cross-voice Synchronization) =====

MusicalCandidate MusicalUtilities::get_value_at_timepoint(const ClusterEngineCore& core, int engine_id, double timepoint) {
    // Translation of cluster's get-cell-at-timepoint
    // Find index that contains the given timepoint
    
    const auto& engine = core.get_engine(engine_id);
    const auto& solution = engine.get_solution();
    
    double current_time = 1.0;
    
    for (int idx = 0; idx < static_cast<int>(solution.size()); ++idx) {
        double start_time = current_time;
        int duration = convert_cluster_rhythm_to_duration(solution[idx]);
        double end_time = start_time + duration;
        
        if (timepoint >= start_time && timepoint < end_time) {
            return solution[idx];
        }
        
        current_time = end_time;
    }
    
    return MusicalCandidate(-1, 0); // No value found at timepoint
}

std::vector<MusicalCandidate> MusicalUtilities::get_values_at_timepoints(const ClusterEngineCore& core, int engine_id, 
                                                           const std::vector<double>& timepoints) {
    // Translation of cluster's get-cells-at-timepoints
    std::vector<MusicalCandidate> values;
    
    for (double timepoint : timepoints) {
        values.push_back(get_value_at_timepoint(core, engine_id, timepoint));
    }
    
    return values;
}

// ===== PITCH-SPECIFIC UTILITIES (Pitch Engine Support) =====

MusicalCandidate MusicalUtilities::get_last_pitch_at_previous_index(const ClusterEngineCore& core, int engine_id) {
    // Translation of cluster's get-last-pitch-at-previous-index
    const auto& engine = core.get_engine(engine_id);
    
    if (engine.get_index() <= 0) {
        return MusicalCandidate(60, 60); // Default C4
    }
    
    const auto& solution = engine.get_solution();
    int prev_index = engine.get_index() - 1;
    
    if (prev_index < static_cast<int>(solution.size())) {
        return solution[prev_index];
    }
    
    return MusicalCandidate(60, 60);
}

int MusicalUtilities::get_total_pitch_count(const ClusterEngineCore& core, int engine_id) {
    // Translation of cluster's get-current-index-total-pitchcount
    const auto& engine = core.get_engine(engine_id);
    int total_count = 0;
    
    const auto& solution = engine.get_solution();
    for (int idx = 0; idx <= engine.get_index() && idx < static_cast<int>(solution.size()); ++idx) {
        if (!is_rest(solution[idx].absolute_value)) {
            total_count++;
        }
    }
    
    return total_count;
}

// ===== VALIDATION AND TESTING (Constraint Support) =====

bool MusicalUtilities::all_constraints_satisfied(const std::vector<bool>& values) {
    // Translation of cluster's p-test-if-all-elements-are-true
    return std::all_of(values.begin(), values.end(), [](bool val) { return val; });
}

bool MusicalUtilities::has_unique_elements(const std::vector<int>& sequence) {
    // Check uniqueness (used in cluster's musical constraint checking)
    std::set<int> unique_elements(sequence.begin(), sequence.end());
    return unique_elements.size() == sequence.size();
}

bool MusicalUtilities::is_grace_note(int value) {
    // Grace note detection (cluster-engine specific logic)
    // In cluster-engine, grace notes might have specific encoding
    // For now, use simple heuristic
    return value > 0 && value < 100; // Simplified grace note detection
}

// ===== DOMAIN AND CONVERSION UTILITIES =====

std::vector<int> MusicalUtilities::to_pitch_classes(const std::vector<MusicalCandidate>& candidates) {
    std::vector<int> pitch_classes;
    pitch_classes.reserve(candidates.size());
    
    for (const auto& candidate : candidates) {
        pitch_classes.push_back(to_pitch_class(candidate.absolute_value));
    }
    
    return pitch_classes;
}

std::vector<int> MusicalUtilities::to_melodic_intervals(const std::vector<MusicalCandidate>& pitch_candidates) {
    std::vector<int> intervals;
    
    if (pitch_candidates.size() < 2) {
        return intervals;
    }
    
    intervals.reserve(pitch_candidates.size() - 1);
    
    for (size_t i = 1; i < pitch_candidates.size(); ++i) {
        int interval = melodic_interval(pitch_candidates[i-1].absolute_value, pitch_candidates[i].absolute_value);
        intervals.push_back(interval);
    }
    
    return intervals;
}

// ===== PRIVATE HELPER FUNCTIONS =====

int MusicalUtilities::convert_cluster_rhythm_to_duration(const MusicalCandidate& rhythm_candidate) {
    // Convert cluster-engine rhythm values to musical durations
    // This mapping depends on cluster-engine's specific encoding
    
    int cluster_rhythm_value = rhythm_candidate.absolute_value;
    
    if (cluster_rhythm_value <= 0) {
        return 1; // Rest duration
    }
    
    // Simple mapping for now - this should be refined based on actual cluster encoding
    // Common duration values: whole=1, half=0.5, quarter=0.25, etc.
    switch (cluster_rhythm_value) {
        case 1: return 4; // Whole note (4 quarter notes)
        case 2: return 2; // Half note
        case 4: return 1; // Quarter note
        case 8: return 1; // Eighth note (simplified)
        default: return 1; // Default quarter note
    }
}

int MusicalUtilities::convert_cluster_pitch_to_midi(const MusicalCandidate& pitch_candidate) {
    // Convert cluster-engine pitch values to MIDI note numbers
    // In many cases, this might be direct mapping
    return pitch_candidate.absolute_value;
}

double MusicalUtilities::calculate_onset_time(const std::vector<MusicalCandidate>& rhythm_sequence, int index) {
    // Calculate onset time based on rhythm sequence up to index
    double onset = 1.0; // Starting time
    
    for (int i = 0; i < index && i < static_cast<int>(rhythm_sequence.size()); ++i) {
        onset += convert_cluster_rhythm_to_duration(rhythm_sequence[i]);
    }
    
    return onset;
}

bool MusicalUtilities::is_valid_engine_index(const ClusterEngineCore& core, int engine_id, int index) {
    if (engine_id < 0 || engine_id >= core.get_num_engines()) {
        return false;
    }
    
    const auto& engine = core.get_engine(engine_id);
    return index >= 0 && index <= engine.get_index();
}

// ===== ADVANCED MUSICAL ANALYZER IMPLEMENTATION =====

MusicalAnalysisResult AdvancedMusicalAnalyzer::analyze_engine(const ClusterEngineCore& core, int engine_id,
                                                           int start_index, int end_index) {
    MusicalAnalysisResult result;
    
    // Extract sequences
    const auto& engine = core.get_engine(engine_id);
    
    if (engine.is_pitch_engine()) {
        result.pitch_sequence = MusicalUtilities::get_pitch_sequence(core, engine_id, start_index, end_index);
        result.interval_sequence = MusicalUtilities::to_melodic_intervals(result.pitch_sequence);
    }
    
    if (engine.is_rhythm_engine()) {
        result.rhythm_sequence = MusicalUtilities::get_rhythm_sequence(core, engine_id, start_index, end_index);
    }
    
    // Calculate onset times
    result.onset_times.reserve(end_index - start_index + 1);
    for (int idx = start_index; idx <= end_index; ++idx) {
        result.onset_times.push_back(MusicalUtilities::get_current_starttime(core, engine_id));
    }
    
    // Analyze content
    result.note_count = 0;
    result.has_rests = false;
    result.has_grace_notes = false;
    
    const auto& sequence = engine.is_rhythm_engine() ? result.rhythm_sequence : result.pitch_sequence;
    
    for (const auto& candidate : sequence) {
        if (MusicalUtilities::is_rest(candidate.absolute_value)) {
            result.has_rests = true;
        } else if (MusicalUtilities::is_grace_note(candidate.absolute_value)) {
            result.has_grace_notes = true;
            result.note_count++;
        } else {
            result.note_count++;
        }
    }
    
    // Calculate total duration
    result.total_duration = 0;
    if (engine.is_rhythm_engine()) {
        for (const auto& rhythm : result.rhythm_sequence) {
            result.total_duration += MusicalUtilities::convert_cluster_rhythm_to_duration(rhythm);
        }
    }
    
    return result;
}

bool AdvancedMusicalAnalyzer::engines_coordinated(const ClusterEngineCore& core, 
                                                 int engine1_id, int engine2_id) {
    // Check if two engines are properly synchronized at current indices
    double start_time1 = MusicalUtilities::get_current_starttime(core, engine1_id);
    double start_time2 = MusicalUtilities::get_current_starttime(core, engine2_id);
    
    // Consider engines coordinated if start times are close (within small tolerance)
    const double tolerance = 0.01;
    return std::abs(start_time1 - start_time2) < tolerance;
}

double AdvancedMusicalAnalyzer::calculate_consonance(const ClusterEngineCore& core, 
                                                   int engine1_id, int engine2_id) {
    // Calculate harmonic consonance between two engines
    MusicalCandidate pitch1 = MusicalUtilities::get_current_pitch_value(core, engine1_id);
    MusicalCandidate pitch2 = MusicalUtilities::get_current_pitch_value(core, engine2_id);
    
    if (MusicalUtilities::is_rest(pitch1.absolute_value) || MusicalUtilities::is_rest(pitch2.absolute_value)) {
        return 0.0; // No consonance with rests
    }
    
    // Calculate interval and map to consonance value
    int interval = std::abs(pitch1.absolute_value - pitch2.absolute_value) % 12;
    
    // Consonance scoring based on traditional music theory
    switch (interval) {
        case 0:  return 1.0;   // Unison
        case 7:  return 0.9;   // Perfect fifth
        case 5:  return 0.9;   // Perfect fourth
        case 4:  return 0.8;   // Major third
        case 8:  return 0.8;   // Minor sixth
        case 3:  return 0.7;   // Minor third
        case 9:  return 0.7;   // Major sixth
        case 2:  return 0.4;   // Major second
        case 10: return 0.4;   // Minor seventh
        case 1:  return 0.2;   // Minor second
        case 11: return 0.2;   // Major seventh
        case 6:  return 0.1;   // Tritone
        default: return 0.0;
    }
}