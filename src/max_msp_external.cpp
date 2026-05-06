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
}

#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace {

t_class* s_maxsolver_class = nullptr;
t_symbol* s_status_sym = nullptr;
t_symbol* s_json_sym = nullptr;
t_symbol* s_voice_pitch_sym = nullptr;
t_symbol* s_voice_rhythm_sym = nullptr;

typedef struct _maxsolver {
    t_object ob;

    MaxMSPWrapper::AsyncSolverWrapper* wrapper;

    // Outlet order in object box (left to right): list, json, status
    void* list_outlet;
    void* json_outlet;
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

void emit_status(t_maxsolver* x, const char* status, const char* message) {
    t_atom out[2];
    atom_setsym(out + 0, gensym(status));
    atom_setsym(out + 1, gensym(message ? message : ""));
    outlet_anything(x->status_outlet, s_status_sym, 2, out);
}

void emit_result_lists(t_maxsolver* x, const MaxMSPWrapper::SolveResult& result) {
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

void emit_result_json(t_maxsolver* x, const std::string& json_payload) {
    if (json_payload.empty()) return;
    t_atom a;
    atom_setsym(&a, gensym(json_payload.c_str()));
    outlet_anything(x->json_outlet, s_json_sym, 1, &a);
}

bool configure_from_json_payload(t_maxsolver* x, const std::string& json_text, const char* success_message) {
    if (!x || !x->wrapper) return false;

    if (json_text.empty()) {
        emit_status(x, "failed", "Empty config payload");
        return false;
    }

    std::string error;
    if (!x->wrapper->configure_from_json(json_text, error)) {
        emit_status(x, "failed", error.c_str());
        return false;
    }

    emit_status(x, "idle", success_message ? success_message : "Configured successfully");
    return true;
}

nlohmann::json max_dictionary_to_json(t_dictionary* d);

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

    t_symbol** keys = nullptr;
    long numkeys = 0;
    if (dictionary_getkeys(d, &numkeys, &keys) != MAX_ERR_NONE || !keys) {
        return obj;
    }

    for (long i = 0; i < numkeys; ++i) {
        t_symbol* key = keys[i];
        if (!key || !key->s_name) continue;
        const std::string key_name = key->s_name;

        if (dictionary_entryisdictionary(d, key)) {
            t_object* child_obj = nullptr;
            if (dictionary_getdictionary(d, key, &child_obj) == MAX_ERR_NONE && child_obj) {
                obj[key_name] = max_dictionary_to_json(reinterpret_cast<t_dictionary*>(child_obj));
            }
            continue;
        }

        if (dictionary_entryisatomarray(d, key)) {
            long argc = 0;
            t_atom* argv = nullptr;
            if (dictionary_getatoms(d, key, &argc, &argv) == MAX_ERR_NONE) {
                obj[key_name] = atoms_to_json_value(argc, argv);
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
            obj[key_name] = atom_to_json_value(a);
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
        emit_result_lists(x, result);
        emit_result_json(x, result.result_json);

        const std::string st = x->wrapper->get_status_string();
        emit_status(x, st.c_str(), result.message.c_str());
        return;
    }

    if (x->wrapper->is_running()) {
        clock_delay(static_cast<t_clock*>(x->poll_clock), 20);
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

    const nlohmann::json j = max_dictionary_to_json(d);
    dictobj_release(d);

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

    const nlohmann::json j = max_dictionary_to_json(d);
    dictobj_release(d);

    const std::string json_text = j.dump();

    t_atom a;
    atom_setsym(&a, gensym(json_text.c_str()));
    outlet_anything(x->json_outlet, s_json_sym, 1, &a);

    emit_status(x, "idle", "Dumped dict JSON to console/json outlet");
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
    emit_status(x, status_text.c_str(), "Status snapshot");
}

void maxsolver_get_last(t_maxsolver* x) {
    if (!x || !x->wrapper) return;
    MaxMSPWrapper::SolveResult result;
    if (!x->wrapper->take_result(result)) {
        emit_status(x, "idle", "No pending result");
        return;
    }

    emit_result_lists(x, result);
    emit_result_json(x, result.result_json);
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
    x->json_outlet = outlet_new(reinterpret_cast<t_object*>(x), nullptr);
    x->list_outlet = outlet_new(reinterpret_cast<t_object*>(x), nullptr);

    x->poll_clock = clock_new(x, reinterpret_cast<method>(maxsolver_poll));

    emit_status(x, "unconfigured", "Send config <json> then solve");
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
            snprintf_zero(s, 512, "JSON/status payload");
        } else {
            snprintf_zero(s, 512, "Status messages");
        }
    }
}

} // namespace

extern "C" C74_EXPORT void ext_main(void* r) {
    (void)r;
    s_status_sym = gensym("status");
    s_json_sym = gensym("json");
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
