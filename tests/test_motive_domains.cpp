#include "gecode_cluster_integration.hh"
#include "motive_domain_parser.hh"
#include "measure_count_plan.hh"
#include "harmonic_domain_parser.hh"
#include "max_msp_solver_wrapper.hh"
#include "cross_voice_rules.hh"
#include "repetition_rules.hh"
#include "tendency_tone_rules.hh"
#include "timepoint_rules.hh"
#include <chrono>
#include <fstream>
#include <thread>
#include <iostream>
#include <stdexcept>
#include <set>
#include <cstdio>
#include <unistd.h>

using namespace Gecode;
using namespace GecodeClusterIntegration;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::unique_ptr<IntegratedMusicalSpace> make_space() {
    auto space = std::make_unique<IntegratedMusicalSpace>(
        5, 1, VariableBranchingStrategy::INPUT_ORDER, ValueSelectionStrategy::MIN,
        std::vector<std::vector<int>>{{60, 61, 62, 64, 67}},
        std::vector<std::vector<int>>{{-1, 1, 2, 4}}, 123);
    space->post_motive_domains({{0, false, {{60, 61}, {67, 64, 62}}},
                               {0, true, {{4, 1, -1}, {2, 2}}}});
    return space;
}

void check_metadata(const nlohmann::json& solution, int event_count) {
    std::map<std::pair<int, std::string>, std::vector<int>> components;
    std::map<std::pair<int, std::string>, int> onsets;
    for (const auto& selection : solution.at("selected_motives")) {
        const int voice = selection.at("voice");
        const std::string component = selection.at("component");
        const auto key = std::make_pair(voice, component);
        auto& flat = components[key];
        require(selection.at("start_event") == flat.size(), "Metadata gap/overlap");
        require(selection.at("onset_ticks") == onsets[key], "Metadata onset");
        const auto values = selection.at("values").get<std::vector<int>>();
        flat.insert(flat.end(), values.begin(), values.end());
        require(selection.at("end_event") == flat.size(), "Metadata boundary");
        onsets[key] += selection.at("duration_ticks").get<int>();
    }
    for (const auto& component : components) {
        require(component.second.size() == static_cast<size_t>(event_count), "Complete metadata coverage");
        const int voice = component.first.first;
        const auto rhythms = solution.at("voice_rhythm_ticks")[voice].get<std::vector<int>>();
        const auto pitches = solution.at("voice_solutions")[voice].get<std::vector<int>>();
        for (int event = 0; event < event_count; ++event) {
            if (component.first.second == "rhythm") require(component.second[event] == rhythms[event], "Rhythm metadata");
            else if (rhythms[event] > 0) require(component.second[event] == pitches[event], "Pitch metadata");
            else require(pitches[event] == -1, "Rest projection");
        }
    }
}

MaxMSPWrapper::SolveResult solve_wrapper(MaxMSPWrapper::AsyncSolverWrapper& wrapper,
                                       const nlohmann::json& cfg, bool expect_solution = true) {
    std::string error;
    require(wrapper.configure_from_json(cfg.dump(), error), "Wrapper configure: " + error);
    require(wrapper.solve_async(error), "Wrapper solve: " + error);
    MaxMSPWrapper::SolveResult result;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!wrapper.take_result(result)) {
        if (std::chrono::steady_clock::now() > deadline) {
            wrapper.request_cancel();
            throw std::runtime_error("Wrapper timeout");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(result.found_solution == expect_solution, "Unexpected wrapper satisfiability: " + result.message);
    return result;
}

void check_exhaustive_timing_equivalence(bool exact_fill, bool melodic_step = false,
        const std::string& pattern = "", bool absolute_intervals = false, const std::string& direction = "both") {
    const std::vector<std::vector<int>> pitch_motives{{60}, {62}, {60}, {60, 62}, {64}};
    const std::vector<std::vector<int>> rhythm_motives{{1}, {-1}, {2, 1}};
    std::set<std::string> expected;
    std::function<void(std::vector<int>, std::vector<int>, int)> enumerate_rhythm;
    enumerate_rhythm = [&](std::vector<int> rhythms, std::vector<int> rhythm_ids, int ticks) {
        if (!rhythms.empty() && (!exact_fill || ticks == 3)) {
            std::function<void(std::vector<int>, std::vector<int>)> enumerate_pitch;
            enumerate_pitch = [&](std::vector<int> pitches, std::vector<int> pitch_ids) {
                if (pitches.size() == rhythms.size()) {
                    if (melodic_step)
                        for (size_t event = 1; event < pitches.size(); ++event)
                            if (rhythms[event - 1] > 0 && rhythms[event] > 0 &&
                                std::abs(pitches[event] - pitches[event - 1]) > 1) return;
                    if (!pattern.empty()) {
                        for (size_t event = 2; event < pitches.size(); ++event) {
                            if (rhythms[event - 2] <= 0 || rhythms[event - 1] <= 0 || rhythms[event] <= 0) continue;
                            const int previous = pitches[event - 1] - pitches[event - 2];
                            const int current = pitches[event] - pitches[event - 1];
                            if (pattern == "r-no-interval-repetition" &&
                                (absolute_intervals ? std::abs(previous) == std::abs(current) : previous == current)) return;
                            if (pattern == "r-no-consecutive-equal" && previous == 0 && current == 0) return;
                            if (pattern == "r-no-consecutive-direction" &&
                                ((direction != "descending" && previous > 0 && current > 0) ||
                                 (direction != "ascending" && previous < 0 && current < 0))) return;
                        }
                    }
                    expected.insert(nlohmann::json({{"pitches", pitches}, {"rhythms", rhythms},
                        {"pitch_ids", pitch_ids}, {"rhythm_ids", rhythm_ids}}).dump());
                    return;
                }
                for (size_t candidate = 0; candidate < pitch_motives.size(); ++candidate) {
                    if (pitches.size() + pitch_motives[candidate].size() > rhythms.size()) continue;
                    auto next_pitches = pitches;
                    next_pitches.insert(next_pitches.end(), pitch_motives[candidate].begin(), pitch_motives[candidate].end());
                    auto next_ids = pitch_ids;
                    next_ids.push_back(static_cast<int>(candidate));
                    enumerate_pitch(std::move(next_pitches), std::move(next_ids));
                }
            };
            enumerate_pitch({}, {});
        }
        for (size_t candidate = 0; candidate < rhythm_motives.size(); ++candidate) {
            int duration = 0;
            for (int value : rhythm_motives[candidate]) duration += std::abs(value);
            if (ticks + duration > 3) continue;
            auto next_rhythms = rhythms;
            next_rhythms.insert(next_rhythms.end(), rhythm_motives[candidate].begin(), rhythm_motives[candidate].end());
            auto next_ids = rhythm_ids;
            next_ids.push_back(static_cast<int>(candidate));
            enumerate_rhythm(std::move(next_rhythms), std::move(next_ids), ticks + duration);
        }
    };
    enumerate_rhythm({}, {}, 0);

    for (bool shared_timing : {false, true}) {
        auto root = std::make_unique<IntegratedMusicalSpace>(3, 1,
            VariableBranchingStrategy::INPUT_ORDER, ValueSelectionStrategy::MIN,
            std::vector<std::vector<int>>{{60, 62, 64}}, std::vector<std::vector<int>>{{-1, 1, 2}}, 3, exact_fill, 123);
        root->post_motive_domains({{0, false, pitch_motives}, {0, true, rhythm_motives}});
        if (melodic_step) {
            DynamicRules::ConstraintContext context(root.get(), &root->get_absolute_vars(),
                &root->get_rhythm_vars(), 1, 3);
            DynamicRules::compile_melodic_step({0}, 1, false, "exhaustive-step")->execute(context);
        }
        if (!pattern.empty()) {
            DynamicRules::ConstraintContext context(root.get(), &root->get_absolute_vars(), &root->get_rhythm_vars(), 1, 3);
            DynamicRules::compile_measure_melodic_pattern({0}, pattern, "exhaustive-pattern", false,
                2, absolute_intervals, direction)->execute(context);
        }
        if (shared_timing) {
            root->initialize_event_timing();
            const auto count = root->get_timing_variable_count();
            root->initialize_event_timing();
            require(root->get_timing_variable_count() == count, "Timing initialization is idempotent");
        }
        DFS<IntegratedMusicalSpace> search(root.get());
        root.reset();
        std::set<std::string> actual;
        while (std::unique_ptr<IntegratedMusicalSpace> solution{search.next()}) {
            const int count = solution->get_active_event_count(0);
            std::vector<int> pitches, pitch_ids, rhythm_ids;
            for (int event = 0; event < count; ++event) pitches.push_back(solution->get_absolute_vars()[event].val());
            const auto rhythms = solution->get_rhythm_sequence_from_vars(0);
            for (const auto& motive : solution->get_selected_motives())
                (motive.rhythm ? rhythm_ids : pitch_ids).push_back(motive.motive_id);
            actual.insert(nlohmann::json({{"pitches", pitches}, {"rhythms", rhythms},
                {"pitch_ids", pitch_ids}, {"rhythm_ids", rhythm_ids}}).dump());
            if (shared_timing) {
                int onset = 0;
                for (int event = 0; event < 3; ++event) {
                    require(solution->get_event_boundary(0, event).val() == onset, "Cloned onset matches cumulative rhythm");
                    onset += std::abs(solution->get_rhythm_vars()[event].val());
                }
                require(solution->get_event_boundary(0, 3).val() == onset, "Cloned final boundary matches total time");
            }
        }
        require(!search.stopped() && actual == expected, "Complete motive-ID-sensitive solution set matches independent oracle");
    }
}

void check_cross_rhythm_equivalence() {
    const std::vector<std::vector<int>> rhythms{{1}, {-1}, {2}, {-2}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    for (bool exact : {false, true}) {
        for (const std::string mode : {"isorhythm", "abs_isorhythm", "augmentation", "diminution", "no_simultaneous_rests", "rest_complement"}) {
            for (bool explicit_index : {false, true}) {
                auto rule = nlohmann::json::parse(R"({"rule_type":"r-rhythm-rhythm","target_voices":[0,1]})");
                rule["constraint"] = mode;
                if (explicit_index) rule["indices"] = {1};
                std::set<std::string> expected;
                for (const auto& first : rhythms) for (const auto& second : rhythms) {
                    int first_total = 0, second_total = 0;
                    for (int value : first) first_total += std::abs(value);
                    for (int value : second) second_total += std::abs(value);
                    if (exact && (first_total != 2 || second_total != 2)) continue;
                    if (explicit_index && (first.size() < 2 || second.size() < 2)) continue;
                    bool valid = true;
                    for (size_t event = 0; event < std::min(first.size(), second.size()); ++event) {
                        if (explicit_index && event != 1) continue;
                        const int first_value = first[event], second_value = second[event];
                        if (mode == "isorhythm") valid = valid && first_value == second_value;
                        if (mode == "abs_isorhythm") valid = valid && std::abs(first_value) == std::abs(second_value);
                        if (mode == "augmentation") valid = valid && first_value == 2 * second_value;
                        if (mode == "diminution") valid = valid && 2 * first_value == second_value;
                        if (mode == "no_simultaneous_rests") valid = valid && (first_value > 0 || second_value > 0);
                        if (mode == "rest_complement") valid = valid && ((first_value > 0) != (second_value > 0));
                    }
                    if (valid) expected.insert(nlohmann::json::array({first, second}).dump());
                }
                auto root = std::make_unique<IntegratedMusicalSpace>(2, 2,
                    VariableBranchingStrategy::INPUT_ORDER, ValueSelectionStrategy::MIN,
                    std::vector<std::vector<int>>{{60}, {48}}, std::vector<std::vector<int>>{{-2, -1, 1, 2}, {-2, -1, 1, 2}}, 2, exact, 123);
                DynamicRules::ConstraintContext context(root.get(), &root->get_absolute_vars(), &root->get_rhythm_vars(), 2, 2);
                CrossVoiceRules::compile_rhythm(rule, 2, 2)->execute(context);
                DFS<IntegratedMusicalSpace> search(root.get());
                root.reset();
                std::set<std::string> actual;
                while (std::unique_ptr<IntegratedMusicalSpace> solution{search.next()})
                    actual.insert(nlohmann::json::array({solution->get_rhythm_sequence_from_vars(0),
                        solution->get_rhythm_sequence_from_vars(1)}).dump());
                require(!search.stopped() && actual == expected, "Cross-rhythm complete solution set matches independent oracle: " + mode);
            }
        }
    }
}

void check_cross_pitch_equivalence() {
    struct Stream { std::vector<int> pitches; std::vector<int> rhythms; };
    auto streams_for = [](const std::vector<int>& domain, bool exact) {
        std::vector<Stream> streams;
        for (int duration : {-2, -1, 1, 2}) {
            for (int pitch : domain) {
                if (!exact || std::abs(duration) == 2) streams.push_back({{pitch}, {duration}});
                if (std::abs(duration) == 1)
                    for (int next_duration : {-1, 1}) for (int next_pitch : domain)
                        if (duration <= 0 || next_duration <= 0 || std::abs(next_pitch - pitch) <= 12)
                            streams.push_back({{pitch, next_pitch}, {duration, next_duration}});
            }
        }
        return streams;
    };
    const std::vector<std::string> modes{"no_unison", "same_pitch", "voice_above", "voice_below", "exact_interval",
        "min_interval", "max_interval", "interval_range", "interval_class", "no_consecutive_fifths",
        "no_consecutive_octaves", "no_parallel_motion", "contrary_motion"};
    for (bool exact : {false, true}) for (const auto& mode : modes) for (int selection : {0, 1, 2}) {
        const bool consecutive = mode == "no_consecutive_fifths" || mode == "no_consecutive_octaves" ||
            mode == "no_parallel_motion" || mode == "contrary_motion";
        const size_t width = consecutive ? 2 : 1;
        auto rule = nlohmann::json::parse(R"({"rule_type":"r-pitch-pitch","target_voices":[0,1]})");
        rule["constraint"] = mode;
        if (mode == "interval_range") rule["parameters"] = {5, 12};
        else if (mode == "interval_class") rule["parameters"] = {0, 7};
        else if (mode == "exact_interval" || mode == "min_interval" || mode == "max_interval") rule["parameters"] = {7};
        const int selected_event = consecutive ? 0 : 1;
        if (selection == 1) rule["indices"] = {selected_event};
        if (selection == 2) { rule["stride"] = 2; rule["offset"] = selected_event; }
        std::set<std::string> expected;
        for (const auto& first : streams_for({60, 67}, exact)) for (const auto& second : streams_for({48, 60, 67}, exact)) {
            if (selection == 1 && (first.pitches.size() < selected_event + width || second.pitches.size() < selected_event + width)) continue;
            bool valid = true;
            for (size_t event = 0; event + width <= std::min(first.pitches.size(), second.pitches.size()); ++event) {
                if (selection != 0 && static_cast<int>(event) != selected_event) continue;
                bool sounded = true;
                for (size_t offset = 0; offset < width; ++offset)
                    sounded = sounded && first.rhythms[event + offset] > 0 && second.rhythms[event + offset] > 0;
                if (!sounded) continue;
                const int difference = first.pitches[event] - second.pitches[event];
                const int distance = std::abs(difference);
                const int pitch_class = (difference % 12 + 12) % 12;
                if (mode == "no_unison") valid = valid && difference != 0;
                if (mode == "same_pitch") valid = valid && difference == 0;
                if (mode == "voice_above") valid = valid && difference > 0;
                if (mode == "voice_below") valid = valid && difference < 0;
                if (mode == "exact_interval") valid = valid && difference == 7;
                if (mode == "min_interval") valid = valid && distance >= 7;
                if (mode == "max_interval") valid = valid && distance <= 7;
                if (mode == "interval_range") valid = valid && distance >= 5 && distance <= 12;
                if (mode == "interval_class") valid = valid && (pitch_class == 0 || pitch_class == 7);
                if (consecutive) {
                    const int next_difference = first.pitches[event + 1] - second.pitches[event + 1];
                    const int next_class = (next_difference % 12 + 12) % 12;
                    const int first_motion = first.pitches[event + 1] - first.pitches[event];
                    const int second_motion = second.pitches[event + 1] - second.pitches[event];
                    if (mode == "no_consecutive_fifths") valid = valid && !(pitch_class == 7 && next_class == 7);
                    if (mode == "no_consecutive_octaves") valid = valid && !(pitch_class == 0 && next_class == 0);
                    if (mode == "no_parallel_motion") valid = valid && !(first_motion * second_motion > 0);
                    if (mode == "contrary_motion") valid = valid && first_motion * second_motion < 0;
                }
            }
            if (valid) expected.insert(nlohmann::json::array({first.pitches, first.rhythms, second.pitches, second.rhythms}).dump());
        }
        auto root = std::make_unique<IntegratedMusicalSpace>(2, 2,
            VariableBranchingStrategy::INPUT_ORDER, ValueSelectionStrategy::MIN,
            std::vector<std::vector<int>>{{60, 67}, {48, 60, 67}}, std::vector<std::vector<int>>{{-2, -1, 1, 2}, {-2, -1, 1, 2}}, 2, exact, 123);
        DynamicRules::ConstraintContext context(root.get(), &root->get_absolute_vars(), &root->get_rhythm_vars(), 2, 2);
        CrossVoiceRules::compile_pitch(rule, 2, 2)->execute(context);
        DFS<IntegratedMusicalSpace> search(root.get());
        root.reset();
        std::set<std::string> actual;
        while (std::unique_ptr<IntegratedMusicalSpace> solution{search.next()}) {
            std::vector<int> first_pitches, second_pitches;
            for (int event = 0; event < solution->get_active_event_count(0); ++event)
                first_pitches.push_back(solution->get_absolute_vars()[event].val());
            for (int event = 0; event < solution->get_active_event_count(1); ++event)
                second_pitches.push_back(solution->get_absolute_vars()[2 + event].val());
            actual.insert(nlohmann::json::array({first_pitches, solution->get_rhythm_sequence_from_vars(0),
                second_pitches, solution->get_rhythm_sequence_from_vars(1)}).dump());
        }
        require(!search.stopped() && actual == expected, "Cross-pitch complete solution set matches independent oracle: " + mode);
    }
}

void check_cross_pitch_wrapper(MaxMSPWrapper::AsyncSolverWrapper& wrapper) {
    struct PitchCase {
        std::string mode;
        std::vector<int> parameters, first, second, bad_first, bad_second;
    };
    const std::vector<PitchCase> cases{
        {"no_unison", {}, {60,62}, {48,50}, {60,62}, {60,62}},
        {"same_pitch", {}, {60,62}, {60,62}, {60,62}, {48,50}},
        {"voice_above", {}, {60,62}, {48,50}, {60,62}, {72,74}},
        {"voice_below", {}, {60,62}, {72,74}, {60,62}, {48,50}},
        {"exact_interval", {7}, {67,69}, {60,62}, {67,69}, {60,61}},
        {"min_interval", {7}, {72,74}, {60,62}, {72,74}, {67,69}},
        {"max_interval", {7}, {67,69}, {60,62}, {72,74}, {60,62}},
        {"interval_range", {5,12}, {67,69}, {60,62}, {67,69}, {66,68}},
        {"interval_class", {7}, {67,69}, {60,62}, {67,69}, {60,61}},
        {"no_consecutive_fifths", {}, {67,69}, {60,64}, {67,69}, {60,62}},
        {"no_consecutive_octaves", {}, {72,74}, {60,65}, {72,74}, {60,62}},
        {"no_parallel_motion", {}, {60,62}, {67,65}, {60,62}, {67,69}},
        {"contrary_motion", {}, {60,62}, {67,65}, {60,62}, {67,69}}
    };
    auto base = nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","num_voices":2,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,62]]},"rhythm":{"motives":[["1/2","1/2"]]}},{"pitch":{"motives":[[48,50]]},"rhythm":{"motives":[["1/2","1/2"]]}}],"rules":[{"rule_type":"r-pitch-pitch","constraint":"voice_above","target_voices":[0,1]}]})");
    for (const auto& test : cases) {
        auto input = base;
        input["rules"][0]["constraint"] = test.mode;
        if (!test.parameters.empty()) input["rules"][0]["parameters"] = test.parameters;
        input["voices"][0]["pitch"]["motives"] = nlohmann::json::array({test.first});
        input["voices"][1]["pitch"]["motives"] = nlohmann::json::array({test.second});
        const auto good = solve_wrapper(wrapper, input);
        require(good.dynamic_rules_posted_ok == 1 && good.dynamic_rules_post_failed == 0, "Cross-pitch rule is enforced: " + test.mode);
        input["voices"][0]["pitch"]["motives"] = nlohmann::json::array({test.bad_first});
        input["voices"][1]["pitch"]["motives"] = nlohmann::json::array({test.bad_second});
        solve_wrapper(wrapper, input, false);
        input["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["-1/2","-1/2"]])");
        solve_wrapper(wrapper, input);
        input["voices"][0]["rhythm"] = base["voices"][0]["rhythm"];
        input["rules"][0]["heuristic"] = true;
        input["search_options"] = {{"value_order", "heuristic"}};
        solve_wrapper(wrapper, input);

        for (bool valid : {true, false}) {
            const auto& first = valid ? test.first : test.bad_first;
            const auto& second = valid ? test.second : test.bad_second;
            auto scoring = std::make_unique<IntegratedMusicalSpace>(4, 2,
                VariableBranchingStrategy::INPUT_ORDER, ValueSelectionStrategy::MIN,
                std::vector<std::vector<int>>{first, second}, std::vector<std::vector<int>>{{2}, {2}}, 4, true, 123);
            for (int event = 0; event < 2; ++event) {
                rel(*scoring, scoring->get_absolute_vars()[event], IRT_EQ, first[event]);
                rel(*scoring, scoring->get_absolute_vars()[4 + event], IRT_EQ, second[event]);
            }
            require(scoring->status() != SS_FAILED, "Cross-pitch scoring assignments are feasible");
            DynamicRules::ConstraintContext context(scoring.get(), &scoring->get_absolute_vars(), &scoring->get_rhythm_vars(), 2, 4);
            auto heuristic = CrossVoiceRules::compile_pitch(input["rules"][0], 2, 4);
            const double expected = valid ? 1.0 : -1.0;
            require(heuristic->evaluate_score(context, {0, 1, first[1]}) == expected &&
                    heuristic->evaluate_score(context, {1, 1, second[1]}) == expected,
                    "Pitch heuristic agrees with hard predicate from either voice: " + test.mode);
            require(heuristic->evaluate_score(context, {0, 2, -1}) == 0.0, "Inactive pitch tail receives no cross score");
        }
    }
    auto explicit_tail = base;
    explicit_tail["rules"][0]["indices"] = {3};
    solve_wrapper(wrapper, explicit_tail, false);
    auto stride_case = base;
    stride_case["voices"][1]["pitch"]["motives"] = nlohmann::json::parse("[[60,61]]");
    solve_wrapper(wrapper, stride_case, false);
    stride_case["rules"][0]["stride"] = 2;
    stride_case["rules"][0]["offset"] = 1;
    solve_wrapper(wrapper, stride_case);
    auto unequal = base;
    unequal["voices"][0]["pitch"]["motives"] = nlohmann::json::parse("[[72]]");
    unequal["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/1"]])");
    unequal["voices"][1]["pitch"]["motives"] = nlohmann::json::parse("[[60]]");
    const auto unequal_result = nlohmann::json::parse(solve_wrapper(wrapper, unequal).result_json);
    require(unequal_result["solutions"][0]["voice_solutions"][0].size() == 1 &&
            unequal_result["solutions"][0]["voice_solutions"][1].size() == 2,
            "Automatic cross-pitch rule does not force equal active voice counts");
        auto index_aligned = base;
        index_aligned["rules"][0]["constraint"] = "no_unison";
        index_aligned["voices"][1]["pitch"]["motives"] = nlohmann::json::parse("[[62,60]]");
        index_aligned["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/4","3/4"]])");
        index_aligned["voices"][1]["rhythm"]["motives"] = nlohmann::json::parse(R"([["3/4","1/4"]])");
        const auto aligned_result = nlohmann::json::parse(solve_wrapper(wrapper, index_aligned).result_json);
        require(aligned_result["solutions"][0]["voice_rhythm_ticks"][0] == nlohmann::json::parse("[1,3]") &&
            aligned_result["solutions"][0]["voice_rhythm_ticks"][1] == nlohmann::json::parse("[3,1]"),
            "Cross rules compare equal event indices even when onsets differ and notes overlap in score time");
}

