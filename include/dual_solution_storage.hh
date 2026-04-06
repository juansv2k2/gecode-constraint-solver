/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 * Dual Solution Storage System
 * 
 * This implements Cluster Engine v4.05's revolutionary dual storage approach
 * where each musical variable maintains both absolute and interval values.
 * 
 * Key Innovation: Rules can access either absolute values (pitches, timepoints)
 * or intervals/durations naturally, enabling sophisticated musical constraints
 * that work with both local intervals and global pitch relationships.
 * 
 * Based on Cluster Engine v4.05 solution storage architecture:
 * - solution[index] = (absolute_val, interval_val)
 * - Automatic maintenance of both formats
 * - Efficient access patterns for musical constraint checking
 */

#ifndef DUAL_SOLUTION_STORAGE_HH
#define DUAL_SOLUTION_STORAGE_HH

#include <vector>
#include <iostream>
#include <cassert>

namespace MusicalConstraints {

// Forward declaration for compatibility
struct DualCandidate {
    int absolute_value;
    int interval_value;
    
    DualCandidate(int abs = 0, int interval = 0) 
        : absolute_value(abs), interval_value(interval) {}
};

/**
 * @brief Musical value that stores both absolute and relative representations
 * 
 * This is the core data structure that enables Cluster Engine's musical intelligence.
 * Each musical event (note, timepoint, etc.) is stored in dual format:
 * - absolute_val: The actual value (MIDI pitch, millisecond timestamp)
 * - interval_val: The difference from previous value (semitone interval, duration)
 */
struct MusicalValue {
    int absolute_val;    ///< Absolute value (pitch in MIDI, time in ms)
    int interval_val;    ///< Interval/duration from previous value
    
    /// Default constructor
    MusicalValue() : absolute_val(0), interval_val(0) {}
    
    /// Constructor with absolute value (interval calculated automatically)
    MusicalValue(int abs_val, int interval = 0) 
        : absolute_val(abs_val), interval_val(interval) {}
        
    /// String representation for debugging
    std::string to_string() const {
        return "(" + std::to_string(absolute_val) + ", " + 
               std::to_string(interval_val) + ")";
    }
};

/**
 * @brief Domain type classification for musical constraints
 * 
 * Based on Cluster Engine v4.05's domain type system.
 * Different domain types enable specialized constraint handling
 * and optimal internal representations.
 */
enum class DomainType {
    BASIC_DOMAIN = -1,      ///< General symbols/objects (non-numeric)
    INTERVAL_DOMAIN = 0,    ///< Melodic intervals (semitones)
    ABSOLUTE_DOMAIN = 1,    ///< Absolute pitches (MIDI) or timepoints
    ABSOLUTE_RHYTHM = 2,    ///< Absolute time positions (milliseconds)
    DURATION_DOMAIN = 3,    ///< Duration values (milliseconds, beats)
    METRIC_DOMAIN = 4       ///< Metrical positions (beat hierarchies, time signatures)
};

/**
 * @brief Dual solution storage that maintains absolute and interval representations
 * 
 * This class implements the core innovation from Cluster Engine v4.05:
 * simultaneous maintenance of absolute and relative musical values.
 * 
 * Key features:
 * - Automatic synchronization between absolute and interval values
 * - Efficient access patterns for musical constraint checking  
 * - Support for different domain types (pitch, rhythm, duration)
 * - Musical rule-friendly interface with (a x) and (i x) style access
 * 
 * Memory layout optimized for constraint propagation performance.
 */
class DualSolutionStorage {
private:
    std::vector<MusicalValue> solution_;    ///< The dual-format solution storage
    DomainType domain_type_;                ///< Type of musical domain
    int starting_value_;                    ///< Initial absolute value (pitch 0, time 0)
    
public:
    /**
     * @brief Construct storage for a musical sequence
     * @param max_length Maximum number of variables in sequence
     * @param domain_type Type of musical domain being stored
     * @param starting_value Initial absolute value (default 60 = Middle C)
     */
    explicit DualSolutionStorage(int max_length, 
                                DomainType domain_type = DomainType::ABSOLUTE_DOMAIN,
                                int starting_value = 60)
        : solution_(max_length + 1), domain_type_(domain_type), starting_value_(starting_value) {
        
        // Initialize starting point (index -1 conceptually, index 0 actually)
        solution_[0].absolute_val = starting_value;
        solution_[0].interval_val = 0;  // First interval is always 0
    }
    
    /**
     * @brief Write an absolute value and auto-calculate interval
     * @param value The absolute value (MIDI pitch, timepoint, etc.)
     * @param index Position in the sequence (0-based)
     * 
     * This mimics Cluster Engine's write-one-absolute-value-to-solution
     */
    void write_absolute(int value, int index) {
        assert(index >= 0 && index < static_cast<int>(solution_.size()) - 1);
        
        int storage_index = index + 1;  // Offset for starting value at index 0
        solution_[storage_index].absolute_val = value;
        
        // Calculate interval from previous absolute value
        int prev_absolute = solution_[storage_index - 1].absolute_val;
        solution_[storage_index].interval_val = value - prev_absolute;
    }
    
