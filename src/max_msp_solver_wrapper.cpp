#include "max_msp_solver_wrapper.hh"
#include "wildcard_rule_extension.hh"

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

bool json_value_to_bool_with_default(const nlohmann::json& value, bool default_value) {
    if (value.is_boolean()) return value.get<bool>();
    if (value.is_number_integer()) return value.get<long long>() != 0;
    if (value.is_number_unsigned()) return value.get<unsigned long long>() != 0;
    if (value.is_number_float()) return std::abs(value.get<double>()) > std::numeric_limits<double>::epsilon();
    if (value.is_string()) {
        std::string text = value.get<std::string>();
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (text == "true" || text == "1" || text == "yes" || text == "on") return true;
        if (text == "false" || text == "0" || text == "no" || text == "off") return false;
    }
    return default_value;
}

std::pair<int, int> parse_time_signature_value(const nlohmann::json& value, const std::string& context) {
    if (value.is_array()) {
        if (value.size() != 2 || !value[0].is_number_integer() || !value[1].is_number_integer()) {
            throw std::runtime_error(context + ": time signature array must be [numerator, denominator]");
        }
        int n = value[0].get<int>();
        int d = value[1].get<int>();
        if (n <= 0 || d <= 0) {
            throw std::runtime_error(context + ": time signature values must be positive");
        }
        return {n, d};
    }

    if (value.is_string()) {
        const std::string s = value.get<std::string>();
        const auto slash = s.find('/');
        if (slash == std::string::npos) {
            throw std::runtime_error(context + ": invalid time signature string '" + s + "' (expected N/D)");
        }
        int n = 0, d = 0;
        try {
            n = std::stoi(s.substr(0, slash));
            d = std::stoi(s.substr(slash + 1));
        } catch (...) {
            throw std::runtime_error(context + ": cannot parse time signature '" + s + "'");
        }
        if (n <= 0 || d <= 0) {
            throw std::runtime_error(context + ": time signature values must be positive in '" + s + "'");
        }
        return {n, d};
    }

    if (value.is_object()) {
        if (!value.contains("numerator") || !value.contains("denominator") ||
            !value["numerator"].is_number_integer() || !value["denominator"].is_number_integer()) {
            throw std::runtime_error(context + ": time signature object requires integer numerator/denominator");
        }
        int n = value["numerator"].get<int>();
        int d = value["denominator"].get<int>();
        if (n <= 0 || d <= 0) {
            throw std::runtime_error(context + ": time signature values must be positive");
        }
        return {n, d};
    }

    throw std::runtime_error(context + ": unsupported time signature format");
}

std::vector<int> parse_positive_int_array(const nlohmann::json& value, const std::string& context) {
    if (!value.is_array()) {
        throw std::runtime_error(context + ": expected array of positive integers");
    }
    std::vector<int> out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (!value[i].is_number_integer()) {
            throw std::runtime_error(context + ": entry " + std::to_string(i) + " must be integer");
        }
        int v = value[i].get<int>();
        if (v <= 0) {
            throw std::runtime_error(context + ": entry " + std::to_string(i) + " must be positive");
        }
        out.push_back(v);
    }
    return out;
}

