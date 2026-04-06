#ifndef MUSICAL_UTILITIES_H
#define MUSICAL_UTILITIES_H

#include <vector>
#include <array>
#include <memory>
#include <algorithm>
#include <cmath>
#include "cluster_engine_core.hh"

namespace ClusterEngine {

/**
 * @class MusicalUtilities
 * @brief Translation of cluster-engine's core utility functions for ClusterEngine C++
 * 
 * This class provides the essential musical intelligence layer from cluster-engine,
 * adapted to work with ClusterEngineCore and MusicalEngine objects.
 * Based on cluster-engine-sources/09.utilities.lisp functions.
 */
class MusicalUtilities {
public:
    // ===== CORE SOLUTION ACCESS (Direct Gecode Variable Translation) =====
    
    /**
     * Translation of get-last-cell-at-current-index for rhythm values
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Rhythm engine ID (even numbers: 0, 2, 4...)
     * @return Current rhythm value at current index
     */
    static MusicalCandidate get_current_rhythm_value(const ClusterEngineCore& core, int engine_id);
    
    /**
     * Translation of get-pitch-motif-at-current-index
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Pitch engine ID (odd numbers: 1, 3, 5...)
     * @return Current pitch value at current index
     */
    static MusicalCandidate get_current_pitch_value(const ClusterEngineCore& core, int engine_id);
    
    /**
     * Translation of get-last-cell-at-current-index-nth for heuristic rules
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Engine ID
     * @param candidate_index Index in domain candidate list for heuristic evaluation
     * @return Candidate rhythm value
     */
    static MusicalCandidate get_candidate_rhythm_value(const ClusterEngineCore& core, int engine_id, int candidate_index);
    
    // ===== TEMPORAL COORDINATION (Time-based Functions) =====
    
    /**
     * Translation of get-current-index-starttime
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Engine ID (must be rhythm engine)
     * @return Start time of current musical cell
     */
    static double get_current_starttime(const ClusterEngineCore& core, int engine_id);
    
    /**
     * Translation of get-current-index-endtime  
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Engine ID (must be rhythm engine)
     * @return End time of current musical cell
     */
    static double get_current_endtime(const ClusterEngineCore& core, int engine_id);
    
    /**
     * Translation of get-previous-index-endtime
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Engine ID
     * @return End time of previous musical cell
     */
    static double get_previous_endtime(const ClusterEngineCore& core, int engine_id);
    
    // ===== SEQUENCE EXTRACTION (Motif and Pattern Functions) =====
    
    /**
     * Translation of get-rhythm-motifs-from-index-to-current-index
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Engine ID
     * @param start_index Starting position
     * @param end_index Ending position (current index)
     * @return Vector of rhythm values in range
     */
    static std::vector<MusicalCandidate> get_rhythm_sequence(const ClusterEngineCore& core, int engine_id, 
                                               int start_index, int end_index);
    
    /**
     * Translation of get-pitch-motif-from-index-to-current-index equivalent
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Engine ID
     * @param start_index Starting position
     * @param end_index Ending position (current index)
     * @return Vector of pitch values in range
     */
    static std::vector<MusicalCandidate> get_pitch_sequence(const ClusterEngineCore& core, int engine_id,
                                              int start_index, int end_index);
    
    /**
     * Translation of get-m-motif (interval motif extraction)
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Engine ID
     * @param start_index Starting position for interval calculation
     * @param end_index Ending position
     * @return Vector of melodic intervals
     */
    static std::vector<int> get_interval_motif(const ClusterEngineCore& core, int engine_id,
                                              int start_index, int end_index);
    
    // ===== COUNTING AND ANALYSIS (Musical Intelligence) =====
    
    /**
     * Translation of count-notes-last-cell-at-current-index
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Engine ID
     * @return Number of notes (excluding rests) at current index
     */
    static int count_notes_at_current_index(const ClusterEngineCore& core, int engine_id);
    
    /**
     * Translation of count-notes-exclude-gracenotes-last-cell-at-current-index
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Engine ID
     * @return Number of notes excluding grace notes and rests
     */
    static int count_main_notes_at_current_index(const ClusterEngineCore& core, int engine_id);
    
    /**
     * Translation of count-events-last-cell-at-current-index
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Engine ID
     * @return Total number of events (notes + rests)
     */
    static int count_events_at_current_index(const ClusterEngineCore& core, int engine_id);
    
    // ===== TIMEPOINT COORDINATION (Cross-voice Synchronization) =====
    
    /**
     * Translation of get-cell-at-timepoint
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Time-based engine ID
     * @param timepoint Specific time point to query
     * @return Musical value at specified timepoint
     */
    static MusicalCandidate get_value_at_timepoint(const ClusterEngineCore& core, int engine_id, double timepoint);
    
    /**
     * Translation of get-cells-at-timepoints
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Time-based engine ID
     * @param timepoints Vector of time points to query
     * @return Vector of musical values at specified timepoints
     */
    static std::vector<MusicalCandidate> get_values_at_timepoints(const ClusterEngineCore& core, int engine_id, 
                                                    const std::vector<double>& timepoints);
    
