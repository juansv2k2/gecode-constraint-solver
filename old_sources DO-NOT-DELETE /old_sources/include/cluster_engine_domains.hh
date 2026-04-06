/**
 * @file cluster_engine_domains.hh
 * @brief Musical Domain System - Authentic Cluster Engine Domain Generation
 * 
 * Implements the sophisticated musical domain generation from Cluster Engine:
 * - Onset grid calculation with tuplets and time signatures
 * - Beat structure generation for metric analysis
 * - Engine-specific domain creation (rhythm, pitch, metric)
 * - Domain randomization and initialization strategies
 */

#ifndef CLUSTER_ENGINE_DOMAINS_HH
#define CLUSTER_ENGINE_DOMAINS_HH

#include "cluster_engine_core.hh"
#include <vector>
#include <map>
#include <utility>
#include <random>

namespace ClusterEngine {

/**
 * @brief Time signature representation
 */
struct TimeSignature {
    int numerator;      // Beats per measure
    int denominator;    // Beat unit (4 = quarter note, 8 = eighth note, etc.)
    
    TimeSignature(int num = 4, int denom = 4) : numerator(num), denominator(denom) {}
    
    double measure_duration() const {
        return static_cast<double>(numerator) / denominator;
    }
    
    double beat_duration() const {
        return 1.0 / denominator;
    }
    
    bool operator==(const TimeSignature& other) const {
        return numerator == other.numerator && denominator == other.denominator;
    }
};

/**
 * @brief Onset grid for a specific time signature and tuplet set
 * 
 * Represents all possible note onset positions within a measure,
 * calculated from tuplet subdivisions and beat patterns.
 */
struct OnsetGrid {
    TimeSignature time_signature;
    std::vector<int> tuplets;           // Subdivision patterns (3, 4, 5, 6, etc.)
    std::vector<double> onset_points;   // Sorted onset positions (0.0 to measure_duration)
    std::vector<double> alt_beat_lengths; // Alternative beat subdivision patterns
    
    OnsetGrid(const TimeSignature& ts, const std::vector<int>& tup)
        : time_signature(ts), tuplets(tup) {
        calculate_onset_points();
    }
    
    void calculate_onset_points();
    void add_alternative_beat_pattern(const std::vector<double>& beat_lengths);
    
    bool has_onset_at(double time_point, double tolerance = 0.001) const;
    double find_nearest_onset(double time_point) const;
    size_t size() const { return onset_points.size(); }
};

/**
 * @brief Beat structure for metric analysis
 * 
 * Represents the hierarchical beat structure within a measure,
 * including strong beats, weak beats, and alternative beat patterns.
 */
struct BeatStructure {
    TimeSignature time_signature;
    std::vector<double> beat_positions;     // Beat start positions
    std::vector<double> beat_strengths;     // Relative strength values (0.0 to 1.0)
    std::vector<double> alt_beat_lengths;   // Alternative subdivision lengths
    
    BeatStructure(const TimeSignature& ts) : time_signature(ts) {
        calculate_default_beat_structure();
    }
    
    void calculate_default_beat_structure();
    void set_alternative_beat_lengths(const std::vector<double>& lengths);
    
    double get_beat_strength_at(double time_point) const;
    bool is_strong_beat_at(double time_point, double threshold = 0.5) const;
};

/**
 * @brief Metric domain vector (from 01.domain.lisp)
 * 
 * Contains the complete metric information:
 * [0] = time signatures list
 * [1] = onset grids for each time signature  
 * [2] = beat structures for each time signature
 */
class MetricDomain {
private:
    std::vector<TimeSignature> time_signatures_;
    std::vector<OnsetGrid> onset_grids_;
    std::vector<BeatStructure> beat_structures_;
    
public:
    MetricDomain() = default;
    
    /**
     * @brief Create metric domain from time signatures and tuplet patterns
     */
    void create_from_time_signatures(
        const std::vector<TimeSignature>& time_sigs,
        const std::vector<std::vector<int>>& tuplets_per_sig,
        const std::vector<std::vector<double>>& alt_beat_lengths = {});
    
    // Accessors
    const std::vector<TimeSignature>& get_time_signatures() const { return time_signatures_; }
    const std::vector<OnsetGrid>& get_onset_grids() const { return onset_grids_; }
    const std::vector<BeatStructure>& get_beat_structures() const { return beat_structures_; }
    
    size_t size() const { return time_signatures_.size(); }
    
    // Lookup methods
    const OnsetGrid* find_onset_grid(const TimeSignature& ts) const;
    const BeatStructure* find_beat_structure(const TimeSignature& ts) const;
};

/**
 * @brief Domain initialization strategies
 */
enum class DomainInitType {
    UNIFORM,            // Same domain at all indexes
    UNIFORM_RANDOM,     // Same domain, randomized order per index
    DYNAMIC,            // Different domain per index
    DYNAMIC_RANDOM      // Different domain per index, randomized
};

/**
 * @brief Engine domain builder for different engine types
 */
class EngineDomainBuilder {
private:
    std::mt19937 random_generator_;
    
    // Domain generation methods
    std::vector<MusicalCandidate> generate_rhythm_domain(
        const std::vector<int>& durations, bool include_rests = true) const;
    
    std::vector<MusicalCandidate> generate_pitch_domain(
        int min_pitch, int max_pitch, const std::vector<int>& scale = {}) const;
    
