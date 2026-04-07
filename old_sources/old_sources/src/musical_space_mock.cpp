#include "musical_space_mock.hh"
#include <algorithm>
#include <random>
#include <iostream>

namespace ClusterEngine {

MusicalSpace::MusicalSpace(int num_variables, int num_voices) 
    : num_voices_(num_voices), num_engines_(0), initialized_(false), current_search_index_(0) {
    
    initialize_dual_variables(num_variables);
    setup_engine_coordination();
    
    musical_utils_ = std::make_unique<MusicalUtilities>();
    initialized_ = true;
}

MusicalSpace::MusicalSpace(const MusicalSpace& space) {
    absolute_vars_ = space.absolute_vars_;
    interval_vars_ = space.interval_vars_;
    
    engine_info_ = space.engine_info_;
    num_voices_ = space.num_voices_;
    num_engines_ = space.num_engines_;
    
    variable_domains_ = space.variable_domains_;
    active_rules_ = space.active_rules_;
    
    initialized_ = space.initialized_;
    current_search_index_ = space.current_search_index_;
    
    musical_utils_ = std::make_unique<MusicalUtilities>();
}

MockGecode::Space* MusicalSpace::copy() {
    return new MusicalSpace(*this);
}

void MusicalSpace::initialize_dual_variables(int num_variables) {
    absolute_vars_.resize(num_variables);
    interval_vars_.resize(num_variables);
    variable_domains_.resize(num_variables, DomainType::ABSOLUTE_DOMAIN);
    
    // Set default values demonstrating dual representation
    for (int i = 0; i < num_variables; ++i) {
        absolute_vars_[i] = MockGecode::IntVar(60 + i);  // Default MIDI values
        if (i == 0) {
            interval_vars_[i] = MockGecode::IntVar(0);    // First interval is 0
        } else {
            interval_vars_[i] = MockGecode::IntVar(1);    // Default stepwise motion
        }
    }
}

void MusicalSpace::setup_engine_coordination() {
    engine_info_.clear();
    
    for (int voice = 0; voice < num_voices_; ++voice) {
        int rhythm_engine = num_engines_++;
        int pitch_engine = num_engines_++;
        
        engine_info_.emplace_back(rhythm_engine, EngineType::RHYTHM_ENGINE, pitch_engine, voice, 
                                 DomainType::ABSOLUTE_DOMAIN);
        engine_info_.emplace_back(pitch_engine, EngineType::PITCH_ENGINE, rhythm_engine, voice, 
                                 DomainType::ABSOLUTE_DOMAIN);
    }
    
    if (num_voices_ > 1) {
        engine_info_.emplace_back(num_engines_++, EngineType::METRIC_ENGINE, -1, -1, 
                                 DomainType::ABSOLUTE_DOMAIN);
    }
}

void MusicalSpace::add_engine(EngineType type, int voice, DomainType domain_type) {
    engine_info_.emplace_back(num_engines_++, type, -1, voice, domain_type);
}

void MusicalSpace::set_engine_partner(int engine1_id, int engine2_id) {
    if (engine1_id < engine_info_.size()) {
        engine_info_[engine1_id].partner_engine_id = engine2_id;
    }
    if (engine2_id < engine_info_.size()) {
        engine_info_[engine2_id].partner_engine_id = engine1_id;
    }
}

void MusicalSpace::initialize_domains(const std::vector<int>& uniform_domain, DomainType type) {
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        variable_domains_[i] = type;
    }
    std::cout << "✅ Initialized uniform domains with " << uniform_domain.size() << " values" << std::endl;
}

void MusicalSpace::initialize_domains(const std::vector<std::vector<int>>& domains, 
                                     const std::vector<DomainType>& types) {
    if (domains.size() != absolute_vars_.size() || types.size() != absolute_vars_.size()) {
        throw std::invalid_argument("Domain and type vectors must match variable count");
    }
    
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        variable_domains_[i] = types[i];
    }
    std::cout << "✅ Initialized dynamic domains for " << domains.size() << " variables" << std::endl;
}

