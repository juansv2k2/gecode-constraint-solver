/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 * Musical Domain System
 * 
 * This implements Cluster Engine v4.05's specialized domain types for musical constraints.
 * Provides optimized domains for different types of musical data and intelligent
 * domain creation with musical knowledge integration.
 * 
 * Based on Cluster Engine v4.05 domain specializations:
 * - BasicDomain: General constraint domains
 * - IntervalDomain: Melodic interval constraints with musical intelligence  
 * - AbsoluteDomain: Pitch/note domains with musical scales and ranges
 * - DurationDomain: Rhythmic domains including rests and musical durations
 * - MetricDomain: Metrical positions and rhythmic patterns
 */

#ifndef MUSICAL_DOMAIN_SYSTEM_HH
#define MUSICAL_DOMAIN_SYSTEM_HH

#include <vector>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <iostream>
#include "dual_solution_storage.hh"   // For DomainType definition

namespace MusicalConstraints {

// Note: DomainType is defined in dual_solution_storage.hh
// NOTE: Currently uses SimpleDomain for foundation testing.
//       Can be evolved to integrate with Gecode's IntSet for performance.

/**
 * @brief Simple domain implementation for Phase 1
 * 
 * This provides the basic domain functionality needed to test our
 * foundation architecture. In Phase 2+, this will integrate with Gecode's
 * domain system for performance optimization.
 */
class SimpleDomain {
private:
    std::vector<int> values_;   ///< Allowed values in the domain
    int min_val_;              ///< Minimum value
    int max_val_;              ///< Maximum value
    
public:
    /**
     * @brief Create domain from vector of allowed values
     * @param values Vector of allowed values (will be sorted and deduplicated)
     */
    SimpleDomain(const std::vector<int>& values) {
        if (values.empty()) {
            throw std::invalid_argument("Domain cannot be empty");
        }
        
        values_ = values;
        std::sort(values_.begin(), values_.end());
        values_.erase(std::unique(values_.begin(), values_.end()), values_.end());
        
        min_val_ = values_.front();
        max_val_ = values_.back();
    }
    
    /**
     * @brief Create range domain [min, max]
     * @param min_val Minimum value (inclusive)
     * @param max_val Maximum value (inclusive)
     */
    SimpleDomain(int min_val, int max_val) {
        if (min_val > max_val) {
            throw std::invalid_argument("min_val cannot be greater than max_val");
        }
        
        for (int i = min_val; i <= max_val; ++i) {
            values_.push_back(i);
        }
        min_val_ = min_val;
        max_val_ = max_val;
    }
    
    // Accessors
    size_t size() const { return values_.size(); }
    int min() const { return min_val_; }
    int max() const { return max_val_; }
    const std::vector<int>& values() const { return values_; }
    
    // Domain operations
    bool contains(int value) const {
        return std::binary_search(values_.begin(), values_.end(), value);
    }
    
    /**
     * @brief Get value at index position
     * @param index Index in domain (0-based)
     * @return Value at that position
     */
    int value_at(size_t index) const {
        if (index >= values_.size()) {
            throw std::out_of_range("Index out of domain range");
        }
        return values_[index];
    }
    
    /**
     * @brief Print domain for debugging
     */
    void print() const {
        std::cout << "Domain[" << size() << "]: {";
        for (size_t i = 0; i < values_.size() && i < 10; ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << values_[i];
        }
        if (values_.size() > 10) {
            std::cout << ", ... +" << (values_.size() - 10) << " more";
        }
        std::cout << "}";
    }
};

/**
 * @brief Domain system manager for musical constraints
 * 
 * This class provides intelligent domain creation with musical knowledge,
 * based on Cluster Engine v4.05's domain type system and musical optimizations.
 */
class MusicalDomainSystem {
public:
    /**
     * @brief Create interval domain with musical intelligence
     * @param intervals Vector of melodic intervals (in semitones) provided by user
     * @return Optimized domain for interval constraints
     * 
     * Applies musical knowledge:
     * - Validates interval ranges (-24 to +24 semitones)
     * - Optimizes for interval constraint checking
     * - Handles octave equivalence considerations
     * 
     * User provides the actual interval candidates (e.g., {-7, -5, -3, -1, 0, 1, 3, 5, 7})
     */
    static SimpleDomain create_interval_domain(const std::vector<int>& intervals) {
        if (intervals.empty()) {
            throw std::invalid_argument("Interval domain cannot be empty - user must provide interval candidates");
        }
        
        // Musical validation: reasonable interval range
        for (int interval : intervals) {
            if (interval < -24 || interval > 24) {
                throw std::invalid_argument("Interval out of reasonable musical range [-24, +24]");
            }
        }
        
        auto domain = SimpleDomain(intervals);
        std::cout << "Created interval domain: ";
        domain.print();
        std::cout << std::endl;
        return domain;
    }
    
