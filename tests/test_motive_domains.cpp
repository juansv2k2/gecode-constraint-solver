#include "gecode_cluster_integration.hh"
#include "motive_domain_parser.hh"
#include "measure_count_plan.hh"
#include "harmonic_domain_parser.hh"
#include "max_msp_solver_wrapper.hh"
#include <chrono>
#include <fstream>
#include <thread>
#include <iostream>
#include <stdexcept>

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

int main() {
    try {
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

        std::cout << "PASS: motive parser, DAG integrity, rests, cloning, UNSAT, Max wrapper, tuplets, metadata, reset, 16-bar measure mode, beat cadences, score padding\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}