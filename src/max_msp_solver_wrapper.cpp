#include "max_msp_solver_wrapper.hh"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

namespace MaxMSPWrapper {
namespace {

int gcd_int(int a, int b) {
    a = std::abs(a);
    b = std::abs(b);
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a == 0 ? 1 : a;
}

int lcm_int(int a, int b) {
    if (a == 0 || b == 0) return 0;
    return std::abs(a / gcd_int(a, b) * b);
}

int parse_duration_denominator(const std::string& s) {
    // Accepted forms: "1/4", "-1/8", "3/8".
    const std::string trimmed = s;
    const std::string core = (!trimmed.empty() && trimmed[0] == '-') ? trimmed.substr(1) : trimmed;

    const auto slash = core.find('/');
    if (slash == std::string::npos) {
        throw std::runtime_error("Invalid duration value (missing '/'): " + s);
    }

    const int num = std::stoi(core.substr(0, slash));
    const int den = std::stoi(core.substr(slash + 1));
    if (num <= 0 || den <= 0) {
        throw std::runtime_error("Invalid duration fraction: " + s);
    }
    return den;
}

int parse_duration_to_ticks(const std::string& s, int rhythm_base) {
    const bool is_rest = !s.empty() && s[0] == '-';
    const std::string core = is_rest ? s.substr(1) : s;

    const auto slash = core.find('/');
    if (slash == std::string::npos) {
        throw std::runtime_error("Invalid duration value (missing '/'): " + s);
    }

    const int num = std::stoi(core.substr(0, slash));
    const int den = std::stoi(core.substr(slash + 1));
    if (num <= 0 || den <= 0) {
        throw std::runtime_error("Invalid duration fraction: " + s);
    }

    if ((rhythm_base % den) != 0) {
        throw std::runtime_error("Duration denominator does not divide rhythm base: " + s);
    }

    const int ticks = num * (rhythm_base / den);
    return is_rest ? -ticks : ticks;
}

AdvancedBackjumping::BackjumpMode parse_backjump_mode(const nlohmann::json& cfg) {
    const std::string method = cfg.value("backtrack_method", std::string("intelligent"));
    if (method == "none") return AdvancedBackjumping::BackjumpMode::NO_BACKJUMPING;
    if (method == "simple") return AdvancedBackjumping::BackjumpMode::NO_BACKJUMPING;
    if (method == "consensus") return AdvancedBackjumping::BackjumpMode::CONSENSUS_BACKJUMP;
    return AdvancedBackjumping::BackjumpMode::INTELLIGENT_BACKJUMP;
}

std::string status_to_string(SolveStatus status) {
    switch (status) {
        case SolveStatus::UNCONFIGURED: return "unconfigured";
        case SolveStatus::IDLE: return "idle";
        case SolveStatus::RUNNING: return "running";
        case SolveStatus::SUCCESS: return "success";
        case SolveStatus::FAILED: return "failed";
        case SolveStatus::CANCELLED: return "cancelled";
    }
    return "unknown";
}

} // namespace

AsyncSolverWrapper::AsyncSolverWrapper()
    : solver_(), configured_(false), cancel_requested_(false), result_ready_(false),
      status_(SolveStatus::UNCONFIGURED) {}

AsyncSolverWrapper::~AsyncSolverWrapper() {
    request_cancel();
    join_worker_if_needed();
}

void AsyncSolverWrapper::join_worker_if_needed() {
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

bool AsyncSolverWrapper::configure_from_json(const std::string& config_json, std::string& error_message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (status_ == SolveStatus::RUNNING) {
        error_message = "Cannot reconfigure while solve is running";
        return false;
    }

    join_worker_if_needed();

    if (!apply_config_json(config_json, error_message)) {
        configured_ = false;
        status_ = SolveStatus::UNCONFIGURED;
        return false;
    }

    configured_ = true;
    status_ = SolveStatus::IDLE;
    cancel_requested_.store(false);
    result_ready_.store(false);
    last_config_json_ = config_json;

    last_result_ = SolveResult{};
    last_result_.status = SolveStatus::IDLE;
    last_result_.message = "Configured successfully";

    return true;
}

bool AsyncSolverWrapper::solve_async(std::string& error_message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!configured_) {
        error_message = "Wrapper is not configured";
        return false;
    }

    if (status_ == SolveStatus::RUNNING) {
        error_message = "Solve already running";
        return false;
    }

    join_worker_if_needed();

    cancel_requested_.store(false);
    result_ready_.store(false);
    status_ = SolveStatus::RUNNING;

    worker_thread_ = std::thread(&AsyncSolverWrapper::run_solve_job, this);
    return true;
}

void AsyncSolverWrapper::request_cancel() {
    cancel_requested_.store(true);
}

SolveStatus AsyncSolverWrapper::get_status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

std::string AsyncSolverWrapper::get_status_string() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_to_string(status_);
}

