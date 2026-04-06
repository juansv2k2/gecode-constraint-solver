// ===================================================================
// Phase 4: State Persistence & Continuity - Core Musical State System
// ===================================================================
//
// PROFESSIONAL STATE MANAGEMENT: Advanced state persistence system for 
// complex musical compositions, enabling real-time composition workflows
// and sophisticated musical memory management.
//
// Revolutionary Capabilities:
//   ✅ Musical state serialization and restoration
//   ✅ Compositional memory with musical context awareness
//   ✅ Cross-segment musical continuity management
//   ✅ Real-time state updates during composition
//   ✅ Musical decision history and backtracking states
//
// State Management:
//   - Musical compositions can be saved/loaded at any point
//   - Previous musical decisions guide future composition choices
//   - Musical context preserved across composition segments
//   - State snapshots for complex musical development workflows
//   - Integration with Phase 1+2+3 foundation architecture
//
// ===================================================================

#ifndef MUSICAL_STATE_PERSISTENCE_HH
#define MUSICAL_STATE_PERSISTENCE_HH

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <chrono>
#include <memory>
#include "dual_solution_storage.hh"
#include "musical_domain_system.hh"

namespace MusicalConstraints {

/**
 * @brief Musical timestamp for state tracking
 */
struct MusicalTimestamp {
    std::chrono::system_clock::time_point creation_time;
    int composition_segment;        // Which musical segment (phrase, section, etc.)
    int measure_number;            // Musical measure within segment
    int beat_position;             // Beat position within measure
    
    MusicalTimestamp(int segment = 0, int measure = 0, int beat = 0)
        : creation_time(std::chrono::system_clock::now()),
          composition_segment(segment), measure_number(measure), beat_position(beat) {}
    
    std::string to_string() const {
        return "Segment:" + std::to_string(composition_segment) +
               " Measure:" + std::to_string(measure_number) +
               " Beat:" + std::to_string(beat_position);
    }
};

/**
 * @brief Compositional context information preserved across composition
 */
struct CompositionContext {
    int key_signature;             // Current key (0=C, 1=G, -1=F, etc.)
    int time_signature_num;        // Time signature numerator (4 in 4/4)
    int time_signature_den;        // Time signature denominator (4 in 4/4)
    int current_tempo;             // BPM tempo
    std::string harmonic_region;   // Current harmonic area (e.g., "tonic", "dominant")
    std::vector<int> active_chord_tones;  // Current harmony notes
    
    CompositionContext(int key = 0, int time_num = 4, int time_den = 4, int tempo = 120)
        : key_signature(key), time_signature_num(time_num), 
          time_signature_den(time_den), current_tempo(tempo),
          harmonic_region("tonic"), active_chord_tones({60, 64, 67}) {}  // C major triad
};

/**
 * @brief Complete musical state snapshot
 */
class MusicalState {
public:
    MusicalTimestamp timestamp;
    CompositionContext musical_context;
    DualSolutionStorage solution_storage;
    std::string state_description;
    
     // Decision history for musical intelligence
    std::vector<std::pair<int, std::string>> decision_history;  // (index, decision_reason)
    std::map<std::string, double> musical_preferences;         // Current musical tendencies
    
    explicit MusicalState(const DualSolutionStorage& storage, 
                         const std::string& description = "Musical state snapshot",
                         const CompositionContext& context = CompositionContext())
        : musical_context(context), 
          solution_storage(storage),
          state_description(description) {}
    
    /**
     * @brief Add a musical decision to the history
     */
    void add_decision(int index, const std::string& reason) {
        decision_history.emplace_back(index, reason);
    }
    
    /**
     * @brief Update musical preference based on composition choices
     */
    void update_preference(const std::string& preference_type, double strength) {
        musical_preferences[preference_type] = strength;
    }
    
    /**
     * @brief Get current preference strength for musical choice
     */
    double get_preference(const std::string& preference_type) const {
        auto it = musical_preferences.find(preference_type);
        return (it != musical_preferences.end()) ? it->second : 1.0;
    }
    
