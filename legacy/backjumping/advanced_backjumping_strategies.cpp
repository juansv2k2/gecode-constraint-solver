/**
 * @file advanced_backjumping_strategies.cpp
 * @brief Implementation of Advanced Backjumping Strategies from Cluster Engine v4.05
 * 
 * Comprehensive implementation of sophisticated backjumping algorithms
 * with multiple modes and intelligent musical conflict resolution.
 */

#include "advanced_backjumping_strategies.hh"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iomanip>
#include <map>
#include <chrono>

namespace AdvancedBackjumping {

// ===============================
// AdvancedBackjumpAnalyzer Implementation
// ===============================

AdvancedBackjumpResult AdvancedBackjumpAnalyzer::analyze_backjump_mode2(
        const std::vector<MusicalConstraints::RuleResult>& rule_results,
        int current_variable_index) const {
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    AdvancedBackjumpResult result;
    result.total_rules_tested = static_cast<int>(rule_results.size());
    
    if (debug_mode_) {
        std::cout << "\\n🔍 Mode 2 Analysis: Intelligent Backjumping" << std::endl;
        std::cout << "   Current variable: " << current_variable_index << std::endl;
        std::cout << "   Rules to analyze: " << result.total_rules_tested << std::endl;
    }
    
    // Collect backjump suggestions from failed rules
    std::vector<int> suggested_distances;
    
    for (size_t i = 0; i < rule_results.size(); ++i) {
        const auto& rule_result = rule_results[i];
        
        if (!rule_result.passes) {
            result.rules_suggesting_backjump++;
            
            // Use rule's suggested backjump distance
            if (rule_result.backjump_distance > 0) {
                suggested_distances.push_back(rule_result.backjump_distance);
                
                if (debug_mode_) {
                    std::cout << "   Rule " << i << " suggests backjump: " 
                              << rule_result.backjump_distance << std::endl;
                }
            }
            
            // Analyze backjump suggestions within the rule result
            for (const auto& suggestion : rule_result.backjump_suggestions) {
                if (suggestion.backjump_distance > 0) {
                    suggested_distances.push_back(suggestion.backjump_distance);
                    result.conflicting_variables.push_back(suggestion.variable_index);
                    result.conflict_reasons.push_back(suggestion.explanation);
                    
                    if (debug_mode_) {
                        std::cout << "     Variable " << suggestion.variable_index 
                                  << " conflict: " << suggestion.explanation << std::endl;
                    }
                }
            }
            
            // Add general conflict reason
            if (!rule_result.failure_reason.empty()) {
                result.conflict_reasons.push_back(rule_result.failure_reason);
            }
        }
    }
    
    // Mode 2: Take minimum backjump distance (cluster-engine v4.05 behavior)
    if (!suggested_distances.empty()) {
        result.has_backjump = true;
        result.minimum_backjump_distance = *std::min_element(suggested_distances.begin(), 
                                                            suggested_distances.end());
        result.maximum_backjump_distance = *std::max_element(suggested_distances.begin(),
                                                            suggested_distances.end());
        
        // Consensus is the minimum for mode 2
        result.consensus_backjump_distance = result.minimum_backjump_distance;
        
        if (debug_mode_) {
            std::cout << "   ✅ Backjump decision: " << result.consensus_backjump_distance 
                      << " (min of " << suggested_distances.size() << " suggestions)" << std::endl;
        }
    } else if (result.rules_suggesting_backjump > 0) {
        // Some rules failed but didn't provide distances - default backjump
        result.has_backjump = true;
        result.minimum_backjump_distance = 1;
        result.maximum_backjump_distance = 1;
        result.consensus_backjump_distance = 1;
        
        if (debug_mode_) {
            std::cout << "   ⚠️  Default backjump: 1 (no specific distances provided)" << std::endl;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.analysis_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    
    return result;
}

AdvancedBackjumpResult AdvancedBackjumpAnalyzer::analyze_backjump_mode3(
        const std::vector<MusicalConstraints::RuleResult>& rule_results,
        int current_variable_index) const {
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    AdvancedBackjumpResult result;
    result.total_rules_tested = static_cast<int>(rule_results.size());
    
    if (debug_mode_) {
        std::cout << "\\n🔍 Mode 3 Analysis: Consensus Backjumping" << std::endl;
        std::cout << "   Current variable: " << current_variable_index << std::endl;
        std::cout << "   Rules to analyze: " << result.total_rules_tested << std::endl;
    }
    
    // Collect all suggested distances and find consensus
    std::vector<int> all_suggestions;
    std::map<int, int> distance_counts; // distance -> count
    
    for (size_t i = 0; i < rule_results.size(); ++i) {
        const auto& rule_result = rule_results[i];
        
        if (!rule_result.passes) {
            result.rules_suggesting_backjump++;
            
            // Collect primary backjump distance
            if (rule_result.backjump_distance > 0) {
                all_suggestions.push_back(rule_result.backjump_distance);
                distance_counts[rule_result.backjump_distance]++;
            }
            
            // Collect suggestion-specific distances
            for (const auto& suggestion : rule_result.backjump_suggestions) {
                if (suggestion.backjump_distance > 0) {
                    all_suggestions.push_back(suggestion.backjump_distance);
                    distance_counts[suggestion.backjump_distance]++;
                    result.conflicting_variables.push_back(suggestion.variable_index);
                    result.conflict_reasons.push_back(suggestion.explanation);
                }
            }
        }
    }
    
    if (!all_suggestions.empty()) {
        result.minimum_backjump_distance = *std::min_element(all_suggestions.begin(), 
                                                            all_suggestions.end());
        result.maximum_backjump_distance = *std::max_element(all_suggestions.begin(),
                                                            all_suggestions.end());
        
        // Mode 3: Consensus - find most frequently suggested distance
        int max_count = 0;
        int consensus_distance = result.minimum_backjump_distance;
        
        for (const auto& pair : distance_counts) {
            if (debug_mode_) {
                std::cout << "   Distance " << pair.first << ": " << pair.second << " votes" << std::endl;
            }
            
            if (pair.second > max_count) {
                max_count = pair.second;
                consensus_distance = pair.first;
            }
        }
        
        // Require at least 2/3 consensus for mode 3
        double consensus_ratio = static_cast<double>(max_count) / all_suggestions.size();
        
        if (consensus_ratio >= 0.67) { // 2/3 consensus required
            result.has_backjump = true;
            result.consensus_backjump_distance = consensus_distance;
            
            if (debug_mode_) {
                std::cout << "   ✅ Consensus achieved: distance " << consensus_distance 
                          << " (" << std::fixed << std::setprecision(1) 
                          << consensus_ratio * 100 << "% agreement)" << std::endl;
            }
        } else {
            result.has_backjump = false;
            
            if (debug_mode_) {
                std::cout << "   ❌ No consensus: best " << std::fixed << std::setprecision(1)
                          << consensus_ratio * 100 << "% (need 67%)" << std::endl;
            }
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.analysis_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    
    return result;
}

AdvancedBackjumpResult AdvancedBackjumpAnalyzer::analyze_backjump(
        const std::vector<MusicalConstraints::RuleResult>& rule_results,
        int current_variable_index) {
    
    total_analyses_++;
    
    AdvancedBackjumpResult result;
    
    switch (current_mode_) {
        case BackjumpMode::NO_BACKJUMPING:
            // Mode 1: No backjumping - just indicate if rules failed
            result.total_rules_tested = static_cast<int>(rule_results.size());
            for (const auto& rule_result : rule_results) {
                if (!rule_result.passes) {
                    result.rules_suggesting_backjump++;
                }
            }
            result.has_backjump = false; // Never backjump in mode 1
            
            if (debug_mode_) {
                std::cout << "\\n🔍 Mode 1: No Backjumping (simple backtracking)" << std::endl;
                std::cout << "   Failed rules: " << result.rules_suggesting_backjump << std::endl;
            }
            break;
            
        case BackjumpMode::INTELLIGENT_BACKJUMP:
            result = analyze_backjump_mode2(rule_results, current_variable_index);
            break;
            
        case BackjumpMode::CONSENSUS_BACKJUMP:
            result = analyze_backjump_mode3(rule_results, current_variable_index);
            break;
    }
    
    // Update performance statistics
    if (result.has_backjump) {
        successful_backjumps_++;
    }
    
    // Update average analysis time
    double current_time_ms = result.analysis_time.count() / 1000000.0; // Convert to ms
    average_analysis_time_ = (average_analysis_time_ * (total_analyses_ - 1) + current_time_ms) / total_analyses_;
    
    return result;
}

std::string AdvancedBackjumpAnalyzer::get_mode_name() const {
    switch (current_mode_) {
        case BackjumpMode::NO_BACKJUMPING: return "No Backjumping";
        case BackjumpMode::INTELLIGENT_BACKJUMP: return "Intelligent Backjump";
        case BackjumpMode::CONSENSUS_BACKJUMP: return "Consensus Backjump";
        default: return "Unknown Mode";
    }
}

AdvancedBackjumpAnalyzer::PerformanceStats AdvancedBackjumpAnalyzer::get_performance_stats() const {
    PerformanceStats stats;
    stats.total_analyses = total_analyses_;
    stats.successful_backjumps = successful_backjumps_;
    stats.success_rate = (total_analyses_ > 0) ? 
        static_cast<double>(successful_backjumps_) / total_analyses_ * 100.0 : 0.0;
    stats.average_analysis_time_ms = average_analysis_time_;
    return stats;
}

void AdvancedBackjumpAnalyzer::reset_performance_stats() {
    total_analyses_ = 0;
    successful_backjumps_ = 0;
    average_analysis_time_ = 0.0;
}

// ===============================
// BackjumpStrategyCoordinator Implementation
// ===============================

BackjumpStrategyCoordinator::BackjumpStrategyCoordinator(BackjumpMode initial_mode) 
    : fallback_mode_(BackjumpMode::NO_BACKJUMPING), 
      adaptive_mode_selection_(false) {
    
    analyzer_ = std::make_unique<AdvancedBackjumpAnalyzer>(initial_mode);
    mode_performance_history_.resize(3, 0.0); // 3 modes
}

void BackjumpStrategyCoordinator::enable_adaptive_mode_selection(bool enable) {
    adaptive_mode_selection_ = enable;
    if (enable && !analyzer_->get_performance_stats().total_analyses) {
        std::cout << "📊 Adaptive mode selection enabled" << std::endl;
    }
}

AdvancedBackjumpResult BackjumpStrategyCoordinator::perform_backjump_analysis(
        const std::vector<std::shared_ptr<MusicalConstraints::MusicalRule>>& rules,
        const MusicalConstraints::DualSolutionStorage& current_solution,
        int current_variable_index) {
    
    // First, evaluate all rules against current solution
    std::vector<MusicalConstraints::RuleResult> rule_results;
    
    for (const auto& rule : rules) {
        if (rule) {
            MusicalConstraints::RuleResult result = rule->check_rule(current_solution, current_variable_index);
            rule_results.push_back(result);
        }
    }
    
    // Apply musical heuristics to enhance the analysis
    AdvancedBackjumpResult musical_analysis = MusicalBackjumpHeuristics::analyze_musical_conflicts(
        current_solution, current_variable_index, rule_results);
    
    // Perform main backjump analysis
    AdvancedBackjumpResult main_result = analyzer_->analyze_backjump(rule_results, current_variable_index);
    
    // Combine analyses - prefer musical heuristics if they provide better insight
    if (musical_analysis.has_backjump && musical_analysis.consensus_backjump_distance > 0) {
        // Musical heuristics override if they suggest a larger, more informed backjump
        if (musical_analysis.consensus_backjump_distance > main_result.consensus_backjump_distance) {
            main_result.consensus_backjump_distance = musical_analysis.consensus_backjump_distance;
            main_result.conflict_reasons.insert(main_result.conflict_reasons.end(),
                                               musical_analysis.conflict_reasons.begin(),
                                               musical_analysis.conflict_reasons.end());
        }
    }
    
    return main_result;
}

void BackjumpStrategyCoordinator::switch_to_fallback_mode() {
    std::cout << "🔄 Switching to fallback mode: " << analyzer_->get_mode_name() 
              << " -> ";
    analyzer_->set_mode(fallback_mode_);
    std::cout << analyzer_->get_mode_name() << std::endl;
}

BackjumpStrategyCoordinator::StrategyInfo BackjumpStrategyCoordinator::get_strategy_info() const {
    StrategyInfo info;
    // Note: We'd need to track current_mode in analyzer to make this complete
    info.fallback_mode = fallback_mode_;
    info.adaptive_enabled = adaptive_mode_selection_;
    info.performance = analyzer_->get_performance_stats();
    return info;
}

void BackjumpStrategyCoordinator::print_analysis_report(const AdvancedBackjumpResult& result) const {
    std::cout << "\\n📊 Advanced Backjump Analysis Report" << std::endl;
    std::cout << "====================================" << std::endl;
    
    std::cout << "Mode: " << analyzer_->get_mode_name() << std::endl;
    std::cout << "Backjump needed: " << (result.has_backjump ? "YES" : "NO") << std::endl;
    
    if (result.has_backjump) {
        std::cout << "Consensus distance: " << result.consensus_backjump_distance << std::endl;
        std::cout << "Distance range: " << result.minimum_backjump_distance 
                  << " - " << result.maximum_backjump_distance << std::endl;
    }
    
    std::cout << "Rules analyzed: " << result.total_rules_tested << std::endl;
    std::cout << "Rules suggesting backjump: " << result.rules_suggesting_backjump << std::endl;
    
    if (!result.conflicting_variables.empty()) {
        std::cout << "Conflicting variables: ";
        for (size_t i = 0; i < result.conflicting_variables.size(); ++i) {
            std::cout << result.conflicting_variables[i];
            if (i < result.conflicting_variables.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
    }
    
    if (!result.conflict_reasons.empty()) {
        std::cout << "Conflict reasons:" << std::endl;
        for (size_t i = 0; i < std::min(result.conflict_reasons.size(), size_t(3)); ++i) {
            std::cout << "  - " << result.conflict_reasons[i] << std::endl;
        }
        if (result.conflict_reasons.size() > 3) {
            std::cout << "  ... and " << (result.conflict_reasons.size() - 3) << " more" << std::endl;
        }
    }
    
    std::cout << "Analysis time: " << std::fixed << std::setprecision(3) 
              << result.analysis_time.count() / 1000000.0 << " ms" << std::endl;
}

// ===============================
// MusicalBackjumpHeuristics Implementation  
// ===============================

AdvancedBackjumpResult MusicalBackjumpHeuristics::analyze_musical_conflicts(
        const MusicalConstraints::DualSolutionStorage& solution,
        int current_index,
        const std::vector<MusicalConstraints::RuleResult>& rule_results) {
    
    AdvancedBackjumpResult result;
    
    // Analyze voice-leading conflicts
    int voice_leading_backjump = calculate_voice_leading_backjump(solution, current_index, 2);
    
    // Analyze harmonic rhythm conflicts  
    int harmonic_rhythm_backjump = calculate_harmonic_rhythm_backjump(solution, current_index);
    
    // Identity phrase boundaries for structural backjumps
    std::vector<int> phrase_boundaries = identify_phrase_boundaries(solution, current_index);
    
    // Combine musical analysis
    std::vector<int> musical_suggestions;
    
    if (voice_leading_backjump > 0) {
        musical_suggestions.push_back(voice_leading_backjump);
        result.conflict_reasons.push_back("Voice leading conflict detected");
    }
    
    if (harmonic_rhythm_backjump > 0) {
        musical_suggestions.push_back(harmonic_rhythm_backjump);
        result.conflict_reasons.push_back("Harmonic rhythm irregularity");
    }
    
    if (!phrase_boundaries.empty()) {
        // Suggest backjump to nearest phrase boundary
        auto boundary = std::upper_bound(phrase_boundaries.begin(), phrase_boundaries.end(), current_index);
        if (boundary != phrase_boundaries.begin()) {
            --boundary;
            int boundary_backjump = current_index - *boundary;
            if (boundary_backjump > 0) {
                musical_suggestions.push_back(boundary_backjump);
                result.conflict_reasons.push_back("Musical phrase structure violation");
            }
        }
    }
    
    if (!musical_suggestions.empty()) {
        result.has_backjump = true;
        result.minimum_backjump_distance = *std::min_element(musical_suggestions.begin(), musical_suggestions.end());
        result.maximum_backjump_distance = *std::max_element(musical_suggestions.begin(), musical_suggestions.end());
        result.consensus_backjump_distance = result.minimum_backjump_distance; // Conservative choice
    }
    
    return result;
}

int MusicalBackjumpHeuristics::calculate_voice_leading_backjump(
        const MusicalConstraints::DualSolutionStorage& solution,
        int current_index, int num_voices) {
    
    // Look for voice crossing or large leaps in recent history
    int lookback_distance = std::min(current_index, 4); // Look back up to 4 positions
    
    for (int i = 1; i <= lookback_distance; ++i) {
        int check_index = current_index - i;
        if (check_index >= 0) {
            // Check for problematic voice leading patterns
            
            // Large melodic leaps (>octave)
            if (check_index > 0) {
                int interval = std::abs(solution.absolute(check_index) - solution.absolute(check_index - 1));
                if (interval > 12) { // Larger than octave
                    return i; // Backjump to before the leap
                }
            }
            
            // Extreme range violations  
            int pitch = solution.absolute(check_index);
            if (pitch < 36 || pitch > 84) { // Outside reasonable range
                return i;
            }
        }
    }
    
    return 0; // No voice leading issues detected
}

int MusicalBackjumpHeuristics::calculate_harmonic_rhythm_backjump(
        const MusicalConstraints::DualSolutionStorage& solution,
        int current_index) {
    
    // Detect irregular harmonic rhythm patterns
    int lookback = std::min(current_index, 6); // Analyze recent harmonic rhythm
    
    // Look for sudden changes in intervallic pattern
    std::vector<int> recent_intervals;
    for (int i = std::max(0, current_index - lookback); i < current_index; ++i) {
        if (i > 0) {
            recent_intervals.push_back(std::abs(solution.absolute(i) - solution.absolute(i-1)));
        }
    }
    
    if (recent_intervals.size() >= 3) {
        // Check for sudden pattern disruption
        double avg_interval = std::accumulate(recent_intervals.begin(), recent_intervals.end(), 0.0) / recent_intervals.size();
        
        for (size_t i = 0; i < recent_intervals.size(); ++i) {
            if (std::abs(recent_intervals[i] - avg_interval) > 6) { // Sudden large deviation
                return static_cast<int>(recent_intervals.size() - i); // Backjump to before disruption
            }
        }
    }
    
    return 0; // No harmonic rhythm issues
}

std::vector<int> MusicalBackjumpHeuristics::identify_phrase_boundaries(
        const MusicalConstraints::DualSolutionStorage& solution,
        int current_index) {
    
    std::vector<int> boundaries;
    
    // Simple phrase boundary detection based on melodic contour
    int phrase_length = 4; // Assume 4-note phrases for simple analysis
    
    for (int i = phrase_length; i <= current_index; i += phrase_length) {
        boundaries.push_back(i);
    }
    
    // Add boundaries based on large intervallic leaps (phrase separations)
    for (int i = 1; i < current_index; ++i) {
        if (i > 0) {
            int interval = std::abs(solution.absolute(i) - solution.absolute(i-1));
            if (interval > 7) { // Large leap suggests phrase boundary
                boundaries.push_back(i);
            }
        }
    }
    
    // Sort and remove duplicates
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
    
    return boundaries;
}

// ===============================
// GecodeBackjumpIntegration Implementation
// ===============================

void GecodeBackjumpIntegration::apply_backjump_to_gecode_search(
        const AdvancedBackjumpResult& backjump_result,
        void* gecode_space_ptr) {
    
    // Note: This would require including Gecode headers and casting gecode_space_ptr
    // For now, we provide the interface for future integration
    
    if (backjump_result.has_backjump) {
        // This would translate backjump suggestions to Gecode branching strategies
        // - Modify variable ordering based on conflicting_variables
        // - Adjust branching heuristics based on consensus_backjump_distance
        // - Apply domain restrictions from backjump suggestions
        
        std::cout << "🔗 Gecode integration: Backjump distance " 
                  << backjump_result.consensus_backjump_distance << std::endl;
    }
}

std::vector<int> GecodeBackjumpIntegration::generate_optimal_posting_order(
        const AdvancedBackjumpResult& backjump_result,
        int total_variables) {
    
    std::vector<int> posting_order;
    for (int i = 0; i < total_variables; ++i) {
        posting_order.push_back(i);
    }
    
    // Prioritize non-conflicting variables first
    if (!backjump_result.conflicting_variables.empty()) {
        // Move conflicting variables to end of posting order
        auto is_conflicting = [&](int var) {
            return std::find(backjump_result.conflicting_variables.begin(),
                           backjump_result.conflicting_variables.end(), var) != 
                   backjump_result.conflicting_variables.end();
        };
        
        std::stable_partition(posting_order.begin(), posting_order.end(),
                            [&](int var) { return !is_conflicting(var); });
    }
    
    return posting_order;
}

std::vector<int> GecodeBackjumpIntegration::create_variable_ordering(
        const AdvancedBackjumpResult& backjump_result,
        int total_variables) {
    
    std::vector<int> variable_order;
    for (int i = 0; i < total_variables; ++i) {
        variable_order.push_back(i);
    }
    
    // Order variables based on conflict analysis
    if (backjump_result.has_backjump && !backjump_result.conflicting_variables.empty()) {
        // Put most recently conflicting variables first for early detection
        std::sort(variable_order.begin(), variable_order.end(),
                [&](int a, int b) {
                    auto it_a = std::find(backjump_result.conflicting_variables.begin(),
                                        backjump_result.conflicting_variables.end(), a);
                    auto it_b = std::find(backjump_result.conflicting_variables.begin(),
                                        backjump_result.conflicting_variables.end(), b);
                    
                    // Conflicting variables come first
                    if (it_a != backjump_result.conflicting_variables.end() && 
                        it_b == backjump_result.conflicting_variables.end()) return true;
                    if (it_a == backjump_result.conflicting_variables.end() && 
                        it_b != backjump_result.conflicting_variables.end()) return false;
                    
                    return a < b; // Otherwise maintain order
                });
    }
    
    return variable_order;
}

} // namespace AdvancedBackjumping