void check_active_global_equivalence() {
    for (bool exact : {false, true}) for (bool rhythm_rule : {false, true})
        for (bool uniform : {false, true}) for (bool indexed : {false, true}) {
            std::set<std::string> expected;
            std::function<void(std::vector<int>, std::vector<int>, int)> enumerate;
            enumerate = [&](std::vector<int> pitches, std::vector<int> rhythms, int ticks) {
                if (!rhythms.empty() && (!exact || ticks == 2) && (!indexed || rhythms.size() == 2)) {
                    bool valid = true;
                    std::vector<int> selected;
                    for (size_t event = 0; event < rhythms.size(); ++event) {
                        if (!rhythm_rule && rhythms[event] < 0) continue;
                        selected.push_back(rhythm_rule ? rhythms[event] : pitches[event]);
                    }
                    for (size_t first = 0; first < selected.size(); ++first) {
                        if (uniform) valid = valid && selected[first] == (rhythm_rule ? 1 : 60);
                        for (size_t second = first + 1; second < selected.size(); ++second)
                            if (!uniform) valid = valid && selected[first] != selected[second];
                    }
                    if (valid) expected.insert(nlohmann::json::array({pitches, rhythms}).dump());
                }
                for (int duration : {-2, -1, 1, 2}) {
                    if (ticks + std::abs(duration) > 2) continue;
                    for (int pitch : {60, 62}) {
                        auto next_pitches = pitches; next_pitches.push_back(pitch);
                        auto next_rhythms = rhythms; next_rhythms.push_back(duration);
                        enumerate(next_pitches, next_rhythms, ticks + std::abs(duration));
                    }
                }
            };
            enumerate({}, {}, 0);
            auto root = std::make_unique<IntegratedMusicalSpace>(2, 1,
                VariableBranchingStrategy::INPUT_ORDER, ValueSelectionStrategy::MIN,
                std::vector<std::vector<int>>{{60,62}}, std::vector<std::vector<int>>{{-2,-1,1,2}}, 2, exact, 123);
            const std::vector<int> indices = indexed ? std::vector<int>{0,1} : std::vector<int>{};
            if (uniform) root->post_active_uniformity(0, rhythm_rule, indices, {rhythm_rule ? 1 : 60});
            else root->post_active_all_different(0, rhythm_rule, indices);
            DFS<IntegratedMusicalSpace> search(root.get()); root.reset();
            std::set<std::string> actual;
            while (std::unique_ptr<IntegratedMusicalSpace> solution{search.next()}) {
                std::vector<int> pitches;
                for (int event = 0; event < solution->get_active_event_count(0); ++event)
                    pitches.push_back(solution->get_absolute_vars()[event].val());
                actual.insert(nlohmann::json::array({pitches, solution->get_rhythm_sequence_from_vars(0)}).dump());
            }
            require(!search.stopped() && actual == expected, "Active global constraint matches exhaustive oracle");
        }
}

void check_active_reverse_equivalence() {
    struct Stream { std::vector<int> pitches; std::vector<int> rhythms; };
    for (bool exact : {false, true}) for (bool inversion : {false, true}) {
        std::vector<Stream> streams;
        for (int duration : {-2,-1,1,2}) for (int pitch : {60,62}) {
            if (!exact || std::abs(duration) == 2) streams.push_back({{pitch},{duration}});
            if (std::abs(duration) == 1)
                for (int next : {-1,1}) for (int next_pitch : {60,62})
                    streams.push_back({{pitch,next_pitch},{duration,next}});
        }
        std::set<std::string> expected;
        for (const auto& first : streams) for (const auto& second : streams) {
            if (first.pitches.size() != second.pitches.size()) continue;
            bool valid = true;
            for (size_t event = 0; event < first.pitches.size(); ++event) {
                const size_t source = first.pitches.size() - 1 - event;
                valid = valid && ((first.rhythms[source] > 0) == (second.rhythms[event] > 0));
                if (second.rhythms[event] > 0)
                    valid = valid && second.pitches[event] == (inversion ? 122 - first.pitches[source] : first.pitches[source]);
            }
            if (valid) expected.insert(nlohmann::json::array({first.pitches,first.rhythms,second.pitches,second.rhythms}).dump());
        }
        auto root = std::make_unique<IntegratedMusicalSpace>(2, 2,
            VariableBranchingStrategy::INPUT_ORDER, ValueSelectionStrategy::MIN,
            std::vector<std::vector<int>>{{60,62},{60,62}}, std::vector<std::vector<int>>{{-2,-1,1,2},{-2,-1,1,2}}, 2, exact, 123);
        root->post_active_reverse(0, 1, inversion, 61);
        DFS<IntegratedMusicalSpace> search(root.get()); root.reset();
        std::set<std::string> actual;
        while (std::unique_ptr<IntegratedMusicalSpace> solution{search.next()}) {
            std::vector<int> first, second;
            for (int event = 0; event < solution->get_active_event_count(0); ++event) first.push_back(solution->get_absolute_vars()[event].val());
            for (int event = 0; event < solution->get_active_event_count(1); ++event) second.push_back(solution->get_absolute_vars()[2 + event].val());
            actual.insert(nlohmann::json::array({first,solution->get_rhythm_sequence_from_vars(0),second,solution->get_rhythm_sequence_from_vars(1)}).dump());
        }
        require(!search.stopped() && actual == expected, "Active retrograde/inversion matches the exhaustive musical oracle");
    }
}

void check_global_wrapper(MaxMSPWrapper::AsyncSolverWrapper& wrapper) {
    auto base = nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","num_voices":2,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,62]]},"rhythm":{"motives":[["1/2","1/2"]]}},{"pitch":{"motives":[[62,60]]},"rhythm":{"motives":[["1/2","1/2"]]}}],"rules":[]})");
    for (const std::string alias : {"r-one-voice","r-pitches-one-engine","r-pitches-all-different","r-twelve-tone-voice1"}) {
        auto input = base;
        input["rules"] = nlohmann::json::array({{{"rule_type",alias},{"constraint","all_different"},
            {"constraint_function",{{"type","builtin"},{"function","all_different"},{"parameters",nlohmann::json::array()}}},
            {"target_voices",{0}},{"target_component","pitch"}}});
        solve_wrapper(wrapper, input);
        input["voices"][0]["pitch"]["motives"] = nlohmann::json::parse("[[60,60]]");
        solve_wrapper(wrapper, input, false);
        input["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/2","-1/2"]])");
        solve_wrapper(wrapper, input);
    }
    for (const std::string alias : {"r-uniformity","r-rhythmic-uniformity"}) {
        auto input = base;
        input["rules"] = nlohmann::json::array({{{"rule_type",alias},
            {"constraint_function",{{"type","builtin"},{"function","equal_values"},{"parameters",{"1/2"}}}},
            {"target_voices",{0}},{"target_component","rhythm"}}});
        solve_wrapper(wrapper, input);
        input["rules"][0].erase("constraint_function");
        input["rules"][0]["constraint"] = "equal_values"; input["rules"][0]["parameters"] = {"1/2"};
        solve_wrapper(wrapper, input);
        input["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["3/4","1/4"],["1/2","3/4"]])");
        solve_wrapper(wrapper, input, false);
    }
    for (const std::string alias : {"r-palindrome-voice","r-palindrome-voice2","r-cross-voice-retrograde-inversion"}) {
        auto input = base;
        const bool inversion = alias == "r-cross-voice-retrograde-inversion";
        input["rules"] = nlohmann::json::array({{{"rule_type",alias},
            {"constraint_function",{{"type","builtin"},{"function",inversion ? "retrograde_inversion_relationship" : "palindrome_of_engine"},{"parameters",{61}}}},
            {"target_voices",{0,1}},{"target_component","pitch"}}});
        if (inversion) input["voices"][1]["pitch"]["motives"] = nlohmann::json::parse("[[60,62]]");
        solve_wrapper(wrapper, input);
        if (inversion) {
            input["rules"][0].erase("constraint_function");
            input["rules"][0]["parameters"] = {61};
            solve_wrapper(wrapper, input);
            auto invalid_center = input;
            invalid_center["rules"][0]["parameters"] = {"61"};
            invalid_center["voices"][1]["pitch"]["motives"] = nlohmann::json::parse("[[68,70]]");
            const auto invalid_result = solve_wrapper(wrapper, invalid_center, false);
            require(invalid_result.message.find("numeric MIDI") != std::string::npos,
                    "String center cannot silently use default center 65");
        }
        input["voices"][1]["pitch"]["motives"] = nlohmann::json::parse("[[60,60]]");
        solve_wrapper(wrapper, input, false);
        input["voices"][1]["pitch"]["motives"] = nlohmann::json::parse("[[60]]");
        input["voices"][1]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/1"]])");
        solve_wrapper(wrapper, input, false);
    }
}

