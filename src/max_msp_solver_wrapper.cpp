#include "max_msp_solver_wrapper.hh"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <cctype>

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

std::string numeric_duration_to_fraction_string(double value) {
    if (!std::isfinite(value) || std::abs(value) < 1e-12) {
        throw std::runtime_error("Invalid numeric duration value");
    }

    const bool is_rest = value < 0.0;
    const double abs_value = std::abs(value);

    int best_num = 0;
    int best_den = 1;
    double best_err = std::numeric_limits<double>::infinity();

    // Find a rational approximation suitable for musical fractions.
    for (int den = 1; den <= 1024; ++den) {
        const int num = static_cast<int>(std::round(abs_value * den));
        if (num <= 0) continue;
        const double approx = static_cast<double>(num) / static_cast<double>(den);
        const double err = std::abs(approx - abs_value);
        if (err < best_err) {
            best_err = err;
            best_num = num;
            best_den = den;
            if (err < 1e-9) break;
        }
    }

    if (best_num <= 0 || best_den <= 0) {
        throw std::runtime_error("Could not convert numeric duration to fraction");
    }

    const int g = gcd_int(best_num, best_den);
    best_num /= g;
    best_den /= g;

    std::ostringstream oss;
    if (is_rest) oss << "-";
    oss << best_num << "/" << best_den;
    return oss.str();
}

std::string rhythm_ticks_to_fraction_string(int tick_value, int rhythm_base) {
    if (rhythm_base <= 0) {
        return std::to_string(tick_value);
    }

    if (tick_value == 0) {
        return "0";
    }

    const bool is_rest = tick_value < 0;
    const int abs_tick = is_rest ? -tick_value : tick_value;
    const int g = gcd_int(abs_tick, rhythm_base);
    const int num = abs_tick / g;
    const int den = rhythm_base / g;

    std::ostringstream oss;
    if (is_rest) oss << "-";
    oss << num << "/" << den;
    return oss.str();
}

std::string normalize_list_string(std::string s) {
    for (char& c : s) {
        if (c == '[' || c == ']' || c == ',' || c == ';') {
            c = ' ';
        }
    }
    return s;
}

std::vector<int> parse_int_list_from_string(const std::string& input) {
    std::vector<int> out;
    std::istringstream iss(normalize_list_string(input));
    std::string token;
    while (iss >> token) {
        // Trim wrapping quotes if present.
        if (token.size() >= 2 &&
            ((token.front() == '"' && token.back() == '"') ||
             (token.front() == '\'' && token.back() == '\''))) {
            token = token.substr(1, token.size() - 2);
        }
        try {
            size_t idx = 0;
            const long v = std::stol(token, &idx, 10);
            if (idx == token.size()) {
                out.push_back(static_cast<int>(v));
            }
        } catch (...) {
            // Ignore non-integer tokens.
        }
    }
    return out;
}

std::vector<std::string> parse_duration_list_from_string(const std::string& input) {
    std::vector<std::string> out;
    std::istringstream iss(normalize_list_string(input));
    std::string token;
    while (iss >> token) {
        if (token.size() >= 2 &&
            ((token.front() == '"' && token.back() == '"') ||
             (token.front() == '\'' && token.back() == '\''))) {
            token = token.substr(1, token.size() - 2);
        }
        if (!token.empty()) {
            out.push_back(token);
        }
    }
    return out;
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

bool json_value_to_bool(const nlohmann::json& value, bool default_value) {
    if (value.is_boolean()) {
        return value.get<bool>();
    }
    if (value.is_number_integer()) {
        return value.get<long long>() != 0;
    }
    if (value.is_number_unsigned()) {
        return value.get<unsigned long long>() != 0;
    }
    if (value.is_number_float()) {
        return std::abs(value.get<double>()) > std::numeric_limits<double>::epsilon();
    }
    if (value.is_string()) {
        const std::string text = value.get<std::string>();
        if (text == "true" || text == "1") return true;
        if (text == "false" || text == "0") return false;
    }
    return default_value;
}

std::vector<std::string> split_path_tokens(const std::string& key) {
    std::vector<std::string> tokens;
    std::string current;
    for (size_t i = 0; i < key.size(); ++i) {
        if (i + 1 < key.size() && key[i] == ':' && key[i + 1] == ':') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            ++i;
            continue;
        }
        if (key[i] == '.') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(key[i]);
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

void set_nested_json_path(nlohmann::json& root,
                          const std::vector<std::string>& tokens,
                          const nlohmann::json& value) {
    if (tokens.empty()) return;

    nlohmann::json* cur = &root;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];
        const bool last = (i + 1 == tokens.size());
        if (last) {
            (*cur)[t] = value;
        } else {
            if (!(*cur).contains(t) || !(*cur)[t].is_object()) {
                (*cur)[t] = nlohmann::json::object();
            }
            cur = &(*cur)[t];
        }
    }
}

