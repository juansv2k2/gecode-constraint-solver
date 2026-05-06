/**
 * @file max_msp_external.cpp
 * @brief Native Max external object that wraps AsyncSolverWrapper.
 */

#include "max_msp_solver_wrapper.hh"

#if __has_include("ext.h")

extern "C" {
#include "ext.h"
#include "ext_obex.h"
}

#include <sstream>
#include <fstream>
#include <string>
#include <vector>

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
            atom_setlong(&atoms[i + 1], static_cast<t_atom_long>(voice[i]));
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
    if (!x || !x->wrapper) return;

    const std::string json_text = atoms_to_text(argc, argv);
    if (json_text.empty()) {
        emit_status(x, "failed", "Empty config payload");
        return;
    }

    std::string error;
    if (!x->wrapper->configure_from_json(json_text, error)) {
        object_error(reinterpret_cast<t_object*>(x), "configure_from_json failed: %s", error.c_str());
        emit_status(x, "failed", error.c_str());
        return;
    }

    emit_status(x, "idle", "Configured successfully");
}

void maxsolver_config_file(t_maxsolver* x, t_symbol* s) {
    if (!x || !x->wrapper) return;

    const char* path_cstr = (s && s->s_name) ? s->s_name : "";
    if (!path_cstr[0]) {
        emit_status(x, "failed", "config_file requires a file path");
        return;
    }

    std::ifstream in(path_cstr, std::ios::in | std::ios::binary);
    if (!in) {
        object_error(reinterpret_cast<t_object*>(x), "config_file failed to open: %s", path_cstr);
        emit_status(x, "failed", "Could not open config file");
        return;
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string json_text = buffer.str();
    if (json_text.empty()) {
        emit_status(x, "failed", "Config file is empty");
        return;
    }

    std::string error;
    if (!x->wrapper->configure_from_json(json_text, error)) {
        object_error(reinterpret_cast<t_object*>(x), "configure_from_json failed: %s", error.c_str());
        emit_status(x, "failed", error.c_str());
        return;
    }

    emit_status(x, "idle", "Configured successfully from file");
}

void maxsolver_solve(t_maxsolver* x) {
    if (!x || !x->wrapper) return;

    std::string error;
    if (!x->wrapper->solve_async(error)) {
        object_error(reinterpret_cast<t_object*>(x), "solve_async failed: %s", error.c_str());
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
                      "Messages: config <json>, config_file <absolute_path>, solve, cancel, status, get_last");
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
