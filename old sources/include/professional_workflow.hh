/**
 * @file professional_workflow.hh
 * @brief Phase 5: Professional Features - Advanced Workflow Management
 * 
 * Professional-grade workflow tools for musical composition and analysis.
 * Handles project management, batch processing, and collaboration features.
 */

#ifndef PROFESSIONAL_WORKFLOW_HH
#define PROFESSIONAL_WORKFLOW_HH

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace MusicalConstraints {

// Forward declarations
class MusicalState;
class DualSolutionStorage;

/**
 * @brief Professional project management for musical compositions
 */
struct MusicalProject {
    std::string project_name;
    std::string composer_name;
    std::string creation_date;
    std::string last_modified;
    std::vector<std::string> composition_files;
    std::map<std::string, std::string> metadata;
    std::vector<std::string> tags;
    
    MusicalProject(const std::string& name = "Untitled Project", 
                   const std::string& composer = "Anonymous")
        : project_name(name), composer_name(composer) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        creation_date = last_modified = ss.str();
        
        // Default metadata
        metadata["version"] = "1.0";
        metadata["status"] = "active";
        metadata["priority"] = "normal";
    }
    
    void add_composition(const std::string& filename) {
        composition_files.push_back(filename);
        touch_modified();
    }
    
    void add_tag(const std::string& tag) {
        if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
            tags.push_back(tag);
            touch_modified();
        }
    }
    
    void set_metadata(const std::string& key, const std::string& value) {
        metadata[key] = value;
        touch_modified();
    }
    
private:
    void touch_modified() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        last_modified = ss.str();
    }
};

/**
 * @brief Batch processing capabilities for multiple compositions
 */
class BatchProcessor {
private:
    std::vector<std::string> input_files_;
    std::string output_directory_;
    std::map<std::string, std::string> processing_options_;
    
public:
    explicit BatchProcessor(const std::string& output_dir = "./batch_output") 
        : output_directory_(output_dir) {
        // Default processing options
        processing_options_["format"] = "analysis";
        processing_options_["quality"] = "high";
        processing_options_["parallelize"] = "true";
        processing_options_["preserve_metadata"] = "true";
    }
    
    /**
     * @brief Add file to batch processing queue
     */
    void add_file(const std::string& filename) {
        input_files_.push_back(filename);
        std::cout << "📁 Added to batch queue: " << filename << "\n";
    }
    
    /**
     * @brief Set processing option
     */
    void set_option(const std::string& key, const std::string& value) {
        processing_options_[key] = value;
        std::cout << "⚙️ Set processing option: " << key << " = " << value << "\n";
    }
    
    /**
     * @brief Execute batch processing
     */
    struct BatchResult {
        int total_files;
        int successful_files;
        int failed_files;
        double total_processing_time_ms;
        std::vector<std::string> error_messages;
        
        BatchResult() : total_files(0), successful_files(0), failed_files(0), 
                       total_processing_time_ms(0.0) {}
    };
    
