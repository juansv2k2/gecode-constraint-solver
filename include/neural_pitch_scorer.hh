/**
 * @file neural_pitch_scorer.hh
 * @brief Phase 1 — Tiny MLP pitch scorer for the Gecode heuristic value-ordering hook.
 *
 * Architecture (regression, Phase 1):
 *   Input  : CONTEXT_SIZE recent MIDI pitches, normalised to [0,1] by /127
 *   Hidden : HIDDEN_SIZE neurons, ReLU
 *   Output : 1 scalar in [0,1] (predicted next pitch, normalised)
 *
 * Scoring:
 *   score(candidate) = -|predicted - candidate/127|
 *   Higher score → candidate is closer to the network's prediction.
 *
 * The scorer is placed in the lowest heuristic bucket (priority 0) so it
 * acts as a tiebreak below any explicit symbolic heuristics (priority ≥ 1).
 *
 * Weights are loaded at startup from a JSON file produced by
 * scripts/train_pitch_mlp.py.  If the file is missing or malformed, the
 * scorer falls back to returning a zero bucket (no preference).
 *
 * Usage:
 *   auto scorer = NeuralPitch::make_scorer("datasets/pitch_mlp_weights.json");
 *   GecodeClusterIntegration::configure_pitch_heuristic_value_ordering(scorer);
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

// Forward-declare the integration types we need.
#include "gecode_cluster_integration.hh"

namespace NeuralPitch {

// ---------------------------------------------------------------------------
// Weight storage
// ---------------------------------------------------------------------------

struct MLPWeights {
    int context_size  = 4;
    int hidden_size   = 16;

    // Training pitch samples stored for solver-level warm-start (inference only).
    // The C++ scorer draws from these when fewer than context_size pitches have
    // been assigned so far, keeping the network in-distribution without changing
    // how training was done.
    std::vector<int> pitch_samples;  // raw MIDI values sampled from training set

    // W1[j][k] — hidden neuron j, input k        shape: [hidden][context]
    std::vector<std::vector<float>> W1;
    std::vector<float>              b1;   // [hidden]
    std::vector<float>              W2;   // [hidden]  (output layer weights)
    float                           b2 = 0.0f;

    bool loaded = false;
};

// ---------------------------------------------------------------------------
// JSON parsing (self-contained, no external library required)
// ---------------------------------------------------------------------------

namespace detail {

// Minimal JSON float-array reader that avoids pulling in nlohmann/json
// just for weight loading.  Supports only the specific structure we write.

// Read a JSON number (float).
inline bool read_float(const std::string& s, size_t& pos, float& out) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' ||
                               s[pos] == '\n' || s[pos] == '\r')) ++pos;
    size_t start = pos;
    if (pos < s.size() && (s[pos] == '-' || s[pos] == '+')) ++pos;
    while (pos < s.size() && (std::isdigit((unsigned char)s[pos]) ||
                               s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E' ||
                               s[pos] == '+' || s[pos] == '-')) ++pos;
    if (pos == start) return false;
    try { out = std::stof(s.substr(start, pos - start)); } catch (...) { return false; }
    return true;
}

// Read a JSON array of floats: [ num, num, ... ]
inline bool read_float_array(const std::string& s, size_t& pos,
                              std::vector<float>& out) {
    while (pos < s.size() && s[pos] != '[') ++pos;
    if (pos >= s.size()) return false;
    ++pos; // skip '['
    out.clear();
    while (pos < s.size()) {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' ||
                                   s[pos] == '\n' || s[pos] == '\r' ||
                                   s[pos] == ',')) ++pos;
        if (pos < s.size() && s[pos] == ']') { ++pos; return true; }
        float v = 0.0f;
        if (!read_float(s, pos, v)) return false;
        out.push_back(v);
    }
    return false;
}

// Read a JSON array of float-arrays: [ [..], [..], ... ]
inline bool read_matrix(const std::string& s, size_t& pos,
                         std::vector<std::vector<float>>& out) {
    while (pos < s.size() && s[pos] != '[') ++pos;
    if (pos >= s.size()) return false;
    ++pos; // outer '['
    out.clear();
    while (pos < s.size()) {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' ||
                                   s[pos] == '\n' || s[pos] == '\r' ||
                                   s[pos] == ',')) ++pos;
        if (pos < s.size() && s[pos] == ']') { ++pos; return true; }
        if (pos < s.size() && s[pos] == '[') {
            std::vector<float> row;
            if (!read_float_array(s, pos, row)) return false;
            out.push_back(std::move(row));
        } else {
            return false;
        }
    }
    return false;
}

// Locate a JSON key and return the position just after the colon.
inline size_t find_key(const std::string& s, const std::string& key,
                        size_t from = 0) {
    std::string quoted = "\"" + key + "\"";
    size_t p = s.find(quoted, from);
    if (p == std::string::npos) return std::string::npos;
    p += quoted.size();
    while (p < s.size() && (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' ||
                              s[p] == '\r')) ++p;
    if (p < s.size() && s[p] == ':') ++p;
    return p;
}

inline bool read_int_at_key(const std::string& s, const std::string& key, int& out) {
    size_t p = find_key(s, key);
    if (p == std::string::npos) return false;
    float v = 0.0f;
    if (!read_float(s, p, v)) return false;
    out = static_cast<int>(v);
    return true;
}

} // namespace detail

// ---------------------------------------------------------------------------
// Load weights from JSON
// ---------------------------------------------------------------------------

inline MLPWeights load_weights(const std::string& path) {
    MLPWeights w;
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "[NeuralPitch] cannot open weights file: " << path << "\n";
        return w;
    }
    std::string s((std::istreambuf_iterator<char>(f)),
                   std::istreambuf_iterator<char>());

    if (!detail::read_int_at_key(s, "context_size", w.context_size) ||
        !detail::read_int_at_key(s, "hidden_size",  w.hidden_size)) {
        std::cerr << "[NeuralPitch] missing context_size / hidden_size in " << path << "\n";
        return w;
    }

    size_t p;

    // W1
    p = detail::find_key(s, "W1");
    if (p == std::string::npos || !detail::read_matrix(s, p, w.W1)) {
        std::cerr << "[NeuralPitch] failed to read W1 from " << path << "\n";
        return w;
    }

    // b1
    p = detail::find_key(s, "b1");
    if (p == std::string::npos || !detail::read_float_array(s, p, w.b1)) {
        std::cerr << "[NeuralPitch] failed to read b1 from " << path << "\n";
        return w;
    }

    // W2
    p = detail::find_key(s, "W2");
    if (p == std::string::npos || !detail::read_float_array(s, p, w.W2)) {
        std::cerr << "[NeuralPitch] failed to read W2 from " << path << "\n";
        return w;
    }

    // b2 (scalar stored as a JSON number)
    p = detail::find_key(s, "b2");
    if (p != std::string::npos) {
        float v = 0.0f;
        if (detail::read_float(s, p, v)) w.b2 = v;
    }

    // pitch_samples — stored for warm-start at inference; optional
    p = detail::find_key(s, "pitch_samples");
    if (p != std::string::npos) {
        std::vector<float> tmp;
        if (detail::read_float_array(s, p, tmp)) {
            w.pitch_samples.reserve(tmp.size());
            for (float v : tmp) w.pitch_samples.push_back(static_cast<int>(v + 0.5f));
        }
    }

    // Validate shapes
    if ((int)w.W1.size() != w.hidden_size) {
        std::cerr << "[NeuralPitch] W1 row count mismatch (expected " << w.hidden_size
                  << ", got " << w.W1.size() << ")\n";
        return w;
    }
    for (const auto& row : w.W1) {
        if ((int)row.size() != w.context_size) {
            std::cerr << "[NeuralPitch] W1 column count mismatch\n";
            return w;
        }
    }
    if ((int)w.b1.size() != w.hidden_size || (int)w.W2.size() != w.hidden_size) {
        std::cerr << "[NeuralPitch] b1 or W2 size mismatch\n";
        return w;
    }

    w.loaded = true;
    std::cerr << "[NeuralPitch] loaded weights from " << path
              << "  context=" << w.context_size << " hidden=" << w.hidden_size
              << " samples=" << w.pitch_samples.size() << "\n";
    return w;
}

// ---------------------------------------------------------------------------
// Forward pass
// ---------------------------------------------------------------------------

inline float forward(const MLPWeights& w, const std::vector<float>& x) {
    // h = relu(W1 @ x + b1)
    std::vector<float> h(w.hidden_size);
    for (int j = 0; j < w.hidden_size; ++j) {
        float s = w.b1[j];
        for (int k = 0; k < w.context_size; ++k) s += w.W1[j][k] * x[k];
        h[j] = s > 0.0f ? s : 0.0f;
    }
    // y = W2 . h + b2
    float y = w.b2;
    for (int j = 0; j < w.hidden_size; ++j) y += w.W2[j] * h[j];
    return y;
}

// ---------------------------------------------------------------------------
// HeuristicValueScorer factory
// ---------------------------------------------------------------------------

/**
 * Build a HeuristicValueScorer from a weights file.
 *
 * warmup_seed  : ties warm-start and temperature noise to the solver seed.
 *                Same seed => same melodic character; different seeds => variety.
 * temperature  : std-dev of Gaussian noise added to the prediction before scoring
 *                (in normalised MIDI units, i.e. semitones / 127).
 *                0.0 => deterministic (closest-to-mean wins every time → oscillation).
 *                0.04 ≈ 5 MIDI semitones: good melodic variety while staying on-style.
 *                0.08 ≈ 10 MIDI semitones: more adventurous.
 *                The noise is per-(voice, position), identical for all candidates at
 *                that position, so search consistency is preserved.
 */