    /**
     * @brief Print detailed state information
     */
    void print_state_summary(std::ostream& os = std::cout) const {
        os << "\n🎼 Musical State Summary:\n";
        os << "  Description: " << state_description << "\n";
        os << "  Timestamp: " << timestamp.to_string() << "\n";
        os << "  Key: " << musical_context.key_signature << "\n";
        os << "  Time Signature: " << musical_context.time_signature_num 
           << "/" << musical_context.time_signature_den << "\n";
        os << "  Tempo: " << musical_context.current_tempo << " BPM\n";
        os << "  Harmonic Region: " << musical_context.harmonic_region << "\n";
        os << "  Solution Size: " << solution_storage.length() << "\n";
        os << "  Decision History: " << decision_history.size() << " decisions\n";
        os << "  Musical Preferences: " << musical_preferences.size() << " preferences\n";
    }
};

/**
 * @brief Musical state persistence manager with serialization
 */
class MusicalStatePersistence {
private:
    std::string base_directory_;
    std::vector<std::unique_ptr<MusicalState>> state_history_;
    std::unique_ptr<MusicalState> current_state_;
    int next_state_id_;
    
public:
    explicit MusicalStatePersistence(const std::string& base_dir = "./musical_states")
        : base_directory_(base_dir), next_state_id_(1) {
        ensure_directory_exists();
    }
    
    /**
     * @brief Save current musical state to persistent storage
     */
    bool save_state(const MusicalState& state, const std::string& filename = "") {
        std::string save_filename = filename.empty() ? 
            generate_state_filename(state) : filename;
        
        std::string full_path = base_directory_ + "/" + save_filename;
        std::ofstream file(full_path, std::ios::binary);
        
        if (!file.is_open()) {
            std::cerr << "Failed to open file for saving: " << full_path << std::endl;
            return false;
        }
        
        try {
            serialize_state(file, state);
            
            // Add to history  
            state_history_.push_back(std::unique_ptr<MusicalState>(
                new MusicalState(state.solution_storage, state.state_description, 
                               state.musical_context)));
            
            std::cout << "✅ Musical state saved: " << save_filename << "\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Error saving musical state: " << e.what() << std::endl;
            return false;
        }
    }
    
    /**
     * @brief Load musical state from persistent storage
     */
    std::unique_ptr<MusicalState> load_state(const std::string& filename) {
        std::string full_path = base_directory_ + "/" + filename;
        std::ifstream file(full_path, std::ios::binary);
        
        if (!file.is_open()) {
            std::cerr << "Failed to open file for loading: " << full_path << std::endl;
            return nullptr;
        }
        
        try {
            auto loaded_state = deserialize_state(file);
            
            if (loaded_state) {
                current_state_ = std::unique_ptr<MusicalState>(
                    new MusicalState(loaded_state->solution_storage, 
                                   loaded_state->state_description,
                                   loaded_state->musical_context));
                
                std::cout << "✅ Musical state loaded: " << filename << "\n";
            }
            
            return loaded_state;
            
        } catch (const std::exception& e) {
            std::cerr << "Error loading musical state: " << e.what() << std::endl;
            return nullptr;
        }
    }
    
    /**
     * @brief Create state snapshot for current composition
     */
    void create_state_snapshot(const DualSolutionStorage& storage,
                              const std::string& description,
                              const CompositionContext& context = CompositionContext()) {
        auto snapshot = std::unique_ptr<MusicalState>(
            new MusicalState(storage, description, context));
        
        snapshot->timestamp = MusicalTimestamp(state_history_.size(), 0, 0);
        
        current_state_ = std::move(snapshot);
        
        std::cout << "📸 State snapshot created: " << description << "\n";
    }
    
    /**
     * @brief Get current musical state
     */
    const MusicalState* get_current_state() const {
        return current_state_.get();
    }
    
    /**
     * @brief List all saved musical states
     */
    void list_saved_states(std::ostream& os = std::cout) const {
        os << "\n🎵 Saved Musical States:\n";
        os << "  Total states in history: " << state_history_.size() << "\n";
        
        for (size_t i = 0; i < state_history_.size(); ++i) {
            const auto& state = state_history_[i];
            os << "  [" << i << "] " << state->state_description 
               << " - " << state->timestamp.to_string() << "\n";
        }
        
        if (current_state_) {
            os << "\n  Current State: " << current_state_->state_description << "\n";
        }
    }
    
