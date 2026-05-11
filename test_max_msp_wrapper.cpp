#include "max_msp_solver_wrapper.hh"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config1.json> [config2.json ...]" << std::endl;
        return 1;
    }

    MaxMSPWrapper::AsyncSolverWrapper wrapper;

    int last_exit_code = 0;

    for (int argi = 1; argi < argc; ++argi) {
        const char* config_path = argv[argi];

        std::ifstream f(config_path);
        if (!f) {
            std::cerr << "Cannot open config file: " << config_path << std::endl;
            return 1;
        }

        std::stringstream buffer;
        buffer << f.rdbuf();

        std::string error;
        if (!wrapper.configure_from_json(buffer.str(), error)) {
            std::cerr << "Configure failed for " << config_path << ": " << error << std::endl;
            return 1;
        }

        if (!wrapper.solve_async(error)) {
            std::cerr << "Failed to start async solve for " << config_path << ": " << error << std::endl;
            return 1;
        }

        auto start = std::chrono::steady_clock::now();
        MaxMSPWrapper::SolveResult result;
        while (!wrapper.take_result(result)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed > 120) {
                wrapper.request_cancel();
                std::cerr << "Timed out waiting for wrapper result for " << config_path << std::endl;
                return 2;
            }
        }

        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();

        std::cout << "config=" << config_path << std::endl;
        std::cout << "status=" << wrapper.get_status_string() << std::endl;
        std::cout << "rules_compile_status="
              << ((result.dynamic_rules_post_failed == 0) ? "ok" : "failed")
              << " (posted=" << result.dynamic_rules_posted_ok
              << ", failed=" << result.dynamic_rules_post_failed << ")" << std::endl;
        std::cout << "found_solution=" << (result.found_solution ? "true" : "false") << std::endl;
        std::cout << "message=" << result.message << std::endl;
        std::cout << "elapsed_ms=" << elapsed_ms << std::endl;
        if (!result.result_json.empty()) {
            std::cout << result.result_json << std::endl;
        }
        std::cout << std::endl;

        last_exit_code = result.found_solution ? 0 : 3;
    }

    return last_exit_code;
}
