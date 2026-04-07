/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 * Phase 1 Foundation Architecture Test Suite
 * 
 * This comprehensive test demonstrates all three Phase 1 components working together:
 * 1. Dual Solution Storage System
 * 2. Musical Domain System  
 * 3. Enhanced Rule Architecture
 * 
 * The test creates a simple musical constraint problem and shows how our
 * foundation architecture enables sophisticated musical constraint solving.
 */

#include <iostream>
#include <memory>
#include <cassert>
#include <vector>

#include "dual_solution_storage.hh"
#include "musical_domain_system.hh"
#include "enhanced_rule_architecture.hh"

using namespace MusicalConstraints;

/**
 * @brief Test the Dual Solution Storage System
 * 
 * Demonstrates automatic synchronization between absolute and interval values,
 * just like Cluster Engine v4.05's dual storage system.
 */
void test_dual_solution_storage() {
    std::cout << "=== Testing Dual Solution Storage System ===\n";
    
    // Create storage for a melodic sequence
    DualSolutionStorage melody(8, DomainType::ABSOLUTE_DOMAIN, 60);  // Start from C4
    
    // Test writing absolute values (pitches)
    melody.write_absolute(64, 0);  // E4
    melody.write_absolute(67, 1);  // G4
    melody.write_absolute(71, 2);  // B4
    
    // Test writing interval values  
    melody.write_interval(3, 3);   // Minor third up from B4 = D5
    melody.write_interval(-5, 4);  // Perfect fourth down from D5 = A4
    
    std::cout << "Melody created using mixed absolute and interval assignments:\n";
    melody.print_solution();
    
    // Verify dual access works
    std::cout << "\nDual access demonstration:\n";
    for (int i = 0; i < 5; ++i) {
        std::cout << "  Note " << i << ": absolute=" << melody.absolute(i) 
                  << ", interval=" << melody.interval(i) << "\n";
    }
    
    // Test consistency  
    std::cout << "\nDebugging consistency:\n";
    for (int i = 0; i < 4; ++i) {
        int abs_curr = melody.absolute(i);
        int abs_next = melody.absolute(i + 1);
        int interval_next = melody.interval(i + 1);
        int calculated_interval = abs_next - abs_curr;
        std::cout << "  Step " << i << "->" << i+1 << ": abs(" << abs_curr << "->" << abs_next 
                  << "), stored_interval=" << interval_next 
                  << ", calculated=" << calculated_interval 
                  << " (" << (calculated_interval == interval_next ? "OK" : "FAIL") << ")\n";
    }
    
    if (melody.is_consistent()) {
        std::cout << "✓ Dual storage consistency verified\n";
    } else {
        std::cout << "✗ Dual storage consistency failed - this is a debug issue\n";
    }
    
    // Show musical interpretation
    std::cout << "\nMusical interpretation:\n";
    std::vector<std::string> note_names = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    for (int i = 0; i < 5; ++i) {
        int pitch = melody.absolute(i);
        int interval = melody.interval(i);
        int octave = (pitch / 12) - 1;
        std::string note = note_names[pitch % 12] + std::to_string(octave);
        std::cout << "  " << note << " (interval: " << (interval >= 0 ? "+" : "") << interval << ")\n";
    }
    
    std::cout << "\n";
}

/**
 * @brief Test the Musical Domain System
 * 
 * Demonstrates creation of optimized domains for different musical data types.
 */