inline GecodeClusterIntegration::HeuristicValueScorer
make_scorer(const std::string& weights_path,
            bool         shadow_mode  = false,
            unsigned int warmup_seed  = 12345,
            float        temperature  = 0.04f) {
    auto w = std::make_shared<MLPWeights>(load_weights(weights_path));

    if (!w->loaded) {
        std::cerr << "[NeuralPitch] WARNING: scorer disabled (weights not loaded)\n";
        return [](const GecodeClusterIntegration::IntegratedMusicalSpace&,
                  int, int, int) -> GecodeClusterIntegration::HeuristicValueScoreBuckets {
            return {};
        };
    }

    const int   ctx_size  = w->context_size;
    const bool  shadow    = shadow_mode;
    const int   n_samples = static_cast<int>(w->pitch_samples.size());
    const float temp      = temperature;

    std::cerr << "[NeuralPitch] temperature=" << temperature
              << " (" << static_cast<int>(temperature * 127 + 0.5f) << " semitones)\n";

    return [w, ctx_size, shadow, warmup_seed, n_samples, temp](
        const GecodeClusterIntegration::IntegratedMusicalSpace& space,
        int voice,
        int position,
        int candidate_value) -> GecodeClusterIntegration::HeuristicValueScoreBuckets
    {
        const int seq_len = space.get_sequence_length();
        const auto& pitch_vars = space.get_absolute_vars();
        const int voice_offset = voice * seq_len;

        // ── 1. Collect actual assigned pitches (backwards from position-1) ──
        std::vector<int> assigned;
        assigned.reserve(ctx_size);
        for (int i = position - 1; i >= 0 && (int)assigned.size() < ctx_size; --i) {
            int var_idx = voice_offset + i;
            if (var_idx < 0 || var_idx >= static_cast<int>(pitch_vars.size())) continue;
            const auto& var = pitch_vars[var_idx];
            if (var.assigned()) assigned.push_back(var.val());
        }

        // ── 2. Build context (warm-start for missing positions) ──
        std::vector<float> ctx(ctx_size);
        const int have = static_cast<int>(assigned.size());
        const int need = ctx_size - have;

        for (int k = 0; k < need; ++k) {
            float v = 0.5f;
            if (n_samples > 0) {
                unsigned int h = warmup_seed ^ (unsigned int)(voice * 31 + k * 1009);
                h ^= (h >> 16); h *= 0x45d9f3bu; h ^= (h >> 16);
                v = static_cast<float>(w->pitch_samples[h % (unsigned int)n_samples]) / 127.0f;
            }
            ctx[k] = v;
        }
        for (int k = 0; k < have; ++k)
            ctx[need + k] = static_cast<float>(assigned[have - 1 - k]) / 127.0f;

        // ── 3. MLP prediction ──
        float predicted = forward(*w, ctx);

        // ── 4. Temperature noise (deterministic per position, same for all candidates) ──
        // Box-Muller from two hashes of (seed, voice, position)
        if (temp > 0.0f) {
            auto hash32 = [](unsigned int seed, unsigned int a, unsigned int b) -> unsigned int {
                unsigned int h = seed ^ (a * 2654435761u) ^ (b * 40503u);
                h ^= (h >> 16); h *= 0x45d9f3bu; h ^= (h >> 16);
                return h;
            };
            unsigned int h1 = hash32(warmup_seed, (unsigned int)voice, (unsigned int)position);
            unsigned int h2 = hash32(warmup_seed + 1, (unsigned int)(voice + 1), (unsigned int)position);
            float u1 = std::max((h1 & 0xFFFFFF) / 16777216.0f, 1e-7f);
            float u2 = (h2 & 0xFFFFFF) / 16777216.0f;
            float noise = temp * std::sqrt(-2.0f * std::log(u1))
                               * std::cos(2.0f * 3.14159265f * u2);
            predicted += noise;
        }

        const float candidate_norm = static_cast<float>(candidate_value) / 127.0f;
        const float score          = -std::fabs(predicted - candidate_norm);

        if (shadow) {
            std::cerr << "[NeuralPitch shadow] v=" << voice << " pos=" << position
                      << " warm=" << need << " actual=" << have
                      << " cand=" << candidate_value
                      << " pred_noisy=" << static_cast<int>(predicted * 127 + 0.5f)
                      << " score=" << score << "\n";
            return {};
        }

        return {{0, static_cast<double>(score)}};
    };
}

} // namespace NeuralPitch

