/**
 * @file neural_pitch_scorer.hh
 * @brief Neural MLP pitch scorer for the Gecode heuristic value-ordering hook.
 *
 * Supports two model types (auto-detected from the weights JSON):
 *
 * Classification (recommended, phase="classification"):
 *   Output: softmax over pitch_vocab  (unique MIDI values in training data)
 *   Loss:   cross-entropy
 *   Score:  log P(candidate | context)  with optional logit temperature T
 *   T=1.0 (default): raw trained probabilities
 *   T<1.0: sharper distribution (more deterministic / folk-like)
 *   T>1.0: flatter distribution (more varied)
 *
 * Regression (legacy, phase absent or numeric):
 *   Output: scalar predicted next pitch
 *   Score:  -|predicted - candidate/127|
 *   (This approach collapses to the conditional mean; use classification instead.)
 *
 * Usage:
 *   auto scorer = NeuralPitch::make_scorer("datasets/pitch_mlp_weights.json",
 *                                          false, seed, 1.0f);
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
    int vocab_size    = 0;   // >0 for classification, 0 for regression
    bool is_classifier = false;

    // Training pitch samples stored for solver-level warm-start (inference only).
    std::vector<int> pitch_samples;  // raw MIDI values sampled from training set
    std::vector<int> pitch_vocab;    // sorted MIDI values (classification only)

    // Shared weights (both model types)
    std::vector<std::vector<float>> W1;  // [hidden x context]
    std::vector<float>              b1;  // [hidden]

    // Regression output
    std::vector<float> W2;    // [hidden]  (flat, regression only)
    float              b2 = 0.0f;

    // Classification output
    std::vector<std::vector<float>> W2_clf;  // [vocab x hidden]
    std::vector<float>              b2_clf;  // [vocab]

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

// Detect whether this is a classification model.
inline bool detect_classifier(const std::string& s) {
    size_t p = find_key(s, "phase");
    if (p == std::string::npos) return false;
    while (p < s.size() && (s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r')) ++p;
    return s.size() > p + 16 && s.substr(p, 16) == "\"classification\"";
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

    // pitch_samples (warm-start, both model types)
    p = detail::find_key(s, "pitch_samples");
    if (p != std::string::npos) {
        std::vector<float> tmp;
        if (detail::read_float_array(s, p, tmp)) {
            w.pitch_samples.reserve(tmp.size());
            for (float v : tmp) w.pitch_samples.push_back(static_cast<int>(v + 0.5f));
        }
    }

    w.is_classifier = detail::detect_classifier(s);

    if (w.is_classifier) {
        // --- Classification model ---
        // pitch_vocab
        p = detail::find_key(s, "pitch_vocab");
        if (p != std::string::npos) {
            std::vector<float> tmp;
            if (detail::read_float_array(s, p, tmp)) {
                w.pitch_vocab.reserve(tmp.size());
                for (float v : tmp) w.pitch_vocab.push_back(static_cast<int>(v + 0.5f));
            }
        }
        if (w.pitch_vocab.empty()) {
            std::cerr << "[NeuralPitch] classification model missing pitch_vocab in " << path << "\n";
            return w;
        }
        w.vocab_size = static_cast<int>(w.pitch_vocab.size());

        // W2 is a matrix [vocab x hidden] for classification
        p = detail::find_key(s, "W2");
        if (p == std::string::npos || !detail::read_matrix(s, p, w.W2_clf)) {
            std::cerr << "[NeuralPitch] failed to read W2 (classifier matrix) from " << path << "\n";
            return w;
        }
        // b2 is a vector [vocab] for classification
        p = detail::find_key(s, "b2");
        if (p == std::string::npos || !detail::read_float_array(s, p, w.b2_clf)) {
            std::cerr << "[NeuralPitch] failed to read b2 (classifier vector) from " << path << "\n";
            return w;
        }
        if ((int)w.W2_clf.size() != w.vocab_size || (int)w.b2_clf.size() != w.vocab_size) {
            std::cerr << "[NeuralPitch] W2_clf/b2_clf size mismatch (expected vocab="
                      << w.vocab_size << ")\n";
            return w;
        }
        std::cerr << "[NeuralPitch] loaded classifier from " << path
                  << "  ctx=" << w.context_size << " hidden=" << w.hidden_size
                  << " vocab=" << w.vocab_size
                  << " samples=" << w.pitch_samples.size() << "\n";
    } else {
        // --- Regression model (legacy) ---
        p = detail::find_key(s, "W2");
        if (p == std::string::npos || !detail::read_float_array(s, p, w.W2)) {
            std::cerr << "[NeuralPitch] failed to read W2 from " << path << "\n";
            return w;
        }
        p = detail::find_key(s, "b2");
        if (p != std::string::npos) {
            float v = 0.0f;
            if (detail::read_float(s, p, v)) w.b2 = v;
        }
        if ((int)w.b1.size() != w.hidden_size || (int)w.W2.size() != w.hidden_size) {
            std::cerr << "[NeuralPitch] b1 or W2 size mismatch\n";
            return w;
        }
        std::cerr << "[NeuralPitch] loaded regression model from " << path
                  << "  ctx=" << w.context_size << " hidden=" << w.hidden_size
                  << " samples=" << w.pitch_samples.size() << "\n";
    }

    w.loaded = true;
    return w;
}

// ---------------------------------------------------------------------------
// Forward passes
// ---------------------------------------------------------------------------

// Regression: returns scalar predicted next pitch (normalised)
inline float forward(const MLPWeights& w, const std::vector<float>& x) {
    std::vector<float> h(w.hidden_size);
    for (int j = 0; j < w.hidden_size; ++j) {
        float s = w.b1[j];
        for (int k = 0; k < w.context_size; ++k) s += w.W1[j][k] * x[k];
        h[j] = s > 0.0f ? s : 0.0f;
    }
    float y = w.b2;
    for (int j = 0; j < w.hidden_size; ++j) y += w.W2[j] * h[j];
    return y;
}

// Classification: returns logits vector [vocab_size]
inline std::vector<float> forward_clf(const MLPWeights& w, const std::vector<float>& x) {
    std::vector<float> h(w.hidden_size);
    for (int j = 0; j < w.hidden_size; ++j) {
        float s = w.b1[j];
        for (int k = 0; k < w.context_size; ++k) s += w.W1[j][k] * x[k];
        h[j] = s > 0.0f ? s : 0.0f;
    }
    const int vs = w.vocab_size;
    std::vector<float> logits(vs);
    for (int k = 0; k < vs; ++k) {
        float s = w.b2_clf[k];
        for (int j = 0; j < w.hidden_size; ++j) s += w.W2_clf[k][j] * h[j];
        logits[k] = s;
    }
    return logits;
}

// Softmax with temperature (T=1.0 = neutral, T<1.0 = sharper, T>1.0 = flatter)
inline std::vector<float> softmax_vec(const std::vector<float>& logits, float temp) {
    const float T = (temp > 1e-5f) ? temp : 1e-5f;
    float max_l = *std::max_element(logits.begin(), logits.end());
    std::vector<float> probs(logits.size());
    float total = 0.0f;
    for (size_t i = 0; i < logits.size(); ++i) {
        probs[i] = std::exp((logits[i] - max_l) / T);
        total   += probs[i];
    }
    for (auto& p : probs) p /= total;
    return probs;
}

// ---------------------------------------------------------------------------
// HeuristicValueScorer factory
// ---------------------------------------------------------------------------

/**
 * Build a HeuristicValueScorer from a weights file.
 *
 * warmup_seed  : ties warm-start context to the solver seed.
 * temperature  : logit temperature for classification (T=1.0 = neutral, default).
 *                For legacy regression models, T adds Gaussian noise to break
 *                the mean-prediction lock (less relevant with classification).
 */
