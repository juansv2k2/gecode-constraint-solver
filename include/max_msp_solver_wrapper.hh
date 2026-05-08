/**
 * @file max_msp_solver_wrapper.hh
 * @brief Async wrapper boundary for Max/MSP integration.
 *
 * This layer intentionally avoids Max SDK symbols so it can be tested
 * independently and then consumed by a native Max external (.mxo).
 */

#ifndef MAX_MSP_SOLVER_WRAPPER_HH
#define MAX_MSP_SOLVER_WRAPPER_HH

#include "musical_constraint_solver.hh"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace MaxMSPWrapper {

enum class SolveStatus {
    UNCONFIGURED,
    IDLE,
    RUNNING,
    SUCCESS,
    FAILED,
    CANCELLED
};

struct SolveResult {
    SolveStatus status = SolveStatus::UNCONFIGURED;
    bool found_solution = false;
    std::string message;
    std::string result_json;

    std::vector<std::vector<int>> voice_solutions;
    std::vector<std::vector<int>> voice_rhythms;

    double solve_time_ms = 0.0;
    int backjumps_performed = 0;
    int total_rules_checked = 0;
    int rhythm_base = 1;
};

class AsyncSolverWrapper {
public:
    AsyncSolverWrapper();
    ~AsyncSolverWrapper();

    AsyncSolverWrapper(const AsyncSolverWrapper&) = delete;
    AsyncSolverWrapper& operator=(const AsyncSolverWrapper&) = delete;

    // Configure from a JSON string compatible with dynamic-solver configs.
    bool configure_from_json(const std::string& config_json, std::string& error_message);

    // Starts an asynchronous solve job. Returns false if wrapper is not ready.
    bool solve_async(std::string& error_message);

    // Cooperative cancel request. Current solve may finish if already deep in search.
    void request_cancel();

    SolveStatus get_status() const;
    std::string get_status_string() const;
    bool is_running() const;

    // Returns true once per solve completion and copies the latest result.
    bool take_result(SolveResult& out_result);

    // Convenience helper for Max dict/json outlet payloads.
    std::string get_status_json() const;

private:
    MusicalConstraintSolver::Solver solver_;
    bool configured_;
    int max_solutions_ = 1;
    double timeout_ms_ = 30000.0;

    std::string export_path_ = ".";
    std::string export_filename_;
    bool export_json_ = false;
    bool export_txt_ = false;
    bool export_xml_ = false;
    bool export_png_ = false;
    bool export_midi_ = false;

    mutable std::mutex mutex_;
    std::thread worker_thread_;
    std::atomic<bool> cancel_requested_;
    std::atomic<bool> result_ready_;

    SolveStatus status_;
    SolveResult last_result_;
    int rhythm_base_;

    std::string last_config_json_;

    void join_worker_if_needed();
    void run_solve_job();

    bool apply_config_json(const std::string& config_json, std::string& error_message);
};

} // namespace MaxMSPWrapper

#endif // MAX_MSP_SOLVER_WRAPPER_HH
