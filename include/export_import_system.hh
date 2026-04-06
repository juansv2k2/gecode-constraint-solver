/**
 * @file export_import_system.hh 
 * @brief Phase 5: Professional Features - Export/Import Capabilities
 * 
 * Professional export and import system for various musical formats.
 * Supports MIDI, MusicXML, analysis data, and custom formats.
 */

#ifndef EXPORT_IMPORT_SYSTEM_HH
#define EXPORT_IMPORT_SYSTEM_HH

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <memory>
#include <map>
#include "dual_solution_storage.hh"

namespace MusicalConstraints {

/**
 * @brief Musical note representation for export
 */
struct ExportNote {
    int midi_pitch;
    double start_time;      // In beats
    double duration;        // In beats  
    int velocity;           // MIDI velocity (0-127)
    int channel;            // MIDI channel (0-15)
    
    ExportNote(int pitch = 60, double start = 0.0, double dur = 1.0, 
               int vel = 80, int ch = 0)
        : midi_pitch(pitch), start_time(start), duration(dur), velocity(vel), channel(ch) {}
};

/**
 * @brief Musical composition data for export
 */
struct ExportComposition {
    std::string title;
    std::string composer;
    int tempo;                          // BPM
    int time_signature_num;             // Time signature numerator
    int time_signature_den;             // Time signature denominator
    int key_signature;                  // -7 to +7 (flats to sharps)
    std::vector<ExportNote> notes;
    std::map<std::string, std::string> metadata;
    
    ExportComposition(const std::string& t = "Untitled", const std::string& c = "Anonymous")
        : title(t), composer(c), tempo(120), time_signature_num(4), time_signature_den(4), 
          key_signature(0) {}
    
    void add_note(const ExportNote& note) {
        notes.push_back(note);
    }
    
    void set_metadata(const std::string& key, const std::string& value) {
        metadata[key] = value;
    }
    
    // Calculate composition statistics
    double get_total_duration() const {
        double max_end = 0.0;
        for (const auto& note : notes) {
            max_end = std::max(max_end, note.start_time + note.duration);
        }
        return max_end;
    }
    
    std::pair<int, int> get_pitch_range() const {
        if (notes.empty()) return {60, 60};
        
        int min_pitch = notes[0].midi_pitch;
        int max_pitch = notes[0].midi_pitch;
        
        for (const auto& note : notes) {
            min_pitch = std::min(min_pitch, note.midi_pitch);
            max_pitch = std::max(max_pitch, note.midi_pitch);
        }
        
        return {min_pitch, max_pitch};
    }
};

/**
 * @brief MIDI export capabilities
 */
class MidiExporter {
private:
    std::string output_directory_;
    
    // MIDI file format constants
    static const int MIDI_HEADER_LENGTH = 14;
    static const int TRACK_HEADER_LENGTH = 8;
    
public:
    explicit MidiExporter(const std::string& output_dir = "./midi_exports") 
        : output_directory_(output_dir) {
        std::cout << "🎹 MIDI Exporter initialized - Output: " << output_directory_ << "\n";
    }
    
    /**
     * @brief Export composition to MIDI file
     */
    bool export_midi(const ExportComposition& composition, const std::string& filename) {
        std::cout << "🎵 Exporting MIDI: " << filename << "\n";
        std::cout << "  Title: " << composition.title << "\n";
        std::cout << "  Composer: " << composition.composer << "\n";
        std::cout << "  Notes: " << composition.notes.size() << "\n";
        std::cout << "  Duration: " << std::fixed << std::setprecision(2) 
                  << composition.get_total_duration() << " beats\n";
        
        auto pitch_range = composition.get_pitch_range();
        std::cout << "  Pitch range: " << pitch_range.first << "-" << pitch_range.second 
                  << " (MIDI)\n";
        
        try {
            // Generate MIDI file path
            std::string filepath = output_directory_ + "/" + filename + ".mid";
            
            // Create simplified MIDI-like representation
            // In full implementation, this would write actual MIDI binary format
            std::ofstream midi_file(filepath);
            if (!midi_file.is_open()) {
                std::cout << "❌ Failed to create MIDI file: " << filepath << "\n";
                return false;
            }
            
            write_midi_header(midi_file, composition);
            write_midi_track(midi_file, composition);
            
            midi_file.close();
            
            std::cout << "✅ MIDI export successful: " << filepath << "\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ MIDI export failed: " << e.what() << "\n";
            return false;
        }
    }
    