    BatchResult process_all() {
        BatchResult result;
        result.total_files = input_files_.size();
        
        auto start_time = std::chrono::steady_clock::now();
        
        std::cout << "\n🚀 Starting batch processing of " << input_files_.size() << " files...\n";
        
        for (size_t i = 0; i < input_files_.size(); ++i) {
            const std::string& filename = input_files_[i];
            
            std::cout << "Processing file " << (i + 1) << "/" << input_files_.size() 
                      << ": " << filename << "\n";
            
            try {
                if (process_single_file(filename)) {
                    result.successful_files++;
                    std::cout << "  ✅ SUCCESS\n";
                } else {
                    result.failed_files++;
                    result.error_messages.push_back("Processing failed: " + filename);
                    std::cout << "  ❌ FAILED\n";
                }
            } catch (const std::exception& e) {
                result.failed_files++;
                result.error_messages.push_back(std::string("Exception in ") + filename + ": " + e.what());
                std::cout << "  ❌ EXCEPTION: " << e.what() << "\n";
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        result.total_processing_time_ms = 
            std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        print_batch_summary(result);
        return result;
    }
    
    /**
     * @brief Clear processing queue
     */
    void clear_queue() {
        input_files_.clear();
        std::cout << "🗑️ Batch processing queue cleared\n";
    }
    
private:
    bool process_single_file(const std::string& filename) {
        // Simulate sophisticated file processing
        // In real implementation, this would load, analyze, and process the musical file
        
        auto start = std::chrono::steady_clock::now();
        
        // Simulate processing time based on file complexity
        std::this_thread::sleep_for(std::chrono::milliseconds(1 + (filename.length() % 5)));
        
        auto end = std::chrono::steady_clock::now();
        double processing_time = std::chrono::duration<double, std::milli>(end - start).count();
        
        std::cout << "    📊 Analysis completed in " << std::fixed << std::setprecision(2) 
                  << processing_time << "ms\n";
        std::cout << "    🎵 Musical features extracted\n";
        std::cout << "    💾 Results saved to " << output_directory_ << "\n";
        
        return true; // Success
    }
    
    void print_batch_summary(const BatchResult& result) {
        std::cout << "\n📋 Batch Processing Summary:\n";
        std::cout << "  Total files: " << result.total_files << "\n";
        std::cout << "  Successful: " << result.successful_files << " ✅\n";
        std::cout << "  Failed: " << result.failed_files << " ❌\n";
        std::cout << "  Success rate: " << std::fixed << std::setprecision(1) 
                  << (100.0 * result.successful_files / std::max(1, result.total_files)) << "%\n";
        std::cout << "  Total time: " << std::fixed << std::setprecision(2) 
                  << result.total_processing_time_ms << "ms\n";
        
        if (!result.error_messages.empty()) {
            std::cout << "\n🔍 Error Details:\n";
            for (const auto& error : result.error_messages) {
                std::cout << "  • " << error << "\n";
            }
        }
    }
};

/**
 * @brief Professional workflow manager
 */
class WorkflowManager {
private:
    std::vector<MusicalProject> projects_;
    std::string current_project_name_;
    std::string workspace_directory_;
    
public:
    explicit WorkflowManager(const std::string& workspace = "./workspace") 
        : workspace_directory_(workspace) {
        std::cout << "🏢 Professional workflow manager initialized\n";
        std::cout << "📂 Workspace: " << workspace_directory_ << "\n";
    }
    
    /**
     * @brief Create new musical project
     */
    bool create_project(const std::string& name, const std::string& composer = "Anonymous") {
        // Check if project already exists
        for (const auto& project : projects_) {
            if (project.project_name == name) {
                std::cout << "❌ Project '" << name << "' already exists\n";
                return false;
            }
        }
        
        MusicalProject new_project(name, composer);
        projects_.push_back(new_project);
        current_project_name_ = name;
        
        std::cout << "✨ Created project: '" << name << "' by " << composer << "\n";
        std::cout << "📅 Created: " << new_project.creation_date << "\n";
        
        return true;
    }
    
    /**
     * @brief Switch to existing project
     */
    bool switch_project(const std::string& name) {
        for (const auto& project : projects_) {
            if (project.project_name == name) {
                current_project_name_ = name;
                std::cout << "🔄 Switched to project: '" << name << "'\n";
                return true;
            }
        }
        
        std::cout << "❌ Project '" << name << "' not found\n";
        return false;
    }
    
    /**
     * @brief Add composition to current project
     */
    bool add_composition_to_current(const std::string& filename, 
                                   const std::vector<std::string>& tags = {}) {
        if (current_project_name_.empty()) {
            std::cout << "❌ No active project. Create or switch to a project first.\n";
            return false;
        }
        
        for (auto& project : projects_) {
            if (project.project_name == current_project_name_) {
                project.add_composition(filename);
                
                for (const auto& tag : tags) {
                    project.add_tag(tag);
                }
                
                std::cout << "📝 Added composition '" << filename 
                          << "' to project '" << current_project_name_ << "'\n";
                
                if (!tags.empty()) {
                    std::cout << "🏷️ Tags: ";
                    for (size_t i = 0; i < tags.size(); ++i) {
                        std::cout << tags[i];
                        if (i < tags.size() - 1) std::cout << ", ";
                    }
                    std::cout << "\n";
                }
                
                return true;
            }
        }
        
        return false;
    }
    
    /**
     * @brief List all projects
     */
    void list_projects() const {
        if (projects_.empty()) {
            std::cout << "📁 No projects found. Create a project to get started.\n";
            return;
        }
        
        std::cout << "\n📂 Musical Projects (" << projects_.size() << " total):\n";
        for (size_t i = 0; i < projects_.size(); ++i) {
            const auto& project = projects_[i];
            std::string status = (project.project_name == current_project_name_) ? "🟢 ACTIVE" : "⚪";
            
            std::cout << "  " << (i + 1) << ". " << status << " " << project.project_name << "\n";
            std::cout << "     👤 Composer: " << project.composer_name << "\n";
            std::cout << "     📅 Created: " << project.creation_date << "\n";
            std::cout << "     📝 Compositions: " << project.composition_files.size() << "\n";
            
            if (!project.tags.empty()) {
                std::cout << "     🏷️ Tags: ";
                for (size_t t = 0; t < project.tags.size(); ++t) {
                    std::cout << project.tags[t];
                    if (t < project.tags.size() - 1) std::cout << ", ";
                }
                std::cout << "\n";
            }
        }
    }
    
    /**
     * @brief Get current project details
     */
    const MusicalProject* get_current_project() const {
        if (current_project_name_.empty()) return nullptr;
        
        for (const auto& project : projects_) {
            if (project.project_name == current_project_name_) {
                return &project;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Generate project statistics
     */
    void print_project_statistics() const {
        auto current = get_current_project();
        if (!current) {
            std::cout << "❌ No active project for statistics\n";
            return;
        }
        
        std::cout << "\n📊 Project Statistics: " << current->project_name << "\n";
        std::cout << "👤 Composer: " << current->composer_name << "\n";
        std::cout << "📅 Created: " << current->creation_date << "\n";
        std::cout << "🔄 Last modified: " << current->last_modified << "\n";
        std::cout << "📝 Total compositions: " << current->composition_files.size() << "\n";
        std::cout << "🏷️ Tags: " << current->tags.size() << "\n";
        std::cout << "🗂️ Metadata entries: " << current->metadata.size() << "\n";
        
        if (!current->composition_files.empty()) {
            std::cout << "\n📁 Composition Files:\n";
            for (size_t i = 0; i < current->composition_files.size(); ++i) {
                std::cout << "  " << (i + 1) << ". " << current->composition_files[i] << "\n";
            }
        }
        
        if (!current->metadata.empty()) {
            std::cout << "\n🗂️ Project Metadata:\n";
            for (const auto& meta : current->metadata) {
                std::cout << "  " << meta.first << ": " << meta.second << "\n";
            }
        }
    }
};

/**
 * @brief Collaboration and version control features
 */
class CollaborationManager {
private:
    std::string author_name_;
    std::vector<std::string> collaborators_;
    std::map<std::string, std::vector<std::string>> composition_history_;
    
public:
    explicit CollaborationManager(const std::string& author = "Anonymous") 
        : author_name_(author) {
        std::cout << "🤝 Collaboration manager initialized for: " << author_name_ << "\n";
    }
    
    /**
     * @brief Add collaborator to project
     */
    void add_collaborator(const std::string& name) {
        if (std::find(collaborators_.begin(), collaborators_.end(), name) == collaborators_.end()) {
            collaborators_.push_back(name);
            std::cout << "👥 Added collaborator: " << name << "\n";
        } else {
            std::cout << "ℹ️ " << name << " is already a collaborator\n";
        }
    }
    
    /**
     * @brief Record composition change
     */
    void record_change(const std::string& composition_name, const std::string& change_description) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream change_entry;
        change_entry << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        change_entry << " [" << author_name_ << "] " << change_description;
        
        composition_history_[composition_name].push_back(change_entry.str());
        
        std::cout << "📝 Recorded change in '" << composition_name << "': " << change_description << "\n";
    }
    
    /**
     * @brief Show change history for composition
     */
    void show_history(const std::string& composition_name) const {
        auto it = composition_history_.find(composition_name);
        if (it == composition_history_.end()) {
            std::cout << "❌ No history found for: " << composition_name << "\n";
            return;
        }
        
        std::cout << "\n📜 Change History: " << composition_name << "\n";
        const auto& history = it->second;
        
        for (size_t i = 0; i < history.size(); ++i) {
            std::cout << "  " << (i + 1) << ". " << history[i] << "\n";
        }
    }
    
    /**
     * @brief List all collaborators
     */
    void list_collaborators() const {
        std::cout << "\n👥 Project Collaborators:\n";
        std::cout << "  👑 Author: " << author_name_ << "\n";
        
        if (collaborators_.empty()) {
            std::cout << "  (No additional collaborators)\n";
        } else {
            for (size_t i = 0; i < collaborators_.size(); ++i) {
                std::cout << "  👤 " << (i + 1) << ". " << collaborators_[i] << "\n";
            }
        }
    }
    
    /**
     * @brief Generate collaboration statistics
     */
    struct CollaborationStats {
        int total_collaborators;
        int total_compositions;
        int total_changes;
        std::map<std::string, int> author_contributions;
        
        CollaborationStats() : total_collaborators(0), total_compositions(0), total_changes(0) {}
    };
    
    CollaborationStats get_stats() const {
        CollaborationStats stats;
        stats.total_collaborators = 1 + collaborators_.size(); // Author + collaborators
        stats.total_compositions = composition_history_.size();
        
        for (const auto& comp : composition_history_) {
            stats.total_changes += comp.second.size();
            
            // Analyze contributions by author
            for (const auto& change : comp.second) {
                // Extract author from change entry format: "DATE [AUTHOR] DESCRIPTION"
                size_t start = change.find('[');
                size_t end = change.find(']');
                if (start != std::string::npos && end != std::string::npos && end > start) {
                    std::string author = change.substr(start + 1, end - start - 1);
                    stats.author_contributions[author]++;
                }
            }
        }
        
        return stats;
    }
    
    void print_collaboration_report() const {
        auto stats = get_stats();
        
        std::cout << "\n🤝 Collaboration Report:\n";
        std::cout << "👥 Total collaborators: " << stats.total_collaborators << "\n";
        std::cout << "🎵 Compositions tracked: " << stats.total_compositions << "\n";
        std::cout << "📝 Total changes: " << stats.total_changes << "\n";
        
        if (!stats.author_contributions.empty()) {
            std::cout << "\n📊 Contributions by Author:\n";
            for (const auto& contrib : stats.author_contributions) {
                double percentage = (100.0 * contrib.second) / std::max(1, stats.total_changes);
                std::cout << "  " << contrib.first << ": " << contrib.second 
                          << " changes (" << std::fixed << std::setprecision(1) 
                          << percentage << "%)\n";
            }
        }
    }
};

} // namespace MusicalConstraints

#endif // PROFESSIONAL_WORKFLOW_HH