bool is_non_negative_integer_string(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

nlohmann::json normalize_numeric_key_objects_to_arrays(const nlohmann::json& value) {
    if (value.is_array()) {
        nlohmann::json normalized = nlohmann::json::array();
        for (const auto& item : value) {
            normalized.push_back(normalize_numeric_key_objects_to_arrays(item));
        }
        return normalized;
    }

    if (!value.is_object()) {
        return value;
    }

    bool all_numeric_keys = !value.empty();
    size_t max_index = 0;
    for (auto it = value.begin(); it != value.end(); ++it) {
        if (!is_non_negative_integer_string(it.key())) {
            all_numeric_keys = false;
            break;
        }
        max_index = std::max(max_index, static_cast<size_t>(std::stoul(it.key())));
    }

    if (all_numeric_keys) {
        nlohmann::json normalized = nlohmann::json::array();
        for (size_t index = 0; index <= max_index; ++index) {
            const std::string key = std::to_string(index);
            if (value.contains(key)) {
                normalized.push_back(normalize_numeric_key_objects_to_arrays(value.at(key)));
            } else {
                normalized.push_back(nullptr);
            }
        }
        return normalized;
    }

    nlohmann::json normalized = nlohmann::json::object();
    for (auto it = value.begin(); it != value.end(); ++it) {
        normalized[it.key()] = normalize_numeric_key_objects_to_arrays(it.value());
    }
    return normalized;
}

void reconstruct_flattened_max_dict_paths(nlohmann::json& cfg) {
    if (!cfg.is_object()) return;

    nlohmann::json reconstructed = cfg;
    bool changed = false;

    for (auto it = cfg.begin(); it != cfg.end(); ++it) {
        const std::string key = it.key();
        if (key.find("::") == std::string::npos && key.find('.') == std::string::npos) {
            continue;
        }

        const std::vector<std::string> tokens = split_path_tokens(key);
        if (tokens.size() < 2) {
            continue;
        }

        set_nested_json_path(reconstructed, tokens, it.value());
        reconstructed.erase(key);
        changed = true;
    }

    cfg = normalize_numeric_key_objects_to_arrays(changed ? reconstructed : cfg);
}

} // namespace