std::vector<MusicalConstraintSolver::SolverConfig::MetricDomainEntry>
parse_metric_domain_from_engine_domains(const nlohmann::json& cfg) {
    std::vector<MusicalConstraintSolver::SolverConfig::MetricDomainEntry> out;
    if (!cfg.contains("engine_domains") || !cfg["engine_domains"].is_object()) {
        return out;
    }

    const auto& engine_domains = cfg["engine_domains"];
    for (auto it = engine_domains.begin(); it != engine_domains.end(); ++it) {
        const std::string key = it.key();
        const auto& domain = it.value();
        if (!domain.is_object()) continue;

        const std::string type = domain.value("type", std::string(""));
        if (type != "metric") continue;

        const std::string ctx = "engine_domains['" + key + "']";
        std::vector<std::pair<int, int>> signatures;

        if (domain.contains("time_signatures")) {
            if (!domain["time_signatures"].is_array() || domain["time_signatures"].empty()) {
                throw std::runtime_error(ctx + ": 'time_signatures' must be a non-empty array");
            }
            for (size_t i = 0; i < domain["time_signatures"].size(); ++i) {
                signatures.push_back(parse_time_signature_value(
                    domain["time_signatures"][i], ctx + " time_signatures[" + std::to_string(i) + "]"));
            }
        } else if (domain.contains("time_signature")) {
            const auto& ts = domain["time_signature"];
            if (ts.is_array() && !ts.empty() &&
                (ts[0].is_string() || ts[0].is_array() || ts[0].is_object())) {
                for (size_t i = 0; i < ts.size(); ++i) {
                    signatures.push_back(parse_time_signature_value(
                        ts[i], ctx + " time_signature[" + std::to_string(i) + "]"));
                }
            } else {
                signatures.push_back(parse_time_signature_value(ts, ctx + " time_signature"));
            }
        } else if (domain.contains("numerator") || domain.contains("denominator")) {
            signatures.push_back(parse_time_signature_value(domain, ctx));
        } else {
            throw std::runtime_error(
                ctx + ": metric domain requires one of: 'time_signatures', 'time_signature', or numerator/denominator");
        }

        std::vector<int> tuplets;
        if (domain.contains("tuplets")) {
            tuplets = parse_positive_int_array(domain["tuplets"], ctx + " tuplets");
        }

        std::vector<int> beat_divisions;
        if (domain.contains("beat_divisions")) {
            beat_divisions = parse_positive_int_array(domain["beat_divisions"], ctx + " beat_divisions");
        }

        for (const auto& ts : signatures) {
            MusicalConstraintSolver::SolverConfig::MetricDomainEntry entry;
            entry.numerator = ts.first;
            entry.denominator = ts.second;
            entry.tuplets = tuplets;
            entry.beat_divisions = beat_divisions;
            out.push_back(std::move(entry));
        }
    }
    return out;
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
        if (key[i] == '[') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }

            std::string index;
            ++i;
            while (i < key.size() && key[i] != ']') {
                index.push_back(key[i]);
                ++i;
            }
            if (!index.empty()) {
                tokens.push_back(index);
            }
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

    auto token_is_index = [](const std::string& s) {
        if (s.empty()) return false;
        for (char c : s) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                return false;
            }
        }
        return true;
    };

    nlohmann::json* cur = &root;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];
        const bool last = (i + 1 == tokens.size());

        if (token_is_index(t)) {
            const std::size_t index = static_cast<std::size_t>(std::stoul(t));
            if (!cur->is_array()) {
                *cur = nlohmann::json::array();
            }
            while (cur->size() <= index) {
                cur->push_back(nullptr);
            }

            if (last) {
                (*cur)[index] = value;
                continue;
            }

            const bool next_is_index = token_is_index(tokens[i + 1]);
            nlohmann::json& child = (*cur)[index];
            if (next_is_index) {
                if (!child.is_array()) {
                    child = nlohmann::json::array();
                }
            } else {
                if (!child.is_object()) {
                    child = nlohmann::json::object();
                }
            }
            cur = &child;
            continue;
        }

        if (!cur->is_object()) {
            *cur = nlohmann::json::object();
        }

        if (last) {
            (*cur)[t] = value;
            continue;
        }

        const bool next_is_index = token_is_index(tokens[i + 1]);
        if (!(*cur).contains(t)) {
            (*cur)[t] = next_is_index ? nlohmann::json::array() : nlohmann::json::object();
        } else if (next_is_index && !(*cur)[t].is_array()) {
            (*cur)[t] = nlohmann::json::array();
        } else if (!next_is_index && !(*cur)[t].is_object()) {
            (*cur)[t] = nlohmann::json::object();
        }
        cur = &(*cur)[t];
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
            // Use solve_multiple() to handle both single and multiple solutions
            auto solutions = solver_.solve_multiple(max_solutions_, static_cast<int>(timeout_ms_));

            if (cancel_requested_.load()) {
                local_result.status = SolveStatus::CANCELLED;
                local_result.message = "Solve cancelled";
            } else if (solutions.empty()) {
                local_result.status = SolveStatus::FAILED;
                local_result.found_solution = false;
                local_result.message = "No solutions found";
            } else {
                // Handle first solution (for backward compatibility)
                const auto& solution = solutions[0];
                
                local_result.status = SolveStatus::SUCCESS;
                local_result.found_solution = true;
                local_result.message = "Found " + std::to_string(solutions.size()) + " solution(s)";

                local_result.voice_solutions = solution.voice_solutions;
                local_result.voice_rhythms = solution.voice_rhythms;
                local_result.solve_time_ms = solution.solve_time_ms;
                local_result.backjumps_performed = solution.backjumps_performed;
                local_result.total_rules_checked = solution.total_rules_checked;
                local_result.rhythm_base = rhythm_base_;

                nlohmann::json j;
                j["found_solution"] = true;
                j["num_solutions"] = solutions.size();
                j["solve_time_ms"] = solution.solve_time_ms;
                j["backjumps_performed"] = solution.backjumps_performed;
                j["total_rules_checked"] = solution.total_rules_checked;
                
                // Return all solutions
                nlohmann::json all_solutions = nlohmann::json::array();
                for (const auto& sol : solutions) {
                    nlohmann::json sol_j;
                    sol_j["voice_solutions"] = sol.voice_solutions;
                    
                    // Convert rhythms to fractions
                    nlohmann::json voice_rhythm_fractions = nlohmann::json::array();
                    for (const auto& voice : sol.voice_rhythms) {
                        nlohmann::json v = nlohmann::json::array();
                        for (const int tick : voice) {
                            v.push_back(rhythm_ticks_to_fraction_string(tick, rhythm_base_));
                        }
                        voice_rhythm_fractions.push_back(v);
                    }
                    sol_j["voice_rhythms"] = voice_rhythm_fractions;
                    sol_j["voice_rhythm_ticks"] = sol.voice_rhythms;
                    
                    nlohmann::json metric_signature = nlohmann::json::array();
                    if (sol.has_canonical_score && !sol.canonical_score.metric_timeline.empty()) {
                        std::vector<int> den_by_index(sol.metric_signature.size(), 4);
                        for (const auto& seg : sol.canonical_score.metric_timeline) {
                            const int start = std::max(0, seg.start_index);
                            const int end = std::min(static_cast<int>(den_by_index.size()) - 1, seg.end_index);
                            for (int i = start; i <= end; ++i) {
                                den_by_index[i] = seg.denominator > 0 ? seg.denominator : 4;
                            }
                        }
                        for (size_t i = 0; i < sol.metric_signature.size(); ++i) {
                            std::ostringstream ts;
                            ts << sol.metric_signature[i] << "/" << den_by_index[i];
                            metric_signature.push_back(ts.str());
                        }
                    } else {
                        for (const int numerator : sol.metric_signature) {
                            std::ostringstream ts;
                            ts << numerator << "/4";
                            metric_signature.push_back(ts.str());
                        }
                    }
                    sol_j["metric_signature"] = metric_signature;
                    sol_j["metric_signature_numerators"] = sol.metric_signature;

                    if (sol.has_canonical_score) {
                        nlohmann::json score_j;
                        score_j["rhythm_base"] = sol.canonical_score.rhythm_base;

                        nlohmann::json metric_timeline = nlohmann::json::array();
                        for (const auto& seg : sol.canonical_score.metric_timeline) {
                            nlohmann::json seg_j;
                            seg_j["start_index"] = seg.start_index;
                            seg_j["end_index"] = seg.end_index;
                            seg_j["numerator"] = seg.numerator;
                            seg_j["denominator"] = seg.denominator;
                            metric_timeline.push_back(seg_j);
                        }
                        score_j["metric_timeline"] = metric_timeline;

                        nlohmann::json measures = nlohmann::json::array();
                        for (const auto& measure : sol.canonical_score.measures) {
                            nlohmann::json m_j;
                            m_j["measure_index"] = measure.measure_index;
                            m_j["start_ticks"] = measure.start_ticks;
                            m_j["end_ticks"] = measure.end_ticks;
                            m_j["numerator"] = measure.numerator;
                            m_j["denominator"] = measure.denominator;

                            nlohmann::json events = nlohmann::json::array();
                            for (const auto& ev : measure.events) {
                                nlohmann::json e_j;
                                e_j["voice"] = ev.voice;
                                e_j["index"] = ev.index;
                                e_j["pitch"] = ev.pitch;
                                e_j["rhythm_ticks"] = ev.rhythm;
                                e_j["onset_ticks"] = ev.onset_ticks;
                                e_j["duration_ticks"] = ev.duration_ticks;
                                e_j["is_rest"] = ev.is_rest;
                                events.push_back(e_j);
                            }
                            m_j["events"] = events;
                            measures.push_back(m_j);
                        }
                        score_j["measures"] = measures;

                        sol_j["score"] = score_j;
                    }
                    
                    all_solutions.push_back(sol_j);
                }
                j["solutions"] = all_solutions;
                
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
                timeout_ms_ = so["timeout_ms"].get<double>();
            }

            if (so.contains("max_solutions") && so["max_solutions"].is_number_integer()) {
                max_solutions_ = so["max_solutions"].get<int>();
                sc.max_solutions = max_solutions_;
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

            const std::string type = domain.value("type", std::string(""));
            if (type == "pitch") {
                const int voice = domain.value("voice", -1);
                if (voice < 0 || voice >= sc.num_voices) continue;
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
                const int voice = domain.value("voice", -1);
                if (voice < 0 || voice >= sc.num_voices) continue;
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

        sc.metric_domain = parse_metric_domain_from_engine_domains(cfg);

        // Keep metric solving disabled by default for safe rollout.
        sc.enable_metric_engine = false;
        if (cfg.contains("search_options") && cfg["search_options"].is_object() &&
            cfg["search_options"].contains("enable_metric_engine")) {
            sc.enable_metric_engine = json_value_to_bool_with_default(
                cfg["search_options"]["enable_metric_engine"], false);
        } else if (cfg.contains("enable_metric_engine")) {
            sc.enable_metric_engine = json_value_to_bool_with_default(cfg["enable_metric_engine"], false);
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

                const bool has_target_engine = target_engine >= 0;
                const bool has_target_engines = !target_engines.empty();
                if (!rule_type.empty() && !has_target_engine && !has_target_engines) {
                    throw std::runtime_error(
                        "rule '" + (r.value("id", std::string("")) .empty() ? rule_type : r.value("id", std::string(""))) +
                        "' is missing explicit target_engine/target_engines");
                }

                std::string engine_type = r.value("engine_type", std::string(""));
                std::string description = r.value("description", std::string(""));

                std::vector<double> parameters;
                std::vector<std::string> parameter_strings;
                if (r.contains("constraint_function") &&
                    r["constraint_function"].is_object() &&
                    r["constraint_function"].contains("parameters") &&
                    r["constraint_function"]["parameters"].is_array()) {
                    for (const auto& p : r["constraint_function"]["parameters"]) {
                        if (p.is_number()) {
                            parameters.push_back(p.get<double>());
                        } else if (p.is_string()) {
                            parameter_strings.push_back(p.get<std::string>());
                        }
                    }
                }

                solver_.add_rule_config(rule_type, function, indices, target_engine,
                                        target_engines, engine_type, description,
                                        parameters, parameter_strings);
            }
        }

        // Dynamic rules path.
        if (cfg.contains("dynamic_rules") && cfg["dynamic_rules"].is_array()) {
            std::vector<nlohmann::json> dynamic_rules;
            for (const auto& dr : cfg["dynamic_rules"]) {
                if (!dr.is_null()) dynamic_rules.push_back(dr);
            }
            solver_.load_dynamic_rules(dynamic_rules);
        }

        // Enhanced rules processing parity with CLI:
        // - wildcard_constraint rules in rules[]
        // - index rules in rules[] (type == "index")
        // - expression-based rules embedded in rules[]
        if (cfg.contains("rules") && (cfg["rules"].is_array() || cfg["rules"].is_object())) {
            std::vector<nlohmann::json> enhanced_rules;
            if (cfg["rules"].is_array()) {
                for (const auto& r : cfg["rules"]) {
                    enhanced_rules.push_back(r);
                }
            } else {
                for (auto it = cfg["rules"].begin(); it != cfg["rules"].end(); ++it) {
                    enhanced_rules.push_back(it.value());
                }
            }

            for (const auto& rule_json : enhanced_rules) {
                try {
                    if (!rule_json.is_object()) continue;
                    if (rule_json.contains("enabled") && !json_value_to_bool(rule_json["enabled"], true)) continue;

                    const std::string rule_type = rule_json.value("rule_type", "");
                    const std::string type_field = rule_json.value("type", "");

                    const bool has_target_engine = rule_json.contains("target_engine") && rule_json["target_engine"].is_number_integer() && rule_json["target_engine"].get<int>() >= 0;
                    const bool has_target_engines = rule_json.contains("target_engines") && rule_json["target_engines"].is_array() && !rule_json["target_engines"].empty();
                    if (!rule_type.empty() && !has_target_engine && !has_target_engines && type_field != "index") {
                        throw std::runtime_error("rule '" + rule_json.value("id", rule_type) + "' is missing explicit target_engine/target_engines");
                    }

                    if (rule_type == "wildcard_constraint") {
                        auto compiled = DynamicRules::WildcardRuleCompiler::compile_wildcard_from_json(rule_json);
                        if (compiled) {
                            solver_.apply_compiled_constraint(std::move(compiled));
                        }
                    } else if (type_field == "index") {
                        const std::string rule_id = rule_json.value("id", "index_rule");
                        if (!rule_json.contains("voice") || !rule_json["voice"].is_number_integer()) {
                            throw std::runtime_error("index rule '" + rule_id + "': missing or invalid 'voice' field");
                        }
                        if (!rule_json.contains("events") || !rule_json["events"].is_array()) {
                            throw std::runtime_error("index rule '" + rule_id + "': missing or invalid 'events' array");
                        }

                        struct IndexEvent {
                            int pos;
                            int rhythm_ticks;
                            int pitch;
                        };

                        const int voice = rule_json["voice"].get<int>();
                        const int rhythm_base = sc.rhythm_base;
                        std::vector<IndexEvent> events;

                        for (size_t ei = 0; ei < rule_json["events"].size(); ++ei) {
                            const auto& ev = rule_json["events"][ei];
                            if (!ev.is_array() || ev.size() != 3) {
                                throw std::runtime_error("index rule '" + rule_id + "': event[" + std::to_string(ei) + "] must be [pos, rhythm, pitch_or_null]");
                            }

                            const int pos = ev[0].get<int>();
                            const std::string rhythm_str = ev[1].get<std::string>();
                            const bool pitch_is_null = ev[2].is_null();
                            const int pitch_midi = pitch_is_null
                                ? GecodeClusterIntegration::IntegratedMusicalSpace::REST_PITCH_SENTINEL
                                : ev[2].get<int>();

                            const int rhythm_ticks = parse_duration_to_ticks(rhythm_str, rhythm_base);
                            const bool is_rest = (rhythm_ticks < 0);
                            if (is_rest && !pitch_is_null) {
                                throw std::runtime_error("index rule '" + rule_id + "' event[" + std::to_string(ei) + "]: rest rhythm requires null pitch");
                            }
                            if (!is_rest && pitch_is_null) {
                                throw std::runtime_error("index rule '" + rule_id + "' event[" + std::to_string(ei) + "]: note rhythm requires non-null pitch");
                            }

                            events.push_back({pos, rhythm_ticks, pitch_midi});
                        }

                        auto compiled = std::make_unique<DynamicRules::CompiledConstraint>(
                            rule_id,
                            "index rule: pin voice " + std::to_string(voice));

                        compiled->post_constraint = [voice, events, rule_id](DynamicRules::ConstraintContext& ctx) {
                            if (voice < 0 || voice >= ctx.num_voices) {
                                throw std::runtime_error("index rule '" + rule_id + "': voice out of range");
                            }

                            for (const auto& ev : events) {
                                if (ev.pos < 0 || ev.pos >= ctx.sequence_length) {
                                    throw std::runtime_error("index rule '" + rule_id + "': position out of range");
                                }

                                const int idx = voice * ctx.sequence_length + ev.pos;
                                if (ctx.rhythm_vars && idx < static_cast<int>(ctx.rhythm_vars->size())) {
                                    Gecode::dom(*ctx.space, (*ctx.rhythm_vars)[idx], Gecode::IntSet(ev.rhythm_ticks, ev.rhythm_ticks));
                                }
                                if (ev.pitch != GecodeClusterIntegration::IntegratedMusicalSpace::REST_PITCH_SENTINEL) {
                                    if (ctx.pitch_vars && idx < static_cast<int>(ctx.pitch_vars->size())) {
                                        Gecode::dom(*ctx.space, (*ctx.pitch_vars)[idx], Gecode::IntSet(ev.pitch, ev.pitch));
                                    }
                                }
                            }
                        };

                        solver_.apply_compiled_constraint(std::move(compiled));
                    } else if (rule_json.contains("id") && rule_json.contains("expression")) {
                        std::vector<nlohmann::json> single_rule = {rule_json};
                        solver_.load_dynamic_rules(single_rule);
                    }
                } catch (...) {
                    // Keep wrapper behavior tolerant: invalid enhanced rules are skipped,
                    // matching CLI's best-effort loading approach.
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        error_message = e.what();
        return false;
    }
}

} // namespace MaxMSPWrapper