    std::vector<MusicalCandidate> generate_metric_domain(
        const MetricDomain& metric_domain) const;
    
    void randomize_domain(std::vector<MusicalCandidate>& domain);
    
public:
    EngineDomainBuilder(unsigned int seed = 0) : random_generator_(seed) {}
    
    /**
     * @brief Build domain for rhythm engine
     */
    MusicalDomain build_rhythm_domain(
        const std::vector<int>& duration_values,    // In milliseconds or beat subdivisions
        bool allow_rests = true,
        DomainInitType init_type = DomainInitType::UNIFORM);
    
    /**
     * @brief Build domain for pitch engine  
     */
    MusicalDomain build_pitch_domain(
        int min_midi_pitch = 21,                    // A0
        int max_midi_pitch = 108,                   // C8
        const std::vector<int>& allowed_pitches = {}, // Empty = chromatic
        DomainInitType init_type = DomainInitType::UNIFORM);
    
    /**
     * @brief Build domain for metric engine
     */
    MusicalDomain build_metric_domain(
        const MetricDomain& metric_domain,
        DomainInitType init_type = DomainInitType::UNIFORM);
    
    /**
     * @brief Build domain from custom candidate list
     */
    MusicalDomain build_custom_domain(
        EngineType engine_type,
        const std::vector<MusicalCandidate>& candidates,
        DomainInitType init_type = DomainInitType::UNIFORM);
    
    // Randomization control
    void set_random_seed(unsigned int seed) { random_generator_.seed(seed); }
    bool get_randomization_enabled() const { return true; }
};

/**
 * @brief Locked engine analysis (from 01.domain.lisp)
 */
class LockedEngineAnalyzer {
public:
    /**
     * @brief Analyze domains to find engines with single values (auto-locked)
     */
    static std::vector<int> find_locked_engines(
        const std::vector<MusicalDomain>& domains,
        const MetricDomain& metric_domain);
    
    /**
     * @brief Check if all engines would be locked (error condition)  
     */
    static bool all_engines_locked(
        const std::vector<MusicalDomain>& domains,
        const MetricDomain& metric_domain);
    
    /**
     * @brief Get recommended engines to lock for performance
     */
    static std::vector<int> recommend_engines_to_lock(
        const std::vector<MusicalDomain>& domains,
        const MetricDomain& metric_domain,
        int max_locked = 2);
};

/**
 * @brief Complete domain configuration for Cluster Engine
 */
struct DomainConfiguration {
    std::vector<MusicalDomain> engine_domains;  // One per engine
    MetricDomain metric_domain;                // Metric information
    std::vector<int> locked_engines;           // Engines that cannot be searched
    DomainInitType initialization_type;        // How to initialize domains
    bool randomize_order;                      // Whether to randomize candidate order
    unsigned int random_seed;                  // Seed for randomization
    
    DomainConfiguration() 
        : initialization_type(DomainInitType::UNIFORM), 
          randomize_order(false), random_seed(0) {}
};

/**
 * @brief Domain factory for common musical scenarios
 */
class MusicalDomainFactory {
public:
    /**
     * @brief Create classical music domain configuration
     */
    static DomainConfiguration create_classical_domains(
        int num_voices,
        int min_pitch = 48,      // C3
        int max_pitch = 84);     // C6
    
    /**
     * @brief Create jazz music domain configuration  
     */
    static DomainConfiguration create_jazz_domains(
        int num_voices,
        const std::vector<int>& chord_tones,
        const std::vector<TimeSignature>& swing_meters = {{4,4}});
    
    /**
     * @brief Create contemporary music domain configuration
     */
    static DomainConfiguration create_contemporary_domains(
        int num_voices,
        const std::vector<int>& pitch_set,      // 12-tone, modal, etc.
        const std::vector<TimeSignature>& complex_meters = {{7,8}, {5,4}});
    
    /**
     * @brief Create minimalist music domain configuration
     */
    static DomainConfiguration create_minimalist_domains(
        int num_voices,  
        const std::vector<int>& limited_pitches,
        const std::vector<int>& repetitive_rhythms);
    
    /**
     * @brief Create educational/pedagogical domain configuration
     */
    static DomainConfiguration create_educational_domains(
        int num_voices,
        int difficulty_level = 1);              // 1=beginner, 5=advanced
};

/**
 * @brief Domain utilities and helpers
 */
class DomainUtils {
public:
    /**
     * @brief Convert duration in milliseconds to beat subdivision
     */
    static int duration_to_subdivision(int duration_ms, const TimeSignature& ts, int tempo_bpm = 120);
    
    /**
     * @brief Convert MIDI pitch to interval from reference pitch
     */
    static int pitch_to_interval(int midi_pitch, int reference_pitch = 60); // C4
    
    /**
     * @brief Generate scale degrees for given scale type
     */
    static std::vector<int> generate_scale_pitches(
        int root_pitch, const std::string& scale_type = "major");
    
    /**
     * @brief Generate chord tones for harmonic domains
     */
    static std::vector<int> generate_chord_tones(
        int root_pitch, const std::string& chord_type = "major");
    
    /**
     * @brief Validate domain configuration for consistency
     */
    static bool validate_domain_configuration(const DomainConfiguration& config);
};

} // namespace ClusterEngine

#endif // CLUSTER_ENGINE_DOMAINS_HH