void check_repetition_equivalence() {
    for (bool exact : {false,true}) for (const std::string relation : {"exact","transposed","rhythm_preserving","contour_preserving","diatonic_sequence"}) {
        std::set<std::string> expected;
        std::function<void(std::vector<int>,std::vector<int>,int)> enumerate;
        enumerate = [&](std::vector<int> pitches, std::vector<int> rhythms, int ticks) {
            const bool contour = relation == "contour_preserving";
            if (rhythms.size() >= (contour ? 3U : 2U) && (!exact || ticks == 3)) {
                bool valid = true;
                const int pairs = contour ? 2 : 1;
                for (int pair = 0; pair < pairs; ++pair) {
                    const int source = pair, target = pair + 1;
                    if (relation == "rhythm_preserving") valid = valid && rhythms[source] == rhythms[target];
                    else {
                        valid = valid && ((rhythms[source] > 0) == (rhythms[target] > 0));
                        if (rhythms[source] > 0 && rhythms[target] > 0) {
                            if (relation == "exact") valid = valid && pitches[source] == pitches[target];
                            if (relation == "transposed" || relation == "diatonic_sequence")
                                valid = valid && pitches[target] == pitches[source] + 2;
                        }
                    }
                }
                if (contour && rhythms[0] > 0 && rhythms[1] > 0 && rhythms[2] > 0) {
                    auto direction = [](int interval) { return (interval > 0) - (interval < 0); };
                    valid = valid && direction(pitches[1]-pitches[0]) == direction(pitches[2]-pitches[1]);
                }
                if (valid) expected.insert(nlohmann::json::array({pitches,rhythms}).dump());
            }
            for (int duration : {-3,-2,-1,1,2,3}) {
                if (ticks + std::abs(duration) > 3) continue;
                for (int pitch : {60,62,64}) {
                    auto next_pitches = pitches; next_pitches.push_back(pitch);
                    auto next_rhythms = rhythms; next_rhythms.push_back(duration);
                    enumerate(next_pitches,next_rhythms,ticks+std::abs(duration));
                }
            }
        };
        enumerate({}, {}, 0);
        auto root = std::make_unique<IntegratedMusicalSpace>(3, 1,
            VariableBranchingStrategy::INPUT_ORDER, ValueSelectionStrategy::MIN,
            std::vector<std::vector<int>>{{60,62,64}}, std::vector<std::vector<int>>{{-3,-2,-1,1,2,3}}, 3, exact, 123);
        DynamicRules::ConstraintContext context(root.get(), &root->get_absolute_vars(), &root->get_rhythm_vars(), 1, 3, nullptr, 4);
        RepetitionRules::RepetitionParams params;
        params.id = "oracle-repetition"; params.relation = relation; params.target_voices = {0};
        params.source_positions = relation == "contour_preserving" ? std::vector<int>{0,1} : std::vector<int>{0};
        params.target_positions = relation == "contour_preserving" ? std::vector<int>{1,2} : std::vector<int>{1};
        params.transpose_semitones = 2; params.sequence_step = 1;
        MusicalConstraintSolver::SolverConfig::HarmonicConfig harmony;
        HarmonicDomainParser::parse(nlohmann::json::parse(R"({"harmonic_domain":[{"beat":0,"degree":"I","key":"C","mode":"major"}]})"), harmony, 3, 4);
        RepetitionRules::post_repetition_constraint(context, params, harmony);
        DFS<IntegratedMusicalSpace> search(root.get()); root.reset();
        std::set<std::string> actual;
        while (std::unique_ptr<IntegratedMusicalSpace> solution{search.next()}) {
            std::vector<int> pitches;
            for (int event=0; event<solution->get_active_event_count(0); ++event) pitches.push_back(solution->get_absolute_vars()[event].val());
            actual.insert(nlohmann::json::array({pitches,solution->get_rhythm_sequence_from_vars(0)}).dump());
        }
        require(!search.stopped() && actual == expected, "Repetition matches independent exact/pad oracle: " + relation);
    }
}

void check_repetition_wrapper(MaxMSPWrapper::AsyncSolverWrapper& wrapper) {
    auto base = nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","num_voices":1,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,62,60,62]]},"rhythm":{"motives":[["1/4","1/4","1/4","1/4"]]}}],"harmonic_domain":[{"beat":0,"degree":"I","key":"C","mode":"major"}],"rules":[{"rule_type":"r-repetition","relation":"exact","source_positions":[0,1],"target_positions":[2,3],"target_voices":[0]}]})");
    for (const std::string relation : {"exact","transposed","contour_preserving","rhythm_preserving","diatonic_sequence"}) {
        auto input = base;
        input["rules"][0]["relation"] = relation;
        if (relation == "transposed") input["rules"][0]["transpose_semitones"] = 2;
        if (relation == "diatonic_sequence") input["rules"][0]["sequence_step"] = 1;
        if (relation == "transposed" || relation == "diatonic_sequence")
            input["voices"][0]["pitch"]["motives"] = nlohmann::json::parse("[[60,62,62,64]]");
        solve_wrapper(wrapper, input);
        if (relation == "rhythm_preserving")
            input["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/8","3/8","1/4","1/4"]])");
        else input["voices"][0]["pitch"]["motives"] = nlohmann::json::parse("[[60,62,65,60]]");
        solve_wrapper(wrapper, input, false);
        if (relation != "diatonic_sequence" && relation != "rhythm_preserving") {
            input["rules"][0]["heuristic"] = 1;
            input["search_options"] = {{"value_order","heuristic"}};
            solve_wrapper(wrapper, input);
        }
    }
    auto rests = base;
    rests["voices"][0]["pitch"]["motives"] = nlohmann::json::parse("[[60,62]]");
    rests["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["-1/2","-1/2"]])");
    rests["rules"][0]["source_positions"] = {0}; rests["rules"][0]["target_positions"] = {1};
    solve_wrapper(wrapper, rests);
    rests["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/2","-1/2"]])");
    solve_wrapper(wrapper, rests, false);
    auto timed_key = base;
    timed_key["voices"][0]["pitch"]["motives"] = nlohmann::json::parse("[[60,64,66]]");
    timed_key["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["3/4","1/8","1/8"]])");
    timed_key["harmonic_domain"] = nlohmann::json::parse(R"([{"beat":0,"degree":"I","key":"C","mode":"major"},{"beat":2,"degree":"I","key":"G"}])");
    timed_key["rules"][0]["relation"] = "diatonic_sequence"; timed_key["rules"][0]["sequence_step"] = 1;
    timed_key["rules"][0]["source_positions"] = {1}; timed_key["rules"][0]["target_positions"] = {2};
    solve_wrapper(wrapper, timed_key);
    timed_key["voices"][0]["pitch"]["motives"] = nlohmann::json::parse("[[60,64,65]]");
    solve_wrapper(wrapper, timed_key, false);
        timed_key["voices"][0]["pitch"]["motives"] = nlohmann::json::parse("[[60,64,62]]");
        timed_key["rules"][0]["source_positions"] = {0};
        solve_wrapper(wrapper, timed_key, false);
        auto minor = base;
        minor["harmonic_domain"] = nlohmann::json::parse(R"([{"beat":0,"degree":"i","key":"A","mode":"minor"}])");
        minor["rules"][0]["relation"] = "diatonic_sequence"; minor["rules"][0]["sequence_step"] = -1;
        minor["voices"][0]["pitch"]["motives"] = nlohmann::json::parse("[[69,72,67,71]]");
        solve_wrapper(wrapper, minor);
        minor["voices"][0]["pitch"]["motives"] = nlohmann::json::parse("[[69,72,68,71]]");
        solve_wrapper(wrapper, minor, false);
        require(RepetitionRules::diatonic_shift(60,0,0,std::numeric_limits<int>::max()) == -1 &&
            RepetitionRules::diatonic_shift(60,0,0,std::numeric_limits<int>::min()) == -1,
            "Extreme scale steps reject unrepresentable pitches without arithmetic overflow");
        auto scoring = std::make_unique<IntegratedMusicalSpace>(4, 1,
        VariableBranchingStrategy::INPUT_ORDER, ValueSelectionStrategy::MIN,
        std::vector<std::vector<int>>{{60,62,64}}, std::vector<std::vector<int>>{{1}}, 4, true, 123);
        rel(*scoring, scoring->get_absolute_vars()[0], IRT_EQ, 60);
        rel(*scoring, scoring->get_absolute_vars()[1], IRT_EQ, 62);
        rel(*scoring, scoring->get_absolute_vars()[2], IRT_EQ, 60);
        require(scoring->status() != SS_FAILED, "Repetition scoring fixture is satisfiable");
        DynamicRules::ConstraintContext context(scoring.get(), &scoring->get_absolute_vars(), &scoring->get_rhythm_vars(), 1, 4);
        auto params = RepetitionRules::resolve_params(base["rules"][0]);
        require(RepetitionRules::score_candidate(params,context,{0,3,62}) == 1.0 &&
            RepetitionRules::score_candidate(params,context,{0,3,64}) == -0.5, "Exact repetition scores matching and nonmatching pitches");
        params.relation = "transposed"; params.transpose_semitones = 2;
        require(RepetitionRules::score_candidate(params,context,{0,3,64}) == 1.0 &&
            RepetitionRules::score_candidate(params,context,{0,3,62}) == -0.5, "Transposed repetition scores the declared offset");
        params.relation = "contour_preserving";
        require(RepetitionRules::score_candidate(params,context,{0,3,62}) == 1.0 &&
            RepetitionRules::score_candidate(params,context,{0,3,60}) == -0.5, "Contour scoring includes repeated-note direction");
        auto rest_scoring = std::make_unique<IntegratedMusicalSpace>(4, 1,
        VariableBranchingStrategy::INPUT_ORDER, ValueSelectionStrategy::MIN,
        std::vector<std::vector<int>>{{60,62,64}}, std::vector<std::vector<int>>{{-1,1}}, 4, false, 123);
        rel(*rest_scoring, rest_scoring->get_rhythm_vars()[0], IRT_EQ, -1);
        rel(*rest_scoring, rest_scoring->get_rhythm_vars()[1], IRT_EQ, 1);
        rel(*rest_scoring, rest_scoring->get_rhythm_vars()[2], IRT_EQ, 1);
        rel(*rest_scoring, rest_scoring->get_rhythm_vars()[3], IRT_EQ, 0);
        rel(*rest_scoring, rest_scoring->get_absolute_vars()[0], IRT_EQ, 60);
        rel(*rest_scoring, rest_scoring->get_absolute_vars()[1], IRT_EQ, 62);
        require(rest_scoring->status() != SS_FAILED, "Rest repetition scoring fixture is satisfiable");
        DynamicRules::ConstraintContext rest_context(rest_scoring.get(), &rest_scoring->get_absolute_vars(), &rest_scoring->get_rhythm_vars(), 1, 4);
        params.relation = "exact";
        require(RepetitionRules::score_candidate(params,rest_context,{0,2,60}) == 0.0 &&
            RepetitionRules::score_candidate(params,rest_context,{0,3,62}) == 0.0,
            "Soft repetition ignores source rests and inactive targets");
    auto invalid = base;
    invalid["rules"][0]["heuristic"] = true; invalid["rules"][0]["source_positions"] = {99,1};
    std::string error;
    require(!wrapper.configure_from_json(invalid.dump(), error) && error.find("capacity") != std::string::npos,
            "Soft repetition cannot silently ignore an invalid address");
}

void check_cli_verbose_transformation() {
    nlohmann::ordered_json config = nlohmann::ordered_json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","num_voices":2,"export_json":false,"export_txt":false,"export_xml":false,"search_options":{"timeout_ms":3000},"voices":[{"pitch":{"motives":[[60,62]]},"rhythm":{"motives":[["1/2","1/2"]]}},{"pitch":{"motives":[[60,62]]},"rhythm":{"motives":[["1/2","1/2"]]}}],"rules":[]})");
    nlohmann::ordered_json rule;
    rule["rule_type"] = "r-cross-voice-retrograde-inversion";
    rule["target_voices"] = {0,1};
    rule["target_component"] = "pitch";
    rule["constraint_function"] = nlohmann::ordered_json::parse(
        R"({"type":"builtin","function":"retrograde_inversion_relationship","parameters":[61]})");
    config["rules"].push_back(rule);
    char filename[] = "/tmp/gecode-cli-transform-XXXXXX";
    const int descriptor = mkstemp(filename);
    require(descriptor >= 0, "Create temporary CLI regression config");
    close(descriptor);
    try {
        for (bool valid : {true,false}) {
            if (!valid) config["voices"][1]["pitch"]["motives"] = nlohmann::ordered_json::parse("[[60,60]]");
            {
                std::ofstream output(filename);
                output << config.dump(2);
                require(bool(output), "Write CLI regression config");
            }
            const std::string command = "bin/dynamic-solver " + std::string(filename) + " 2>&1";
            std::FILE* stream = popen(command.c_str(), "r");
            require(stream != nullptr, "Run CLI transformation regression");
            std::string output;
            char buffer[1024];
            while (std::fgets(buffer, sizeof(buffer), stream)) output += buffer;
            const int exit_code = pclose(stream);
            require(valid ? (exit_code == 0 && output.find("Found 1 solution") != std::string::npos) :
                            (exit_code != 0 && output.find("No solution found: no solutions") != std::string::npos),
                    "CLI must load a last-property verbose rule and enforce its multiline center");
        }
    } catch (...) {
        std::remove(filename);
        throw;
    }
    std::remove(filename);
}

void check_cli_solution(const nlohmann::json& config, bool expected=true) {
    char filename[]="/tmp/gecode-cli-temporal-XXXXXX";
    const int descriptor=mkstemp(filename);
    require(descriptor>=0,"Create temporal CLI regression config");
    close(descriptor);
    try {
        {
            std::ofstream output(filename);
            output<<config.dump(2);
            require(bool(output),"Write temporal CLI regression config");
        }
        const std::string command="bin/dynamic-solver "+std::string(filename)+" 2>&1";
        std::FILE* stream=popen(command.c_str(),"r");
        require(stream!=nullptr,"Run temporal CLI regression");
        std::string output;
        char buffer[1024];
        while (std::fgets(buffer,sizeof(buffer),stream)) output+=buffer;
        const int exit_code=pclose(stream);
        require(expected ? (exit_code==0 && output.find("Found 1 solution")!=std::string::npos) :
                           (exit_code!=0 && output.find("No solution found: no solutions")!=std::string::npos),
                "CLI must enforce temporal rules, not skip them or confuse UNSAT with configuration errors: "+output);
    } catch (...) {
        std::remove(filename);
        throw;
    }
    std::remove(filename);
}

void check_tendency_equivalence() {
    for (bool beat : {false,true}) for (const std::string tendency : {"leading_tone","chordal_seventh"}) {
        const std::vector<int> pitches = tendency == "leading_tone" ? std::vector<int>{67,71,72} : std::vector<int>{52,53,55};
        std::set<std::string> expected;
        for (int first : pitches) for (int second : pitches) for (int first_rhythm : {-1,1}) for (int second_rhythm : {-1,1}) {
            const int carrier = tendency == "leading_tone" ? 11 : 5;
            const int resolved = tendency == "leading_tone" ? 72 : 52;
            if (first_rhythm > 0 && first%12 == carrier && (second_rhythm < 0 || second != resolved)) continue;
            expected.insert(nlohmann::json::array({std::vector<int>{first,second},std::vector<int>{first_rhythm,second_rhythm}}).dump());
        }
        auto root = std::make_unique<IntegratedMusicalSpace>(2,1,
            VariableBranchingStrategy::INPUT_ORDER,ValueSelectionStrategy::MIN,
            std::vector<std::vector<int>>{pitches},std::vector<std::vector<int>>{{-1,1}},2,true,123);
        DynamicRules::ConstraintContext context(root.get(),&root->get_absolute_vars(),&root->get_rhythm_vars(),1,2,nullptr,4);
        auto rule = nlohmann::json::parse(R"({"rule_type":"r-tendency-tone","target_voices":[0]})");
        rule["tendency"] = tendency; rule[beat ? "beat_positions" : "positions"] = {0,1};
        MusicalConstraintSolver::SolverConfig::HarmonicConfig harmony;
        HarmonicDomainParser::parse(nlohmann::json::parse(R"({"harmonic_domain":[{"beat":0,"degree":"V7","key":"C","mode":"major"}]})"),harmony,2,4);
        TendencyToneRules::post_tendency_constraint(context,TendencyToneRules::resolve_params(rule),harmony);
        DFS<IntegratedMusicalSpace> search(root.get()); root.reset();
        std::set<std::string> actual;
        while (std::unique_ptr<IntegratedMusicalSpace> solution{search.next()}) {
            std::vector<int> values{solution->get_absolute_vars()[0].val(),solution->get_absolute_vars()[1].val()};
            actual.insert(nlohmann::json::array({values,solution->get_rhythm_sequence_from_vars(0)}).dump());
        }
        require(!search.stopped() && actual==expected,"Tendency carrier/rest semantics match complete oracle: "+tendency);
    }
}