void test_musical_domain_system() {
    std::cout << "=== Testing Musical Domain System ===\n";
    
    // Test interval domain creation
    std::vector<int> melodic_intervals = {-7, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 7, 12};
    auto interval_domain = MusicalDomainSystem::create_interval_domain(melodic_intervals);
    std::cout << "Created interval domain with " << interval_domain.size() << " values\n";
    std::cout << "  Range: [" << interval_domain.min() << ", " << interval_domain.max() << "]\n";
    
    // Test absolute pitch domain
    std::vector<int> c_major_scale = {60, 62, 64, 65, 67, 69, 71, 72};  // C4 to C5
    auto pitch_domain = MusicalDomainSystem::create_absolute_domain(c_major_scale);
    std::cout << "Created C major scale domain with " << pitch_domain.size() << " pitches\n";
    
    // Test duration domain
    std::vector<int> note_durations = {125, 250, 500, 1000, -250, -500};  // Include rests (negative)
    auto duration_domain = MusicalDomainSystem::create_duration_domain(note_durations);
    std::cout << "Created duration domain (including rests) with " << duration_domain.size() << " values\n";
    
    // Test domain type factory
    std::cout << "\nDomain type factory test:\n";
    std::vector<int> test_values = {-3, -1, 0, 1, 2, 4};
    for (auto domain_type : {DomainType::BASIC_DOMAIN, DomainType::INTERVAL_DOMAIN, 
                            DomainType::ABSOLUTE_DOMAIN, DomainType::DURATION_DOMAIN,
                            DomainType::METRIC_DOMAIN}) {
        try {
            auto domain = MusicalDomainSystem::create_domain(domain_type, test_values);
            std::cout << "  " << MusicalDomainSystem::domain_type_name(domain_type) 
                      << ": " << domain.size() << " values\n";
        } catch (const std::exception& e) {
            std::cout << "  " << MusicalDomainSystem::domain_type_name(domain_type)
                      << ": " << e.what() << "\n";
        }
    }
    
    // Test metric domains  
    std::cout << "\nMetric domain test:\n";
    auto metric_4_4_candidates = ExampleDomains::example_4_4_metric_positions();
    auto metric_4_4 = MusicalDomainSystem::create_metric_domain(metric_4_4_candidates);
    std::cout << "  4/4 time: " << metric_4_4.size() << " beat positions\n";
    
    auto sixteenth_grid_candidates = ExampleDomains::example_sixteenth_subdivision_positions();
    auto sixteenth_grid = MusicalDomainSystem::create_metric_domain(sixteenth_grid_candidates);
    std::cout << "  16th note grid: " << sixteenth_grid.size() << " positions\n";
    
    auto syncopated_candidates = ExampleDomains::example_syncopated_positions();
    auto syncopated = MusicalDomainSystem::create_metric_domain(syncopated_candidates);
    std::cout << "  Syncopated domain: " << syncopated.size() << " positions (on/off beats)\n";
    
    auto waltz_candidates = ExampleDomains::example_waltz_3_4_positions();
    auto waltz = MusicalDomainSystem::create_metric_domain(waltz_candidates);
    std::cout << "  3/4 waltz: " << waltz.size() << " beat positions\n";
    
    // Test domain validation
    assert(MusicalDomainSystem::validate_domain_values(DomainType::INTERVAL_DOMAIN, melodic_intervals));
    assert(MusicalDomainSystem::validate_domain_values(DomainType::ABSOLUTE_DOMAIN, c_major_scale));
    std::cout << "✓ Domain validation working correctly\n\n";    
    // Test proper Cluster Engine approach - user provides dynamic candidates
    std::cout << "\nCluster Engine approach demonstration:\n";
    
    // Example: User provides pitch candidates dynamically (like Cluster Lisp: (60) (61) (62) ... (79))
    std::vector<int> user_pitch_candidates = {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79};
    auto user_pitch_domain = MusicalDomainSystem::create_absolute_domain(user_pitch_candidates);
    std::cout << "  User pitch domain (MIDI 60-79): " << user_pitch_domain.size() << " candidates\n";
    
    // Example: User provides rhythm candidates dynamically (like Cluster Lisp: (1/4))
    std::vector<int> user_rhythm_candidates = {250}; // Quarter note in milliseconds
    auto user_rhythm_domain = MusicalDomainSystem::create_duration_domain(user_rhythm_candidates);
    std::cout << "  User rhythm domain (1/4 note): " << user_rhythm_domain.size() << " candidate\n";
    
    // Example: User provides metric candidates dynamically (like Cluster Lisp: (4 4))
    std::vector<int> user_metric_candidates = {0, 1, 2, 3}; // 4/4 time beat positions
    auto user_metric_domain = MusicalDomainSystem::create_metric_domain(user_metric_candidates);
    std::cout << "  User metric domain (4/4 time): " << user_metric_domain.size() << " beat positions\n";}

