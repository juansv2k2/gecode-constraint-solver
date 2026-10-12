#ifndef MOTIVE_DOMAINS_HH
#define MOTIVE_DOMAINS_HH

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace MotiveDomains {

struct Domain {
    int voice = 0;
    bool rhythm = false;
    std::vector<std::vector<int>> candidates;
};

struct Selection {
    int voice = 0;
    bool rhythm = false;
    int motive_id = 0;
    int start_event = 0;
    int end_event = 0;
    int onset_ticks = 0;
    int duration_ticks = 0;
    std::vector<int> values;
};

inline nlohmann::json to_json(const std::vector<Selection>& selections) {
    auto result = nlohmann::json::array();
    for (const auto& selection : selections) {
        result.push_back({{"voice", selection.voice},
                          {"component", selection.rhythm ? "rhythm" : "pitch"},
                          {"motive_id", selection.motive_id},
                          {"start_event", selection.start_event},
                          {"end_event", selection.end_event},
                          {"onset_ticks", selection.onset_ticks},
                          {"duration_ticks", selection.duration_ticks},
                          {"values", selection.values}});
    }
    return result;
}

}
#endif