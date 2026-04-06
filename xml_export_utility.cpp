/**
 * @file xml_export_utility.cpp
 * @brief Standalone XML export utility for musical constraint solver
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

/**
 * @brief Simple structure to hold musical solution data for XML export
 */
struct XMLMusicalSolution {
    std::vector<std::vector<int>> voice_solutions;  // Pitch data
    std::vector<std::vector<int>> voice_rhythms;    // Rhythm data  
    std::vector<int> metric_signature;              // Metric data
    std::string config_file;
    std::string problem_name;
    std::string problem_description;
    double solve_time_ms;
    int rules_applied;
    int rules_checked;
};

/**
 * @brief Convert MIDI note number to note name
 */
std::string midi_to_note_name(int midi) {
    const std::string note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (midi / 12) - 1;
    int note = midi % 12;
    return note_names[note] + std::to_string(octave);
}

/**
 * @brief Export musical solution to XML format
 */
void export_solution_to_xml(const std::string& filename, const XMLMusicalSolution& solution) {
    std::ofstream xml_out(filename);
    if (!xml_out.is_open()) {
        std::cerr << "Error: Could not create XML file " << filename << std::endl;
        return;
    }
    
    xml_out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
    xml_out << "<musical_solution>" << std::endl;
    xml_out << "  <metadata>" << std::endl;
    xml_out << "    <config_file>" << solution.config_file << "</config_file>" << std::endl;
    xml_out << "    <problem_name>" << solution.problem_name << "</problem_name>" << std::endl;
    xml_out << "    <problem_description>" << solution.problem_description << "</problem_description>" << std::endl;
    xml_out << "    <solve_time_ms>" << solution.solve_time_ms << "</solve_time_ms>" << std::endl;
    xml_out << "    <rules_applied>" << solution.rules_applied << "</rules_applied>" << std::endl;
    xml_out << "    <rules_checked>" << solution.rules_checked << "</rules_checked>" << std::endl;
    xml_out << "  </metadata>" << std::endl;
    
    if (!solution.voice_solutions.empty()) {
        xml_out << "  <voices>" << std::endl;
        for (size_t voice = 0; voice < solution.voice_solutions.size(); ++voice) {
            xml_out << "    <voice id=\"" << voice << "\">" << std::endl;
            
            xml_out << "      <pitch_sequence>" << std::endl;
            for (size_t i = 0; i < solution.voice_solutions[voice].size(); ++i) {
                xml_out << "        <note position=\"" << (i + 1) << "\" midi=\"" << solution.voice_solutions[voice][i]
                       << "\" name=\"" << midi_to_note_name(solution.voice_solutions[voice][i])
                       << "\"/>" << std::endl;
            }
            xml_out << "      </pitch_sequence>" << std::endl;
            
            if (voice < solution.voice_rhythms.size()) {
                xml_out << "      <rhythm_sequence>" << std::endl;
                for (size_t i = 0; i < solution.voice_rhythms[voice].size(); ++i) {
                    xml_out << "        <duration position=\"" << (i + 1) << "\" value=\"" << solution.voice_rhythms[voice][i]
                           << "\" note_type=\"1/" << (16 / solution.voice_rhythms[voice][i]) << "\"/>" << std::endl;
                }
                xml_out << "      </rhythm_sequence>" << std::endl;
            }
            
            xml_out << "    </voice>" << std::endl;
        }
        xml_out << "  </voices>" << std::endl;
        
        if (!solution.metric_signature.empty()) {
            xml_out << "  <metric_signature>" << std::endl;
            for (size_t i = 0; i < solution.metric_signature.size(); ++i) {
                xml_out << "    <time_signature numerator=\"" << solution.metric_signature[i] << "\" denominator=\"4\"/>" << std::endl;
            }
            xml_out << "  </metric_signature>" << std::endl;
        }
    }
    
    xml_out << "</musical_solution>" << std::endl;
    xml_out.close();
    std::cout << "✅ XML: " << filename << std::endl;
}

// Test function
int main() {
    // Example usage
    XMLMusicalSolution solution;
    solution.voice_solutions = {{0, 1, 2}, {3, 4, 5}};
    solution.voice_rhythms = {{4, 4, 4}, {4, 4, 4}};
    solution.metric_signature = {3};
    solution.config_file = "test_config.json";
    solution.problem_name = "Test Problem";
    solution.problem_description = "Test Description";
    solution.solve_time_ms = 5.0;
    solution.rules_applied = 3;
    solution.rules_checked = 10;
    
    export_solution_to_xml("test_output.xml", solution);
    
    return 0;
}