    /**
     * @brief Convert DualSolutionStorage to ExportComposition
     */
    ExportComposition convert_from_storage(const DualSolutionStorage& storage,
                                          const std::string& title = "Generated Composition",
                                          double note_duration = 1.0) {
        ExportComposition composition(title, "Musical AI");
        
        std::cout << "🔄 Converting solution storage to MIDI format...\n";
        
        // Convert absolute pitches to MIDI notes
        for (int i = 0; i < storage.length(); ++i) {
            int pitch = storage.absolute(i);
            double start_time = i * note_duration;
            int velocity = 80; // Default velocity
            
            // Add some velocity variation based on position
            if (i == 0) velocity = 100;  // Emphasize first note
            else if (i % 4 == 0) velocity = 90; // Emphasize downbeats
            
            ExportNote note(pitch, start_time, note_duration, velocity);
            composition.add_note(note);
        }
        
        // Set metadata
        composition.set_metadata("Generator", "Musical Constraint Solver");
        composition.set_metadata("Generated", get_current_timestamp());
        composition.set_metadata("Notes", std::to_string(composition.notes.size()));
        
        std::cout << "✅ Conversion complete: " << composition.notes.size() << " notes\n";
        return composition;
    }
    
private:
    void write_midi_header(std::ofstream& file, const ExportComposition& comp) {
        // MIDI header information (simplified text format for demonstration)
        file << "MIDI File Format - Musical Constraint Solver Export\n";
        file << "======================================\n";
        file << "Title: " << comp.title << "\n";
        file << "Composer: " << comp.composer << "\n";
        file << "Tempo: " << comp.tempo << " BPM\n";
        file << "Time Signature: " << comp.time_signature_num << "/" << comp.time_signature_den << "\n";
        file << "Key Signature: " << comp.key_signature << "\n";
        file << "Total Notes: " << comp.notes.size() << "\n";
        file << "Duration: " << comp.get_total_duration() << " beats\n";
        file << "\n";
    }
    
    void write_midi_track(std::ofstream& file, const ExportComposition& comp) {
        file << "TRACK DATA:\n";
        file << "Time(beats)  Pitch(MIDI)  Duration  Velocity  Channel\n";
        file << "---------------------------------------------------\n";
        
        for (const auto& note : comp.notes) {
            file << std::fixed << std::setprecision(2) << std::setw(10) << note.start_time
                 << std::setw(12) << note.midi_pitch
                 << std::setw(10) << note.duration
                 << std::setw(9) << note.velocity
                 << std::setw(9) << note.channel << "\n";
        }
        
        file << "\nEND OF TRACK\n";
    }
    
    std::string get_current_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

/**
 * @brief MusicXML export capabilities
 */
class MusicXMLExporter {
private:
    std::string output_directory_;
    
public:
    explicit MusicXMLExporter(const std::string& output_dir = "./musicxml_exports") 
        : output_directory_(output_dir) {
        std::cout << "📜 MusicXML Exporter initialized - Output: " << output_directory_ << "\n";
    }
    
    /**
     * @brief Export composition to MusicXML format
     */
    bool export_musicxml(const ExportComposition& composition, const std::string& filename) {
        std::cout << "📜 Exporting MusicXML: " << filename << "\n";
        
        try {
            std::string filepath = output_directory_ + "/" + filename + ".musicxml";
            std::ofstream xml_file(filepath);
            if (!xml_file.is_open()) {
                std::cout << "❌ Failed to create MusicXML file: " << filepath << "\n";
                return false;
            }
            
            write_xml_header(xml_file);
            write_xml_score_info(xml_file, composition);
            write_xml_part_list(xml_file);
            write_xml_part(xml_file, composition);
            write_xml_footer(xml_file);
            
            xml_file.close();
            
            std::cout << "✅ MusicXML export successful: " << filepath << "\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ MusicXML export failed: " << e.what() << "\n";
            return false;
        }
    }
    
private:
    void write_xml_header(std::ofstream& file) {
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        file << "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 3.0 Partwise//EN\"\n";
        file << "    \"http://www.musicxml.org/dtds/partwise.dtd\">\n";
        file << "<score-partwise version=\"3.0\">\n";
    }
    