int MusicalSpace::get_absolute_value(int var_idx) const {
    if (var_idx >= 0 && var_idx < absolute_vars_.size()) {
        return absolute_vars_[var_idx].val();
    }
    return 0;
}

int MusicalSpace::get_interval_value(int var_idx) const {
    if (var_idx >= 0 && var_idx < interval_vars_.size()) {
        return interval_vars_[var_idx].val();
    }
    return 0;
}

int MusicalSpace::get_basic_value(int var_idx) const {
    return get_absolute_value(var_idx);
}

std::vector<int> MusicalSpace::get_absolute_sequence(int start_idx, int length) const {
    std::vector<int> sequence;
    sequence.reserve(length);
    
    for (int i = start_idx; i < start_idx + length && i < absolute_vars_.size(); ++i) {
        sequence.push_back(get_absolute_value(i));
    }
    
    return sequence;
}

std::vector<int> MusicalSpace::get_interval_sequence(int start_idx, int length) const {
    std::vector<int> sequence;
    sequence.reserve(length);
    
    for (int i = start_idx; i < start_idx + length && i < interval_vars_.size(); ++i) {
        sequence.push_back(get_interval_value(i));
    }
    
    return sequence;
}

void MusicalSpace::post_musical_rule(std::shared_ptr<MusicalRule> rule) {
    active_rules_.push_back(rule);
    std::cout << "✅ Posted musical rule: " << rule->get_description() << std::endl;
}

void MusicalSpace::post_coordination_constraints() {
    std::cout << "✅ Posted engine coordination constraints" << std::endl;
}

void MusicalSpace::post_interval_calculation_constraints() {
    // Ensure interval[i] = absolute[i] - absolute[i-1]
    for (int i = 1; i < absolute_vars_.size() && i < interval_vars_.size(); ++i) {
        int calculated_interval = absolute_vars_[i].val() - absolute_vars_[i-1].val();
        interval_vars_[i] = MockGecode::IntVar(calculated_interval);
        interval_vars_[i].assign(calculated_interval);
    }
    std::cout << "✅ Posted interval calculation constraints" << std::endl;
}

EngineType MusicalSpace::get_engine_type(int engine_id) const {
    if (engine_id >= 0 && engine_id < engine_info_.size()) {
        return engine_info_[engine_id].type;
    }
    return EngineType::PITCH_ENGINE;
}

int MusicalSpace::get_engine_partner(int engine_id) const {
    if (engine_id >= 0 && engine_id < engine_info_.size()) {
        return engine_info_[engine_id].partner_engine_id;
    }
    return -1;
}

int MusicalSpace::get_engine_voice(int engine_id) const {
    if (engine_id >= 0 && engine_id < engine_info_.size()) {
        return engine_info_[engine_id].voice;
    }
    return -1;
}

void MusicalSpace::set_musical_utilities(std::unique_ptr<MusicalUtilities> utils) {
    musical_utils_ = std::move(utils);
}

MusicalUtilities* MusicalSpace::get_musical_utilities() const {
    return musical_utils_.get();
}

bool MusicalSpace::is_complete_solution() const {
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        if (!absolute_vars_[i].assigned()) {
            return false;
        }
    }
    return true;
}

std::vector<DualCandidate> MusicalSpace::extract_solution() const {
    std::vector<DualCandidate> solution;
    solution.reserve(absolute_vars_.size());
    
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        int abs_val = absolute_vars_[i].val();
        int int_val = interval_vars_[i].val();
        solution.emplace_back(abs_val, int_val);
    }
    
    return solution;
}

void MusicalSpace::print_musical_solution() const {
    std::cout << "🎵 Musical Solution:" << std::endl;
    std::cout << "Variables: " << absolute_vars_.size() << std::endl;
    std::cout << "Voices: " << num_voices_ << std::endl;
    std::cout << "Engines: " << num_engines_ << std::endl;
    
    auto solution = extract_solution();
    for (size_t i = 0; i < solution.size(); ++i) {
        std::cout << "  Var " << i << ": absolute=" << solution[i].absolute_value 
                  << " interval=" << solution[i].interval_value << std::endl;
    }
}