void check_tendency_wrapper(MaxMSPWrapper::AsyncSolverWrapper& wrapper) {
    auto base = nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","num_voices":1,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[71,72]]},"rhythm":{"motives":[["1/2","1/2"]]}}],"harmonic_domain":[{"beat":0,"degree":"V7","key":"C","mode":"major"},{"beat":2,"degree":"I"}],"rules":[{"rule_type":"r-tendency-tone","tendency":"leading_tone","target_voices":[0]}]})");
    for (bool beat : {false,true}) for (const std::string tendency : {"leading_tone","chordal_seventh"}) {
        auto input=base;
        input["rules"][0]["tendency"]=tendency;
        input["rules"][0][beat ? "beat_positions" : "positions"] = beat ? nlohmann::json::parse("[1,3]") : nlohmann::json::parse("[0,1]");
        input["voices"][0]["pitch"]["motives"] = nlohmann::json::parse(tendency=="leading_tone" ? "[[71,72]]" : "[[53,52]]");
        solve_wrapper(wrapper,input);
        check_cli_solution(input);
        if (!beat) {
            auto legacy=input;
            legacy["solution_length"]=2;
            legacy.erase("measure_count"); legacy.erase("measure_plan"); legacy.erase("measure_fill_policy");
            const auto result=solve_wrapper(wrapper,legacy);
            require(result.dynamic_rules_posted_ok==1 && result.dynamic_rules_post_failed==0,
                "Integer-mode tendency still posts its carrier-resolution table");
            check_cli_solution(legacy);
        }
        input["voices"][0]["pitch"]["motives"] = nlohmann::json::parse(tendency=="leading_tone" ? "[[71,69]]" : "[[53,55]]");
        solve_wrapper(wrapper,input,false);
        check_cli_solution(input,false);
        input["voices"][0]["pitch"]["motives"] = nlohmann::json::parse(tendency=="leading_tone" ? "[[67,69]]" : "[[55,57]]");
        solve_wrapper(wrapper,input);
        check_cli_solution(input);
        input["voices"][0]["pitch"]["motives"] = nlohmann::json::parse(tendency=="leading_tone" ? "[[71,72]]" : "[[53,52]]");
        input["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/2","-1/2"]])");
        solve_wrapper(wrapper,input,false);
        check_cli_solution(input,false);
        input["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["-1/2","-1/2"]])");
        solve_wrapper(wrapper,input);
        check_cli_solution(input);
    }
    auto timed=base;
    timed["rules"][0]["tendency"]="chordal_seventh"; timed["rules"][0]["positions"]={1,2};
    timed["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,53,52]]");
    timed["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["3/4","1/8","1/8"]])");
    timed["harmonic_domain"]=nlohmann::json::parse(R"([{"beat":0,"degree":"I","key":"C","mode":"major"},{"beat":2,"degree":"V7"}])");
    solve_wrapper(wrapper,timed);
    check_cli_solution(timed);
    timed["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,53,55]]");
    solve_wrapper(wrapper,timed,false);
    check_cli_solution(timed,false);
    auto padded=base;
    padded["measure_fill_policy"]="pad"; padded["rules"][0]["beat_positions"]={0,3};
    padded["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[71]]");
    padded["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/2"]])");
    solve_wrapper(wrapper,padded,false);
    padded["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[67]]");
    solve_wrapper(wrapper,padded);
    auto inactive=base;
    inactive["rules"][0]["positions"]={0,3};
    solve_wrapper(wrapper,inactive,false);
    auto oversized=base; oversized["rules"][0]["positions"]={4294967296LL,1};
    std::string oversized_error;
    require(!wrapper.configure_from_json(oversized.dump(),oversized_error) && !oversized_error.empty(),
            "Oversized event selectors cannot wrap to event zero");
}

void check_timepoint_equivalence() {
    struct Stream { std::vector<int> pitches,rhythms; };
    std::vector<Stream> streams;
    for (int duration : {-2,-1,1,2}) for (int pitch : {60,62}) {
        streams.push_back({{pitch},{duration}});
        if (std::abs(duration)==1)
            for (int next : {-1,1}) for (int next_pitch : {60,62}) streams.push_back({{pitch,next_pitch},{duration,next}});
    }
    for (const std::string scope : {"all_timepoints","at_timepoints"}) for (bool excluded : {false,true}) {
        std::set<std::string> expected;
        for (const auto& first : streams) for (const auto& second : streams) {
            bool valid=true;
            auto pitch_at=[](const Stream& stream,int tick) {
                int onset=0;
                for (size_t event=0; event<stream.rhythms.size(); ++event) {
                    const int duration=stream.rhythms[event];
                    if (onset<=tick && tick<onset+std::abs(duration)) return duration>0 ? stream.pitches[event] : -1;
                    onset+=std::abs(duration);
                }
                return -1;
            };
            for (int tick=0; tick<2; ++tick) {
                if (scope=="at_timepoints" && tick!=1) continue;
                const int first_pitch=pitch_at(first,tick),second_pitch=pitch_at(second,tick);
                if (first_pitch>=0 && second_pitch>=0) valid=valid && (excluded ? first_pitch!=second_pitch : first_pitch==second_pitch);
            }
            if (valid) expected.insert(nlohmann::json::array({first.pitches,first.rhythms,second.pitches,second.rhythms}).dump());
        }
        auto root=std::make_unique<IntegratedMusicalSpace>(2,2,
            VariableBranchingStrategy::INPUT_ORDER,ValueSelectionStrategy::MIN,
            std::vector<std::vector<int>>{{60,62},{60,62}},std::vector<std::vector<int>>{{-2,-1,1,2},{-2,-1,1,2}},2,false,123);
        MusicalConstraintSolver::SolverConfig config;
        config.num_voices=2; config.sequence_length=2; config.rhythm_base=4; config.measure_horizon_ticks=2;
        auto rule=nlohmann::json::parse(R"({"rule_type":"r-timepoint-relationship","target_voices":[0,1],"parameters":[0],"timepoints":["1q"]})");
        rule["time_scope"]=scope; rule["function"]=excluded ? "interval_not_in_set" : "interval_in_set";
        if (scope=="all_timepoints") rule.erase("timepoints");
        DynamicRules::ConstraintContext context(root.get(),&root->get_absolute_vars(),&root->get_rhythm_vars(),2,2,nullptr,4);
        TimepointRules::compile(rule,config)->execute(context);
        DFS<IntegratedMusicalSpace> search(root.get()); root.reset();
        std::set<std::string> actual;
        while (std::unique_ptr<IntegratedMusicalSpace> solution{search.next()}) {
            std::vector<int> first,second;
            for (int event=0; event<solution->get_active_event_count(0); ++event) first.push_back(solution->get_absolute_vars()[event].val());
            for (int event=0; event<solution->get_active_event_count(1); ++event) second.push_back(solution->get_absolute_vars()[2+event].val());
            actual.insert(nlohmann::json::array({first,solution->get_rhythm_sequence_from_vars(0),second,solution->get_rhythm_sequence_from_vars(1)}).dump());
        }
        require(!search.stopped() && actual==expected,"Score-time intervals match independent sounding-pitch oracle");
    }
}

void check_timepoint_wrapper(MaxMSPWrapper::AsyncSolverWrapper& wrapper) {
    auto base=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","num_voices":2,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,62]]},"rhythm":{"motives":[["1/4","3/4"]]}},{"pitch":{"motives":[[62,60]]},"rhythm":{"motives":[["3/4","1/4"]]}}],"rules":[{"rule_type":"r-pitch-pitch","constraint":"no_unison","target_voices":[0,1]}]})");
    solve_wrapper(wrapper,base);
    base["rules"]=nlohmann::json::parse(R"([{"rule_type":"r-timepoint-relationship","function":"interval_not_in_set","parameters":[0],"target_voices":[0,1]}])");
    solve_wrapper(wrapper,base,false);
    check_cli_solution(base,false);
    base["rules"][0]["time_scope"]="at_timepoints"; base["rules"][0]["timepoints"]={"0q"};
    solve_wrapper(wrapper,base);
    check_cli_solution(base);
    base["rules"][0]["timepoints"]={"2q"}; solve_wrapper(wrapper,base,false);
    check_cli_solution(base,false);
    base["rules"][0]["timepoints"]={"3q"}; solve_wrapper(wrapper,base);
    base["rules"][0]["timepoints"]={"1q"};
    base["rules"][0].erase("function"); base["rules"][0].erase("parameters");
    base["rules"][0]["constraint"]="abs(voice[v2].pitch[i] - voice[v1].pitch[i]) not_in [0]";
    solve_wrapper(wrapper,base,false);
    check_cli_solution(base,false);
    base["rules"][0]["constraint"]="abs(voice[v2].pitch[i] - voice[v1].pitch[i]) in [0]";
    solve_wrapper(wrapper,base);
    check_cli_solution(base);
    base["rules"][0].erase("constraint"); base["rules"][0]["function"]="interval_not_in_set"; base["rules"][0]["parameters"]={0};
    base["rules"][0]["time_scope"]="on_beats"; base["rules"][0].erase("timepoints"); base["rules"][0]["beat_numbers"]={1};
    solve_wrapper(wrapper,base);
    check_cli_solution(base);
    base["rules"][0]["beat_numbers"]={3}; solve_wrapper(wrapper,base,false);
    check_cli_solution(base,false);
    base["rules"][0]["beat_numbers"]={4}; solve_wrapper(wrapper,base);
    base["rules"][0]["time_scope"]="all_timepoints"; base["rules"][0].erase("beat_numbers");
    base["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/4","-3/4"]])");
    solve_wrapper(wrapper,base);
    require(TimepointRules::parse_tick("3/2q",8,16)==3 && TimepointRules::parse_tick("1.5q",8,16)==3 &&
            TimepointRules::parse_tick(3,8,16)==3,"Fractional score units and raw integer ticks convert exactly");
    auto mixed=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":2,"measure_plan":[{"bars":1,"time_signature":"2/4"},{"bars":1,"time_signature":"3/8"}],"measure_fill_policy":"exact","num_voices":2,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,62]]},"rhythm":{"motives":[["1/2","3/8"]]}},{"pitch":{"motives":[[62]]},"rhythm":{"motives":[["7/8"]]}}],"rules":[{"rule_type":"r-timepoint-relationship","time_scope":"on_beats","beat_numbers":[3],"function":"interval_not_in_set","parameters":[0],"target_voices":[0,1]}]})");
    solve_wrapper(wrapper,mixed,false);
    check_cli_solution(mixed,false);
    mixed["rules"][0]["beat_numbers"]={2}; solve_wrapper(wrapper,mixed,false);
    mixed["rules"][0]["beat_numbers"]={1}; solve_wrapper(wrapper,mixed,false);
    mixed["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,64]]");
    solve_wrapper(wrapper,mixed);
    check_cli_solution(mixed);
    auto ordered=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","num_voices":2,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[70,60]]},"rhythm":{"motives":[["1/2","1/2"]]}},{"pitch":{"motives":[[65]]},"rhythm":{"motives":[["1/1"]]}}],"rules":[{"rule_type":"r-timepoint-relationship","function":"interval_in_set","parameters":[0,1,2,3,4,5,6,7,8,9,10,11,12],"target_voices":[0,1]}]})");
    solve_wrapper(wrapper,ordered);
    check_cli_solution(ordered);
    ordered["rules"][0]["voice_order"]="descending";
    solve_wrapper(wrapper,ordered,false);
    check_cli_solution(ordered,false);
    ordered["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[70,66]]");
    solve_wrapper(wrapper,ordered);
    check_cli_solution(ordered);
    ordered["rules"][0]["voice_order"]="ascending";
    {
        std::string error;
        require(!wrapper.configure_from_json(ordered.dump(),error) && !error.empty(),"Unknown voice_order fails explicitly");
    }
    for (const auto& invalid_rule : {
            R"({"rule_type":"r-timepoint-relationship","time_scope":"at_timepoints","timepoints":["4q"],"function":"interval_in_set","parameters":[0],"target_voices":[0,1]})",
            R"({"rule_type":"r-timepoint-relationship","time_scope":"at_timepoints","timepoints":["1/3q"],"function":"interval_in_set","parameters":[0],"target_voices":[0,1]})",
            R"({"rule_type":"r-timepoint-relationship","time_scope":"on_beats","beat_numbers":[1,99],"function":"interval_in_set","parameters":[0],"target_voices":[0,1]})",
            R"({"rule_type":"r-timepoint-relationship","constraint":"unknown(voice[v1].pitch[i]-voice[v2].pitch[i]) in [0]","target_voices":[0,1]})",
            R"({"rule_type":"r-timepoint-relationship","constraint":"abs(voice[v1].pitch[i]-voice[v2].pitch[i]) in [0.5]","target_voices":[0,1]})",
            R"({"rule_type":"r-timepoint-relationship","constraint":"abs(voice[v1].pitch[i]-voice[v2].pitch[i]) in [0,wrong]","target_voices":[0,1]})",
            R"({"rule_type":"r-timepoint-relationship","constraint":"abs(voice[0].pitch[i]-voice[1].pitch[i]) in [0]","target_voices":[1,2]})",
            R"({"rule_type":"r-timepoint-relationship","constraint":"voice[v1].pitch[i+1] == voice[v2].pitch[i]","target_voices":[0,1]})"}) {
        auto invalid=base;
        invalid["rules"]=nlohmann::json::array({nlohmann::json::parse(invalid_rule)});
        std::string error;
        require(!wrapper.configure_from_json(invalid.dump(),error) && !error.empty(),"Malformed/unsupported timepoint selectors fail explicitly");
    }
}

void check_signature_wrapper(MaxMSPWrapper::AsyncSolverWrapper& wrapper) {
    auto mixed=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":2,"measure_plan":[{"bars":1,"time_signature":"2/4"},{"bars":1,"time_signature":"3/4"}],"measure_fill_policy":"exact","num_voices":1,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60]]},"rhythm":{"motives":[["1/4"]]}}],"rules":[{"rule_type":"r-time-signature","parameters":["2/4","3/4"],"timepoints":["0q","2q"]}]})");
    const auto result=solve_wrapper(wrapper,mixed);
    check_cli_solution(mixed);
    require(result.dynamic_rules_posted_ok==1 && result.dynamic_rules_post_failed==0,"Fixed-plan signature rule is explicitly posted");
    const auto solution=nlohmann::json::parse(result.result_json)["solutions"][0];
    require(solution["score"]["measures"].size()==2 && solution["score"]["measures"][0]["numerator"]==2 &&
            solution["score"]["measures"][1]["numerator"]==3,"Plan validation preserves actual mixed-meter measures");
    auto fixed=mixed;
    fixed["rules"]=nlohmann::json::parse(R"([{"rule_type":"r-time-signature","bar_pattern_type":"fixed","bar_pattern":["2/4","3/4"]}])");
    solve_wrapper(wrapper,fixed);
    auto repeated=fixed;
    repeated["measure_count"]=4;
    repeated["measure_plan"]=nlohmann::json::parse(R"([{"bars":1,"time_signature":"2/4"},{"bars":1,"time_signature":"3/4"},{"bars":1,"time_signature":"2/4"},{"bars":1,"time_signature":"3/4"}])");
    repeated["rules"][0]["bar_pattern_type"]="repeating"; repeated["rules"][0]["bar_pattern_repetitions"]=2;
    solve_wrapper(wrapper,repeated);
    check_cli_solution(repeated);
    auto crossing=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":2,"measure_plan":[{"bars":2,"time_signature":"2/4"}],"measure_fill_policy":"exact","num_voices":1,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60]]},"rhythm":{"motives":[["1/1"]]}}],"rules":[{"rule_type":"r-time-signature","bar_pattern_type":"fixed","bar_pattern":["2/4","2/4"]}]})");
    solve_wrapper(wrapper,crossing,false);
    check_cli_solution(crossing,false);
    crossing["rules"][0]["allow_cross_barline"]=true;
    solve_wrapper(wrapper,crossing);
    check_cli_solution(crossing);
    crossing["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["-1/1"]])");
    solve_wrapper(wrapper,crossing);
    check_cli_solution(crossing);
    crossing["rules"][0]["allow_cross_barline"]=false;
    solve_wrapper(wrapper,crossing,false);
    check_cli_solution(crossing,false);
    for (const auto& rule : {
            R"({"rule_type":"r-time-signature","parameters":["2/4","4/4"],"timepoints":["0q","2q"]})",
            R"({"rule_type":"r-time-signature","parameters":["2/4","3/4"],"timepoints":["0q","1q"]})",
            R"({"rule_type":"r-time-signature","bar_pattern_type":"fixed","bar_pattern":["2/4"]})",
            R"({"rule_type":"r-time-signature","parameters":["2/4","3/4"],"indices":[0,2]})",
            R"({"rule_type":"r-time-signature","bar_pattern_type":"weighted","bar_pattern":["2/4","3/4"]})"}) {
        auto invalid=mixed; invalid["rules"]=nlohmann::json::array({nlohmann::json::parse(rule)});
        std::string error;
        require(!wrapper.configure_from_json(invalid.dump(),error) && !error.empty(),"Conflicting/unsupported meter-plan constraint fails explicitly");
    }
}