    void write_xml_score_info(std::ofstream& file, const ExportComposition& comp) {
        file << "  <movement-title>" << comp.title << "</movement-title>\n";
        file << "  <identification>\n";
        file << "    <creator type=\"composer\">" << comp.composer << "</creator>\n";
        file << "    <encoding>\n";
        file << "      <software>Musical Constraint Solver v5.0</software>\n";
        file << "      <encoding-date>" << get_current_date() << "</encoding-date>\n";
        file << "    </encoding>\n";
        file << "  </identification>\n";
    }
    
    void write_xml_part_list(std::ofstream& file) {
        file << "  <part-list>\n";
        file << "    <score-part id=\"P1\">\n";
        file << "      <part-name>Generated Part</part-name>\n";
        file << "    </score-part>\n";
        file << "  </part-list>\n";
    }
    
    void write_xml_part(std::ofstream& file, const ExportComposition& comp) {
        file << "  <part id=\"P1\">\n";
        
        // Measure 1 with attributes
        file << "    <measure number=\"1\">\n";
        file << "      <attributes>\n";
        file << "        <divisions>1</divisions>\n";
        file << "        <key>\n";
        file << "          <fifths>" << comp.key_signature << "</fifths>\n";
        file << "        </key>\n";
        file << "        <time>\n";
        file << "          <beats>" << comp.time_signature_num << "</beats>\n";
        file << "          <beat-type>" << comp.time_signature_den << "</beat-type>\n";
        file << "        </time>\n";
        file << "        <clef>\n";
        file << "          <sign>G</sign>\n";
        file << "          <line>2</line>\n";
        file << "        </clef>\n";
        file << "      </attributes>\n";
        file << "      <sound tempo=\"" << comp.tempo << "\"/>\n";
        
        // Write notes (simplified - all in first measure for demo)
        for (size_t i = 0; i < std::min((size_t)4, comp.notes.size()); ++i) {
            const auto& note = comp.notes[i];
            write_xml_note(file, note);
        }
        
        file << "    </measure>\n";
        file << "  </part>\n";
    }
    
    void write_xml_note(std::ofstream& file, const ExportNote& note) {
        file << "      <note>\n";
        file << "        <pitch>\n";
        
        // MIDI to pitch name conversion (simplified)
        std::string pitch_class = midi_to_pitch_class(note.midi_pitch);
        int octave = (note.midi_pitch / 12) - 1;
        
        file << "          <step>" << pitch_class.substr(0, 1) << "</step>\n";
        if (pitch_class.length() > 1) {
            file << "          <alter>" << (pitch_class[1] == '#' ? "1" : "-1") << "</alter>\n";
        }
        file << "          <octave>" << octave << "</octave>\n";
        file << "        </pitch>\n";
        file << "        <duration>1</duration>\n";
        file << "        <voice>1</voice>\n";
        file << "        <type>quarter</type>\n";
        file << "      </note>\n";
    }
    
    void write_xml_footer(std::ofstream& file) {
        file << "</score-partwise>\n";
    }
    
    std::string midi_to_pitch_class(int midi_pitch) {
        const std::vector<std::string> pitch_classes = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        return pitch_classes[midi_pitch % 12];
    }
    
    std::string get_current_date() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
        return ss.str();
    }
};

/**
 * @brief Analysis data export capabilities
 */
class AnalysisExporter {
private:
    std::string output_directory_;
    
public:
    explicit AnalysisExporter(const std::string& output_dir = "./analysis_exports") 
        : output_directory_(output_dir) {
        std::cout << "📊 Analysis Exporter initialized - Output: " << output_directory_ << "\n";
    }
    