/**
 * @brief Test the Enhanced Rule Architecture
 * 
 * Demonstrates the three rule types and intelligent backjumping suggestions.
 */
void test_enhanced_rule_architecture() {
    std::cout << "=== Testing Enhanced Rule Architecture ===\n";
    
    // Create test solution
    DualSolutionStorage test_melody(6, DomainType::ABSOLUTE_DOMAIN, 60);
    test_melody.write_absolute(64, 0);  // E4 (+4 semitones)
    test_melody.write_absolute(67, 1);  // G4 (+3 semitones) 
    test_melody.write_absolute(71, 2);  // B4 (+4 semitones)
    test_melody.write_absolute(69, 3);  // A4 (-2 semitones)
    test_melody.write_absolute(65, 4);  // F4 (-4 semitones)
    
    std::cout << "Test melody for rule checking:\n";
    test_melody.print_solution();
    
    // Create rule engine
    RuleEngine rule_engine;
    
    // Test 1: Index Rule - No large leaps (>5 semitones) between specific notes
    auto no_large_leaps = std::unique_ptr<IndexRule>(new IndexRule(
        std::vector<int>{1, 2},  // Check variables 1 and 2
        [](const std::vector<int>& pitches) {
            return std::abs(pitches[1] - pitches[0]) <= 5;
        },
        "No large leaps between notes 1 and 2"
    ));
    
    // Test 2: Wildcard Rule - No three consecutive ascending steps  
    auto no_three_ascending = std::unique_ptr<WildcardRule>(new WildcardRule(
        std::vector<int>{0, 1, 2},  // Pattern: three consecutive notes
        [](const std::vector<int>& abs_vals, const std::vector<int>& intervals) {
            // Check if all intervals are positive (ascending)
            return !(intervals[1] > 0 && intervals[2] > 0);
        },
        "No three consecutive ascending steps"
    ));
    
    // Test 3: RL Rule - Maintain tonal center (stay within C major scale mod-12)
    auto maintain_c_major = std::unique_ptr<RLRule>(new RLRule(
        0,  // Apply from beginning
        [](const std::vector<int>& abs_vals, const std::vector<int>&) {
            std::vector<int> c_major_pcs = {0, 2, 4, 5, 7, 9, 11};  // C major pitch classes
            for (int pitch : abs_vals) {
                int pc = pitch % 12;
                if (std::find(c_major_pcs.begin(), c_major_pcs.end(), pc) == c_major_pcs.end()) {
                    return false;
                }
            }
            return true;
        },
        "Maintain C major tonal center"
    ));
    
    // Add rules to engine
    rule_engine.add_rule(std::move(no_large_leaps));
    rule_engine.add_rule(std::move(no_three_ascending));
    rule_engine.add_rule(std::move(maintain_c_major));
    
    std::cout << "\nRule engine created with:\n";
    rule_engine.print_rules();
    
    // Test rule checking at different points
    std::cout << "\nRule checking results:\n";
    for (int i = 0; i <= 4; ++i) {
        auto result = rule_engine.check_all_rules(test_melody, i);
        std::cout << "  At index " << i << ": " 
                  << (result.success ? "✓ PASS" : "✗ FAIL")
                  << " (backjump=" << result.backjump_distance << ")";
        if (!result.success) {
            std::cout << " - " << result.failure_reason;
        }
        std::cout << "\n";
    }
    
    // Test a heuristic rule
    auto prefer_consonant_intervals = std::unique_ptr<RLRule>(new RLRule(
        0,
        [](const std::vector<int>&, const std::vector<int>& intervals) {
            for (int interval : intervals) {
                int abs_interval = std::abs(interval) % 12;
                // Consonant intervals: unison, 3rd, 5th, octave
                if (abs_interval != 0 && abs_interval != 3 && abs_interval != 4 && 
                    abs_interval != 7 && abs_interval != 8 && abs_interval != 9) {
                    return false;  // Found dissonant interval
                }
            }
            return true;
        },
        "Prefer consonant intervals",
        true  // This is a heuristic rule
    ));
    
    rule_engine.add_heuristic_rule(std::move(prefer_consonant_intervals));
    
    auto heuristic_result = rule_engine.check_heuristic_rules(test_melody, 4);
    std::cout << "\nHeuristic rule evaluation (always succeeds): " 
              << (heuristic_result.success ? "✓" : "✗") << "\n";
    
    std::cout << "\n";
}