    /**
     * @brief Get musical state history size
     */
    size_t get_history_size() const {
        return state_history_.size();
    }
    
    /**
     * @brief Clear all musical state history
     */
    void clear_history() {
        state_history_.clear();
        current_state_.reset();
        next_state_id_ = 1;
        std::cout << "🗑️ Musical state history cleared\n";
    }

private:
    void ensure_directory_exists() {
        // Simple directory creation - in a real implementation, 
        // you'd use proper filesystem operations
        // For this phase, we'll assume directory exists or create it manually
    }
    
    std::string generate_state_filename(const MusicalState& state) {
        return "musical_state_" + std::to_string(next_state_id_++) + 
               "_" + std::to_string(state.timestamp.composition_segment) + ".dat";
    }
    
    void serialize_state(std::ofstream& file, const MusicalState& state) {
        // Serialize timestamp
        file.write(reinterpret_cast<const char*>(&state.timestamp.composition_segment), sizeof(int));
        file.write(reinterpret_cast<const char*>(&state.timestamp.measure_number), sizeof(int));
        file.write(reinterpret_cast<const char*>(&state.timestamp.beat_position), sizeof(int));
        
        // Serialize musical context
        file.write(reinterpret_cast<const char*>(&state.musical_context.key_signature), sizeof(int));
        file.write(reinterpret_cast<const char*>(&state.musical_context.time_signature_num), sizeof(int));
        file.write(reinterpret_cast<const char*>(&state.musical_context.time_signature_den), sizeof(int));
        file.write(reinterpret_cast<const char*>(&state.musical_context.current_tempo), sizeof(int));
        
        // Serialize description length and content
        size_t desc_len = state.state_description.length();
        file.write(reinterpret_cast<const char*>(&desc_len), sizeof(size_t));
        file.write(state.state_description.c_str(), desc_len);
        
        // Serialize solution storage data
        int storage_size = state.solution_storage.length();
        file.write(reinterpret_cast<const char*>(&storage_size), sizeof(int));
        
        for (int i = 0; i < storage_size; ++i) {
            int abs_val = state.solution_storage.absolute(i);
            int int_val = state.solution_storage.interval(i);
            file.write(reinterpret_cast<const char*>(&abs_val), sizeof(int));
            file.write(reinterpret_cast<const char*>(&int_val), sizeof(int));
        }
        
        // Serialize decision history
        size_t history_size = state.decision_history.size();
        file.write(reinterpret_cast<const char*>(&history_size), sizeof(size_t));
        
        for (const auto& decision : state.decision_history) {
            file.write(reinterpret_cast<const char*>(&decision.first), sizeof(int));
            size_t reason_len = decision.second.length();
            file.write(reinterpret_cast<const char*>(&reason_len), sizeof(size_t));
            file.write(decision.second.c_str(), reason_len);
        }
    }
    
