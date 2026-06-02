/**
 * @file max_msp_external.cpp
 * @brief Native Max external object that wraps AsyncSolverWrapper.
 */

#include "max_msp_solver_wrapper.hh"

#if __has_include("ext.h")

extern "C" {
#include "ext.h"
#include "ext_obex.h"
#include "ext_dictobj.h"
#include "ext_path.h"
#include "jpatcher_api.h"
}

#include <sstream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <set>
#include <nlohmann/json.hpp>

namespace {

t_class* s_maxsolver_class = nullptr;
t_symbol* s_status_sym = nullptr;
t_symbol* s_json_sym = nullptr;
t_symbol* s_export_sym = nullptr;
t_symbol* s_voice_pitch_sym = nullptr;
t_symbol* s_voice_rhythm_sym = nullptr;

typedef struct _maxsolver {
    t_object ob;

    MaxMSPWrapper::AsyncSolverWrapper* wrapper;

    // Outlet order in object box (left to right): list, json, export, status
    void* list_outlet;
    void* json_outlet;
    void* export_outlet;
    void* status_outlet;

    void* poll_clock;
} t_maxsolver;

std::string atoms_to_text(long argc, t_atom* argv) {
    std::ostringstream oss;
    for (long i = 0; i < argc; ++i) {
        const auto type = atom_gettype(argv + i);
        if (i > 0) oss << " ";

        if (type == A_SYM) {
            t_symbol* s = atom_getsym(argv + i);
            oss << (s ? s->s_name : "");
        } else if (type == A_LONG) {
            oss << atom_getlong(argv + i);
        } else if (type == A_FLOAT) {
            oss << atom_getfloat(argv + i);
        } else {
            // Best effort for non-primitive atom types.
            oss << "";
        }
    }
    return oss.str();
}

int gcd_int(int a, int b) {
    a = a < 0 ? -a : a;
    b = b < 0 ? -b : b;
    while (b != 0) {
        const int t = a % b;
        a = b;
        b = t;
    }
    return a == 0 ? 1 : a;
}

std::string rhythm_tick_to_fraction(int tick_value, int rhythm_base) {
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

void emit_status(t_maxsolver* x, const char* status, const char* message = nullptr) {
    if (!x || !x->status_outlet) return;

    t_atom out[2];
    atom_setsym(out + 0, gensym(status ? status : "unknown"));

    const bool has_message = (message && message[0]);
    if (has_message) {
        atom_setsym(out + 1, gensym(message));
        outlet_anything(x->status_outlet, s_status_sym, 2, out);
    } else {
        outlet_anything(x->status_outlet, s_status_sym, 1, out);
    }
}

void emit_export_info(t_maxsolver* x, const std::string& result_json_str) {
    if (!x || !x->export_outlet || result_json_str.empty()) return;

    try {
        const nlohmann::json result = nlohmann::json::parse(result_json_str);
        if (!result.contains("exported_files") || !result["exported_files"].is_array()) {
            return;
        }

        std::vector<t_atom> atoms;
        atoms.reserve(result["exported_files"].size());
        for (const auto& file_path : result["exported_files"]) {
            if (!file_path.is_string()) continue;
            t_atom a;
            const std::string path = file_path.get<std::string>();
            atom_setsym(&a, gensym(path.c_str()));
            atoms.push_back(a);
        }

        if (!atoms.empty()) {
            outlet_anything(x->export_outlet, s_export_sym, static_cast<short>(atoms.size()), atoms.data());
        }
    } catch (const std::exception&) {
    }
}

void emit_result_lists(t_maxsolver* x, const MaxMSPWrapper::SolveResult& result) {
    // For backward compatibility: emit first solution using old format
    for (size_t v = 0; v < result.voice_solutions.size(); ++v) {
        const auto& voice = result.voice_solutions[v];
        std::vector<t_atom> atoms(voice.size() + 1);
        atom_setlong(&atoms[0], static_cast<t_atom_long>(v));
        for (size_t i = 0; i < voice.size(); ++i) {
            atom_setlong(&atoms[i + 1], static_cast<t_atom_long>(voice[i]));
        }
        outlet_anything(x->list_outlet, s_voice_pitch_sym, static_cast<short>(atoms.size()), atoms.data());
    }

    for (size_t v = 0; v < result.voice_rhythms.size(); ++v) {
        const auto& voice = result.voice_rhythms[v];
        std::vector<t_atom> atoms(voice.size() + 1);
        atom_setlong(&atoms[0], static_cast<t_atom_long>(v));
        for (size_t i = 0; i < voice.size(); ++i) {
            const std::string fraction = rhythm_tick_to_fraction(voice[i], result.rhythm_base);
            atom_setsym(&atoms[i + 1], gensym(fraction.c_str()));
        }
        outlet_anything(x->list_outlet, s_voice_rhythm_sym, static_cast<short>(atoms.size()), atoms.data());
    }
}

void emit_all_solutions(t_maxsolver* x, const std::string& result_json_str) {
    try {
        nlohmann::json result = nlohmann::json::parse(result_json_str);
        if (!result.contains("solutions") || !result["solutions"].is_array()) return;

        const auto& solutions = result["solutions"];
        t_symbol* open  = gensym("(");
        t_symbol* close = gensym(")");

        std::vector<t_atom> all;

        // outer open paren — wraps all solutions
        atom_setsym(&all.emplace_back(), open);

        for (const auto& solution : solutions) {
            atom_setsym(&all.emplace_back(), open);   // solution open

            // metric sublist
            atom_setsym(&all.emplace_back(), open);
            if (solution.contains("metric_signature") && solution["metric_signature"].is_array()) {
                for (const auto& ts : solution["metric_signature"])
                    if (ts.is_string())
                        atom_setsym(&all.emplace_back(), gensym(ts.get<std::string>().c_str()));
            }
            atom_setsym(&all.emplace_back(), close);

            // pitch sublists (one per voice)
            if (solution.contains("voice_solutions") && solution["voice_solutions"].is_array()) {
                for (const auto& pitches : solution["voice_solutions"]) {
                    atom_setsym(&all.emplace_back(), open);
                    if (pitches.is_array())
                        for (const auto& p : pitches)
                            if (p.is_number())
                                atom_setlong(&all.emplace_back(), static_cast<t_atom_long>(p.get<int>()));
                    atom_setsym(&all.emplace_back(), close);
                }
            }

            // rhythm sublists (one per voice)
            if (solution.contains("voice_rhythms") && solution["voice_rhythms"].is_array()) {
                for (const auto& rhythms : solution["voice_rhythms"]) {
                    atom_setsym(&all.emplace_back(), open);
                    if (rhythms.is_array())
                        for (const auto& r : rhythms)
                            if (r.is_string())
                                atom_setsym(&all.emplace_back(), gensym(r.get<std::string>().c_str()));
                    atom_setsym(&all.emplace_back(), close);
                }
            }

            atom_setsym(&all.emplace_back(), close);  // solution close
        }

        // outer close paren
        atom_setsym(&all.emplace_back(), close);

        outlet_list(x->list_outlet, nullptr, static_cast<short>(all.size()), all.data());

    } catch (const std::exception&) {}
}

void emit_result_json(t_maxsolver* x, const std::string& json_payload) {
    if (json_payload.empty()) return;
    t_atom a;
    atom_setsym(&a, gensym(json_payload.c_str()));
    outlet_anything(x->json_outlet, s_json_sym, 1, &a);
}

// Returns the filesystem folder that contains the currently open .maxpat file,
// or an empty string if the patch has not been saved yet.
// Falls back to Max's default path when the patch file path is unavailable.
static std::string get_patch_folder(t_maxsolver* x) {
    if (!x) return std::string();

    // Try to get the parent patcher and its saved filepath.
    t_object* patcher = nullptr;
    object_obex_lookup((t_object*)x, gensym("#P"), (t_object**)&patcher);
    if (patcher) {
        t_symbol* filepath = jpatcher_get_filepath(patcher);
        if (filepath && filepath->s_name && filepath->s_name[0]) {
            // jpatcher_get_filepath can return an HFS-style path on macOS
            // ("Macintosh HD:/Users/..." or "Macintosh HD:Users:dir:...").
            // Run it through Max's own path API to get a guaranteed POSIX path.
            short path_id = 0;
            char filename[MAX_PATH_CHARS] = {};
            char abspath[MAX_PATH_CHARS] = {};
            if (path_frompathname(filepath->s_name, &path_id, filename) == MAX_ERR_NONE &&
                path_toabsolutesystempath(path_id, filename, abspath) == MAX_ERR_NONE &&
                abspath[0]) {
                return std::filesystem::path(abspath).parent_path().string();
            }

            // Fallback: manually strip "Volume:" prefix so std::filesystem can
            // handle the rest (covers "Macintosh HD:/posix/path" and pure HFS
            // "Volume:dir1:dir2:file" variants).
            std::string raw(filepath->s_name);
            if (!raw.empty() && raw[0] != '/') {
                const auto colon = raw.find(':');
                if (colon != std::string::npos) {
                    std::string rest = raw.substr(colon + 1);
                    if (!rest.empty() && rest[0] != '/') {
                        // Pure HFS separators — convert colons to slashes
                        for (auto& c : rest) if (c == ':') c = '/';
                        raw = "/" + rest;
                    } else {
                        raw = rest;   // "Volume:/posix/path" → "/posix/path"
                    }
                }
            }
            return std::filesystem::path(raw).parent_path().string();
        }
    }

    // Patch is unsaved — use Max's current default path.
    char path_str[MAX_PATH_CHARS] = {};
    const short path_id = path_getdefault();
    if (path_id > 0) {
        if (path_toabsolutesystempath(path_id, "", path_str) == MAX_ERR_NONE && path_str[0])
            return std::string(path_str);
    }
    return std::string();  // wrapper will fall back to cwd
}

bool configure_from_json_payload(t_maxsolver* x, const std::string& json_text, const char* success_message) {
    if (!x || !x->wrapper) return false;

    if (json_text.empty()) {
        emit_status(x, "failed", "Empty config payload");
        return false;
    }

    // Inject the patch folder before configuring so path resolution works correctly.
    x->wrapper->set_patch_folder(get_patch_folder(x));

    std::string error;
    if (!x->wrapper->configure_from_json(json_text, error)) {
        emit_status(x, "failed", error.c_str());
        return false;
    }

    emit_status(x, "idle", success_message ? success_message : "Configured successfully");
    return true;
}

nlohmann::json max_dictionary_to_json(t_dictionary* d);

void normalize_array_schema_fields(nlohmann::json& value) {
    static const std::set<std::string> array_keys = {
        "dynamic_rules",
        "rules",
        "voices",
        "duration_values",
        "midi_values",
        "time_signatures",
        "tuplets",
        "beat_divisions",
        "indices",
        "timepoints",
        "parameters",
        "bar_pattern",
        "voice_solutions",
        "voice_rhythms"
    };

    // Keys that Max may serialize from JSON arrays as objects with numeric string keys.
    // These must be converted back from {"0":{...},"1":{...}} to [{...},{...}].
    static const std::set<std::string> numeric_keyed_object_to_array_keys = {
        "voices",
        "rules",
        "dynamic_rules"
    };

    if (value.is_object()) {
        for (auto it = value.begin(); it != value.end(); ++it) {
            const std::string key = it.key();

            // If Max serialized a JSON array as an object with numeric string keys, restore it.
            if (numeric_keyed_object_to_array_keys.count(key) > 0 && it.value().is_object()) {
                bool all_numeric = true;
                for (auto eit = it.value().begin(); eit != it.value().end(); ++eit) {
                    try { std::stoi(eit.key()); }
                    catch (...) { all_numeric = false; break; }
                }
                if (all_numeric && !it.value().empty()) {
                    // Sort by numeric key and build array
                    std::vector<std::pair<int, nlohmann::json>> entries;
                    for (auto eit = it.value().begin(); eit != it.value().end(); ++eit) {
                        entries.emplace_back(std::stoi(eit.key()), eit.value());
                    }
                    std::sort(entries.begin(), entries.end(),
                              [](const auto& a, const auto& b) { return a.first < b.first; });
                    nlohmann::json arr = nlohmann::json::array();
                    for (auto& e : entries) arr.push_back(std::move(e.second));
                    it.value() = std::move(arr);
                }
            }

            if (array_keys.count(key) > 0 && !it.value().is_array() && !it.value().is_null()) {
                nlohmann::json wrapped = nlohmann::json::array();
                wrapped.push_back(it.value());
                it.value() = std::move(wrapped);
            }
            normalize_array_schema_fields(it.value());
        }
    } else if (value.is_array()) {
        for (auto& item : value) {
            normalize_array_schema_fields(item);
        }
    }
}

nlohmann::json atom_to_json_value(const t_atom& a) {
    const auto type = atom_gettype(const_cast<t_atom*>(&a));
    if (type == A_LONG) {
        return nlohmann::json(static_cast<long long>(atom_getlong(const_cast<t_atom*>(&a))));
    }
    if (type == A_FLOAT) {
        return nlohmann::json(static_cast<double>(atom_getfloat(const_cast<t_atom*>(&a))));
    }
    if (type == A_SYM) {
        t_symbol* s = atom_getsym(const_cast<t_atom*>(&a));
        if (s) {
            // Max stores dictionaries inside arrays as symbol references to dict names.
            // Resolve these references so arrays like rules remain arrays of JSON objects.
            t_dictionary* nested = dictobj_findregistered_retain(s);
            if (!nested) {
                const std::string sym_name = s->s_name ? std::string(s->s_name) : std::string();
                const std::string prefix = "dictionary ";
                if (sym_name.rfind(prefix, 0) == 0 && sym_name.size() > prefix.size()) {
                    t_symbol* nested_name = gensym(sym_name.substr(prefix.size()).c_str());
                    nested = dictobj_findregistered_retain(nested_name);
                }
            }
            if (nested) {
                nlohmann::json nested_json = max_dictionary_to_json(nested);
                dictobj_release(nested);
                return nested_json;
            }
        }
        return nlohmann::json(s ? std::string(s->s_name) : std::string(""));
    }
    if (type == A_OBJ) {
        void* obj = atom_getobj(const_cast<t_atom*>(&a));
        if (obj) {
            t_symbol* class_name = object_classname(obj);
            if (class_name && class_name == gensym("dictionary")) {
                return max_dictionary_to_json(reinterpret_cast<t_dictionary*>(obj));
            }
        }
    }
    return nullptr;
}

nlohmann::json atoms_to_json_value(long argc, t_atom* argv) {
    if (argc <= 0 || !argv) {
        return nlohmann::json::array();
    }
    if (argc == 1) {
        return atom_to_json_value(argv[0]);
    }

    nlohmann::json arr = nlohmann::json::array();
    for (long i = 0; i < argc; ++i) {
        arr.push_back(atom_to_json_value(argv[i]));
    }
    return arr;
}

nlohmann::json max_dictionary_to_json(t_dictionary* d) {
    nlohmann::json obj = nlohmann::json::object();
    if (!d) return obj;

    // Keys that should be arrays in the JSON schema
    static const std::set<std::string> array_keys = {
        "dynamic_rules", "rules"
    };

    t_symbol** keys = nullptr;
    long numkeys = 0;
    if (dictionary_getkeys(d, &numkeys, &keys) != MAX_ERR_NONE || !keys) {
        return obj;
    }

    for (long i = 0; i < numkeys; ++i) {
        t_symbol* key = keys[i];
        if (!key || !key->s_name) continue;
        const std::string key_name = key->s_name;

        // Debug: print key type for dynamic_rules
        if (key_name == "dynamic_rules") {
            post("[gecode.solver] DEBUG: processing key '%s'", key_name.c_str());
            if (dictionary_entryisdictionary(d, key)) {
                post("[gecode.solver]   -> is dictionary");
            } else if (dictionary_entryisatomarray(d, key)) {
                post("[gecode.solver]   -> is atom array");
            } else if (dictionary_entryisstring(d, key)) {
                post("[gecode.solver]   -> is string");
            } else {
                post("[gecode.solver]   -> is other (atom?)");
            }
        }

        if (dictionary_entryisdictionary(d, key)) {
            t_object* child_obj = nullptr;
            if (dictionary_getdictionary(d, key, &child_obj) == MAX_ERR_NONE && child_obj) {
                nlohmann::json nested_json = max_dictionary_to_json(reinterpret_cast<t_dictionary*>(child_obj));
                
                // If this key should be an array and we got an object, wrap it in an array
                if (array_keys.count(key_name) > 0 && nested_json.is_object()) {
                    if (key_name == "dynamic_rules") {
                        post("[gecode.solver]   -> wrapping single object into array");
                    }
                    nlohmann::json arr = nlohmann::json::array();
                    arr.push_back(nested_json);
                    obj[key_name] = arr;
                } else {
                    obj[key_name] = nested_json;
                }
            }
            continue;
        }

        if (dictionary_entryisatomarray(d, key)) {
            long argc = 0;
            t_atom* argv = nullptr;
            if (dictionary_getatoms(d, key, &argc, &argv) == MAX_ERR_NONE) {
                if (key_name == "dynamic_rules") {
                    post("[gecode.solver]   -> atom array path with argc=%ld", argc);
                }
                nlohmann::json arr_json = atoms_to_json_value(argc, argv);
                
                // Ensure array keys are always arrays, even with single element
                if (array_keys.count(key_name) > 0) {
                    if (!arr_json.is_array()) {
                        if (key_name == "dynamic_rules") {
                            post("[gecode.solver]   -> wrapping non-array result into array");
                        }
                        nlohmann::json wrapped = nlohmann::json::array();
                        wrapped.push_back(arr_json);
                        obj[key_name] = wrapped;
                    } else {
                        obj[key_name] = arr_json;
                    }
                } else {
                    obj[key_name] = arr_json;
                }
            }
            continue;
        }

        if (dictionary_entryisstring(d, key)) {
            const char* s = nullptr;
            if (dictionary_getstring(d, key, &s) == MAX_ERR_NONE && s) {
                obj[key_name] = std::string(s);
            } else {
                obj[key_name] = "";
            }
            continue;
        }

        t_atom a;
        if (dictionary_getatom(d, key, &a) == MAX_ERR_NONE) {
            nlohmann::json atom_json = atom_to_json_value(a);
            
            // If this key should be an array and we got an object, wrap it in an array
            if (array_keys.count(key_name) > 0 && atom_json.is_object()) {
                if (key_name == "dynamic_rules") {
                    post("[gecode.solver]   -> atom path: wrapping single object into array");
                }
                nlohmann::json arr = nlohmann::json::array();
                arr.push_back(atom_json);
                obj[key_name] = arr;
            } else {
                obj[key_name] = atom_json;
            }
            continue;
        }

        long argc = 0;
        t_atom* argv = nullptr;
        if (dictionary_getatoms(d, key, &argc, &argv) == MAX_ERR_NONE) {
            obj[key_name] = atoms_to_json_value(argc, argv);
        }
    }

    dictionary_freekeys(d, numkeys, keys);
    return obj;
}

void maxsolver_poll(t_maxsolver* x) {
    if (!x || !x->wrapper) return;

    MaxMSPWrapper::SolveResult result;
    if (x->wrapper->take_result(result)) {
        // Emit all solutions from the JSON
        emit_all_solutions(x, result.result_json);
        
        // Also emit JSON for inspection
        emit_result_json(x, result.result_json);
        emit_export_info(x, result.result_json);

        const std::string st = x->wrapper->get_status_string();
        emit_status(x, st.c_str(), result.message.c_str());
        return;
    }

    if (x->wrapper->is_running()) {
        clock_delay(static_cast<t_clock*>(x->poll_clock), 20);
        return;
    }

    // The thread may have finished between the take_result check above and the
    // is_running check: with result_ready_ set inside the status mutex, this
    // final take is guaranteed to see the result if the thread just completed.
    if (x->wrapper->take_result(result)) {
        emit_all_solutions(x, result.result_json);
        emit_result_json(x, result.result_json);
        emit_export_info(x, result.result_json);
        const std::string st = x->wrapper->get_status_string();
        emit_status(x, st.c_str(), result.message.c_str());
    }
}

void maxsolver_config(t_maxsolver* x, t_symbol* s, long argc, t_atom* argv) {
    (void)s;
    const std::string json_text = atoms_to_text(argc, argv);
    (void)configure_from_json_payload(x, json_text, "Configured successfully");
}

void maxsolver_config_file(t_maxsolver* x, t_symbol* s) {
    if (!x) return;

    const char* path_cstr = (s && s->s_name) ? s->s_name : "";
    if (!path_cstr[0]) {
        emit_status(x, "failed", "config_file requires a file path");
        return;
    }

    std::ifstream in(path_cstr, std::ios::in | std::ios::binary);
    if (!in) {
        emit_status(x, "failed", "Could not open config file");
        return;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string json_text = buffer.str();
    (void)configure_from_json_payload(x, json_text, "Configured successfully from file");
}

void maxsolver_config_dict(t_maxsolver* x, t_symbol* dict_name) {
    if (!x) return;

    if (!dict_name || !dict_name->s_name || !dict_name->s_name[0]) {
        emit_status(x, "failed", "config_dict requires a dictionary name");
        return;
    }

    t_dictionary* d = dictobj_findregistered_retain(dict_name);
    if (!d) {
        emit_status(x, "failed", "Dictionary not found");
        return;
    }

    nlohmann::json j = max_dictionary_to_json(d);
    dictobj_release(d);

    normalize_array_schema_fields(j);

    const std::string json_text = j.dump();

    (void)configure_from_json_payload(x, json_text, "Configured successfully from dict");
}

void maxsolver_config_dict_debug(t_maxsolver* x, t_symbol* dict_name) {
    if (!x) return;

    if (!dict_name || !dict_name->s_name || !dict_name->s_name[0]) {
        emit_status(x, "failed", "config_dict_debug requires a dictionary name");
        return;
    }

    t_dictionary* d = dictobj_findregistered_retain(dict_name);
    if (!d) {
        emit_status(x, "failed", "Dictionary not found");
        return;
    }

    nlohmann::json j = max_dictionary_to_json(d);
    dictobj_release(d);

    normalize_array_schema_fields(j);

    const std::string json_text = j.dump();

    t_atom a;
    atom_setsym(&a, gensym(json_text.c_str()));
    outlet_anything(x->json_outlet, s_json_sym, 1, &a);

    emit_status(x, "debug", "Dumped dict JSON to console/json outlet");
}

void maxsolver_solve(t_maxsolver* x) {
    if (!x || !x->wrapper) return;

    std::string error;
    if (!x->wrapper->solve_async(error)) {
        emit_status(x, "failed", error.c_str());
        return;
    }

    emit_status(x, "running", "Solve started");
    clock_delay(static_cast<t_clock*>(x->poll_clock), 5);
}

void maxsolver_cancel(t_maxsolver* x) {
    if (!x || !x->wrapper) return;
    x->wrapper->request_cancel();
    emit_status(x, "cancelled", "Cancellation requested");
}

void maxsolver_status(t_maxsolver* x) {
    if (!x || !x->wrapper) return;

    const std::string status_json = x->wrapper->get_status_json();
    t_atom a;
    atom_setsym(&a, gensym(status_json.c_str()));
    outlet_anything(x->json_outlet, s_json_sym, 1, &a);

    const std::string status_text = x->wrapper->get_status_string();
    emit_status(x, status_text.c_str());
}

void maxsolver_get_last(t_maxsolver* x) {
    if (!x || !x->wrapper) return;
    MaxMSPWrapper::SolveResult result;
    if (!x->wrapper->take_result(result)) {
        emit_status(x, "idle", "No pending result");
        return;
    }

    emit_all_solutions(x, result.result_json);
    emit_result_json(x, result.result_json);
    emit_export_info(x, result.result_json);
    const std::string st = x->wrapper->get_status_string();
    emit_status(x, st.c_str(), result.message.c_str());
}

void* maxsolver_new(t_symbol* s, long argc, t_atom* argv) {
    (void)s;
    (void)argc;
    (void)argv;

    t_maxsolver* x = reinterpret_cast<t_maxsolver*>(object_alloc(s_maxsolver_class));
    if (!x) return nullptr;

    x->wrapper = new MaxMSPWrapper::AsyncSolverWrapper();

    // Create outlets right-to-left.
    x->status_outlet = outlet_new(reinterpret_cast<t_object*>(x), nullptr);
    x->export_outlet = outlet_new(reinterpret_cast<t_object*>(x), nullptr);
    x->json_outlet = outlet_new(reinterpret_cast<t_object*>(x), nullptr);
    x->list_outlet = outlet_new(reinterpret_cast<t_object*>(x), nullptr);

    x->poll_clock = clock_new(x, reinterpret_cast<method>(maxsolver_poll));

    emit_status(x, "unconfigured");
    return x;
}

void maxsolver_free(t_maxsolver* x) {
    if (!x) return;

    if (x->poll_clock) {
        clock_unset(static_cast<t_clock*>(x->poll_clock));
        object_free(x->poll_clock);
        x->poll_clock = nullptr;
    }

    if (x->wrapper) {
        delete x->wrapper;
        x->wrapper = nullptr;
    }
}

void maxsolver_assist(t_maxsolver* x, void* b, long m, long a, char* s) {
    (void)x;
    (void)b;
    if (m == ASSIST_INLET) {
        snprintf_zero(s, 512,
                      "Messages: config <json>, config_file <absolute_path>, config_dict <dict_name>, config_dict_debug <dict_name>, solve, cancel, status, get_last");
    } else {
        if (a == 0) {
            snprintf_zero(s, 512, "Lists: voice_pitch / voice_rhythm");
        } else if (a == 1) {
            snprintf_zero(s, 512, "JSON solve payload");
        } else if (a == 2) {
            snprintf_zero(s, 512, "Export metadata (path/files)");
        } else {
            snprintf_zero(s, 512, "Status only");
        }
    }
}

} // namespace

extern "C" C74_EXPORT void ext_main(void* r) {
    (void)r;
    s_status_sym = gensym("status");
    s_json_sym = gensym("json");
    s_export_sym = gensym("export");
    s_voice_pitch_sym = gensym("voice_pitch");
    s_voice_rhythm_sym = gensym("voice_rhythm");

    t_class* c = class_new("gecode.solver",
                           reinterpret_cast<method>(maxsolver_new),
                           reinterpret_cast<method>(maxsolver_free),
                           static_cast<long>(sizeof(t_maxsolver)),
                           nullptr,
                           A_GIMME,
                           0);

    class_addmethod(c, reinterpret_cast<method>(maxsolver_config), "config", A_GIMME, 0);
    class_addmethod(c, reinterpret_cast<method>(maxsolver_config_file), "config_file", A_SYM, 0);
    class_addmethod(c, reinterpret_cast<method>(maxsolver_config_dict), "config_dict", A_SYM, 0);
    class_addmethod(c, reinterpret_cast<method>(maxsolver_config_dict_debug), "config_dict_debug", A_SYM, 0);
    class_addmethod(c, reinterpret_cast<method>(maxsolver_solve), "solve", 0);
    class_addmethod(c, reinterpret_cast<method>(maxsolver_cancel), "cancel", 0);
    class_addmethod(c, reinterpret_cast<method>(maxsolver_status), "status", 0);
    class_addmethod(c, reinterpret_cast<method>(maxsolver_get_last), "get_last", 0);
    class_addmethod(c, reinterpret_cast<method>(maxsolver_assist), "assist", A_CANT, 0);

    class_register(CLASS_BOX, c);
    s_maxsolver_class = c;

    object_post(nullptr, "gecode.solver loaded");
}

#else

// Stub compilation path when Max SDK headers are unavailable.
int max_msp_external_stub_compiled_without_sdk = 1;

#endif