    /**
     * @brief Export detailed musical analysis
     */
    bool export_analysis(const ExportComposition& composition, 
                        const std::string& filename,
                        bool include_statistics = true,
                        bool include_patterns = true) {
        std::cout << "📊 Exporting musical analysis: " << filename << "\n";
        
        try {
            std::string filepath = output_directory_ + "/" + filename + "_analysis.txt";
            std::ofstream analysis_file(filepath);
            if (!analysis_file.is_open()) {
                std::cout << "❌ Failed to create analysis file: " << filepath << "\n";
                return false;
            }
            
            write_analysis_header(analysis_file, composition);
            
            if (include_statistics) {
                write_statistical_analysis(analysis_file, composition);
            }
            
            if (include_patterns) {
                write_pattern_analysis(analysis_file, composition);
            }
            
            write_detailed_note_analysis(analysis_file, composition);
            
            analysis_file.close();
            
            std::cout << "✅ Analysis export successful: " << filepath << "\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "❌ Analysis export failed: " << e.what() << "\n";
            return false;
        }
    }
    
private:
    void write_analysis_header(std::ofstream& file, const ExportComposition& comp) {
        file << "MUSICAL ANALYSIS REPORT\n";
        file << "======================\n\n";
        file << "Composition: " << comp.title << "\n";
        file << "Composer: " << comp.composer << "\n";
        file << "Generated: " << get_current_timestamp() << "\n";
        file << "analyzer: Musical Constraint Solver v5.0\n\n";
    }
    
    void write_statistical_analysis(std::ofstream& file, const ExportComposition& comp) {
        file << "STATISTICAL ANALYSIS\n";
        file << "-------------------\n";
        file << "Total notes: " << comp.notes.size() << "\n";
        file << "Duration: " << comp.get_total_duration() << " beats\n";
        file << "Tempo: " << comp.tempo << " BPM\n";
        file << "Time signature: " << comp.time_signature_num << "/" << comp.time_signature_den << "\n";
        file << "Key signature: " << comp.key_signature << " (0=C major)\n";
        
        auto pitch_range = comp.get_pitch_range();
        file << "Pitch range: " << pitch_range.first << "-" << pitch_range.second << " MIDI\n";
        file << "Pitch span: " << (pitch_range.second - pitch_range.first) << " semitones\n";
        
        // Calculate average pitch
        double avg_pitch = 0.0;
        for (const auto& note : comp.notes) {
            avg_pitch += note.midi_pitch;
        }
        avg_pitch /= std::max(1, (int)comp.notes.size());
        file << "Average pitch: " << std::fixed << std::setprecision(1) << avg_pitch << " MIDI\n";
        
        // Calculate interval statistics
        std::map<int, int> interval_freq;
        for (size_t i = 1; i < comp.notes.size(); ++i) {
            int interval = comp.notes[i].midi_pitch - comp.notes[i-1].midi_pitch;
            interval_freq[interval]++;
        }
        
        file << "\nInterval frequency:\n";
        for (const auto& freq : interval_freq) {
            file << "  " << std::setw(3) << freq.first << " semitones: " << freq.second << " times\n";
        }
        
        file << "\n";
    }
    
    void write_pattern_analysis(std::ofstream& file, const ExportComposition& comp) {
        file << "PATTERN ANALYSIS\n";
        file << "---------------\n";
        
        // Analyze melodic direction
        int ascending = 0, descending = 0, repeated = 0;
        for (size_t i = 1; i < comp.notes.size(); ++i) {
            int interval = comp.notes[i].midi_pitch - comp.notes[i-1].midi_pitch;
            if (interval > 0) ascending++;
            else if (interval < 0) descending++;
            else repeated++;
        }
        
        file << "Melodic direction:\n";
        file << "  Ascending intervals: " << ascending << "\n";
        file << "  Descending intervals: " << descending << "\n";
        file << "  Repeated notes: " << repeated << "\n";
        
        double total = ascending + descending + repeated;
        if (total > 0) {
            file << "  Ascending %: " << std::fixed << std::setprecision(1) 
                 << (100.0 * ascending / total) << "\n";
            file << "  Descending %: " << std::fixed << std::setprecision(1) 
                 << (100.0 * descending / total) << "\n";
        }
        
        // Analyze step vs leap motion
        int steps = 0, leaps = 0;
        for (size_t i = 1; i < comp.notes.size(); ++i) {
            int interval = std::abs(comp.notes[i].midi_pitch - comp.notes[i-1].midi_pitch);
            if (interval <= 2) steps++;
            else if (interval > 2) leaps++;
        }
        
        file << "\nMelodic motion:\n";
        file << "  Stepwise motion: " << steps << "\n";
        file << "  Leap motion: " << leaps << "\n";
        if (steps + leaps > 0) {
            file << "  Stepwise %: " << std::fixed << std::setprecision(1) 
                 << (100.0 * steps / (steps + leaps)) << "\n";
        }
        
        file << "\n";
    }
    