    /**
     * @brief Write an interval value and auto-calculate absolute
     * @param value The interval/duration value
     * @param index Position in the sequence (0-based)
     * 
     * This mimics Cluster Engine's write-one-interval-value-to-solution
     */
    void write_interval(int value, int index) {
        assert(index >= 0 && index < static_cast<int>(solution_.size()) - 1);
        
        int storage_index = index + 1;  // Offset for starting value
        solution_[storage_index].interval_val = value;
        
        // Calculate absolute from previous absolute + this interval
        int prev_absolute = solution_[storage_index - 1].absolute_val;
        
        if (domain_type_ == DomainType::DURATION_DOMAIN) {
            // For durations, use absolute value for timepoint calculation
            solution_[storage_index].absolute_val = prev_absolute + std::abs(value);
        } else {
            solution_[storage_index].absolute_val = prev_absolute + value;
        }
    }
    
    /**
     * @brief Access absolute value (Cluster Engine's (a x) macro)
     * @param index Position in sequence (0-based)
     * @return Absolute value at index
     */
    int absolute(int index) const {
        assert(index >= -1 && index < static_cast<int>(solution_.size()) - 1);
        return solution_[index + 1].absolute_val;  // Offset for starting value
    }
    
    /**
     * @brief Access interval value (Cluster Engine's (i x) macro)
     * @param index Position in sequence (0-based)
     * @return Interval value at index
     */
    int interval(int index) const {
        assert(index >= -1 && index < static_cast<int>(solution_.size()) - 1);
        return solution_[index + 1].interval_val;  // Offset for starting value
    }
    
    /**
     * @brief Access basic value (Cluster Engine's (b x) macro)
     * @param index Position in sequence (0-based)
     * @return Absolute value (same as absolute() for numeric domains)
     */
    int basic(int index) const {
        return absolute(index);
    }
    
    /**
     * @brief Access duration value (Cluster Engine's (d x) macro)
     * @param index Position in sequence (0-based)  
     * @return Duration/interval value (same as interval())
     */
    int duration(int index) const {
        return interval(index);
    }
    
    /**
     * @brief Get the domain type for this storage
     * @return Domain type classification
     */
    DomainType domain_type() const { return domain_type_; }
    
    /**
     * @brief Get current sequence length (number of written values)
     * @return Length of the stored sequence
     */
    int length() const { return static_cast<int>(solution_.size()) - 1; }
    
    /**
     * @brief Get starting value (conceptual index -1)
     * @return The initial absolute value
     */
    int starting_value() const { return starting_value_; }
    
    // ========================================
    // Compatibility methods for rule integration
    // ========================================
    
    /**
     * @brief Check if a value is assigned at given index
     * @param index Position in sequence (0-based)
     * @return true if value is assigned at this position
     */
    bool has_candidate(int index) const {
        return index >= -1 && index < static_cast<int>(solution_.size()) - 1;
    }
    
    /**
     * @brief Get dual candidate at index (compatibility with rule integration)
     * @param index Position in sequence (0-based)
     * @return DualCandidate with absolute and interval values
     */
    DualCandidate get_candidate(int index) const {
        if (!has_candidate(index)) {
            return DualCandidate(0, 0);
        }
        return DualCandidate(absolute(index), interval(index));
    }
    
    /**
     * @brief Add candidate value (compatibility with rule integration)
     * @param index Position in sequence (0-based)
     * @param candidate Dual candidate with absolute and interval values
     */
    void add_candidate(int index, const DualCandidate& candidate) {
        if (index >= 0 && index < static_cast<int>(solution_.size()) - 1) {
            int storage_index = index + 1;
            solution_[storage_index].absolute_val = candidate.absolute_value;
            solution_[storage_index].interval_val = candidate.interval_value;
        }
    }
    
    /**
     * @brief Get size for iteration (compatibility with rule integration)
     * @return Number of variable positions
     */
    size_t size() const {
        return static_cast<size_t>(length());
    }
    
    /**
     * @brief Print the complete solution for debugging
     * @param os Output stream
     */
    void print_solution(std::ostream& os = std::cout) const {
        os << "Dual Solution Storage (domain_type=" << static_cast<int>(domain_type_) << "):\n";
        os << "  Start: " << solution_[0].to_string() << "\n";
        
        for (int i = 0; i < length(); ++i) {
            if (solution_[i + 1].absolute_val != 0 || solution_[i + 1].interval_val != 0) {
                os << "  [" << i << "]: " << solution_[i + 1].to_string() 
                   << " (abs=" << absolute(i) << ", int=" << interval(i) << ")\n";
            }
        }
    }
    
    /**
     * @brief Check if the storage is valid (intervals and absolutes consistent)
     * @return true if all values are consistent
     */
    bool is_consistent() const {
        for (int i = 0; i < length() - 1; ++i) {
            int calculated_interval = solution_[i + 2].absolute_val - solution_[i + 1].absolute_val;
            if (calculated_interval != solution_[i + 2].interval_val) {
                return false;
            }
        }
        return true;
    }
    
    /**
     * @brief Clear all stored values (keep starting value)
     */
    void clear() {
        for (size_t i = 1; i < solution_.size(); ++i) {
            solution_[i] = MusicalValue();
        }
    }
};

} // namespace MusicalConstraints

#endif // DUAL_SOLUTION_STORAGE_HH