MockGecode::IntVar MusicalSpace::get_absolute_var(int idx) const {
    if (idx >= 0 && idx < absolute_vars_.size()) {
        return absolute_vars_[idx];
    }
    return MockGecode::IntVar(0);
}

MockGecode::IntVar MusicalSpace::get_interval_var(int idx) const {
    if (idx >= 0 && idx < interval_vars_.size()) {
        return interval_vars_[idx];
    }
    return MockGecode::IntVar(0);
}

std::vector<int> MusicalSpace::get_engine_variable_indices(int engine_id) const {
    std::vector<int> indices;
    
    int vars_per_engine = absolute_vars_.size() / num_engines_;
    int start_idx = engine_id * vars_per_engine;
    int end_idx = std::min(start_idx + vars_per_engine, (int)absolute_vars_.size());
    
    for (int i = start_idx; i < end_idx; ++i) {
        indices.push_back(i);
    }
    
    return indices;
}

bool MusicalSpace::solve_with_constraints() {
    std::cout << "🔍 Mock constraint satisfaction solving..." << std::endl;
    
    // Check all posted musical rules
    std::vector<MusicalCandidate> candidates;
    candidates.reserve(absolute_vars_.size());
    
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        candidates.emplace_back(get_absolute_value(i), get_interval_value(i));
    }
    
    bool all_satisfied = true;
    
    // Create musical rule context with mock engines
    std::vector<MusicalEngine*> mock_engines; // Empty for mock
    ClusterEngine::LinearSolution mock_linear_solution; // Default constructed for mock
    
    for (const auto& rule : active_rules_) {
        MusicalRuleContext context(0, 0, candidates, mock_engines, mock_linear_solution);
        RuleTestResult result = rule->test_rule(context);
        
        if (!result.passed) {
            std::cout << "❌ Rule failed: " << rule->get_description() << " (" << result.failure_reason << ")" << std::endl;
            all_satisfied = false;
        } else {
            std::cout << "✅ Rule passed: " << rule->get_description() << std::endl;
        }
    }
    
    if (!all_satisfied) {
        std::cout << "🔧 Generating new solution to satisfy constraints..." << std::endl;
        generate_mock_solution();
        
        // Re-check after generating new solution
        candidates.clear();
        for (int i = 0; i < absolute_vars_.size(); ++i) {
            candidates.emplace_back(get_absolute_value(i), get_interval_value(i));
        }
        
        all_satisfied = true;
        MusicalRuleContext new_context(0, 0, candidates, mock_engines, mock_linear_solution);
        
        for (const auto& rule : active_rules_) {
            RuleTestResult result = rule->test_rule(new_context);
            if (!result.passed) {
                all_satisfied = false;
                break;
            }
        }
    }
    
    return all_satisfied;
}

void MusicalSpace::generate_mock_solution() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> pitch_dist(60, 72);  // C4 to C5
    std::uniform_int_distribution<> interval_dist(-2, 2); // Small intervals
    
    // Generate solution that respects musical constraints
    for (int i = 0; i < absolute_vars_.size(); ++i) {
        if (i == 0) {
            int start_pitch = pitch_dist(gen);
            absolute_vars_[i].assign(start_pitch);
            interval_vars_[i].assign(0);
        } else {
            int interval = interval_dist(gen);
            int new_pitch = absolute_vars_[i-1].val() + interval;
            
            // Keep in reasonable range
            new_pitch = std::max(60, std::min(72, new_pitch));
            
            absolute_vars_[i].assign(new_pitch);
            interval_vars_[i].assign(interval);
        }
    }
    
    std::cout << "✅ Generated mock solution with dual representation" << std::endl;
}

} // namespace ClusterEngine