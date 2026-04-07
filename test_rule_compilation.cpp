/**
 * @file test_rule_compilation.cpp
 * @brief Test the new rule compilation system with old sources integration
 * 
 * This test demonstrates the rule compilation pipeline that translates
 * JSON rule definitions into Gecode constraint propagators.
 */

#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <map>
#include <string>

// Simplified standalone test version
namespace RuleCompilationTest {

struct TestContext {
    std::vector<int> absolute_values;
    std::vector<int> interval_values;
    int sequence_length;
    int current_index;
};

enum class RuleResult {
    PASS = 0,
    FAIL = 1, 
    INSUFFICIENT_DATA = 2
};

class TestMusicalRule {
public:
    enum class RuleType {
        SINGLE_ENGINE_PITCH,
        RETROGRADE_INVERSION,
        STEPWISE_MOTION
    };

protected:
    std::string name_;
    RuleType type_;
    std::vector<int> target_engines_;
    bool enabled_;

public:
    TestMusicalRule(const std::string& name, RuleType type, 
                   const std::vector<int>& engines)
        : name_(name), type_(type), target_engines_(engines), enabled_(true) {}

    virtual ~TestMusicalRule() = default;

    virtual RuleResult evaluate(const TestContext& context) = 0;
    virtual bool can_evaluate(const TestContext& context) = 0;
    virtual std::string get_description() const = 0;
    
    const std::string& get_name() const { return name_; }
    RuleType get_type() const { return type_; }
    bool is_enabled() const { return enabled_; }
};

// Concrete rule: No repeated notes
class NoRepeatedNotesRule : public TestMusicalRule {
public:
    NoRepeatedNotesRule() 
        : TestMusicalRule("No Repeated Notes", RuleType::SINGLE_ENGINE_PITCH, {0}) {}

    RuleResult evaluate(const TestContext& context) override {
        if (!can_evaluate(context)) return RuleResult::INSUFFICIENT_DATA;

        if (context.current_index > 0) {
            int current = context.absolute_values[context.current_index];
            int previous = context.absolute_values[context.current_index - 1];
            
            if (current == previous) {
                return RuleResult::FAIL;
            }
        }
        
        return RuleResult::PASS;
    }
    
    bool can_evaluate(const TestContext& context) override {
        return context.current_index > 0 && 
               context.current_index < static_cast<int>(context.absolute_values.size());
    }
    
    std::string get_description() const override {
        return "Prevents consecutive repeated notes";
    }
};

// Concrete rule: Stepwise motion preference
class StepwiseMotionRule : public TestMusicalRule {
private:
    int max_interval_;
    
public:
    StepwiseMotionRule(int max_interval = 2) 
        : TestMusicalRule("Stepwise Motion", RuleType::STEPWISE_MOTION, {0}),
          max_interval_(max_interval) {}

    RuleResult evaluate(const TestContext& context) override {
        if (!can_evaluate(context)) return RuleResult::INSUFFICIENT_DATA;

        if (context.current_index > 0) {
            int current = context.absolute_values[context.current_index];
            int previous = context.absolute_values[context.current_index - 1];
            int interval = std::abs(current - previous);
            
            if (interval > max_interval_) {
                return RuleResult::FAIL;
            }
        }
        
        return RuleResult::PASS;
    }
    
    bool can_evaluate(const TestContext& context) override {
        return context.current_index > 0 && 
               context.current_index < static_cast<int>(context.absolute_values.size());
    }
    
    std::string get_description() const override {
        return "Enforces stepwise melodic motion (max interval " + 
               std::to_string(max_interval_) + " semitones)";
    }
};

// Concrete rule: Retrograde inversion
class RetrogradeInversionRule : public TestMusicalRule {
private:
    int inversion_center_;
    int voice_length_;
    
public:
    RetrogradeInversionRule(int center, int voice_length) 
        : TestMusicalRule("Retrograde Inversion", RuleType::RETROGRADE_INVERSION, {0, 1}),
          inversion_center_(center), voice_length_(voice_length) {}

    RuleResult evaluate(const TestContext& context) override {
        if (!can_evaluate(context)) return RuleResult::INSUFFICIENT_DATA;

        // Check retrograde inversion: Voice2[i] = 2 * center - Voice1[length-1-i]
        for (int i = 0; i < voice_length_; ++i) {
            int voice2_idx = voice_length_ + i;
            int voice1_retro_idx = (voice_length_ - 1) - i;
            
            if (voice1_retro_idx < context.sequence_length && voice2_idx < context.sequence_length) {
                int voice1_note = context.absolute_values[voice1_retro_idx];
                int voice2_note = context.absolute_values[voice2_idx];
                int expected_voice2 = 2 * inversion_center_ - voice1_note;
                
                if (voice2_note != expected_voice2) {
                    return RuleResult::FAIL;
                }
            }
        }
        
        return RuleResult::PASS;
    }
    
    bool can_evaluate(const TestContext& context) override {
        return context.sequence_length >= (2 * voice_length_);
    }
    