    std::unique_ptr<MusicalState> deserialize_state(std::ifstream& file) {
        // Deserialize timestamp
        MusicalTimestamp timestamp;
        file.read(reinterpret_cast<char*>(&timestamp.composition_segment), sizeof(int));
        file.read(reinterpret_cast<char*>(&timestamp.measure_number), sizeof(int));
        file.read(reinterpret_cast<char*>(&timestamp.beat_position), sizeof(int));
        
        // Deserialize musical context
        CompositionContext context;
        file.read(reinterpret_cast<char*>(&context.key_signature), sizeof(int));
        file.read(reinterpret_cast<char*>(&context.time_signature_num), sizeof(int));
        file.read(reinterpret_cast<char*>(&context.time_signature_den), sizeof(int));
        file.read(reinterpret_cast<char*>(&context.current_tempo), sizeof(int));
        
        // Deserialize description
        size_t desc_len;
        file.read(reinterpret_cast<char*>(&desc_len), sizeof(size_t));
        std::string description(desc_len, '\0');
        file.read(&description[0], desc_len);
        
        // Deserialize solution storage
        int storage_size;
        file.read(reinterpret_cast<char*>(&storage_size), sizeof(int));
        
        DualSolutionStorage storage(storage_size + 2, DomainType::INTERVAL_DOMAIN, 60);
        
        for (int i = 0; i < storage_size; ++i) {
            int abs_val, int_val;
            file.read(reinterpret_cast<char*>(&abs_val), sizeof(int));
            file.read(reinterpret_cast<char*>(&int_val), sizeof(int));
            storage.write_absolute(abs_val, i);
            storage.write_interval(int_val, i);
        }
        
        // Create state with loaded data
        auto loaded_state = std::unique_ptr<MusicalState>(
            new MusicalState(storage, description, context));
        loaded_state->timestamp = timestamp;
        
        // Deserialize decision history
        size_t history_size;
        file.read(reinterpret_cast<char*>(&history_size), sizeof(size_t));
        
        for (size_t i = 0; i < history_size; ++i) {
            int index;
            file.read(reinterpret_cast<char*>(&index), sizeof(int));
            
            size_t reason_len;
            file.read(reinterpret_cast<char*>(&reason_len), sizeof(size_t));
            std::string reason(reason_len, '\0');
            file.read(&reason[0], reason_len);
            
            loaded_state->decision_history.emplace_back(index, reason);
        }
        
        return loaded_state;
    }
};

/**
 * @brief Musical continuity manager for seamless composition
 */
class MusicalContinuityManager {
private:
    std::vector<MusicalState> segment_states_;
    CompositionContext global_context_;
    std::map<std::string, double> continuity_preferences_;
    
public:
    explicit MusicalContinuityManager(const CompositionContext& context = CompositionContext())
        : global_context_(context) {
        // Initialize default continuity preferences
        continuity_preferences_["melodic_coherence"] = 0.8;
        continuity_preferences_["harmonic_consistency"] = 0.9;
        continuity_preferences_["rhythmic_flow"] = 0.7;
        continuity_preferences_["dynamic_development"] = 0.6;
    }
    
    /**
     * @brief Analyze musical continuity between segments
     */
    double analyze_continuity(const MusicalState& previous, const MusicalState& current) const {
        double melodic_score = analyze_melodic_continuity(previous, current);
        double harmonic_score = analyze_harmonic_continuity(previous, current);
        double rhythmic_score = analyze_rhythmic_continuity(previous, current);
        
        double weighted_score = 
            melodic_score * continuity_preferences_.at("melodic_coherence") +
            harmonic_score * continuity_preferences_.at("harmonic_consistency") +
            rhythmic_score * continuity_preferences_.at("rhythmic_flow");
        
        return weighted_score / 3.0; // Normalize to 0-1 range
    }
    
    /**
     * @brief Generate continuity suggestions for next musical segment
     */
    std::vector<std::string> generate_continuity_suggestions(const MusicalState& current) {
        std::vector<std::string> suggestions;
        
        // Analyze current musical context for suggestions
        if (current.solution_storage.length() > 0) {
            int last_pitch = current.solution_storage.absolute(
                current.solution_storage.length() - 1);
            
            suggestions.push_back("Continue melodic line from pitch " + std::to_string(last_pitch));
            suggestions.push_back("Develop harmonic progression in " + 
                                current.musical_context.harmonic_region);
            suggestions.push_back("Maintain " + std::to_string(current.musical_context.current_tempo) + 
                                " BPM tempo");
        }
        
        // Add context-specific suggestions
        if (current.musical_context.harmonic_region == "tonic") {
            suggestions.push_back("Consider modulation to dominant region");
        } else if (current.musical_context.harmonic_region == "dominant") {
            suggestions.push_back("Prepare return to tonic resolution");
        }
        
        return suggestions;
    }
    
    /**
     * @brief Update global musical context based on composition progress
     */
    void update_global_context(const MusicalState& new_state) {
        global_context_.key_signature = new_state.musical_context.key_signature;
        global_context_.harmonic_region = new_state.musical_context.harmonic_region;
        
        // Add to segment history
        segment_states_.push_back(new_state);
        
        std::cout << "🔄 Global musical context updated - Segment " 
                  << segment_states_.size() << "\n";
    }
    
