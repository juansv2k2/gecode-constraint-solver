#include "max_msp_solver_wrapper.hh"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

namespace {

using json = nlohmann::json;

std::string read_file_or_throw(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open config file: " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

MaxMSPWrapper::SolveResult solve_config_or_throw(const std::string& config_path) {
    MaxMSPWrapper::AsyncSolverWrapper wrapper;
    std::string error;
    if (!wrapper.configure_from_json(read_file_or_throw(config_path), error)) {
        throw std::runtime_error("configure_from_json failed for " + config_path + ": " + error);
    }
    if (!wrapper.solve_async(error)) {
        throw std::runtime_error("solve_async failed for " + config_path + ": " + error);
    }

    auto start = std::chrono::steady_clock::now();
    MaxMSPWrapper::SolveResult result;
    while (!wrapper.take_result(result)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > 30) {
            wrapper.request_cancel();
            throw std::runtime_error("Timed out waiting for result for " + config_path);
        }
    }
    return result;
}

void assert_true(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_metric_timepoint_success() {
    const std::string config_path = "configs/metric_domain_example.json";
    const auto result = solve_config_or_throw(config_path);

    assert_true(result.found_solution, "Expected solution for " + config_path + ", got failure: " + result.message);
    assert_true(!result.result_json.empty(), "Expected non-empty result JSON for " + config_path);

    const auto payload = json::parse(result.result_json);
    const auto& solution = payload.at("solutions").at(0);
    const auto& timeline = solution.at("score").at("metric_timeline");

    assert_true(timeline.size() == 3, "Expected three metric segments in " + config_path);
    assert_true(timeline.at(0).at("start_tick") == 0, "Expected first segment to start at tick 0");
    assert_true(timeline.at(0).at("end_tick") == 8, "Expected first segment to end at tick 8");
    assert_true(timeline.at(1).at("start_tick") == 8, "Expected second segment to start at tick 8");
    assert_true(timeline.at(1).at("end_tick") == 14, "Expected second segment to end at tick 14");
    assert_true(timeline.at(2).at("start_tick") == 14, "Expected third segment to start at tick 14");
    assert_true(timeline.at(2).at("end_tick") == 20, "Expected third segment to end at tick 20");

    assert_true(timeline.at(0).contains("start_index"), "Expected compatibility field start_index on metric segment");
    assert_true(timeline.at(0).contains("end_index"), "Expected compatibility field end_index on metric segment");
    assert_true(timeline.at(2).at("end_index") == 16, "Expected last segment compatibility end_index to match 17-event example");

    const auto numerators = solution.at("metric_signature_numerators").get<std::vector<int>>();
    assert_true(numerators.size() == 17, "Expected 17 projected metric numerators for example config");
    assert_true(numerators.at(0) == 4 && numerators.at(7) == 4, "Expected first segment numerator projection to be 4");
    assert_true(numerators.at(8) == 3 && numerators.at(13) == 3, "Expected second segment numerator projection to be 3");
    assert_true(numerators.at(14) == 6 && numerators.at(16) == 6, "Expected third segment numerator projection to be 6");
}

void test_metric_timepoint_exact_fill_success() {
    const std::string config_path = "configs/metric_domain_exact_fill_success.json";
    const auto result = solve_config_or_throw(config_path);

    assert_true(result.found_solution, "Expected solution for " + config_path + ", got failure: " + result.message);
    const auto payload = json::parse(result.result_json);
    const auto& solution = payload.at("solutions").at(0);
    const auto& timeline = solution.at("score").at("metric_timeline");
    const auto numerators = solution.at("metric_signature_numerators").get<std::vector<int>>();

    assert_true(timeline.at(2).at("end_tick") == 20, "Expected exact-fill config to end at tick 20");
    assert_true(timeline.at(2).at("end_index") == 19, "Expected exact-fill config compatibility end_index to be 19");
    assert_true(numerators.size() == 20, "Expected 20 projected metric numerators for exact-fill config");
    assert_true(numerators.at(19) == 6, "Expected exact-fill final event to remain in 6/8 segment");
}

void test_metric_timepoint_failures() {
    {
        const std::string config_path = "configs/metric_domain_bad_timepoints_descending.json";
        const auto result = solve_config_or_throw(config_path);
        assert_true(!result.found_solution, "Expected failure for descending timepoints fixture");
        assert_true(result.message.find("strictly increasing") != std::string::npos,
                    "Expected descending-timepoints error message to mention strictly increasing");
    }

    {
        const std::string config_path = "configs/metric_domain_bad_timepoints_out_of_range.json";
        const auto result = solve_config_or_throw(config_path);
        assert_true(!result.found_solution, "Expected failure for out-of-range timepoints fixture");
        assert_true(result.message.find("outside score_length") != std::string::npos,
                    "Expected out-of-range error message to mention score_length bounds");
    }
}

} // namespace

int main() {
    try {
        std::cout << "Running metric timepoint regression tests..." << std::endl;
        test_metric_timepoint_success();
        test_metric_timepoint_exact_fill_success();
        test_metric_timepoint_failures();
        std::cout << "Metric timepoint regression tests passed." << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Metric timepoint regression tests failed: " << ex.what() << std::endl;
        return 1;
    }
}