AsyncSolverWrapper::AsyncSolverWrapper()
    : solver_(), configured_(false), cancel_requested_(false), result_ready_(false),
    status_(SolveStatus::UNCONFIGURED), rhythm_base_(1) {}

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
    last_result_.rhythm_base = rhythm_base_;

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
                local_result.rhythm_base = rhythm_base_;

                nlohmann::json j;
                j["found_solution"] = true;
                j["solve_time_ms"] = solution.solve_time_ms;
                j["backjumps_performed"] = solution.backjumps_performed;
                j["total_rules_checked"] = solution.total_rules_checked;
                j["voice_solutions"] = solution.voice_solutions;
                // Return rhythm durations in musical fraction form for Max consumers.
                nlohmann::json voice_rhythm_fractions = nlohmann::json::array();
                for (const auto& voice : solution.voice_rhythms) {
                    nlohmann::json v = nlohmann::json::array();
                    for (const int tick : voice) {
                        v.push_back(rhythm_ticks_to_fraction_string(tick, rhythm_base_));
                    }
                    voice_rhythm_fractions.push_back(v);
                }
                j["voice_rhythms"] = voice_rhythm_fractions;
                j["voice_rhythm_ticks"] = solution.voice_rhythms;

                // Report meter in musical notation while keeping raw values for compatibility.
                nlohmann::json metric_signature = nlohmann::json::array();
                for (const int numerator : solution.metric_signature) {
                    std::ostringstream ts;
                    ts << numerator << "/4";
                    metric_signature.push_back(ts.str());
                }
                j["metric_signature"] = metric_signature;
                j["metric_signature_numerators"] = solution.metric_signature;
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
        nlohmann::json cfg = nlohmann::json::parse(config_json);
        reconstruct_flattened_max_dict_paths(cfg);

        // Backward compatibility: map legacy domains[] format to engine_domains{}.
        if ((!cfg.contains("engine_domains") || !cfg["engine_domains"].is_object()) &&
            cfg.contains("domains") && (cfg["domains"].is_array() || cfg["domains"].is_object())) {
            nlohmann::json mapped = nlohmann::json::object();
            std::vector<nlohmann::json> domain_entries;
            if (cfg["domains"].is_array()) {
                for (const auto& d : cfg["domains"]) {
                    domain_entries.push_back(d);
                }
            } else {
                for (auto it = cfg["domains"].begin(); it != cfg["domains"].end(); ++it) {
                    domain_entries.push_back(it.value());
                }
            }

            for (const auto& d : domain_entries) {
                if (!d.is_object()) continue;

                int voice = d.value("voice", -1);
                if (voice < 0) {
                    const int engine_id = d.value("engine_id", -1);
                    if (engine_id >= 0) {
                        if ((engine_id % 2) == 0) {
                            voice = engine_id / 2;
                        } else {
                            voice = (engine_id - 1) / 2;
                        }
                    }
                }

                if (voice < 0) continue;

                const std::string type = d.value("type", std::string(""));
                std::string key;
                if (type == "pitch") {
                    key = "engine_" + std::to_string(voice * 2 + 1);
                } else if (type == "rhythm") {
                    key = "engine_" + std::to_string(voice * 2);
                } else {
                    continue;
                }

                mapped[key] = nlohmann::json::object();
                mapped[key]["type"] = type;
                mapped[key]["voice"] = voice;

                if (d.contains("values")) {
                    if (type == "pitch") {
                        mapped[key]["midi_values"] = d["values"];
                    } else if (type == "rhythm") {
                        mapped[key]["duration_values"] = d["values"];
                    }
                }
            }
            cfg["engine_domains"] = mapped;
        }

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
                sc.random_seed = s <= 0 ? 0u : static_cast<unsigned int>(s);
            }

            if (so.contains("timeout_ms") && so["timeout_ms"].is_number()) {
                sc.timeout_seconds = so["timeout_ms"].get<double>() / 1000.0;
            }

            if (so.contains("heuristic_top_k") && so["heuristic_top_k"].is_number_integer()) {
                sc.heuristic_top_k = std::max(0, so["heuristic_top_k"].get<int>());
            }

            if (so.contains("heuristic_trace") && so["heuristic_trace"].is_boolean()) {
                sc.heuristic_trace = so["heuristic_trace"].get<bool>();
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
                if (domain.contains("midi_values")) {
                    const auto& midi = domain["midi_values"];
                    if (midi.is_array()) {
                        for (const auto& v : midi) {
                            if (v.is_number_integer()) {
                                sc.voice_domains[voice].push_back(v.get<int>());
                            } else if (v.is_number_float()) {
                                sc.voice_domains[voice].push_back(static_cast<int>(std::lround(v.get<double>())));
                            } else if (v.is_string()) {
                                const auto parsed = parse_int_list_from_string(v.get<std::string>());
                                sc.voice_domains[voice].insert(sc.voice_domains[voice].end(), parsed.begin(), parsed.end());
                            }
                        }
                    } else if (midi.is_number_integer()) {
                        sc.voice_domains[voice].push_back(midi.get<int>());
                    } else if (midi.is_number_float()) {
                        sc.voice_domains[voice].push_back(static_cast<int>(std::lround(midi.get<double>())));
                    } else if (midi.is_string()) {
                        const auto parsed = parse_int_list_from_string(midi.get<std::string>());
                        sc.voice_domains[voice].insert(sc.voice_domains[voice].end(), parsed.begin(), parsed.end());
                    }
                }
            } else if (type == "rhythm") {
                if (domain.contains("duration_values")) {
                    const auto& durations = domain["duration_values"];
                    if (durations.is_array()) {
                        for (const auto& dv : durations) {
                            if (dv.is_string()) {
                                const auto parsed = parse_duration_list_from_string(dv.get<std::string>());
                                rhythm_strings[voice].insert(rhythm_strings[voice].end(), parsed.begin(), parsed.end());
                            } else if (dv.is_number()) {
                                rhythm_strings[voice].push_back(numeric_duration_to_fraction_string(dv.get<double>()));
                            }
                        }
                    } else if (durations.is_string()) {
                        const auto parsed = parse_duration_list_from_string(durations.get<std::string>());
                        rhythm_strings[voice].insert(rhythm_strings[voice].end(), parsed.begin(), parsed.end());
                    } else if (durations.is_number()) {
                        rhythm_strings[voice].push_back(numeric_duration_to_fraction_string(durations.get<double>()));
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
                // Backward compatibility for pitch-only configs.
                rhythm_strings[v].push_back("1/4");
            }
            for (const auto& s : rhythm_strings[v]) {
                rhythm_base = lcm_int(rhythm_base, parse_duration_denominator(s));
            }
        }

        sc.rhythm_base = rhythm_base;
        rhythm_base_ = rhythm_base;
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
        if (cfg.contains("rules") && (cfg["rules"].is_array() || cfg["rules"].is_object())) {
            std::vector<nlohmann::json> rule_entries;
            if (cfg["rules"].is_array()) {
                for (const auto& r : cfg["rules"]) {
                    rule_entries.push_back(r);
                }
            } else {
                for (auto it = cfg["rules"].begin(); it != cfg["rules"].end(); ++it) {
                    rule_entries.push_back(it.value());
                }
            }

            for (const auto& r : rule_entries) {
                if (!r.is_object()) continue;
                if (r.contains("enabled") && !json_value_to_bool(r["enabled"], true)) continue;

                const std::string rule_type = r.value("rule_type", std::string(""));
                std::string function;
                if (r.contains("constraint_function") && r["constraint_function"].is_object()) {
                    function = r["constraint_function"].value("function", std::string(""));
                }

                // Be tolerant of dict-shaped payloads where function may be missing.
                if (function.empty()) {
                    if (rule_type == "r-twelve-tone-voice1") {
                        function = "all_different";
                    } else if (rule_type == "r-palindrome-voice2" || rule_type == "r-palindrome-voice") {
                        function = "palindrome_of_engine";
                    }
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
                if (dr.is_null()) continue;
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