void check_measure_metric_hierarchy(MaxMSPWrapper::AsyncSolverWrapper& wrapper) {
    auto mixed=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":2,"measure_plan":[{"bars":1,"time_signature":"2/4"},{"bars":1,"time_signature":"3/8"}],"measure_fill_policy":"pad","num_voices":1,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,62,64,65,67]]},"rhythm":{"motives":[["1/4","1/4","1/8","1/8","1/8"]]}}],"rules":[{"rule_type":"r-metric-hierarchy","constraint":"no-syncopation","target_voices":[0]}]})");
    const auto valid=solve_wrapper(wrapper,mixed);
    const auto valid_rhythm=nlohmann::json::parse(valid.result_json)["solutions"][0]["voice_rhythm_ticks"][0];
    require(valid_rhythm==nlohmann::json::parse("[2,2,1,1,1]"),
            "No-syncopation uses the 2/4 and 3/8 bars' actual beat lengths");
    check_cli_solution(mixed);

        auto indexed=mixed;
        indexed["rules"][0]["indices"]={2};
        const auto indexed_result=solve_wrapper(wrapper,indexed);
        require(nlohmann::json::parse(indexed_result.result_json)["solutions"][0]["voice_rhythm_ticks"][0].size()==5,
            "An explicit hierarchy index selects an active event");
        check_cli_solution(indexed);

    mixed["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,62,64]]");
    mixed["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/4","1/4","1/4"]])");
    solve_wrapper(wrapper,mixed,false);
    check_cli_solution(mixed,false);

    mixed["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/4","1/4","-1/4"]])");
    solve_wrapper(wrapper,mixed);
    check_cli_solution(mixed);

    auto bar_period=mixed;
    bar_period["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,62,64]]");
    bar_period["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/4","1/4","1/4"]])");
    bar_period["rules"][0]["parameters"]={"beats:1"};
    solve_wrapper(wrapper,bar_period);
    check_cli_solution(bar_period);

    auto barline=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":2,"measure_plan":[{"bars":2,"time_signature":"2/4"}],"measure_fill_policy":"pad","num_voices":1,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60]]},"rhythm":{"motives":[["3/4"]]}}],"rules":[{"rule_type":"r-metric-hierarchy","constraint":"no_syncopation","target_voices":[0]}]})");
    solve_wrapper(wrapper,barline,false);
    check_cli_solution(barline,false);
    barline["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["-3/4"]])");
    solve_wrapper(wrapper,barline);
    check_cli_solution(barline);

    auto inactive=mixed;
    inactive["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60]]");
    inactive["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/4"]])");
    inactive["rules"][0]["indices"]={2};
    solve_wrapper(wrapper,inactive,false);

    auto unsupported=mixed;
    unsupported["rules"][0]["constraint"]="no_syncopation";
    unsupported["rules"][0]["heuristic"]=true;
    std::string error;
    require(!wrapper.configure_from_json(unsupported.dump(),error) && !error.empty(),
            "Heuristic metric-hierarchy modes remain fail-closed in measure mode");
}

void check_measure_metric_grids(MaxMSPWrapper::AsyncSolverWrapper& wrapper) {
    auto minimum=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"pad","num_voices":1,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60],[62]]},"rhythm":{"motives":[["1/8"],["1/4"]]}}],"rules":[{"rule_type":"r-metric-hierarchy","constraint":"min_grid","parameters":["min-grid","1/4"],"target_voices":[0]}]})");
    const auto min_result=solve_wrapper(wrapper,minimum);
    require(nlohmann::json::parse(min_result.result_json)["solutions"][0]["voice_rhythm_ticks"][0]==nlohmann::json::parse("[2]"),
            "min_grid filters durations by the requested tick multiple");
    check_cli_solution(minimum);
    minimum["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/8"]])");
    solve_wrapper(wrapper,minimum,false);
    check_cli_solution(minimum,false);

    auto duration_grid=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"pad","num_voices":1,"meter":{"time_signatures":["4/4"],"tuplets":[3],"beat_divisions":[2]},"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60],[62]]},"rhythm":{"motives":[["1/16"],["1/12"]]}}],"rules":[{"rule_type":"r-metric-hierarchy","constraint":"durations","target_voices":[0]}]})");
    const auto grid_result=solve_wrapper(wrapper,duration_grid);
    require(nlohmann::json::parse(grid_result.result_json)["solutions"][0]["voice_rhythm_ticks"][0]==nlohmann::json::parse("[4]"),
            "Duration grid uses tuplet and beat-division metadata for the planned signature");
    check_cli_solution(duration_grid);
    duration_grid["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/16"]])");
    solve_wrapper(wrapper,duration_grid,false);
    check_cli_solution(duration_grid,false);

    auto no_tuplets=duration_grid;
    no_tuplets["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/8"],["1/24"]])");
    no_tuplets["rules"][0]["parameters"]={"no-tuplets"};
    const auto no_tuplets_result=solve_wrapper(wrapper,no_tuplets);
    require(nlohmann::json::parse(no_tuplets_result.result_json)["solutions"][0]["voice_rhythm_ticks"][0]==nlohmann::json::parse("[3]"),
            "no-tuplets excludes tuplet-only grid durations while retaining native beat divisions");
    check_cli_solution(no_tuplets);

    auto unused_meter=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"pad","num_voices":1,"meter":{"time_signatures":["4/4","3/8"],"tuplets":[3],"beat_divisions":[2]},"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60]]},"rhythm":{"motives":[["1/24"]]}}],"rules":[{"rule_type":"r-metric-hierarchy","constraint":"durations","target_voices":[0]}]})");
    solve_wrapper(wrapper,unused_meter,false);
    check_cli_solution(unused_meter,false);
}

void check_measure_tuplet_starts(MaxMSPWrapper::AsyncSolverWrapper& wrapper) {
    auto mixed=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":2,"measure_plan":[{"bars":1,"time_signature":"2/4"},{"bars":1,"time_signature":"3/8"}],"measure_fill_policy":"exact","num_voices":1,"meter":{"time_signatures":["2/4","3/8"],"tuplets":[3],"beat_divisions":[2,4]},"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,62,64,65,67,69,71,72]]},"rhythm":{"motives":[["1/12","1/12","1/12","1/4","1/24","1/24","1/24","1/4"]]}}],"rules":[{"rule_type":"r-metric-hierarchy","constraint":"tuplet_on_beat_start","target_voices":[0]}]})");
    const auto result=solve_wrapper(wrapper,mixed);
    require(nlohmann::json::parse(result.result_json)["solutions"][0]["voice_rhythm_ticks"][0].size()==8,
            "Tuplet groups fill quarter and eighth-note beats in their actual mixed bars");
    check_cli_solution(mixed);

        auto backtrack=mixed;
        backtrack["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(
        R"([["1/8","1/12","1/12","1/12","1/8","1/8","1/8","1/8"],["1/12","1/12","1/12","1/4","1/24","1/24","1/24","1/4"]])");
        const auto backtracked=solve_wrapper(wrapper,backtrack);
        require(nlohmann::json::parse(backtracked.result_json)["solutions"][0]["voice_rhythm_ticks"][0]==
            nlohmann::json::parse("[2,2,2,6,1,1,1,6]"),
            "Cloned tuplet propagator rejects an offbeat motive and continues to a valid group");
        check_cli_solution(backtrack);

    mixed["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["-1/12","-1/12","-1/12","1/4","1/24","1/24","1/24","1/4"]])");
    solve_wrapper(wrapper,mixed);
    check_cli_solution(mixed);

    auto offbeat=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","num_voices":1,"meter":{"time_signatures":["4/4"],"tuplets":[3]},"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,62,64,65,67]]},"rhythm":{"motives":[["1/8","1/12","1/12","1/12","5/8"]]}}],"rules":[{"rule_type":"r-metric-hierarchy","constraint":"tuplet_on_beat_start","target_voices":[0]}]})");
    solve_wrapper(wrapper,offbeat,false);
    check_cli_solution(offbeat,false);

    auto token=mixed;
    token["rules"][0].erase("constraint");
    token["rules"][0]["parameters"]={"tuplet_on_beat_start"};
    solve_wrapper(wrapper,token);
    check_cli_solution(token);

    token["rules"][0]["indices"]={0};
    std::string error;
    require(!wrapper.configure_from_json(token.dump(),error) && !error.empty(),
            "Tuplet group constraints reject event-index subsets in measure mode");
}

void check_measure_hierarchical_voices(MaxMSPWrapper::AsyncSolverWrapper& wrapper) {
    auto mixed=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":2,"measure_plan":[{"bars":1,"time_signature":"2/4"},{"bars":1,"time_signature":"3/8"}],"measure_fill_policy":"exact","num_voices":2,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,62,64,65,67]]},"rhythm":{"motives":[["1/4","1/4","1/8","1/8","1/8"]]}},{"pitch":{"motives":[[48,50,52,53,55,57,59]]},"rhythm":{"motives":[["1/8","1/8","1/8","1/8","1/8","1/8","1/8"]]}}],"rules":[{"rule_type":"r-metric-hierarchy","constraint":"hierarchical_voices","parameters":["0<-1"],"target_voices":[0,1]}]})");
    const auto anchors=solve_wrapper(wrapper,mixed);
    const auto anchor_solution=nlohmann::json::parse(anchors.result_json)["solutions"][0];
    require(anchor_solution["voice_rhythm_ticks"][0].size()==5 && anchor_solution["voice_rhythm_ticks"][1].size()==7,
            "Hierarchy compares unequal active prefixes by score-time beat anchors");
    check_cli_solution(mixed);

    auto strict=mixed;
    strict["rules"][0]["parameters"]={"0<-1","strict_all_onsets"};
    solve_wrapper(wrapper,strict,false);
    check_cli_solution(strict,false);

    auto partial=mixed;
    partial["rules"][0]["indices"]={1};
    solve_wrapper(wrapper,partial,false);
    check_cli_solution(partial,false);
    partial["rules"][0]["indices"]={2};
    solve_wrapper(wrapper,partial);
    check_cli_solution(partial);

    auto rests=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","num_voices":2,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,62]]},"rhythm":{"motives":[["1/2","-1/2"]]}},{"pitch":{"motives":[[48,50]]},"rhythm":{"motives":[["1/2","-1/2"]]}}],"rules":[{"rule_type":"r-metric-hierarchy","constraint":"hierarchical_voices","parameters":["0<-1"],"target_voices":[0,1]}]})");
    solve_wrapper(wrapper,rests);
    check_cli_solution(rests);
    rests["voices"][1]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/2","1/2"]])");
    solve_wrapper(wrapper,rests,false);
    check_cli_solution(rests,false);
}

void check_measure_dynamic_expressions(MaxMSPWrapper::AsyncSolverWrapper& wrapper) {
    auto config=nlohmann::json::parse(
    R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","num_voices":1,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,62]]},"rhythm":{"motives":[["1/2","1/2"]]}}],"dynamic_rules":[{"id":"literal_pitch_pair","type":"basic_constraint","mode":"true_false","expression":"voice[0].pitch[0] != voice[0].pitch[1]"}]})");
    const auto valid=solve_wrapper(wrapper,config);
    require(valid.dynamic_rules_posted_ok==1 && valid.dynamic_rules_post_failed==0,
        "Hard literal-address dynamic comparison is posted successfully");
    check_cli_solution(config);

        auto legacy=config;
        legacy["solution_length"]=2;
        legacy.erase("measure_count"); legacy.erase("measure_plan"); legacy.erase("measure_fill_policy");
        const auto legacy_valid=solve_wrapper(wrapper,legacy);
        require(legacy_valid.dynamic_rules_posted_ok==1 && legacy_valid.dynamic_rules_post_failed==0,
            "Integer-mode dynamic expressions retain their existing posting path");
        check_cli_solution(legacy);

    config["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,60]]");
    solve_wrapper(wrapper,config,false);
    check_cli_solution(config,false);
    legacy["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,60]]");
    solve_wrapper(wrapper,legacy,false);
    check_cli_solution(legacy,false);

    config["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,62]]");
    config["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["-1/2","1/2"]])");
    const auto rest=solve_wrapper(wrapper,config,false);
        require(rest.dynamic_rules_posted_ok==1 && rest.dynamic_rules_post_failed==0,
            "Pitch expression is posted while a rest cannot satisfy its sounded-event guard");

    config["dynamic_rules"][0]["expression"]="voice[0].rhythm[0] < 0";
    const auto rhythm=solve_wrapper(wrapper,config);
    require(rhythm.dynamic_rules_posted_ok==1 && rhythm.dynamic_rules_post_failed==0,
        "Hard literal rhythm expressions permit active rests");
    check_cli_solution(config);

    auto variable=config;
    variable["dynamic_rules"][0]["expression"]="voice[0].pitch[i] != voice[0].pitch[i+1]";
    const auto rejected=solve_wrapper(wrapper,variable,false);
    require(rejected.message.find("literal pitch/rhythm event addresses")!=std::string::npos,
        "Variable-index dynamic expressions fail closed instead of binding to event zero");

    auto heuristic=config;
    heuristic["dynamic_rules"][0]["mode"]="real_heuristic";
    std::string error;
    require(!wrapper.configure_from_json(heuristic.dump(),error) && !error.empty(),
        "Measure dynamic heuristics remain unsupported");
        heuristic["dynamic_rules"][0]["mode"]="unknown_mode";
        require(!wrapper.configure_from_json(heuristic.dump(),error) && !error.empty(),
            "Unknown dynamic modes fail closed instead of becoming hard rules");
}

void check_measure_wildcard_windows(MaxMSPWrapper::AsyncSolverWrapper& wrapper) {
    auto config=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","num_voices":1,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,61]]},"rhythm":{"motives":[["1/2","1/2"]]}}],"rules":[{"id":"stepwise_windows","rule_type":"wildcard_constraint","wildcard_type":"sliding_window","pattern_offsets":[0,1],"constraint":"abs(voice[v].pitch[i+1] - voice[v].pitch[i]) <= 1"}]})");
    const auto valid=solve_wrapper(wrapper,config);
    require(valid.dynamic_rules_posted_ok==1 && valid.dynamic_rules_post_failed==0,
            "Measure sliding-window wildcard posts guarded comparisons");
    check_cli_solution(config);

    config["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,63]]");
    solve_wrapper(wrapper,config,false);
    check_cli_solution(config,false);

    config["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/2","-1/2"]])");
    solve_wrapper(wrapper,config);
    check_cli_solution(config);

    auto padded=config;
    padded["measure_fill_policy"]="pad";
    padded["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60]]");
    padded["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/2"]])");
    solve_wrapper(wrapper,padded);
    check_cli_solution(padded);

    auto unsupported=config;
    unsupported["rules"][0]["wildcard_type"]="for_all_pairs";
    std::string error;
    require(!wrapper.configure_from_json(unsupported.dump(),error) && !error.empty(),
            "Unmigrated wildcard expansions remain fail-closed in measure mode");

    auto targeted=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","num_voices":2,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,61]]},"rhythm":{"motives":[["1/2","1/2"]]}},{"pitch":{"motives":[[60,63]]},"rhythm":{"motives":[["1/2","1/2"]]}}],"rules":[{"id":"targeted_window","rule_type":"wildcard_constraint","wildcard_type":"sliding_window","pattern_offsets":[0,1],"constraint":"abs(voice[v].pitch[i+1] - voice[v].pitch[i]) <= 1","target_voices":[0]}]})");
    solve_wrapper(wrapper,targeted);
    check_cli_solution(targeted);
    targeted["rules"][0]["target_voices"]={0,1};
    solve_wrapper(wrapper,targeted,false);
    check_cli_solution(targeted,false);

    auto membership=config;
    membership["rules"][0]["pattern_offsets"]={0};
    membership["rules"][0]["indices"]={0,1};
    membership["rules"][0]["constraint"]="voice[v].pitch[i] in [60,61]";
    membership["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/2","1/2"]])");
    membership["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,61]]");
    solve_wrapper(wrapper,membership);
    check_cli_solution(membership);
    membership["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,63]]");
    solve_wrapper(wrapper,membership,false);
    check_cli_solution(membership,false);
    membership["rules"][0]["constraint"]="voice[v].pitch[i] not_in [63]";
    membership["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,61]]");
    solve_wrapper(wrapper,membership);
    check_cli_solution(membership);
    membership["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,63]]");
    solve_wrapper(wrapper,membership,false);
    check_cli_solution(membership,false);

    for (const std::string shape : {"for_all_positions","for_all_voices"}) {
        auto expanded=membership;
        expanded["rules"][0]["wildcard_type"]=shape;
        expanded["rules"][0].erase("indices");
        expanded["rules"][0]["constraint"]="voice[v].pitch[i] in [60,61]";
        expanded["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,61]]");
        solve_wrapper(wrapper,expanded);
        check_cli_solution(expanded);
        expanded["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,63]]");
        solve_wrapper(wrapper,expanded,false);
        check_cli_solution(expanded,false);
    }

    auto logic=membership;
    logic["rules"][0]["constraint"]="voice[v].pitch[i] == 60 || voice[v].pitch[i] == 61";
    logic["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,61]]");
    solve_wrapper(wrapper,logic);
    check_cli_solution(logic);
    logic["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,63]]");
    solve_wrapper(wrapper,logic,false);
    check_cli_solution(logic,false);
    logic["rules"][0]["constraint"]="not (voice[v].pitch[i] == 63)";
    logic["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,61]]");
    solve_wrapper(wrapper,logic);
    check_cli_solution(logic);
    logic["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,63]]");
    solve_wrapper(wrapper,logic,false);
    check_cli_solution(logic,false);
}