bool AsyncSolverWrapper::is_running() const {
    return get_status() == SolveStatus::RUNNING;
}

bool AsyncSolverWrapper::take_result(SolveResult& out_result) {
    if (!result_ready_.load()) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    if (!result_ready_.load()) return false;

    out_result = last_result_;
    result_ready_.store(false);
    return true;
}

std::string AsyncSolverWrapper::get_status_json() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j;
    j["status"] = status_to_string(status_);
    j["configured"] = configured_;
    j["running"] = (status_ == SolveStatus::RUNNING);
    j["result_ready"] = result_ready_.load();
    j["last_message"] = last_result_.message;
    return j.dump();
}

void AsyncSolverWrapper::run_solve_job() {
    SolveResult local_result;

    if (cancel_requested_.load()) {
        local_result.status = SolveStatus::CANCELLED;
        local_result.message = "Solve cancelled before start";
    } else {
        try {
            MusicalConstraintSolver::MusicalSolution solution = solver_.solve();

            if (cancel_requested_.load()) {
                local_result.status = SolveStatus::CANCELLED;
                local_result.message = "Solve cancelled";
            } else if (!solution.found_solution) {
                local_result.status = SolveStatus::FAILED;
                local_result.found_solution = false;
                local_result.message = solution.failure_reason.empty()
                    ? "No solution found"
                    : solution.failure_reason;
            } else {
                local_result.status = SolveStatus::SUCCESS;
                local_result.found_solution = true;
                local_result.message = "Solution found";

                local_result.voice_solutions = solution.voice_solutions;
                local_result.voice_rhythms = solution.voice_rhythms;
                local_result.solve_time_ms = solution.solve_time_ms;
                local_result.backjumps_performed = solution.backjumps_performed;
                local_result.total_rules_checked = solution.total_rules_checked;

                nlohmann::json j;
                j["found_solution"] = true;
                j["solve_time_ms"] = solution.solve_time_ms;
                j["backjumps_performed"] = solution.backjumps_performed;
                j["total_rules_checked"] = solution.total_rules_checked;
                j["voice_solutions"] = solution.voice_solutions;
                j["voice_rhythms"] = solution.voice_rhythms;
                j["metric_signature"] = solution.metric_signature;
                local_result.result_json = j.dump();
            }
        } catch (const std::exception& e) {
            local_result.status = SolveStatus::FAILED;
            local_result.found_solution = false;
            local_result.message = std::string("Exception during solve: ") + e.what();
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_result_ = local_result;
        status_ = local_result.status;
    }
    result_ready_.store(true);
}

bool AsyncSolverWrapper::apply_config_json(const std::string& config_json, std::string& error_message) {
    try {
        const nlohmann::json cfg = nlohmann::json::parse(config_json);

        MusicalConstraintSolver::SolverConfig sc;
        sc.sequence_length = cfg.value("solution_length", 12);
        sc.num_voices = cfg.value("num_voices", 2);
        sc.backjump_mode = parse_backjump_mode(cfg);
        sc.verbose_output = false;

        if (cfg.contains("search_options") && cfg["search_options"].is_object()) {
            const auto& so = cfg["search_options"];
            if (so.contains("random_seed") && so["random_seed"].is_number_unsigned()) {
                sc.random_seed = so["random_seed"].get<unsigned int>();
            } else if (so.contains("random_seed") && so["random_seed"].is_number_integer()) {
                int s = so["random_seed"].get<int>();
                sc.random_seed = s < 0 ? 0u : static_cast<unsigned int>(s);
            }

            if (so.contains("timeout_ms") && so["timeout_ms"].is_number()) {
                sc.timeout_seconds = so["timeout_ms"].get<double>() / 1000.0;
            }
        }

        if (cfg.contains("timeout_ms") && cfg["timeout_ms"].is_number()) {
            sc.timeout_seconds = cfg["timeout_ms"].get<double>() / 1000.0;
        }

        if (!cfg.contains("engine_domains") || !cfg["engine_domains"].is_object()) {
            throw std::runtime_error("Missing required object: engine_domains");
        }

        sc.voice_domains.assign(sc.num_voices, {});
        std::vector<std::vector<std::string>> rhythm_strings(sc.num_voices);

        const auto& engine_domains = cfg["engine_domains"];
        for (auto it = engine_domains.begin(); it != engine_domains.end(); ++it) {
            const auto& domain = it.value();
            if (!domain.is_object()) continue;

            const int voice = domain.value("voice", -1);
            if (voice < 0 || voice >= sc.num_voices) continue;

            const std::string type = domain.value("type", std::string(""));
            if (type == "pitch") {
                if (domain.contains("midi_values") && domain["midi_values"].is_array()) {
                    for (const auto& v : domain["midi_values"]) {
                        if (v.is_number_integer()) {
                            sc.voice_domains[voice].push_back(v.get<int>());
                        }
                    }
                }
            } else if (type == "rhythm") {
                if (domain.contains("duration_values") && domain["duration_values"].is_array()) {
                    for (const auto& dv : domain["duration_values"]) {
                        if (dv.is_string()) {
                            rhythm_strings[voice].push_back(dv.get<std::string>());
                        }
                    }
                }
            }
        }

        // Validate pitch domains and compute global pitch range.
        int global_min = 127;
        int global_max = 0;
        for (int v = 0; v < sc.num_voices; ++v) {
            auto& d = sc.voice_domains[v];
            if (d.empty()) {
                throw std::runtime_error("Missing pitch domain for voice " + std::to_string(v));
            }
            std::sort(d.begin(), d.end());
            d.erase(std::unique(d.begin(), d.end()), d.end());
            global_min = std::min(global_min, d.front());
            global_max = std::max(global_max, d.back());
        }
        sc.min_note = global_min;
        sc.max_note = global_max;

        // Compute rhythm base and convert all duration strings to integer ticks.
        int rhythm_base = 1;
        for (int v = 0; v < sc.num_voices; ++v) {
            if (rhythm_strings[v].empty()) {
                throw std::runtime_error("Missing rhythm domain for voice " + std::to_string(v));
            }
            for (const auto& s : rhythm_strings[v]) {
                rhythm_base = lcm_int(rhythm_base, parse_duration_denominator(s));
            }
        }

        sc.rhythm_base = rhythm_base;
        sc.voice_rhythm_domains.assign(sc.num_voices, {});
        for (int v = 0; v < sc.num_voices; ++v) {
            for (const auto& s : rhythm_strings[v]) {
                sc.voice_rhythm_domains[v].push_back(parse_duration_to_ticks(s, rhythm_base));
            }
            auto& rd = sc.voice_rhythm_domains[v];
            std::sort(rd.begin(), rd.end());
            rd.erase(std::unique(rd.begin(), rd.end()), rd.end());
        }

        solver_.configure(sc);
        solver_.clear_rules();
        solver_.clear_dynamic_rules();

        // Legacy rules[] handling (rule_type / constraint_function.function, etc.)
        if (cfg.contains("rules") && cfg["rules"].is_array()) {
            for (const auto& r : cfg["rules"]) {
                if (!r.is_object()) continue;
                if (!r.value("enabled", true)) continue;

                const std::string rule_type = r.value("rule_type", std::string(""));
                std::string function;
                if (r.contains("constraint_function") && r["constraint_function"].is_object()) {
                    function = r["constraint_function"].value("function", std::string(""));
                }

                std::vector<int> indices;
                if (r.contains("indices") && r["indices"].is_array()) {
                    for (const auto& idx : r["indices"]) {
                        if (idx.is_number_integer()) indices.push_back(idx.get<int>());
                    }
                }

                int target_engine = r.value("target_engine", -1);
                std::vector<int> target_engines;
                if (r.contains("target_engines") && r["target_engines"].is_array()) {
                    for (const auto& te : r["target_engines"]) {
                        if (te.is_number_integer()) target_engines.push_back(te.get<int>());
                    }
                }

                std::string engine_type = r.value("engine_type", std::string(""));
                std::string description = r.value("description", std::string(""));

                std::vector<double> parameters;
                if (r.contains("constraint_function") &&
                    r["constraint_function"].is_object() &&
                    r["constraint_function"].contains("parameters") &&
                    r["constraint_function"]["parameters"].is_array()) {
                    for (const auto& p : r["constraint_function"]["parameters"]) {
                        if (p.is_number()) parameters.push_back(p.get<double>());
                    }
                }

                solver_.add_rule_config(rule_type, function, indices, target_engine,
                                        target_engines, engine_type, description, parameters);
            }
        }

        // Dynamic rules path.
        if (cfg.contains("dynamic_rules") && cfg["dynamic_rules"].is_array()) {
            for (const auto& dr : cfg["dynamic_rules"]) {
                solver_.add_dynamic_rule(dr);
            }
        }

        return true;
    } catch (const std::exception& e) {
        error_message = e.what();
        return false;
    }
}

} // namespace MaxMSPWrapper