    /**
     * @brief Print continuity analysis report
     */
    void print_continuity_report(std::ostream& os = std::cout) const {
        os << "\n🎵 Musical Continuity Report:\n";
        os << "  Total segments: " << segment_states_.size() << "\n";
        os << "  Global Key: " << global_context_.key_signature << "\n";
        os << "  Global Harmonic Region: " << global_context_.harmonic_region << "\n";
        
        if (segment_states_.size() >= 2) {
            // Calculate average continuity across all segments
            double total_continuity = 0.0;
            int continuity_checks = 0;
            
            for (size_t i = 1; i < segment_states_.size(); ++i) {
                double continuity = analyze_continuity(segment_states_[i-1], segment_states_[i]);
                total_continuity += continuity;
                continuity_checks++;
            }
            
            if (continuity_checks > 0) {
                double avg_continuity = total_continuity / continuity_checks;
                os << "  Average Continuity Score: " << std::fixed << std::setprecision(2) 
                   << avg_continuity << "/1.00\n";
            }
        }
        
        os << "\n  Continuity Preferences:\n";
        for (const auto& pref : continuity_preferences_) {
            os << "    " << pref.first << ": " << std::fixed << std::setprecision(1) 
               << pref.second << "\n";
        }
    }
    
    // Getters
    const CompositionContext& get_global_context() const { return global_context_; }
    size_t get_segment_count() const { return segment_states_.size(); }
    
private:
    double analyze_melodic_continuity(const MusicalState& prev, const MusicalState& curr) const {
        // Simple melodic continuity analysis based on interval relationships
        if (prev.solution_storage.length() == 0 || 
            curr.solution_storage.length() == 0) return 1.0;
        
        int prev_last = prev.solution_storage.absolute(prev.solution_storage.length() - 1);
        int curr_first = curr.solution_storage.absolute(0);
        
        int transition_interval = std::abs(curr_first - prev_last);
        
        // Prefer stepwise or small interval transitions for continuity
        if (transition_interval <= 2) return 1.0;      // Stepwise = excellent
        else if (transition_interval <= 4) return 0.8; // Small leap = good
        else if (transition_interval <= 7) return 0.6; // Medium leap = ok
        else return 0.3;                               // Large leap = poor continuity
    }
    
    double analyze_harmonic_continuity(const MusicalState& prev, const MusicalState& curr) const {
        // Analyze harmonic region consistency
        if (prev.musical_context.harmonic_region == curr.musical_context.harmonic_region) {
            return 1.0; // Same harmonic region = perfect continuity
        } else {
            // Check for logical harmonic progressions
            if ((prev.musical_context.harmonic_region == "tonic" && 
                 curr.musical_context.harmonic_region == "dominant") ||
                (prev.musical_context.harmonic_region == "dominant" && 
                 curr.musical_context.harmonic_region == "tonic")) {
                return 0.9; // Classical harmonic progression = excellent
            } else {
                return 0.5; // Other transitions = moderate continuity
            }
        }
    }
    
    double analyze_rhythmic_continuity(const MusicalState& prev, const MusicalState& curr) const {
        // Analyze tempo and time signature consistency
        double tempo_consistency = 1.0;
        if (prev.musical_context.current_tempo != curr.musical_context.current_tempo) {
            int tempo_diff = std::abs(prev.musical_context.current_tempo - 
                                    curr.musical_context.current_tempo);
            if (tempo_diff <= 5) tempo_consistency = 0.9;    // Slight tempo change = good
            else if (tempo_diff <= 15) tempo_consistency = 0.7; // Moderate change = ok
            else tempo_consistency = 0.4;                       // Large change = poor
        }
        
        double meter_consistency = 1.0;
        if (prev.musical_context.time_signature_num != curr.musical_context.time_signature_num ||
            prev.musical_context.time_signature_den != curr.musical_context.time_signature_den) {
            meter_consistency = 0.3; // Meter change = significant discontinuity
        }
        
        return (tempo_consistency + meter_consistency) / 2.0;
    }
};

} // namespace MusicalConstraints

#endif // MUSICAL_STATE_PERSISTENCE_HH