void check_measure_index_and_logic_rules(MaxMSPWrapper::AsyncSolverWrapper& wrapper) {
    auto base=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","num_voices":1,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,62]]},"rhythm":{"motives":[["1/2","1/2"]]}}],"rules":[{"type":"index","id":"pin_events","voice":0,"events":[[0,"1/2",60],[1,"1/2",62]]}]})");
    solve_wrapper(wrapper,base);
    check_cli_solution(base);
    auto wrong_pitch=base;
    wrong_pitch["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60,63]]");
    solve_wrapper(wrapper,wrong_pitch,false);
    check_cli_solution(wrong_pitch,false);

    auto rest=base;
    rest["rules"][0]["events"]=nlohmann::json::parse(R"([[0,"1/2",60],[1,"-1/2",null]])");
    rest["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/2","-1/2"]])");
    solve_wrapper(wrapper,rest);
    check_cli_solution(rest);
    rest["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/2","1/2"]])");
    solve_wrapper(wrapper,rest,false);
    check_cli_solution(rest,false);

    auto beyond_prefix=base;
    beyond_prefix["measure_fill_policy"]="pad";
    beyond_prefix["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[60]]");
    beyond_prefix["voices"][0]["rhythm"]["motives"]=nlohmann::json::parse(R"([["1/2"]])");
    solve_wrapper(wrapper,beyond_prefix,false);

    auto dynamic=nlohmann::json::parse(
        R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","num_voices":1,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,62]]},"rhythm":{"motives":[["1/2","1/2"]]}}],"dynamic_rules":[{"id":"either_is_c","type":"basic_constraint","mode":"true_false","expression":"voice[0].pitch[0] == 60 || voice[0].pitch[1] == 60"}]})");
    solve_wrapper(wrapper,dynamic);
    check_cli_solution(dynamic);
    dynamic["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[61,62]]");
    solve_wrapper(wrapper,dynamic,false);
    check_cli_solution(dynamic,false);
    dynamic["dynamic_rules"][0]["expression"]="not (voice[0].pitch[0] == voice[0].pitch[1])";
    solve_wrapper(wrapper,dynamic);
    check_cli_solution(dynamic);
    dynamic["voices"][0]["pitch"]["motives"]=nlohmann::json::parse("[[62,62]]");
    solve_wrapper(wrapper,dynamic,false);
    check_cli_solution(dynamic,false);

    for (const std::string type : {"r-cross-voice-no-unisons","r-perfect-fifth-intervals","r-pitches-one-voice","r-index-pitches-one-voice"}) {
        auto rejected=base;
        rejected["rules"]=nlohmann::json::array({{{"rule_type",type},{"target_voices",{0}}}});
        std::string error;
        require(!wrapper.configure_from_json(rejected.dump(),error) && !error.empty(),
                "Placeholder/legacy rule fails closed with an explicit message: "+type);
    }
}

void check_legacy_temporal(MaxMSPWrapper::AsyncSolverWrapper& wrapper) {
    for (const std::string filename : {"configs/timepoint_relationship_smoke.json","configs/metric_domain_exact_fill_success.json"}) {
        std::ifstream input(filename);
        require(bool(input),"Load existing integer-mode temporal fixture");
        auto config=nlohmann::json::parse(input);
        config["export_json"]=false; config["export_txt"]=false; config["export_xml"]=false;
        config["search_options"]["max_solutions"]=1;
        const bool timepoint=filename=="configs/timepoint_relationship_smoke.json";
        if (timepoint) config["rules"][1]["constraint"]="abs(voice[v2].pitch[i] - voice[v1].pitch[i]) in [7]";
        else config["rules"][1]["engine_type"]="rhythm";
        const auto result=solve_wrapper(wrapper,config);
        require(result.dynamic_rules_post_failed==0,"Legacy temporal posting remains successful");
        const auto solution=nlohmann::json::parse(result.result_json)["solutions"][0];
        if (timepoint) {
            for (int event : {0,4})
                require(std::abs(solution["voice_solutions"][0][event].get<int>()-solution["voice_solutions"][1][event].get<int>())==7,
                        "Legacy timepoint singleton interval is enforced in extracted pitches");
        } else {
            const auto& measures=solution["score"]["measures"];
            require(measures.size()==3 && measures[0]["numerator"]==4 && measures[1]["numerator"]==3 &&
                    measures[2]["numerator"]==6 && measures[2]["denominator"]==8,
                    "Legacy metric signatures retain the actual 4/4, 3/4, 6/8 timeline");
        }
        auto cli_config=config;
        if (!timepoint) {
            cli_config["rules"][2].erase("timepoints");
            cli_config["rules"][2]["indices"]={0,8,14};
        }
        check_cli_solution(cli_config);
    }
}