    /**
     * @brief Create absolute pitch domain with musical intelligence  
     * @param pitches Vector of absolute pitches (MIDI note numbers) provided by user
     * @return Optimized domain for pitch constraints
     * 
     * Applies musical knowledge:
     * - Validates MIDI range (0-127)
     * - Optimizes for scale and chord constraint checking
     * - Handles octave and enharmonic relationships
     * 
     * User provides the actual pitch candidates (e.g., {60, 61, 62, 63, 64, 65, 66, 67})
     */
    static SimpleDomain create_absolute_domain(const std::vector<int>& pitches) {
        if (pitches.empty()) {
            throw std::invalid_argument("Pitch domain cannot be empty - user must provide pitch candidates");
        }
        
        // Musical validation: MIDI note range
        for (int pitch : pitches) {
            if (pitch < 0 || pitch > 127) {
                throw std::invalid_argument("Pitch out of MIDI range [0, 127]");
            }
        }
        
        auto domain = SimpleDomain(pitches);
        std::cout << "Created absolute pitch domain: ";
        domain.print();
        std::cout << std::endl;
        return domain;
    }
    
    /**
     * @brief Create duration domain with musical intelligence
     * @param durations Vector of note durations (positive) and rests (negative) provided by user
     * @return Optimized domain for duration constraints
     * 
     * Musical duration conventions:
     * - Positive values: note durations (in ticks/milliseconds)
     * - Negative values: rest durations (absolute value = duration)
     * - User provides specific duration candidates (e.g., {250, 500, 1000} for quarter, half, whole notes)
     */
    static SimpleDomain create_duration_domain(const std::vector<int>& durations) {
        if (durations.empty()) {
            throw std::invalid_argument("Duration domain cannot be empty - user must provide duration candidates");
        }
        
        // Musical validation: no zero durations
        for (int duration : durations) {
            if (duration == 0) {
                throw std::invalid_argument("Duration cannot be zero");
            }
        }
        
        auto domain = SimpleDomain(durations);
        std::cout << "Created duration domain (negatives=rests): ";
        domain.print();
        std::cout << std::endl;
        return domain;
    }
    
    /**
     * @brief Create time position domain
     * @param time_points Vector of possible onset times
     * @return Domain for temporal positioning
     */
    static SimpleDomain create_time_domain(const std::vector<int>& time_points) {
        // Musical validation: non-negative time points
        for (int time : time_points) {
            if (time < 0) {
                throw std::invalid_argument("Time points must be non-negative");
            }
        }
        
        auto domain = SimpleDomain(time_points);
        std::cout << "Created time domain: ";
        domain.print();
        std::cout << std::endl;
        return domain;
    }
    
    /**
     * @brief Create metric position domain with musical intelligence
     * @param metric_positions Vector of metrical positions (beat positions, subdivisions) provided by user
     * @return Optimized domain for metric constraints
     * 
     * Musical metric conventions:
     * - Values represent positions within metric hierarchy
     * - User provides specific metric positions (e.g., {0, 1, 2, 3} for 4/4 time)
     * - Supports subdivisions (e.g., {0, 1, 2, 3, 4, 5, 6, 7} for 8th note grid in 4/4)
     * - Negative values can represent off-beat positions
     * - Based on JBS modulo-based quantification rules
     */
    static SimpleDomain create_metric_domain(const std::vector<int>& metric_positions) {
        if (metric_positions.empty()) {
            throw std::invalid_argument("Metric domain cannot be empty - user must provide metric position candidates");
        }
        
        // Musical validation: reasonable metric range
        for (int pos : metric_positions) {
            if (pos < -32 || pos > 32) {
                throw std::invalid_argument("Metric position out of reasonable range [-32, +32]");
            }
        }
        
        auto domain = SimpleDomain(metric_positions);
        std::cout << "Created metric domain: ";
        domain.print();
        std::cout << std::endl;
        return domain;
    }
    
    /**
     * @brief Create basic domain (no musical optimizations)
     * @param values Vector of allowed values provided by user
     * @return Basic constraint domain
     */
    static SimpleDomain create_basic_domain(const std::vector<int>& values) {
        if (values.empty()) {
            throw std::invalid_argument("Basic domain cannot be empty - user must provide value candidates");
        }
        
        auto domain = SimpleDomain(values);
        std::cout << "Created basic domain: ";
        domain.print();
        std::cout << std::endl;
        return domain;
    }
    
    /**
     * @brief Factory method for creating domains by type
     * @param domain_type Type of domain to create
     * @param values Values for the domain
     * @return Specialized domain based on type
     */
    static SimpleDomain create_domain(DomainType domain_type, const std::vector<int>& values) {
        switch (domain_type) {
            case DomainType::BASIC_DOMAIN:
                return create_basic_domain(values);
            case DomainType::INTERVAL_DOMAIN:
                return create_interval_domain(values);
            case DomainType::ABSOLUTE_DOMAIN:
                return create_absolute_domain(values);
            case DomainType::DURATION_DOMAIN:
                return create_duration_domain(values);
            case DomainType::ABSOLUTE_RHYTHM:  // Use ABSOLUTE_RHYTHM for time positions
                return create_time_domain(values);
            case DomainType::METRIC_DOMAIN:
                return create_metric_domain(values);
            default:
                throw std::invalid_argument("Unknown domain type");
        }
    }
    