inline GecodeClusterIntegration::HeuristicValueScorer
make_scorer(const std::string& weights_path,
            bool         shadow_mode  = false,
            unsigned int warmup_seed  = 12345,
            float        temperature  = 1.0f) {
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

    if (w->is_classifier) {
        std::cerr << "[NeuralPitch] classifier active — vocab=" << w->vocab_size
                  << " temperature=" << temperature << "\n";
    } else {
        std::cerr << "[NeuralPitch] regression model (legacy) — temperature=" << temperature << "\n";
    }

    return [w, ctx_size, shadow, warmup_seed, n_samples, temp](
        const GecodeClusterIntegration::IntegratedMusicalSpace& space,
        int voice,
        int position,
        int candidate_value) -> GecodeClusterIntegration::HeuristicValueScoreBuckets
    {
        const int seq_len     = space.get_sequence_length();
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

        // ── 3. Score ──
        float score;

        if (w->is_classifier) {
            // Classification: Gumbel-max sampling from P(candidate | context).
            // score = logit_i/T + Gumbel(seed, voice, pos, cand)
            // → solver argmax ≡ sampling from the distribution with temperature T.
            // Different seeds produce different melodies; T controls sharpness.
            // Out-of-vocab candidates score -20 (below any Gumbel in-vocab score).
            const auto& vocab = w->pitch_vocab;
            const auto it = std::find(vocab.begin(), vocab.end(), candidate_value);
            if (it == vocab.end()) {
                score = -20.0f;
            } else {
                const int idx    = static_cast<int>(it - vocab.begin());
                auto logits      = forward_clf(*w, ctx);
                // Scaled logit (temperature controls sharpness)
                float logit_i    = logits[idx] / std::max(temp, 1e-6f);
                // Gumbel noise: hash (seed, voice, position, candidate) → Uniform(0,1) → Gumbel
                unsigned int h = warmup_seed;
                h ^= static_cast<unsigned int>(voice)     * 2654435761u;
                h ^= static_cast<unsigned int>(position)  * 40503u;
                h ^= static_cast<unsigned int>(candidate_value + 200) * 16777619u;
                h ^= (h >> 16); h *= 0x45d9f3bu; h ^= (h >> 16);
                float U     = std::max(static_cast<float>(h & 0xFFFFFFu) / 16777216.0f, 1e-7f);
                float gumbel = -std::log(-std::log(U));
                score = logit_i + gumbel;
            }

            if (shadow) {
                const auto it2 = std::find(w->pitch_vocab.begin(), w->pitch_vocab.end(), candidate_value);
                float prob = 0.0f;
                if (it2 != w->pitch_vocab.end()) {
                    auto logits = forward_clf(*w, ctx);
                    auto probs  = softmax_vec(logits, temp);
                    prob = probs[static_cast<int>(it2 - w->pitch_vocab.begin())];
                }
                std::cerr << "[NeuralPitch shadow] v=" << voice << " pos=" << position
                          << " warm=" << need << " actual=" << have
                          << " cand=" << candidate_value
                          << " prob=" << prob << " gumbel_score=" << score << "\n";
                return {};
            }
        } else {
            // Regression (legacy): score by distance to predicted value
            float predicted = forward(*w, ctx);
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
            score = -std::fabs(predicted - candidate_norm);

            if (shadow) {
                std::cerr << "[NeuralPitch shadow] v=" << voice << " pos=" << position
                          << " warm=" << need << " actual=" << have
                          << " cand=" << candidate_value
                          << " pred_noisy=" << static_cast<int>(predicted * 127 + 0.5f)
                          << " score=" << score << "\n";
                return {};
            }
        }

        return {{0, static_cast<double>(score)}};
    };
}

} // namespace NeuralPitch