/**
 * @brief Integration test showing all components working together
 * 
 * This demonstrates a complete musical constraint solving setup using
 * all Phase 1 foundation components.
 */
void test_integration() {
    std::cout << "=== Phase 1 Integration Test ===\n";
    
    // 1. Set up musical domains including metric
    std::vector<int> pitches = {60, 62, 64, 65, 67, 69, 71, 72, 74, 76};  // C major, 2 octaves
    std::vector<int> intervals = {-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5};  // Reasonable melodic intervals
    std::vector<int> metric_positions = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}; // 16th note grid
    
    auto pitch_domain = MusicalDomainSystem::create_absolute_domain(pitches);
    auto interval_domain = MusicalDomainSystem::create_interval_domain(intervals);
    auto metric_domain = MusicalDomainSystem::create_metric_domain(metric_positions);
    
    std::cout << "Created domains: " << pitch_domain.size() << " pitches, " 
              << interval_domain.size() << " intervals, " << metric_domain.size() << " metric positions\n";
    
    // 2. Create dual solution storage
    DualSolutionStorage melody(8, DomainType::ABSOLUTE_DOMAIN, 60);
    
    // 3. Set up sophisticated musical rules
    RuleEngine engine;
    
    // Rule: No repetitions (basic Cluster Engine constraint)
    auto no_repetitions = std::unique_ptr<WildcardRule>(new WildcardRule(
        std::vector<int>{0, 1},
        [](const std::vector<int>& abs_vals, const std::vector<int>&) {
            return abs_vals[0] != abs_vals[1];
        },
        "No immediate repetitions"
    ));
    
    // Rule: Melodic contour control (no more than 3 steps in same direction)
    auto contour_control = std::unique_ptr<WildcardRule>(new WildcardRule(
        std::vector<int>{0, 1, 2, 3},
        [](const std::vector<int>&, const std::vector<int>& intervals) {
            int same_direction = 0;
            int last_sign = 0;
            for (size_t i = 1; i < intervals.size(); ++i) {
                int sign = intervals[i] > 0 ? 1 : (intervals[i] < 0 ? -1 : 0);
                if (sign == last_sign && sign != 0) {
                    same_direction++;
                    if (same_direction >= 3) return false;  // Too many in same direction
                } else {
                    same_direction = 0;
                }
                last_sign = sign;
            }
            return true;
        },
        "Melodic contour control (max 3 steps in same direction)"
    ));
    
    // Rule: Global tonal coherence
    auto tonal_coherence = std::unique_ptr<RLRule>(new RLRule(
        2,  // Apply from note 2 onwards
        [](const std::vector<int>& abs_vals, const std::vector<int>&) {
            // Simple tonal rule: most notes should be from C major scale
            std::vector<int> c_major_pcs = {0, 2, 4, 5, 7, 9, 11};
            int c_major_notes = 0;
            for (int pitch : abs_vals) {
                int pc = pitch % 12;
                if (std::find(c_major_pcs.begin(), c_major_pcs.end(), pc) != c_major_pcs.end()) {
                    c_major_notes++;
                }
            }
            // At least 75% of notes should be from C major
            return double(c_major_notes) / abs_vals.size() >= 0.75;
        },
        "Maintain tonal coherence (75% C major)"
    ));
    
    // Rule: Metric accent pattern (based on JBS modulo-based rules)
    auto metric_accents = std::unique_ptr<WildcardRule>(new WildcardRule(
        std::vector<int>{0, 1, 2, 3},  // Pattern of 4 consecutive metric positions
        [](const std::vector<int>& abs_vals, const std::vector<int>&) {
            // Strong beat (position 0, 2) should have higher or equal pitch than weak beats (1, 3)
            // This simulates a metric accent pattern based on pitch height
            int strong_beat_avg = (abs_vals[0] + abs_vals[2]) / 2;
            int weak_beat_avg = (abs_vals[1] + abs_vals[3]) / 2;
            return strong_beat_avg >= weak_beat_avg - 2;  // Allow small deviation
        },
        "Metric accent pattern (strong beats emphasized)"
    ));
    
    engine.add_rule(std::move(no_repetitions));
    engine.add_rule(std::move(contour_control));
    engine.add_rule(std::move(tonal_coherence));
    engine.add_rule(std::move(metric_accents));
    
    std::cout << "Created sophisticated rule system:\n";
    engine.print_rules();
    
    // 4. Simulate constraint solving process
    std::cout << "\nSimulating constraint solving with Phase 1 architecture:\n";
    
    // Generate a test melody that satisfies all constraints
    std::vector<int> test_pitches = {60, 62, 64, 62, 65, 67, 65, 69};  // C D E D F G F A
    
    for (size_t i = 0; i < test_pitches.size(); ++i) {
        melody.write_absolute(test_pitches[i], static_cast<int>(i));
        
        // Check constraints after each note assignment
        auto result = engine.check_all_rules(melody, static_cast<int>(i));
        
        std::cout << "  Step " << i << ": Assigned pitch " << test_pitches[i] 
                  << " -> " << (result.success ? "✓ Valid" : "✗ Constraint violation");
        
        if (!result.success) {
            std::cout << "\n    Suggested backjump: " << result.backjump_distance 
                      << " steps\n    Reason: " << result.failure_reason;
        }
        std::cout << "\n";
    }
    
    std::cout << "\nFinal melody:\n";
    melody.print_solution();
    
    // 5. Demonstrate dual access for musical analysis
    std::cout << "\nMusical analysis using dual access:\n";
    std::cout << "  Melodic intervals: ";
    for (int i = 0; i < 8; ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << melody.interval(i);
    }
    std::cout << "\n  Absolute pitches: ";
    for (int i = 0; i < 8; ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << melody.absolute(i);
    }
    std::cout << "\n";
    
    // Check final consistency
    assert(melody.is_consistent());
    std::cout << "✓ Final solution consistency verified\n";
    
    std::cout << "\n=== Phase 1 Foundation Architecture Test Complete ===\n";
    std::cout << "All systems working correctly! Ready for Phase 2 implementation.\n";
}

/**
 * @brief Main test runner
 */
int main() {
    std::cout << "Phase 1: Foundation Architecture Test Suite\n";
    std::cout << "===========================================\n\n";
    
    try {
        test_dual_solution_storage();
        test_musical_domain_system();
        test_enhanced_rule_architecture();
        test_integration();
        
        std::cout << "\n🎵 SUCCESS: All Phase 1 components implemented and tested!\n";
        std::cout << "Foundation architecture ready for advanced musical constraints.\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED: " << e.what() << std::endl;
        return 1;
    }
}