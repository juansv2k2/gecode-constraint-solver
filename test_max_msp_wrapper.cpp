#include "max_msp_solver_wrapper.hh"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config.json>" << std::endl;
        return 1;
    }

    std::ifstream f(argv[1]);
    if (!f) {
        std::cerr << "Cannot open config file: " << argv[1] << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << f.rdbuf();

    MaxMSPWrapper::AsyncSolverWrapper wrapper;

    std::string error;
    if (!wrapper.configure_from_json(buffer.str(), error)) {
        std::cerr << "Configure failed: " << error << std::endl;
        return 1;
    }

    if (!wrapper.solve_async(error)) {
        std::cerr << "Failed to start async solve: " << error << std::endl;
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
            std::cerr << "Timed out waiting for wrapper result" << std::endl;
            return 2;
        }
    }

    std::cout << "status=" << wrapper.get_status_string() << std::endl;
    std::cout << "found_solution=" << (result.found_solution ? "true" : "false") << std::endl;
    std::cout << "message=" << result.message << std::endl;
    if (!result.result_json.empty()) {
        std::cout << result.result_json << std::endl;
    }

    return result.found_solution ? 0 : 3;
}
