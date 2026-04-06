/**
 * @file advanced_configuration.hh
 * @brief Phase 5: Professional Features - Advanced Configuration System
 * 
 * Professional configuration management with presets, plugins, and performance tuning.
 */

#ifndef ADVANCED_CONFIGURATION_HH
#define ADVANCED_CONFIGURATION_HH

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace MusicalConstraints {

/**
 * @brief Configuration parameter with validation
 */
template<typename T>
struct ConfigParameter {
    T value;
    T default_value;
    T min_value;
    T max_value;
    std::string description;
    bool is_advanced;
    
    // Default constructor
    ConfigParameter() : value(T{}), default_value(T{}), min_value(T{}), 
                       max_value(T{}), description(""), is_advanced(false) {}
    
    ConfigParameter(const T& def_val, const T& min_val, const T& max_val,
                   const std::string& desc, bool advanced = false)
        : value(def_val), default_value(def_val), min_value(min_val), 
          max_value(max_val), description(desc), is_advanced(advanced) {}
    
    bool set_value(const T& new_value) {
        if (new_value >= min_value && new_value <= max_value) {
            value = new_value;
            return true;
        }
        return false;
    }
    
    void reset_to_default() {
        value = default_value;
    }
    
    bool is_at_default() const {
        return value == default_value;
    }
};

/**
 * @brief Performance tuning configuration
 */
class PerformanceConfig {
private:
    std::map<std::string, ConfigParameter<int>> int_params_;
    std::map<std::string, ConfigParameter<double>> double_params_;  
    std::map<std::string, ConfigParameter<bool>> bool_params_;
    
public:
    PerformanceConfig() {
        initialize_default_params();
    }
    
    void initialize_default_params() {
        // Constraint solving performance
        int_params_["max_search_depth"] = ConfigParameter<int>(1000, 10, 10000, 
            "Maximum search depth for constraint solving");
        int_params_["backtrack_limit"] = ConfigParameter<int>(500, 10, 5000,
            "Maximum number of backtracks before giving up");
        int_params_["solution_limit"] = ConfigParameter<int>(10, 1, 100,
            "Maximum number of solutions to find");
        int_params_["parallel_threads"] = ConfigParameter<int>(4, 1, 16,
            "Number of parallel threads for solving", true);
        
        // Memory management
        int_params_["memory_cache_size"] = ConfigParameter<int>(1000, 100, 10000,
            "Size of internal memory cache for patterns", true);
        int_params_["pattern_history_size"] = ConfigParameter<int>(50, 10, 200,
            "Number of patterns to keep in memory");
        
        // Musical intelligence
        double_params_["intelligence_threshold"] = ConfigParameter<double>(0.7, 0.0, 1.0,
            "Threshold for intelligent constraint suggestions");
        double_params_["pattern_confidence"] = ConfigParameter<double>(0.8, 0.0, 1.0,
            "Minimum confidence for pattern recognition");
        double_params_["learning_rate"] = ConfigParameter<double>(0.1, 0.01, 1.0,
            "Rate of adaptation for compositional learning", true);
        
        // Quality settings
        bool_params_["enable_advanced_analysis"] = ConfigParameter<bool>(true, false, true,
            "Enable advanced musical analysis features");
        bool_params_["enable_pattern_learning"] = ConfigParameter<bool>(true, false, true,
            "Enable automatic pattern learning from compositions");
        bool_params_["enable_intelligent_backjump"] = ConfigParameter<bool>(true, false, true,
            "Enable intelligent backjumping for performance");
        bool_params_["enable_verbose_logging"] = ConfigParameter<bool>(false, false, true,
            "Enable detailed logging for debugging", true);
    }
    
    // Getters
    int get_int(const std::string& key) const {
        auto it = int_params_.find(key);
        return (it != int_params_.end()) ? it->second.value : 0;
    }
    
    double get_double(const std::string& key) const {
        auto it = double_params_.find(key);
        return (it != double_params_.end()) ? it->second.value : 0.0;
    }
    
    bool get_bool(const std::string& key) const {
        auto it = bool_params_.find(key);
        return (it != bool_params_.end()) ? it->second.value : false;
    }
    
    // Setters with validation
    bool set_int(const std::string& key, int value) {
        auto it = int_params_.find(key);
        if (it != int_params_.end()) {
            return it->second.set_value(value);
        }
        return false;
    }
    
    bool set_double(const std::string& key, double value) {
        auto it = double_params_.find(key);
        if (it != double_params_.end()) {
            return it->second.set_value(value);
        }
        return false;
    }
    
    bool set_bool(const std::string& key, bool value) {
        auto it = bool_params_.find(key);
        if (it != bool_params_.end()) {
            return it->second.set_value(value);
        }
        return false;
    }
    
    void print_configuration(bool show_advanced = false) const {
        std::cout << "\n⚙️ Performance Configuration:\n";
        std::cout << "============================\n";
        
        std::cout << "\n📊 Integer Parameters:\n";
        for (const auto& param : int_params_) {
            if (!param.second.is_advanced || show_advanced) {
                std::cout << "  " << param.first << ": " << param.second.value;
                if (!param.second.is_at_default()) std::cout << " (modified)";
                if (param.second.is_advanced) std::cout << " [ADVANCED]";
                std::cout << "\n    " << param.second.description << "\n";
            }
        }
        
        std::cout << "\n📈 Double Parameters:\n";
        for (const auto& param : double_params_) {
            if (!param.second.is_advanced || show_advanced) {
                std::cout << "  " << param.first << ": " << param.second.value;
                if (!param.second.is_at_default()) std::cout << " (modified)";
                if (param.second.is_advanced) std::cout << " [ADVANCED]";
                std::cout << "\n    " << param.second.description << "\n";
            }
        }
        
        std::cout << "\n🔘 Boolean Parameters:\n";
        for (const auto& param : bool_params_) {
            if (!param.second.is_advanced || show_advanced) {
                std::cout << "  " << param.first << ": " << (param.second.value ? "true" : "false");
                if (!param.second.is_at_default()) std::cout << " (modified)";
                if (param.second.is_advanced) std::cout << " [ADVANCED]";
                std::cout << "\n    " << param.second.description << "\n";
            }
        }
    }
    
    void reset_to_defaults() {
        for (auto& param : int_params_) {
            param.second.reset_to_default();
        }
        for (auto& param : double_params_) {
            param.second.reset_to_default();
        }
        for (auto& param : bool_params_) {
            param.second.reset_to_default();
        }
        std::cout << "🔄 All parameters reset to default values\n";
    }
};

/**
 * @brief Musical preset management
 */
class MusicalPresetManager {
private:
    struct MusicalPreset {
        std::string name;
        std::string description;
        std::string category;
        std::map<std::string, std::string> parameters;
        
        MusicalPreset(const std::string& n, const std::string& desc, const std::string& cat)
            : name(n), description(desc), category(cat) {}
    };
    
    std::vector<MusicalPreset> presets_;
    
public:
    MusicalPresetManager() {
        initialize_default_presets();
    }
    
    void initialize_default_presets() {
        // Classical music presets
        MusicalPreset classical("Classical Composition", 
                               "Traditional classical music composition settings", "Classical");
        classical.parameters["intelligence_threshold"] = "0.8";
        classical.parameters["pattern_confidence"] = "0.9";
        classical.parameters["enable_advanced_analysis"] = "true";
        classical.parameters["solution_limit"] = "5";
        presets_.push_back(classical);
        
        // Jazz presets
        MusicalPreset jazz("Jazz Improvisation", 
                          "Jazz improvisation and chord progression settings", "Jazz");
        jazz.parameters["intelligence_threshold"] = "0.6";
        jazz.parameters["pattern_confidence"] = "0.7";
        jazz.parameters["learning_rate"] = "0.15";
        jazz.parameters["solution_limit"] = "15";
        presets_.push_back(jazz);
        
        // Contemporary presets
        MusicalPreset contemporary("Contemporary Music",
                                 "Modern and experimental music settings", "Contemporary");
        contemporary.parameters["intelligence_threshold"] = "0.5";
        contemporary.parameters["pattern_confidence"] = "0.6";
        contemporary.parameters["learning_rate"] = "0.2";
        contemporary.parameters["solution_limit"] = "20";
        presets_.push_back(contemporary);
        
        // Performance optimization
        MusicalPreset performance("High Performance",
                                "Optimized for maximum solving speed", "Performance");
        performance.parameters["max_search_depth"] = "500";
        performance.parameters["backtrack_limit"] = "250";
        performance.parameters["parallel_threads"] = "8";
        performance.parameters["enable_verbose_logging"] = "false";
        presets_.push_back(performance);
        
        // Educational presets
        MusicalPreset educational("Educational Mode",
                                "Settings for learning and teaching music theory", "Educational");
        educational.parameters["enable_verbose_logging"] = "true";
        educational.parameters["enable_advanced_analysis"] = "true";
        educational.parameters["intelligence_threshold"] = "0.9";
        educational.parameters["solution_limit"] = "3";
        presets_.push_back(educational);
    }
    
    void list_presets() const {
        std::cout << "\n🎼 Available Musical Presets:\n";
        std::cout << "=============================\n";
        
        std::map<std::string, std::vector<const MusicalPreset*>> by_category;
        for (const auto& preset : presets_) {
            by_category[preset.category].push_back(&preset);
        }
        
        for (const auto& category : by_category) {
            std::cout << "\n📁 " << category.first << ":\n";
            for (const auto& preset : category.second) {
                std::cout << "  🎵 " << preset->name << "\n";
                std::cout << "     " << preset->description << "\n";
                std::cout << "     Parameters: " << preset->parameters.size() << "\n";
            }
        }
    }
    
    bool apply_preset(const std::string& preset_name, PerformanceConfig& config) {
        for (const auto& preset : presets_) {
            if (preset.name == preset_name) {
                std::cout << "🎼 Applying preset: " << preset_name << "\n";
                std::cout << "   " << preset.description << "\n";
                
                int applied = 0;
                for (const auto& param : preset.parameters) {
                    if (apply_parameter(param.first, param.second, config)) {
                        applied++;
                    }
                }
                
                std::cout << "✅ Applied " << applied << "/" << preset.parameters.size() 
                          << " parameters\n";
                return true;
            }
        }
        
        std::cout << "❌ Preset not found: " << preset_name << "\n";
        return false;
    }
    
    void create_custom_preset(const std::string& name, const std::string& description,
                            const std::string& category, 
                            const std::map<std::string, std::string>& parameters) {
        MusicalPreset custom(name, description, category);
        custom.parameters = parameters;
        presets_.push_back(custom);
        
        std::cout << "✨ Created custom preset: " << name << "\n";
        std::cout << "   Category: " << category << "\n";
        std::cout << "   Parameters: " << parameters.size() << "\n";
    }
    
private:
    bool apply_parameter(const std::string& key, const std::string& value, 
                        PerformanceConfig& config) {
        // Try integer parameter
        try {
            int int_val = std::stoi(value);
            if (config.set_int(key, int_val)) {
                std::cout << "   Set " << key << " = " << int_val << "\n";
                return true;
            }
        } catch (...) {}
        
        // Try double parameter
        try {
            double double_val = std::stod(value);
            if (config.set_double(key, double_val)) {
                std::cout << "   Set " << key << " = " << double_val << "\n";
                return true;
            }
        } catch (...) {}
        
        // Try boolean parameter
        if (value == "true" || value == "false") {
            bool bool_val = (value == "true");
            if (config.set_bool(key, bool_val)) {
                std::cout << "   Set " << key << " = " << (bool_val ? "true" : "false") << "\n";
                return true;
            }
        }
        
        std::cout << "   ❌ Failed to apply " << key << " = " << value << "\n";
        return false;
    }
};

/**
 * @brief Plugin architecture for extensibility
 */
class PluginManager {
private:
    struct Plugin {
        std::string name;
        std::string version;
        std::string category;
        std::string description;
        bool is_loaded;
        bool is_enabled;
        
        Plugin(const std::string& n, const std::string& v, const std::string& cat, 
               const std::string& desc)
            : name(n), version(v), category(cat), description(desc), 
              is_loaded(false), is_enabled(false) {}
    };
    
    std::vector<Plugin> available_plugins_;
    
public:
    PluginManager() {
        initialize_core_plugins();
    }
    
    void initialize_core_plugins() {
        available_plugins_.emplace_back("Advanced Harmony", "1.0", "Musical Analysis",
            "Extended harmonic analysis and chord progression detection");
        available_plugins_.emplace_back("Jazz Improvisation", "1.2", "Performance",
            "Real-time jazz improvisation support with chord changes");
        available_plugins_.emplace_back("Classical Forms", "1.1", "Composition",
            "Classical musical form analysis (sonata, fugue, etc.)");
        available_plugins_.emplace_back("Performance Optimizer", "2.0", "Performance",
            "Advanced performance optimization techniques");
        available_plugins_.emplace_back("MIDI Enhancement", "1.3", "Export",
            "Enhanced MIDI export with expression and articulation");
        available_plugins_.emplace_back("Neural Networks", "0.9", "AI",
            "Neural network integration for pattern recognition");
        available_plugins_.emplace_back("Music Theory Tutor", "1.0", "Educational",
            "Interactive music theory learning and validation");
    }
    
    void list_plugins() const {
        std::cout << "\n🔌 Available Plugins:\n";
        std::cout << "====================\n";
        
        std::map<std::string, std::vector<const Plugin*>> by_category;
        for (const auto& plugin : available_plugins_) {
            by_category[plugin.category].push_back(&plugin);
        }
        
        for (const auto& category : by_category) {
            std::cout << "\n📂 " << category.first << ":\n";
            for (const auto& plugin : category.second) {
                std::string status = plugin->is_enabled ? "🟢 ENABLED" : 
                                   plugin->is_loaded ? "🟡 LOADED" : "⚪ AVAILABLE";
                std::cout << "  " << status << " " << plugin->name << " v" << plugin->version << "\n";
                std::cout << "     " << plugin->description << "\n";
            }
        }
    }
    
    bool load_plugin(const std::string& plugin_name) {
        for (auto& plugin : available_plugins_) {
            if (plugin.name == plugin_name) {
                if (plugin.is_loaded) {
                    std::cout << "ℹ️ Plugin '" << plugin_name << "' is already loaded\n";
                    return true;
                }
                
                std::cout << "🔄 Loading plugin: " << plugin_name << " v" << plugin.version << "\n";
                // Simulate plugin loading
                plugin.is_loaded = true;
                std::cout << "✅ Plugin loaded successfully\n";
                return true;
            }
        }
        
        std::cout << "❌ Plugin not found: " << plugin_name << "\n";
        return false;
    }
    
    bool enable_plugin(const std::string& plugin_name) {
        for (auto& plugin : available_plugins_) {
            if (plugin.name == plugin_name) {
                if (!plugin.is_loaded) {
                    std::cout << "⚠️ Loading plugin first...\n";
                    if (!load_plugin(plugin_name)) return false;
                }
                
                if (plugin.is_enabled) {
                    std::cout << "ℹ️ Plugin '" << plugin_name << "' is already enabled\n";
                    return true;
                }
                
                std::cout << "🎯 Enabling plugin: " << plugin_name << "\n";
                plugin.is_enabled = true;
                std::cout << "✅ Plugin enabled and operational\n";
                return true;
            }
        }
        
        std::cout << "❌ Plugin not found: " << plugin_name << "\n";
        return false;
    }
    
    bool disable_plugin(const std::string& plugin_name) {
        for (auto& plugin : available_plugins_) {
            if (plugin.name == plugin_name) {
                if (!plugin.is_enabled) {
                    std::cout << "ℹ️ Plugin '" << plugin_name << "' is not enabled\n";
                    return true;
                }
                
                std::cout << "⏸️ Disabling plugin: " << plugin_name << "\n";
                plugin.is_enabled = false;
                std::cout << "✅ Plugin disabled\n";
                return true;
            }
        }
        
        std::cout << "❌ Plugin not found: " << plugin_name << "\n";
        return false;
    }
    
    std::vector<std::string> get_enabled_plugins() const {
        std::vector<std::string> enabled;
        for (const auto& plugin : available_plugins_) {
            if (plugin.is_enabled) {
                enabled.push_back(plugin.name);
            }
        }
        return enabled;
    }
    
    void print_plugin_statistics() const {
        int total = available_plugins_.size();
        int loaded = 0, enabled = 0;
        
        for (const auto& plugin : available_plugins_) {
            if (plugin.is_loaded) loaded++;
            if (plugin.is_enabled) enabled++;
        }
        
        std::cout << "\n🔌 Plugin Statistics:\n";
        std::cout << "  Total available: " << total << "\n";
        std::cout << "  Loaded: " << loaded << "\n";
        std::cout << "  Enabled: " << enabled << "\n";
        std::cout << "  Utilization: " << std::fixed << std::setprecision(1) 
                  << (100.0 * enabled / std::max(1, total)) << "%\n";
    }
};

/**
 * @brief Master configuration manager
 */
class MasterConfigurationManager {
private:
    PerformanceConfig performance_config_;
    MusicalPresetManager preset_manager_;
    PluginManager plugin_manager_;
    
public:
    MasterConfigurationManager() {
        std::cout << "🎛️ Master Configuration System initialized\n";
        std::cout << "   - Performance tuning\n";
        std::cout << "   - Musical presets\n";
        std::cout << "   - Plugin management\n";
    }
    
    // Access to subsystems
    PerformanceConfig& performance() { return performance_config_; }
    MusicalPresetManager& presets() { return preset_manager_; }
    PluginManager& plugins() { return plugin_manager_; }
    
    const PerformanceConfig& performance() const { return performance_config_; }
    const MusicalPresetManager& presets() const { return preset_manager_; }
    const PluginManager& plugins() const { return plugin_manager_; }
    
    /**
     * @brief Quick setup for common use cases
     */
    void quick_setup(const std::string& use_case) {
        std::cout << "\n🚀 Quick Setup: " << use_case << "\n";
        
        if (use_case == "classical") {
            preset_manager_.apply_preset("Classical Composition", performance_config_);
            plugin_manager_.enable_plugin("Advanced Harmony");
            plugin_manager_.enable_plugin("Classical Forms");
        }
        else if (use_case == "jazz") {
            preset_manager_.apply_preset("Jazz Improvisation", performance_config_);
            plugin_manager_.enable_plugin("Jazz Improvisation");
            plugin_manager_.enable_plugin("Advanced Harmony");
        }
        else if (use_case == "performance") {
            preset_manager_.apply_preset("High Performance", performance_config_);
            plugin_manager_.enable_plugin("Performance Optimizer");
        }
        else if (use_case == "educational") {
            preset_manager_.apply_preset("Educational Mode", performance_config_);
            plugin_manager_.enable_plugin("Music Theory Tutor");
            plugin_manager_.enable_plugin("Advanced Harmony");
        }
        else {
            std::cout << "❌ Unknown use case. Available: classical, jazz, performance, educational\n";
            return;
        }
        
        std::cout << "✅ Quick setup complete for " << use_case << "\n";
    }
    
    /**
     * @brief Print complete system status
     */
    void print_system_status(bool show_advanced = false) const {
        std::cout << "\n🎛️ Master Configuration System Status\n";
        std::cout << "====================================\n";
        
        performance_config_.print_configuration(show_advanced);
        plugin_manager_.print_plugin_statistics();
        
        auto enabled_plugins = plugin_manager_.get_enabled_plugins();
        if (!enabled_plugins.empty()) {
            std::cout << "\n✅ Active Plugins:\n";
            for (const auto& plugin : enabled_plugins) {
                std::cout << "  • " << plugin << "\n";
            }
        }
    }
};

} // namespace MusicalConstraints

#endif // ADVANCED_CONFIGURATION_HH