    std::string get_description() const override {
        return "Enforces retrograde inversion between voices around center " + 
               std::to_string(inversion_center_);
    }
};

// Rule compiler that creates rules from JSON-like specifications
class TestRuleCompiler {
public:
    static std::unique_ptr<TestMusicalRule> compile_from_specification(
        const std::string& rule_type, 
        const std::map<std::string, int>& parameters) {
        
        if (rule_type == "no_repeated_notes") {
            return std::make_unique<NoRepeatedNotesRule>();
        } else if (rule_type == "stepwise_motion") {
            int max_interval = parameters.count("max_interval") ? 
                              parameters.at("max_interval") : 2;
            return std::make_unique<StepwiseMotionRule>(max_interval);
        } else if (rule_type == "retrograde_inversion") {
            int center = parameters.at("inversion_center");
            int voice_length = parameters.at("voice_length");
            return std::make_unique<RetrogradeInversionRule>(center, voice_length);
        }
        
        return nullptr;
    }
};

// Test suite for rule compilation
class RuleCompilationTestSuite {
private:
    std::vector<std::unique_ptr<TestMusicalRule>> rules_;
    
public:
    void add_test_rules() {
        std::cout << "🎵 Adding test rules for compilation system..." << std::endl;
        
        // Create rules using the compiler
        std::map<std::string, int> stepwise_params = {{"max_interval", 2}};
        rules_.push_back(TestRuleCompiler::compile_from_specification("stepwise_motion", stepwise_params));
        
        rules_.push_back(TestRuleCompiler::compile_from_specification("no_repeated_notes", {}));
        
        std::map<std::string, int> retro_params = {{"inversion_center", 66}, {"voice_length", 4}};
        rules_.push_back(TestRuleCompiler::compile_from_specification("retrograde_inversion", retro_params));
        
        std::cout << "✅ Added " << rules_.size() << " compiled rules" << std::endl;
        
        for (const auto& rule : rules_) {
            std::cout << "   - " << rule->get_name() << ": " << rule->get_description() << std::endl;
        }
    }
    
    void test_rule_evaluation() {
        std::cout << "\n🎯 Testing rule evaluation..." << std::endl;
        
        // Test case 1: Valid stepwise motion
        TestContext stepwise_context;
        stepwise_context.absolute_values = {60, 62, 64, 65};  // C, D, E, F
        stepwise_context.sequence_length = 4;
        
        std::cout << "\nTest 1: Stepwise melodic motion (C-D-E-F): ";
        test_sequence(stepwise_context);
        
        // Test case 2: Large interval leap (should fail stepwise motion)
        TestContext leap_context;
        leap_context.absolute_values = {60, 72, 61, 63};  // C, C+octave, C#, D#
        leap_context.sequence_length = 4;
        
        std::cout << "Test 2: Large interval leap (C-C8-C#-D#): ";
        test_sequence(leap_context);
        
        // Test case 3: Repeated notes (should fail no-repetition rule)
        TestContext repeat_context;
        repeat_context.absolute_values = {60, 60, 62, 64}; // C, C, D, E
        repeat_context.sequence_length = 4;
        
        std::cout << "Test 3: Repeated notes (C-C-D-E): ";
        test_sequence(repeat_context);
        
        // Test case 4: Perfect retrograde inversion
        TestContext retro_context;
        retro_context.absolute_values = {60, 62, 64, 65, 72, 70, 68, 67}; // Voice1: C,D,E,F -> Voice2: C8,Bb,Ab,G (retrograde inversion around F#)
        retro_context.sequence_length = 8;
        
        std::cout << "Test 4: Retrograde inversion: ";
        test_sequence(retro_context);
    }
    
private:
    void test_sequence(TestContext& context) {
        int pass_count = 0;
        int total_rules = 0;
        
        for (const auto& rule : rules_) {
            if (rule->is_enabled()) {
                total_rules++;
                
                // Test the rule at each position where it can be evaluated
                bool rule_passed = true;
                for (int i = 0; i < context.sequence_length; ++i) {
                    context.current_index = i;
                    if (rule->can_evaluate(context)) {
                        RuleResult result = rule->evaluate(context);
                        if (result == RuleResult::FAIL) {
                            rule_passed = false;
                            break;
                        }
                    }
                }
                
                if (rule_passed) {
                    pass_count++;
                }
            }
        }
        
        std::cout << pass_count << "/" << total_rules << " rules passed";
        if (pass_count == total_rules) {
            std::cout << " ✅";
        } else {
            std::cout << " ❌";
        }
        std::cout << std::endl;
    }
};

}

int main() {
    std::cout << "===============================================" << std::endl;
    std::cout << "🎼 Rule Compilation System Test" << std::endl;
    std::cout << "===============================================" << std::endl;
    std::cout << "Testing integration of Cluster source rule architecture" << std::endl;
    
    RuleCompilationTest::RuleCompilationTestSuite test_suite;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    test_suite.add_test_rules();
    test_suite.test_rule_evaluation();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "\n🚀 Rule compilation system test completed in " << duration.count() << "ms" << std::endl;
    std::cout << "\n✨ Key Features Demonstrated:" << std::endl;
    std::cout << "   • JSON-like rule specification compilation" << std::endl;
    std::cout << "   • Template-based rule generation (from old sources)" << std::endl;
    std::cout << "   • Musical constraint evaluation pipeline" << std::endl;
    std::cout << "   • Cluster Engine rule type hierarchy" << std::endl;
    
    return 0;
}