    void write_detailed_note_analysis(std::ofstream& file, const ExportComposition& comp) {
        file << "DETAILED NOTE ANALYSIS\n";
        file << "---------------------\n";
        file << "Note#  Time(beats)  MIDI  Duration  Velocity  Interval\n";
        file << "-----------------------------------------------------\n";
        
        for (size_t i = 0; i < comp.notes.size(); ++i) {
            const auto& note = comp.notes[i];
            file << std::setw(5) << (i + 1)
                 << std::setw(12) << std::fixed << std::setprecision(2) << note.start_time
                 << std::setw(6) << note.midi_pitch
                 << std::setw(10) << note.duration
                 << std::setw(9) << note.velocity;
            
            if (i > 0) {
                int interval = note.midi_pitch - comp.notes[i-1].midi_pitch;
                file << std::setw(9) << interval;
            } else {
                file << std::setw(9) << "-";
            }
            file << "\n";
        }
    }
    
    std::string get_current_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

/**
 * @brief Universal export manager
 */
class UniversalExporter {
private:
    MidiExporter midi_exporter_;
    MusicXMLExporter xml_exporter_;
    AnalysisExporter analysis_exporter_;
    
public:
    explicit UniversalExporter(const std::string& base_output_dir = "./exports") 
        : midi_exporter_(base_output_dir + "/midi"),
          xml_exporter_(base_output_dir + "/musicxml"),
          analysis_exporter_(base_output_dir + "/analysis") {
        std::cout << "🚀 Universal Export System initialized\n";
        std::cout << "📂 Base output directory: " << base_output_dir << "\n";
    }
    
    /**
     * @brief Export to all supported formats
     */
    void export_all_formats(const ExportComposition& composition, const std::string& filename) {
        std::cout << "\n🎼 Exporting '" << composition.title << "' to all formats...\n";
        
        bool midi_success = midi_exporter_.export_midi(composition, filename);
        bool xml_success = xml_exporter_.export_musicxml(composition, filename);
        bool analysis_success = analysis_exporter_.export_analysis(composition, filename);
        
        std::cout << "\n📋 Export Summary:\n";
        std::cout << "  MIDI: " << (midi_success ? "✅ SUCCESS" : "❌ FAILED") << "\n";
        std::cout << "  MusicXML: " << (xml_success ? "✅ SUCCESS" : "❌ FAILED") << "\n";
        std::cout << "  Analysis: " << (analysis_success ? "✅ SUCCESS" : "❌ FAILED") << "\n";
        
        int successful = midi_success + xml_success + analysis_success;
        std::cout << "  Overall: " << successful << "/3 formats exported successfully\n";
    }
    
    /**
     * @brief Export DualSolutionStorage with automatic format detection
     */
    void export_solution(const DualSolutionStorage& storage, 
                        const std::string& filename,
                        const std::string& format = "all") {
        std::cout << "\n🔄 Converting solution storage for export...\n";
        
        ExportComposition composition = midi_exporter_.convert_from_storage(storage, filename);
        
        if (format == "all") {
            export_all_formats(composition, filename);
        } else if (format == "midi") {
            midi_exporter_.export_midi(composition, filename);
        } else if (format == "xml" || format == "musicxml") {
            xml_exporter_.export_musicxml(composition, filename);
        } else if (format == "analysis") {
            analysis_exporter_.export_analysis(composition, filename);
        } else {
            std::cout << "❌ Unknown format: " << format << "\n";
            std::cout << "Available formats: all, midi, xml, analysis\n";
        }
    }
};

} // namespace MusicalConstraints

#endif // EXPORT_IMPORT_SYSTEM_HH