    // ===== PITCH-SPECIFIC UTILITIES (Pitch Engine Support) =====
    
    /**
     * Translation of get-last-pitch-at-previous-index
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Pitch engine ID
     * @return Last pitch from previous index
     */
    static MusicalCandidate get_last_pitch_at_previous_index(const ClusterEngineCore& core, int engine_id);
    
    /**
     * Translation of get-current-index-total-pitchcount
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Pitch engine ID
     * @return Total pitch count up to current index
     */
    static int get_total_pitch_count(const ClusterEngineCore& core, int engine_id);
    
    // ===== VALIDATION AND TESTING (Constraint Support) =====
    
    /**
     * Translation of p-test-if-all-elements-are-true equivalent
     * @param values Vector of constraint satisfaction results
     * @return True if all constraint values are satisfied
     */
    static bool all_constraints_satisfied(const std::vector<bool>& values);
    
    /**
     * Check if a sequence has unique elements (translation pattern from cluster utilities)
     * @param sequence Vector of musical values
     * @return True if all elements are unique
     */
    static bool has_unique_elements(const std::vector<int>& sequence);
    
    /**
     * Check if a musical value represents a rest (negative value in cluster-engine)
     * @param value Musical value to check
     * @return True if value represents a rest
     */
    static bool is_rest(int value) { 
        return value < 0; 
    }
    
    /**
     * Check if a musical value represents a grace note (cluster-engine pattern)
     * @param value Musical value to check
     * @return True if value represents a grace note
     */
    static bool is_grace_note(int value);
    
    // ===== DOMAIN AND CONVERSION UTILITIES =====
    
    /**
     * Convert pitch to pitch class (mod 12 operation from cluster-engine)
     * @param pitch MIDI pitch value
     * @return Pitch class (0-11)
     */
    static int to_pitch_class(int pitch) {
        return ((pitch % 12) + 12) % 12; // Handle negative values correctly
    }
    
    /**
     * Convert sequence to pitch classes (translation of clustering mod operations)
     * @param candidates Vector of MusicalCandidate values
     * @return Vector of pitch classes
     */
    static std::vector<int> to_pitch_classes(const std::vector<MusicalCandidate>& candidates);
    
    /**
     * Calculate melodic interval between two pitches
     * @param pitch1 First pitch
     * @param pitch2 Second pitch
     * @return Melodic interval (semitones)
     */
    static int melodic_interval(int pitch1, int pitch2) {
        return pitch2 - pitch1;
    }
    
    /**
     * Calculate sequence of melodic intervals from pitch sequence
     * @param pitch_candidates Vector of MusicalCandidate pitch values
     * @return Vector of intervals between consecutive pitches
     */
    static std::vector<int> to_melodic_intervals(const std::vector<MusicalCandidate>& pitch_candidates);
    
private:
    // Helper functions for internal calculations
    static int convert_cluster_pitch_to_midi(const MusicalCandidate& pitch_candidate);
    static double calculate_onset_time(const std::vector<MusicalCandidate>& rhythm_sequence, int index);
    static bool is_valid_engine_index(const ClusterEngineCore& core, int engine_id, int index);

public:
    // Make this public so it can be used by AdvancedMusicalAnalyzer
    static int convert_cluster_rhythm_to_duration(const MusicalCandidate& rhythm_candidate);
};

/**
 * @struct MusicalAnalysisResult
 * @brief Return structure for complex musical analysis functions
 */
struct MusicalAnalysisResult {
    std::vector<MusicalCandidate> pitch_sequence;
    std::vector<MusicalCandidate> rhythm_sequence;
    std::vector<int> interval_sequence;
    std::vector<double> onset_times;
    int total_duration;
    int note_count;
    bool has_rests;
    bool has_grace_notes;
};

/**
 * @class AdvancedMusicalAnalyzer
 * @brief Higher-level musical analysis functions based on MusicalUtilities
 */
class AdvancedMusicalAnalyzer {
public:
    /**
     * Comprehensive analysis of a musical engine
     * @param core The ClusterEngineCore containing musical engines
     * @param engine_id Engine to analyze
     * @param start_index Starting position for analysis
     * @param end_index Ending position for analysis
     * @return Complete musical analysis result
     */
    static MusicalAnalysisResult analyze_engine(const ClusterEngineCore& core, int engine_id,
                                              int start_index, int end_index);
    
    /**
     * Check temporal coordination between two engines
     * @param core The ClusterEngineCore containing musical engines
     * @param engine1_id First engine ID
     * @param engine2_id Second engine ID
     * @return True if engines are properly coordinated at current indices
     */
    static bool engines_coordinated(const ClusterEngineCore& core, 
                                              int engine1_id, int engine2_id);
    
    /**
     * Calculate harmonic consonance between two engines
     * @param core The ClusterEngineCore containing musical engines
     * @param engine1_id First engine ID
     * @param engine2_id Second engine ID
     * @return Consonance score (higher = more consonant)
     */
    static double calculate_consonance(const ClusterEngineCore& core, 
                                     int engine1_id, int engine2_id);
};

} // namespace ClusterEngine

#endif // MUSICAL_UTILITIES_H