int main() {
    try {
        check_cross_rhythm_equivalence();
        check_cross_pitch_equivalence();
        check_active_global_equivalence();
        check_active_reverse_equivalence();
        check_repetition_equivalence();
        check_tendency_equivalence();
        check_timepoint_equivalence();
        check_cli_verbose_transformation();
        check_exhaustive_timing_equivalence(false);
        check_exhaustive_timing_equivalence(true);
        check_exhaustive_timing_equivalence(false, true);
        check_exhaustive_timing_equivalence(true, true);
        for (bool exact : {false, true}) {
            check_exhaustive_timing_equivalence(exact, false, "r-no-interval-repetition");
            check_exhaustive_timing_equivalence(exact, false, "r-no-interval-repetition", true);
            check_exhaustive_timing_equivalence(exact, false, "r-no-consecutive-equal");
            for (const std::string direction : {"both", "ascending", "descending"})
                check_exhaustive_timing_equivalence(exact, false, "r-no-consecutive-direction", false, direction);
        }
    MusicalConstraintSolver::SolverConfig profile_config;
    profile_config.sequence_length = 2;
    profile_config.num_voices = 1;
    profile_config.voice_domains = {{60, 62}};
    profile_config.voice_rhythm_domains = {{1, 2}};
    profile_config.rhythm_base = 4;
    MusicalConstraintSolver::Solver profile_solver;
    profile_solver.configure(profile_config);
    const auto exhaustive_solutions = profile_solver.solve_multiple(-1);
    require(exhaustive_solutions.size() == 16, "Small scalar baseline enumerates all pitch/rhythm assignments");
    require(profile_solver.last_search_stats().completion_reason == "exhausted", "Enumeration reports exhaustion");
    require(profile_solver.last_search_stats().model_build_ms >= 0.0 &&
        profile_solver.last_search_stats().search_ms >= 0.0 &&
        profile_solver.last_search_stats().extraction_ms >= 0.0, "Separate profiling phases are nonnegative");
    const auto limited_solutions = profile_solver.solve_multiple(1);
    require(limited_solutions.size() == 1 && profile_solver.last_search_stats().completion_reason == "solution_limit",
        "A new solve resets completion state and reports a solution limit");

        const auto sixteen_bar_plan = MeasureCountPlan::parse(nlohmann::json::parse(
            R"({"solution_length":"measure_count","measure_count":16,"measure_fill_policy":"exact","measure_plan":[{"bars":16,"time_signature":"4/4"}]})"));
        require(MeasureCountPlan::fixed_horizon_ticks(sixteen_bar_plan,
                    MeasureCountPlan::required_rhythm_base(sixteen_bar_plan, 8)) == 128,
                "Sixteen 4/4 bars produce 128 eighth-note ticks");
        const auto mixed_meter_plan = MeasureCountPlan::parse(nlohmann::json::parse(
            R"({"solution_length":"measure_count","measure_count":7,"measure_plan":[{"bars":4,"time_signature":"4/4"},{"bars":3,"time_signature":"3/4"}]})"));
        require(MeasureCountPlan::fixed_horizon_ticks(mixed_meter_plan,
                    MeasureCountPlan::required_rhythm_base(mixed_meter_plan, 8)) == 50,
                "Grouped mixed meters produce an exact 50-tick horizon");
        const auto meter_domain_plan = MeasureCountPlan::parse(nlohmann::json::parse(
            R"({"solution_length":"measure_count","measure_count":8,"meter":{"time_signatures":["3/4","4/4"]}})"));
        require(meter_domain_plan.solver_selects_meter && meter_domain_plan.meter_domain.size() == 2,
                "Missing fixed plan selects meter independently for each bar");
        auto allowed_measure_rules = nlohmann::json::parse(
            R"({"solution_length":"measure_count","rules":[{"rule_type":"r-cadence","beat_positions":[2,3]}]})");
        MeasureCountPlan::validate_rule_support(allowed_measure_rules);
        auto disabled_rule = allowed_measure_rules;
        disabled_rule["rules"].push_back({{"rule_type","not-yet-supported"},{"enabled",false}});
        MeasureCountPlan::validate_rule_support(disabled_rule);
        auto invalid_entry = allowed_measure_rules;
        invalid_entry["rules"].push_back(42);
        bool invalid_entry_rejected = false;
        try { MeasureCountPlan::validate_rule_support(invalid_entry); }
        catch (const std::exception&) { invalid_entry_rejected = true; }
        require(invalid_entry_rejected, "Malformed enabled rule entry cannot be silently dropped");
        auto fixed_index_measure_rule = allowed_measure_rules;
        fixed_index_measure_rule["rules"][0].erase("beat_positions");
        fixed_index_measure_rule["rules"][0]["positions"] = {2, 3};
        bool fixed_index_rejected = false;
        try { MeasureCountPlan::validate_rule_support(fixed_index_measure_rule); }
        catch (const std::exception&) { fixed_index_rejected = true; }
        require(fixed_index_rejected, "Measure mode rejects event-index cadence rules");
        auto dynamic_measure_rule = allowed_measure_rules;
        dynamic_measure_rule["dynamic_rules"] = nlohmann::json::array({nlohmann::json::object()});
        bool dynamic_rejected = false;
        try { MeasureCountPlan::validate_rule_support(dynamic_measure_rule); }
        catch (const std::exception&) { dynamic_rejected = true; }
        require(dynamic_rejected, "Measure mode rejects non-active-aware dynamic rules");
        for (const auto& invalid_plan : {
                R"({"solution_length":"measure_count","measure_count":8})",
                R"({"solution_length":"measure_count","measure_count":8,"measure_plan":[{"bars":7,"time_signature":"4/4"}]})",
                R"({"solution_length":"measure_count","measure_count":8,"measure_fill_policy":"sometimes","measure_plan":[{"bars":8,"time_signature":"4/4"}]})"}) {
            bool invalid_rejected = false;
            try { MeasureCountPlan::parse(nlohmann::json::parse(invalid_plan)); }
            catch (const std::exception&) { invalid_rejected = true; }
            require(invalid_rejected, "Invalid measure-count plan must be rejected");
        }

        auto cfg = nlohmann::json::parse(R"({"solution_length":5,"voices":[{"pitch":{"motives":[[60,61],[67,64,62]]},"rhythm":{"motives":[["1/2","1/8","-1/8"],["1/4","1/4"]]}}]})");
        MotiveDomains::normalize_domains(cfg);
        const auto domains = MotiveDomains::parse_domains(cfg, 1, 5, 8);
        require(domains.size() == 2 && domains[1].candidates[0] == std::vector<int>({4, 1, -1}), "Tick conversion");
        auto numeric_rhythm = nlohmann::json::parse(R"({"solution_length":2,"voices":[{"rhythm":{"motives":[[0.25,-0.125]]}}]})");
        MotiveDomains::normalize_domains(numeric_rhythm);
        require(numeric_rhythm["voices"][0]["rhythm"]["motives"] == nlohmann::json::parse(R"([["1/4","-1/8"]])"),
            "Numeric rhythm atoms normalize to exact fractions");
        const auto numeric_domains = MotiveDomains::parse_domains(numeric_rhythm, 1, 2, 8);
        require(numeric_domains.size() == 1 && numeric_domains[0].candidates[0] == std::vector<int>({2, -1}),
            "Numeric rhythm atom tick conversion");
        bool rejected = false;
        try { MotiveDomains::validate_motives(nlohmann::json::parse("[[60,61],[67,64,62]]"), false, 1); }
        catch (const std::exception&) { rejected = true; }
        require(rejected, "Impossible lengths must be rejected");

        auto space = make_space();
        rel(*space, space->get_absolute_vars()[0], IRT_EQ, 60);
        rel(*space, space->get_rhythm_vars()[0], IRT_EQ, 4);
        DFS<IntegratedMusicalSpace> search(space.get());
        space.reset();
        std::unique_ptr<IntegratedMusicalSpace> solved(search.next());
        require(bool(solved), "Independent motive paths must solve");
        const auto selected = solved->get_selected_motives();
        std::vector<int> pitches, rhythms;
        for (const auto& selection : selected) {
            auto& flat = selection.rhythm ? rhythms : pitches;
            require(selection.start_event == static_cast<int>(flat.size()), "No gaps or overlaps");
            flat.insert(flat.end(), selection.values.begin(), selection.values.end());
            require(selection.end_event == static_cast<int>(flat.size()), "Exact boundaries");
        }
        require(pitches == std::vector<int>({60, 61, 67, 64, 62}), "Pitch length 2+3");
        require(rhythms == std::vector<int>({4, 1, -1, 2, 2}), "Rhythm length 3+2");
        require(solved->get_pitch_sequence(0)[2] == IntegratedMusicalSpace::REST_PITCH_SENTINEL, "Rest keeps event slot");

        auto hybrid = make_space();
        rel(*hybrid, hybrid->get_absolute_vars()[0], IRT_EQ, 60);
        rel(*hybrid, hybrid->get_absolute_vars()[1], IRT_EQ, 64);
        DFS<IntegratedMusicalSpace> invalid(hybrid.get());
        hybrid.reset();
        std::unique_ptr<IntegratedMusicalSpace> wrong(invalid.next());
        require(!wrong, "Hybrid motive must be UNSAT");

        auto nested = nlohmann::json::parse(R"({"solution_length":1,"voices":[{"pitch":{"motives":{"0":{"0":60}}}}]})");
        MotiveDomains::normalize_domains(nested);
        require(nested["voices"][0]["pitch"]["motives"] == nlohmann::json::parse("[[60]]"), "Max nested singleton shape");
        for (const auto& invalid_cfg : {
                R"({"solution_length":2,"voices":[{"pitch":{"motives":[[]]}}]})",
                R"({"solution_length":2,"voices":[{"pitch":{"motives":[[128]]}}]})",
                R"({"solution_length":2,"voices":[{"pitch":{"motives":[[60.5]]}}]})",
                R"({"solution_length":2,"voices":[{"pitch":{"motives":[[60]],"midi_values":[60]}}]})",
                R"({"solution_length":2,"voices":[{"rhythm":{"motives":[["0/4"]]}}]})",
                R"({"solution_length":2,"voices":[{"rhythm":{"motives":[[0]]}}]})",
                R"({"solution_length":2,"voices":[{"rhythm":{"motives":[["1/0"]]}}]})",
                R"({"solution_length":2,"voices":[{"pitch":{"motives":[60,61]}}]})"}) {
            bool invalid_rejected = false;
            try { auto input = nlohmann::json::parse(invalid_cfg); MotiveDomains::normalize_domains(input); }
            catch (const std::exception&) { invalid_rejected = true; }
            require(invalid_rejected, "Malformed motive must fail configuration");
        }

        auto input = nlohmann::json::parse(R"({"solution_length":6,"num_voices":2,"export_json":false,"export_txt":false,"export_xml":false,"search_options":{"random_seed":42,"value_order":"random","timeout_ms":3000},"global_domain":{"pitch":{"motives":[[60,61,62],[67,64]]},"rhythm":{"motives":[["1/4","1/12","-1/12"],["1/8"]]}},"voices":[{"voice":1,"pitch":{"midi_values":[48,50,52]}}],"rules":[]})");
        MaxMSPWrapper::AsyncSolverWrapper wrapper;
        check_global_wrapper(wrapper);
        check_repetition_wrapper(wrapper);
        check_tendency_wrapper(wrapper);
        check_timepoint_wrapper(wrapper);
        check_signature_wrapper(wrapper);
        check_measure_metric_hierarchy(wrapper);
        check_measure_metric_grids(wrapper);
        check_measure_tuplet_starts(wrapper);
        check_measure_hierarchical_voices(wrapper);
        check_measure_dynamic_expressions(wrapper);
        check_measure_wildcard_windows(wrapper);
        check_measure_index_and_logic_rules(wrapper);
        check_legacy_temporal(wrapper);
        check_cross_pitch_wrapper(wrapper);
        const auto first = solve_wrapper(wrapper, input);
        const auto first_json = nlohmann::json::parse(first.result_json);
        const auto first_solution = first_json.at("solutions")[0];
        check_metadata(first_solution, 6);
        require(first_solution["selected_motives"].size() > 0, "Global motive fallback");
        require(first_solution["score"]["rhythm_base"] == 24, "Tuplet denominators in LCM");
        const auto again = nlohmann::json::parse(solve_wrapper(wrapper, input).result_json);
        require(again["solutions"][0]["voice_solutions"] == first_solution["voice_solutions"], "Fixed seed repeatability");
        const auto scalar = nlohmann::json::parse(R"({"solution_length":2,"num_voices":1,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"midi_values":[60]},"rhythm":{"duration_values":["1/4"]}}],"rules":[]})");
        const auto reset = nlohmann::json::parse(solve_wrapper(wrapper, scalar).result_json);
        require(!reset["solutions"][0].contains("selected_motives"), "No stale motives after scalar reconfigure");
        auto locked = scalar;
        locked["solution_length"] = 4;
        locked["voices"][0]["pitch"] = nlohmann::json::parse(R"({"motives":[[60,61]]})");
        const auto locked_result = nlohmann::json::parse(solve_wrapper(wrapper, locked).result_json);
        require(locked_result["solutions"][0]["voice_solutions"][0] == nlohmann::json::parse("[60,61,60,61]"), "Locked motive repeats whole");
        check_metadata(locked_result["solutions"][0], 4);
        const auto cadence = nlohmann::json::parse(R"({"solution_length":2,"num_voices":4,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[62,60],[71,72]]},"rhythm":{"duration_values":["1/4"]}},{"pitch":{"midi_values":[55,59,60,64,65]},"rhythm":{"duration_values":["1/4"]}},{"pitch":{"motives":[[53,52],[59,60]]},"rhythm":{"duration_values":["1/4"]}},{"pitch":{"motives":[[43,36]]},"rhythm":{"duration_values":["1/4"]}}],"harmonic_domain":[{"beat":0,"degree":"V7","key":"C","mode":"major"},{"beat":1,"degree":"I"}],"rules":[{"rule_type":"r-cadence","cadence_type":"PAC","positions":[0,1],"target_voices":[0,1,2,3],"require_chord_tones":true},{"rule_type":"r-tendency-tone","tendency":"chordal_seventh","positions":[0,1],"target_voices":[0,1,2,3]},{"rule_type":"r-tendency-tone","tendency":"leading_tone","positions":[0,1],"target_voices":[0,1,2,3]}]})");
        const auto cadenced = nlohmann::json::parse(solve_wrapper(wrapper, cadence).result_json);
        const auto cadenced_solution = cadenced["solutions"][0];
        check_metadata(cadenced_solution, 2);
        require(cadenced_solution["voice_solutions"][3] == nlohmann::json::parse("[43,36]"), "PAC bass motive");
        require(cadenced_solution["voice_solutions"][0][1].get<int>() % 12 == 0, "PAC soprano target");
        const auto cadenced_tenor = cadenced_solution["voice_solutions"][2].get<std::vector<int>>();
        require(cadenced_tenor == std::vector<int>({53,52}) || cadenced_tenor == std::vector<int>({59,60}), "Tendency-tone motive compatibility");

        std::ifstream measure_config_file("configs/measure_count_16bar_cadence_motives.json");
        require(bool(measure_config_file), "16-bar measure-count test config exists");
        nlohmann::json measure_config;
        measure_config_file >> measure_config;
        const auto measure_result = nlohmann::json::parse(solve_wrapper(wrapper, measure_config).result_json);
        const auto& measure_solution = measure_result["solutions"][0];
        const auto& measure_rhythms = measure_solution["voice_rhythm_ticks"];
        const auto& measure_pitches = measure_solution["voice_solutions"];
        auto tonal_config=measure_config;
        tonal_config["export_json"]=false; tonal_config["export_txt"]=false; tonal_config["export_xml"]=false;
        for (int period : {0,1}) for (const std::string tendency : {"leading_tone","chordal_seventh"})
            tonal_config["rules"].push_back({{"rule_type","r-tendency-tone"},{"tendency",tendency},
                {"beat_positions",{30+32*period,31+32*period}},{"target_voices",{0,1,2,3}}});
        const auto tonal_result=solve_wrapper(wrapper,tonal_config);
        require(tonal_result.dynamic_rules_posted_ok==8 && tonal_result.dynamic_rules_post_failed==0,
                "Four cadences and four tendency rules post together with hard harmony");
        const auto tonal_solution=nlohmann::json::parse(tonal_result.result_json)["solutions"][0];
        auto tonal_pitch=[&](int voice,int beat) {
            int onset=0;
            for (size_t event=0; event<tonal_solution["voice_rhythm_ticks"][voice].size(); ++event) {
                const int duration=tonal_solution["voice_rhythm_ticks"][voice][event].get<int>();
                if (onset<=beat && beat<onset+std::abs(duration)) {
                    require(duration>0,"Tonal cadence times require sounded voices");
                    return tonal_solution["voice_solutions"][voice][event].get<int>();
                }
                onset+=std::abs(duration);
            }
            throw std::runtime_error("Tonal integration sample outside active rhythm");
        };
        for (int beat : {30,62}) for (int voice=0; voice<4; ++voice) {
            const int source=tonal_pitch(voice,beat),target=tonal_pitch(voice,beat+1);
            if (source%12==11) require(target==source+1,"Every actual leading-tone carrier resolves up to tonic");
            if (source%12==5) require(target==source-1,"Every actual dominant seventh carrier resolves down a step");
        }
        auto combined_config = measure_config;
        combined_config["export_json"] = false;
        combined_config["export_txt"] = false;
        combined_config["export_xml"] = false;
        combined_config["rules"].push_back(nlohmann::json::parse(
            R"({"rule_type":"r-pitch-pitch","constraint":"voice_above","target_voices":[0,1]})"));
        combined_config["rules"].push_back(nlohmann::json::parse(
            R"({"rule_type":"r-rhythm-rhythm","constraint":"isorhythm","target_voices":[0,1]})"));
        const auto combined_result = solve_wrapper(wrapper, combined_config);
        require(combined_result.dynamic_rules_posted_ok == 6 && combined_result.dynamic_rules_post_failed == 0,
            "Cross-pitch/rhythm rules and four cadences all post in the 16-bar score");
        const auto combined_solution = nlohmann::json::parse(combined_result.result_json)["solutions"][0];
        check_metadata(combined_solution, 48);
        require(combined_solution["score"]["measures"].size() == 16, "Combined rules preserve the complete bar plan");
        for (int event = 0; event < 48; ++event) {
            require(combined_solution["voice_solutions"][0][event].get<int>() > combined_solution["voice_solutions"][1][event].get<int>(),
                "Combined score keeps Soprano above Alto at aligned events");
            require(combined_solution["voice_rhythm_ticks"][0][event] == combined_solution["voice_rhythm_ticks"][1][event],
                "Combined score keeps upper-voice rhythms equal");
        }
        {
            // Full-rule gate: later rule families together on the real 16-bar score, each checked on the extracted output.
            int max_step = 0;
            for (int voice = 0; voice < 4; ++voice)
                for (size_t event = 1; event < measure_rhythms[voice].size(); ++event)
                    if (measure_rhythms[voice][event - 1].get<int>() > 0 && measure_rhythms[voice][event].get<int>() > 0)
                        max_step = std::max(max_step, std::abs(measure_pitches[voice][event].get<int>() - measure_pitches[voice][event - 1].get<int>()));
            const int baseline_interval = std::abs(measure_pitches[0][0].get<int>() - measure_pitches[3][0].get<int>());
            const int pinned_pitch = measure_pitches[1][0].get<int>();
            const int pinned_ticks = measure_rhythms[1][0].get<int>();
            require(pinned_ticks > 0, "Baseline Alto opens on a sounded note");

            auto gate_config = measure_config;
            gate_config["export_json"] = false; gate_config["export_txt"] = false; gate_config["export_xml"] = false;
            for (const char* rule : {
                    R"({"rule_type":"r-metric-hierarchy","constraint":"no_syncopation","parameters":["beats:1"],"target_voices":[0,1,2,3]})",
                    R"({"rule_type":"r-metric-hierarchy","constraint":"hierarchical_voices","parameters":["0<-1"],"target_voices":[0,1]})",
                    R"({"rule_type":"r-time-signature","bar_pattern_type":"repeating","bar_pattern":["4/4"],"bar_pattern_repetitions":16})"})
                gate_config["rules"].push_back(nlohmann::json::parse(rule));
            gate_config["rules"].push_back({{"id","melodic_cap"},{"rule_type","wildcard_constraint"},{"wildcard_type","sliding_window"},
                {"pattern_offsets",{0,1}},{"constraint","abs(voice[v].pitch[i+1] - voice[v].pitch[i]) <= "+std::to_string(max_step)}});
            gate_config["rules"].push_back({{"rule_type","r-timepoint-relationship"},{"time_scope","at_timepoints"},{"timepoints",{"0q"}},
                {"function","interval_in_set"},{"parameters",{baseline_interval}},{"target_voices",{0,3}}});
            gate_config["rules"].push_back({{"type","index"},{"id","pin_alto_opening"},{"voice",1},
                {"events",nlohmann::json::array({nlohmann::json::array({0,std::to_string(pinned_ticks)+"/4",pinned_pitch})})}});

            const auto gate_result = solve_wrapper(wrapper, gate_config);
            require(gate_result.dynamic_rules_post_failed == 0, "Every later rule family posts together on the 16-bar score");
            const auto gate_solution = nlohmann::json::parse(gate_result.result_json)["solutions"][0];
            check_metadata(gate_solution, 48);
            const auto& gate_pitches = gate_solution["voice_solutions"];
            const auto& gate_rhythms = gate_solution["voice_rhythm_ticks"];
            require(gate_pitches[1][0].get<int>() == pinned_pitch && gate_rhythms[1][0].get<int>() == pinned_ticks,
                    "Index rule pins the Alto opening event");
            require(std::abs(gate_pitches[0][0].get<int>() - gate_pitches[3][0].get<int>()) == baseline_interval,
                    "Timepoint rule holds the opening Soprano-Bass interval");
            std::set<int> alto_onsets, soprano_sounded;
            for (int voice = 0; voice < 4; ++voice) {
                int onset = 0;
                for (size_t event = 0; event < gate_rhythms[voice].size(); ++event) {
                    const int duration = gate_rhythms[voice][event].get<int>();
                    if (duration > 0) {
                        require(onset / 4 == (onset + duration - 1) / 4, "No sounded note crosses a barline");
                        if (voice == 0) soprano_sounded.insert(onset);
                        if (voice == 1) alto_onsets.insert(onset);
                        if (event > 0 && gate_rhythms[voice][event - 1].get<int>() > 0)
                            require(std::abs(gate_pitches[voice][event].get<int>() - gate_pitches[voice][event - 1].get<int>()) <= max_step,
                                    "Wildcard melodic cap holds between adjacent sounded notes");
                    }
                    onset += std::abs(duration);
                }
            }
            for (int onset : alto_onsets)
                require(soprano_sounded.count(onset) == 1, "Hierarchical voices: every Alto onset is inherited by the Soprano");

            // Each rule must have teeth: making it impossible turns the combined solve UNSAT.
            auto impossible_pin = gate_config;
            impossible_pin["rules"].back()["events"][0][2] = 1;
            solve_wrapper(wrapper, impossible_pin, false);
            auto impossible_cap = gate_config;
            impossible_cap["rules"][impossible_cap["rules"].size() - 3]["constraint"] = "abs(voice[v].pitch[i+1] - voice[v].pitch[i]) <= -1";
            solve_wrapper(wrapper, impossible_cap, false);
            auto impossible_interval = gate_config;
            impossible_interval["rules"][impossible_interval["rules"].size() - 2]["parameters"] = nlohmann::json::array({200});
            solve_wrapper(wrapper, impossible_interval, false);

            // Max dictionaries deliver every boolean as 0/1; the full rule set must configure and solve identically.
            std::function<void(nlohmann::json&)> booleans_to_numbers = [&](nlohmann::json& value) {
                if (value.is_boolean()) value = value.get<bool>() ? 1 : 0;
                else if (value.is_object() || value.is_array()) for (auto& child : value) booleans_to_numbers(child);
            };
            auto numeric_config = gate_config;
            booleans_to_numbers(numeric_config);
            require(numeric_config["require_harmonic_chord_tones"] == 1, "Numeric-boolean variant actually contains numeric flags");
            const auto numeric_result = solve_wrapper(wrapper, numeric_config);
            require(numeric_result.dynamic_rules_post_failed == 0, "Numeric booleans configure every rule family as Max dictionaries do");
            const auto numeric_solution = nlohmann::json::parse(numeric_result.result_json)["solutions"][0];
            require(numeric_solution["voice_solutions"] == gate_solution["voice_solutions"],
                    "Numeric booleans produce the same solution as true/false");
        }
        MusicalConstraintSolver::SolverConfig::HarmonicConfig measure_harmony;
        HarmonicDomainParser::parse(measure_config, measure_harmony, 64, 4);
        require(measure_solution["score"]["measures"].size() == 16, "Measure mode exports all 16 bars");
        require(measure_solution["score"]["rhythm_base"] == 4, "Measure mode uses quarter-note tick grid");
        require(measure_solution["selected_motives"].size() > 0, "Measure mode exports selected motives");
        for (int voice = 0; voice < 4; ++voice) {
            const auto rhythm = measure_rhythms[voice].get<std::vector<int>>();
            require(rhythm.size() == 48, "Motive paths select 48 events for the 64-quarter horizon");
            int total_ticks = 0;
            for (size_t event = 0; event < rhythm.size(); ++event) {
                const int duration = rhythm[event];
                const int end_tick = total_ticks + std::abs(duration);
                for (size_t segment = 0; segment < measure_harmony.entries.size(); ++segment) {
                    const auto& chord = measure_harmony.entries[segment];
                    const int segment_end = segment + 1 < measure_harmony.entries.size()
                        ? measure_harmony.entries[segment + 1].beat_position : 64;
                    if (duration > 0 && total_ticks < segment_end && end_tick > chord.beat_position) {
                        const int pitch_class = measure_pitches[voice][event].get<int>() % 12;
                        require(std::find(chord.chord_tones.begin(), chord.chord_tones.end(), pitch_class) != chord.chord_tones.end(),
                                "Every sounding note belongs to every overlapping harmonic segment");
                    }
                }
                total_ticks = end_tick;
            }
            require(total_ticks == 64, "Each voice exactly fills the 16-bar horizon");
            for (const std::string component : {"pitch", "rhythm"}) {
                int boundary = 0;
                for (const auto& selected : measure_solution["selected_motives"])
                    if (selected["voice"] == voice && selected["component"] == component) {
                        require(selected["start_event"] == boundary, "Measure motive boundaries are contiguous");
                        boundary = selected["end_event"].get<int>();
                    }
                require(boundary == static_cast<int>(rhythm.size()), "Measure motive path covers active events only");
            }
        }
        auto sounding_pitch = [&](int voice, int beat) {
            int onset = 0;
            for (size_t event = 0; event < measure_rhythms[voice].size(); ++event) {
                const int duration = measure_rhythms[voice][event].get<int>();
                if (onset <= beat && beat < onset + std::abs(duration)) {
                    require(duration > 0, "Cadence target beat must have a sounding note");
                    return measure_pitches[voice][event].get<int>();
                }
                onset += std::abs(duration);
            }
            throw std::runtime_error("No active event sounds at cadence beat " + std::to_string(beat));
        };
        for (int beat : {15, 47}) require(sounding_pitch(3, beat) % 12 == 7, "HC bass ends on dominant");
        for (int beat : {30, 62}) require(sounding_pitch(3, beat) % 12 == 7, "PAC bass is dominant at pre-resolution beat");
        for (int beat : {31, 63}) {
            require(sounding_pitch(3, beat) % 12 == 0, "PAC bass resolves to tonic");
            require(sounding_pitch(0, beat) % 12 == 0, "PAC soprano resolves to tonic");
        }
        auto alto_values = measure_pitches[1].get<std::vector<int>>();
        std::sort(alto_values.begin(), alto_values.end());
        alto_values.erase(std::unique(alto_values.begin(), alto_values.end()), alto_values.end());
        require(alto_values.size() >= 4, "Harmonic Alto uses varied pitches rather than repeated C");

        const auto padded_config = nlohmann::json::parse(
            R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"pad","num_voices":1,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"midi_values":[60]},"rhythm":{"motives":[["1/2"]]}}],"rules":[]})");
        const auto padded_result = nlohmann::json::parse(solve_wrapper(wrapper, padded_config).result_json);
        const auto& padded_solution = padded_result["solutions"][0];
        require(padded_solution["voice_rhythm_ticks"][0] == nlohmann::json::parse("[2]"),
            "Score padding is excluded from the solver event sequence");
        require(padded_solution["selected_motives"].size() == 1 &&
            padded_solution["selected_motives"][0]["end_event"] == 1,
            "Score padding is excluded from motive coverage");
        const auto& padded_events = padded_solution["score"]["measures"][0]["events"];
        require(padded_events.size() == 2 && padded_events[1]["index"] == -1 &&
            padded_events[1]["is_rest"] == true && padded_events[1]["duration_ticks"] == 2,
            "Short voice receives a two-tick score-level trailing rest");

        auto hard_harmony = nlohmann::json::parse(
            R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","require_harmonic_chord_tones":true,"num_voices":1,"voices":[{"pitch":{"motives":[[60]]},"rhythm":{"motives":[["1/1"]]}}],"harmonic_domain":[{"beat":0,"degree":"I","key":"C","mode":"major"},{"beat":2,"degree":"V7"}],"rules":[],"export_json":false,"export_xml":false,"export_txt":false})");
        solve_wrapper(wrapper, hard_harmony, false);
        hard_harmony["voices"][0]["pitch"]["motives"] = {{67}};
        const auto common_tone = nlohmann::json::parse(solve_wrapper(wrapper, hard_harmony).result_json);
        require(common_tone["solutions"][0]["voice_solutions"][0] == nlohmann::json::parse("[67]"),
            "A sustained common chord tone is valid across a harmony change");
        hard_harmony["voices"][0]["pitch"]["motives"] = {{60}};
        hard_harmony["require_harmonic_chord_tones"] = false;
        solve_wrapper(wrapper, hard_harmony);
        hard_harmony["require_harmonic_chord_tones"] = true;
        hard_harmony["voices"][0]["pitch"]["motives"] = {{67, 60}};
        hard_harmony["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/2","1/2"]])");
        hard_harmony["harmonic_domain"] = nlohmann::json::parse(
            R"([{"beat":0,"degree":"V7","key":"C","mode":"major"},{"beat":2,"degree":"I"}])");
        hard_harmony["rules"] = nlohmann::json::parse(
            R"([{"rule_type":"r-cadence","cadence_type":"PAC","beat_positions":[1,3],"target_voices":[0],"require_chord_tones":true}])");
        const auto sustained_cadence = nlohmann::json::parse(solve_wrapper(wrapper, hard_harmony).result_json);
        require(sustained_cadence["solutions"][0]["voice_rhythm_ticks"][0] == nlohmann::json::parse("[2,2]") &&
                sustained_cadence["solutions"][0]["voice_solutions"][0] == nlohmann::json::parse("[67,60]"),
                "Cadence at beat 3 constrains tonic that began at beat 2 and is still sounding");

            auto step_config = nlohmann::json::parse(
                R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"exact","num_voices":1,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60,62]]},"rhythm":{"motives":[["1/2","1/2"]]}}],"rules":[{"id":"active-step","rule_type":"r-melodic-step","max_step":2,"target_voices":[0]}]})");
            const auto step_result = solve_wrapper(wrapper, step_config);
            require(step_result.dynamic_rules_post_failed == 0 && step_result.dynamic_rules_posted_ok == 1,
                "Measure melodic-step hard rule is posted successfully");
            require(nlohmann::json::parse(step_result.result_json)["solutions"][0]["voice_solutions"][0] ==
                nlohmann::json::parse("[60,62]"), "Inactive tails do not violate the melodic-step rule");
            step_config["rules"][0]["max_step"] = 1;
            solve_wrapper(wrapper, step_config, false);
            step_config["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/2","-1/2"]])");
            solve_wrapper(wrapper, step_config);
            step_config["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["-1/2","1/2"]])");
            solve_wrapper(wrapper, step_config);
            step_config["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/2","1/2"]])");
            step_config["rules"][0]["heuristic"] = true;
            step_config["search_options"] = {{"value_order", "heuristic"}};
            solve_wrapper(wrapper, step_config);

            auto scoring_space = std::make_unique<IntegratedMusicalSpace>(4, 1,
                VariableBranchingStrategy::INPUT_ORDER, ValueSelectionStrategy::MIN,
                std::vector<std::vector<int>>{{60, 62, 65, 72}}, std::vector<std::vector<int>>{{-1, 1, 2}}, 4, false, 123);
            rel(*scoring_space, scoring_space->get_absolute_vars()[0], IRT_EQ, 60);
            rel(*scoring_space, scoring_space->get_rhythm_vars()[0], IRT_EQ, 2);
            rel(*scoring_space, scoring_space->get_rhythm_vars()[1], IRT_EQ, 2);
            require(scoring_space->status() != SS_FAILED, "Scoring space is satisfiable");
            DynamicRules::ConstraintContext scoring_context(scoring_space.get(), &scoring_space->get_absolute_vars(),
                &scoring_space->get_rhythm_vars(), 1, 4);
            auto heuristic_step = DynamicRules::compile_melodic_step({0}, 2, true, "soft-step");
            require(heuristic_step->evaluate_score(scoring_context, {0, 1, 62}) == 1.0 &&
                heuristic_step->evaluate_score(scoring_context, {0, 1, 65}) == 0.3 &&
                heuristic_step->evaluate_score(scoring_context, {0, 1, 72}) == 0.0,
                "Soft melodic-step scoring distinguishes step, skip and leap");
            require(heuristic_step->evaluate_score(scoring_context, {0, 2, -1}) == 0.0,
                "Inactive pitch candidates are not scored as melody");

            for (const std::string pattern : {"r-no-interval-repetition", "r-no-consecutive-equal", "r-no-consecutive-direction"}) {
                auto pattern_config = step_config;
                pattern_config.erase("search_options");
                pattern_config["rules"] = nlohmann::json::array({
                    {{"rule_type", pattern}, {"target_voices", {0}}, {"max_consecutive", 2}}});
                pattern_config["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/4","1/4","1/2"]])");
                pattern_config["voices"][0]["pitch"]["motives"] = pattern == "r-no-consecutive-equal"
                    ? nlohmann::json::parse("[[60,60,62]]") : nlohmann::json::parse("[[60,62,60]]");
                solve_wrapper(wrapper, pattern_config);
                pattern_config["voices"][0]["pitch"]["motives"] = pattern == "r-no-consecutive-equal"
                    ? nlohmann::json::parse("[[60,60,60]]") : nlohmann::json::parse("[[60,62,64]]");
                solve_wrapper(wrapper, pattern_config, false);
                pattern_config["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/4","-1/4","1/2"]])");
                solve_wrapper(wrapper, pattern_config);
                pattern_config["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/4","1/4","1/2"]])");
                pattern_config["rules"][0]["heuristic"] = true;
                pattern_config["search_options"] = {{"value_order", "heuristic"}};
                solve_wrapper(wrapper, pattern_config);
            }
            auto absolute_config = step_config;
            absolute_config.erase("search_options");
            absolute_config["rules"] = nlohmann::json::parse(
                R"([{"rule_type":"r-no-interval-repetition","interval_mode":"absolute","target_voices":[0]}])");
            absolute_config["voices"][0]["pitch"]["motives"] = nlohmann::json::parse("[[60,62,60]]");
            absolute_config["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/4","1/4","1/2"]])");
            solve_wrapper(wrapper, absolute_config, false);
            absolute_config["voices"][0]["pitch"]["motives"] = nlohmann::json::parse("[[60,62,65]]");
            solve_wrapper(wrapper, absolute_config);

            auto pattern_score_space = std::make_unique<IntegratedMusicalSpace>(4, 1,
                VariableBranchingStrategy::INPUT_ORDER, ValueSelectionStrategy::MIN,
                std::vector<std::vector<int>>{{60, 62, 64, 65}}, std::vector<std::vector<int>>{{1}}, 4, true, 123);
            rel(*pattern_score_space, pattern_score_space->get_absolute_vars()[0], IRT_EQ, 60);
            rel(*pattern_score_space, pattern_score_space->get_absolute_vars()[1], IRT_EQ, 62);
            require(pattern_score_space->status() != SS_FAILED, "Pattern scoring space is satisfiable");
            DynamicRules::ConstraintContext pattern_context(pattern_score_space.get(), &pattern_score_space->get_absolute_vars(),
                &pattern_score_space->get_rhythm_vars(), 1, 4);
            auto interval_score = DynamicRules::compile_measure_melodic_pattern({0}, "r-no-interval-repetition", "interval-score", true);
            auto absolute_score = DynamicRules::compile_measure_melodic_pattern({0}, "r-no-interval-repetition", "absolute-score", true, 2, true);
            auto direction_score = DynamicRules::compile_measure_melodic_pattern({0}, "r-no-consecutive-direction", "direction-score", true);
            require(interval_score->evaluate_score(pattern_context, {0, 2, 64}) == 0.0 &&
                    interval_score->evaluate_score(pattern_context, {0, 2, 60}) == 1.0 &&
                    absolute_score->evaluate_score(pattern_context, {0, 2, 60}) == 0.0 &&
                    absolute_score->evaluate_score(pattern_context, {0, 2, 65}) == 1.0 &&
                    direction_score->evaluate_score(pattern_context, {0, 2, 64}) == 0.0 &&
                    direction_score->evaluate_score(pattern_context, {0, 2, 60}) == 1.0,
                    "Pattern heuristics agree with signed/absolute interval and direction hard predicates");
            auto repeated_score = DynamicRules::compile_measure_melodic_pattern({0}, "r-no-consecutive-equal", "repeat-score", true, 1);
            require(repeated_score->evaluate_score(pattern_context, {0, 2, 62}) == 0.0 &&
                    repeated_score->evaluate_score(pattern_context, {0, 2, 64}) == 1.0,
                    "Repeated-note heuristic distinguishes repeated and changed active notes");

            auto rest_score_space = std::make_unique<IntegratedMusicalSpace>(4, 1,
                VariableBranchingStrategy::INPUT_ORDER, ValueSelectionStrategy::MIN,
                std::vector<std::vector<int>>{{60, 62, 64}}, std::vector<std::vector<int>>{{-1, 1, 2}}, 4, true, 123);
            rel(*rest_score_space, rest_score_space->get_rhythm_vars()[0], IRT_EQ, -1);
            rel(*rest_score_space, rest_score_space->get_rhythm_vars()[1], IRT_EQ, 1);
            rel(*rest_score_space, rest_score_space->get_rhythm_vars()[2], IRT_EQ, 2);
            rel(*rest_score_space, rest_score_space->get_absolute_vars()[0], IRT_EQ, 60);
            rel(*rest_score_space, rest_score_space->get_absolute_vars()[1], IRT_EQ, 62);
            require(rest_score_space->status() != SS_FAILED, "Rest scoring space is satisfiable");
            DynamicRules::ConstraintContext rest_context(rest_score_space.get(), &rest_score_space->get_absolute_vars(),
                &rest_score_space->get_rhythm_vars(), 1, 4);
            require(heuristic_step->evaluate_score(rest_context, {0, 1, 64}) == 0.0 &&
                    interval_score->evaluate_score(rest_context, {0, 2, 64}) == 0.0 &&
                    direction_score->evaluate_score(rest_context, {0, 2, 64}) == 0.0 &&
                    repeated_score->evaluate_score(rest_context, {0, 1, 60}) == 0.0 &&
                    repeated_score->evaluate_score(rest_context, {0, 3, -1}) == 0.0,
                    "Rests break soft melodic windows and inactive tails receive neutral scores");
            auto malformed_rule = step_config;
            malformed_rule["rules"] = nlohmann::json::parse(
                R"([{"rule_type":"r-no-consecutive-direction","direction":"invalid","target_voices":[0]}])");
            std::string malformed_error;
            require(!wrapper.configure_from_json(malformed_rule.dump(), malformed_error) &&
                    malformed_error.find("invalid melodic pattern parameters") != std::string::npos,
                    "Measure-mode compilation errors cannot be swallowed by the wrapper");

            MusicalConstraintSolver::SolverConfig failed_post_config;
            failed_post_config.sequence_length = 4;
            failed_post_config.num_voices = 1;
            failed_post_config.voice_domains = {{60}};
            failed_post_config.voice_rhythm_domains = {{1}};
            failed_post_config.rhythm_base = 4;
            failed_post_config.measure_count_mode = true;
            failed_post_config.measure_count_plan = MeasureCountPlan::parse(step_config);
            failed_post_config.measure_horizon_ticks = 4;
            MusicalConstraintSolver::Solver failed_post_solver;
            failed_post_solver.configure(failed_post_config);
            auto failed_rule = std::make_unique<DynamicRules::CompiledConstraint>("throwing-hard-rule", "test posting failure");
            failed_rule->post_constraint = [](DynamicRules::ConstraintContext&) {
                throw std::runtime_error("deliberate posting failure");
            };
            failed_post_solver.apply_compiled_constraint(std::move(failed_rule));
            const auto failed_post_results = failed_post_solver.solve_multiple(1);
            require(failed_post_results.size() == 1 && !failed_post_results.front().found_solution &&
                failed_post_results.front().failure_reason.find("throwing-hard-rule") != std::string::npos &&
                failed_post_solver.last_search_stats().completion_reason == "error",
                "A failed hard rule returns an explicit error, not a measure-mode solution");
            require(failed_post_solver.get_dynamic_rule_post_failed_count() == 1, "Failed posting is counted explicitly");

            auto cross_config = nlohmann::json::parse(
                R"({"solution_length":"measure_count","measure_count":1,"measure_plan":[{"bars":1,"time_signature":"4/4"}],"measure_fill_policy":"pad","num_voices":2,"export_json":false,"export_txt":false,"export_xml":false,"voices":[{"pitch":{"motives":[[60]]},"rhythm":{"motives":[["1/4"]]}},{"pitch":{"motives":[[48]]},"rhythm":{"motives":[["1/4"]]}}],"rules":[{"rule_type":"r-rhythm-rhythm","constraint":"isorhythm","target_voices":[0,1]}]})");
            for (const std::string mode : {"isorhythm", "abs_isorhythm", "augmentation", "diminution", "no_simultaneous_rests", "rest_complement"}) {
                auto input = cross_config;
                input["rules"][0]["constraint"] = mode;
                const bool rest_mode = mode == "no_simultaneous_rests" || mode == "rest_complement";
                const std::string first = mode == "augmentation" ? "1/2" : (rest_mode ? "-1/4" : "1/4");
                const std::string second = mode == "diminution" ? "1/2" : (mode == "abs_isorhythm" ? "-1/4" : "1/4");
                input["voices"][0]["rhythm"]["motives"] = nlohmann::json::array({nlohmann::json::array({first})});
                input["voices"][1]["rhythm"]["motives"] = nlohmann::json::array({nlohmann::json::array({second})});
                solve_wrapper(wrapper, input);
                input["voices"][1]["rhythm"]["motives"] = nlohmann::json::parse(rest_mode ? R"([["-1/4"]])" : R"([["3/4"]])");
                solve_wrapper(wrapper, input, false);
                input["rules"][0]["heuristic"] = true;
                input["search_options"] = {{"value_order", "heuristic"}};
                solve_wrapper(wrapper, input);
            }
            cross_config["rules"][0]["constraint"] = "augmentation";
            cross_config["rules"][0]["parameters"] = {1.5};
            cross_config["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["3/8"]])");
            cross_config["voices"][1]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/4"]])");
            solve_wrapper(wrapper, cross_config);
            auto verbose_rhythm = cross_config;
            verbose_rhythm["rules"][0].erase("constraint");
            verbose_rhythm["rules"][0].erase("parameters");
            verbose_rhythm["rules"][0]["constraint_function"] = {{"type", "builtin"}, {"function", "augmentation"}, {"parameters", {1.5}}};
            solve_wrapper(wrapper, verbose_rhythm);
            verbose_rhythm["rules"][0]["constraint_function"]["parameters"] = {2};
            solve_wrapper(wrapper, verbose_rhythm, false);
            cross_config["rules"][0]["parameters"] = {0.5};
            cross_config["voices"][0]["rhythm"]["motives"] = nlohmann::json::parse(R"([["1/8"]])");
            solve_wrapper(wrapper, cross_config);

            auto ratio_space = std::make_unique<IntegratedMusicalSpace>(2, 2,
                VariableBranchingStrategy::INPUT_ORDER, ValueSelectionStrategy::MIN,
                std::vector<std::vector<int>>{{60}, {48}}, std::vector<std::vector<int>>{{1, 2}, {1}}, 2, false, 123);
            rel(*ratio_space, ratio_space->get_rhythm_vars()[0], IRT_EQ, 2);
            rel(*ratio_space, ratio_space->get_rhythm_vars()[2], IRT_EQ, 1);
            require(ratio_space->status() != SS_FAILED, "Ratio scoring fixture is satisfiable");
            DynamicRules::ConstraintContext ratio_context(ratio_space.get(), &ratio_space->get_absolute_vars(), &ratio_space->get_rhythm_vars(), 2, 2);
            auto ratio_rule = nlohmann::json::parse(R"({"constraint":"augmentation","parameters":[2],"target_voices":[0,1],"heuristic":true})");
            auto ratio_score = CrossVoiceRules::compile_rhythm(ratio_rule, 2, 2);
            require(ratio_score->evaluate_score(ratio_context, {0, 0, 2}) == 1.0 &&
                    ratio_score->evaluate_score(ratio_context, {1, 0, 1}) == 1.0 &&
                    ratio_score->evaluate_score(ratio_context, {1, 0, -1}) == -1.0 &&
                    ratio_score->evaluate_score(ratio_context, {0, 1, 0}) == 0.0,
                    "Rhythm heuristic respects voice orientation, rest signs and inactive candidates");
                for (const auto& invalid_rule : {
                    R"({"rule_type":"r-rhythm-rhythm","constraint":"augmentation","parameters":[0],"target_voices":[0,1]})",
                    R"({"rule_type":"r-rhythm-rhythm","constraint":"unknown","target_voices":[0,1]})",
                    R"({"rule_type":"r-pitch-pitch","constraint":"interval_class","parameters":[12],"target_voices":[0,1]})",
                    R"({"rule_type":"r-pitch-pitch","constraint":"interval_range","parameters":[12,5],"target_voices":[0,1]})",
                    R"({"rule_type":"r-pitch-pitch","constraint":"same_pitch","indices":[99],"target_voices":[0,1]})",
                    R"({"rule_type":"r-pitch-pitch","constraint":"same_pitch","stride":0,"target_voices":[0,1]})"}) {
                    auto invalid_config = cross_config;
                    invalid_config["rules"] = nlohmann::json::array({nlohmann::json::parse(invalid_rule)});
                    std::string error;
                    require(!wrapper.configure_from_json(invalid_config.dump(), error) && !error.empty(),
                        "Malformed cross-voice rule is rejected rather than silently skipped");
                }

                std::cout << "PASS: active rules, motives, harmony, cadences, tendency resolution, score-time relationships, metric hierarchy, dynamic/wildcard expressions, note/rest semantics, profiling, cloning and fail-closed posting\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}