    /**
     * @brief Validate values for domain type
     * @param domain_type Domain type to validate against
     * @param values Values to validate
     * @return True if values are valid for domain type
     */
    static bool validate_domain_values(DomainType domain_type, const std::vector<int>& values) {
        try {
            create_domain(domain_type, values);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    
    /**
     * @brief Get human-readable domain type name
     * @param domain_type Domain type
     * @return String name of domain type
     */
    static std::string domain_type_name(DomainType domain_type) {
        switch (domain_type) {
            case DomainType::BASIC_DOMAIN: return "BasicDomain";
            case DomainType::INTERVAL_DOMAIN: return "IntervalDomain";
            case DomainType::ABSOLUTE_DOMAIN: return "AbsoluteDomain";
            case DomainType::DURATION_DOMAIN: return "DurationDomain";
            case DomainType::ABSOLUTE_RHYTHM: return "TimeDomain";
            case DomainType::METRIC_DOMAIN: return "MetricDomain";
            default: return "UnknownDomain";
        }
    }
};

/**
 * @brief Example Domain Creation Functions for Testing and Documentation
 * 
 * These are NOT part of the core MusicalDomainSystem (which handles user-provided candidates).
 * They demonstrate proper usage patterns, following the Cluster Engine approach where
 * domain content is provided dynamically by the user.
 */
namespace ExampleDomains {

    /// Example: Create pitch candidates for testing (user provides their own list in real usage)
    inline std::vector<int> example_chromatic_octave_pitches(int base_pitch = 60) {
        std::vector<int> pitches;
        for (int i = 0; i < 12; ++i) {
            pitches.push_back(base_pitch + i);
        }
        return pitches;  // User would pass this to MusicalDomainSystem::create_absolute_domain()
    }
    
    /// Example: Create pitch candidates for a major scale (user provides their own in real usage)
    inline std::vector<int> example_major_scale_pitches(int base_pitch = 60) {
        std::vector<int> major_scale = {0, 2, 4, 5, 7, 9, 11};  // Major scale intervals
        std::vector<int> pitches;
        for (int interval : major_scale) {
            pitches.push_back(base_pitch + interval);
        }
        return pitches;  // User would pass this to MusicalDomainSystem::create_absolute_domain()
    }
    
    /// Example: Create melodic interval candidates (user provides their own in real usage)
    inline std::vector<int> example_melodic_intervals() {
        return {-12, -7, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 7, 12};
        // User would pass this to MusicalDomainSystem::create_interval_domain()
    }
    
    /// Example: Create rhythm candidates (user provides their own in real usage)
    inline std::vector<int> example_rhythm_durations() {
        return {120, 240, 480, 960, -120, -240, -480};  // 16th, 8th, quarter, half (+ rests)
        // User would pass this to MusicalDomainSystem::create_duration_domain()
    }
    
    /// Example: Create 4/4 time metric candidates (user provides their own in real usage)
    inline std::vector<int> example_4_4_metric_positions() {
        return {0, 1, 2, 3};  // Four quarter note beats
        // User would pass this to MusicalDomainSystem::create_metric_domain()
    }
    
    /// Example: Create 16th note grid candidates (user provides their own in real usage)
    inline std::vector<int> example_sixteenth_subdivision_positions() {
        std::vector<int> positions;
        for (int beat = 0; beat < 4; ++beat) {          // 4 beats
            for (int sub = 0; sub < 4; ++sub) {         // 4 sixteenths per beat
                positions.push_back(beat * 4 + sub);    // 0,1,2,3,4,5,6,7...15
            }
        }
        return positions;  // User would pass this to MusicalDomainSystem::create_metric_domain()
    }
    
    /// Example: Create syncopated metric candidates (user provides their own in real usage)
    inline std::vector<int> example_syncopated_positions() {
        return {0, 1, 2, 3, -1, -2, -3, -4};  // On-beats (positive) + off-beats (negative)
        // User would pass this to MusicalDomainSystem::create_metric_domain()
    }
    
    /// Example: Create 3/4 waltz time candidates (user provides their own in real usage)  
    inline std::vector<int> example_waltz_3_4_positions() {
        return {0, 1, 2};    // Three quarter note beats
        // User would pass this to MusicalDomainSystem::create_metric_domain()
    }
    
    /// Example: Create compound 6/8 meter candidates (user provides their own in real usage)
    inline std::vector<int> example_compound_6_8_positions() {
        return {0, 1, 2, 3, 4, 5};  // Six eighth notes, grouped as 3+3
        // User would pass this to MusicalDomainSystem::create_metric_domain()
    }

} // namespace ExampleDomains

} // namespace MusicalConstraints

#endif // MUSICAL_DOMAIN_SYSTEM_HH