#ifndef MUSICXML_SVG_RENDERER_HH
#define MUSICXML_SVG_RENDERER_HH

#include <cstdlib>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <spawn.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace MusicXMLSvgRenderer {

inline bool is_executable_file(const std::filesystem::path& path) {
    return std::filesystem::is_regular_file(path) && ::access(path.c_str(), X_OK) == 0;
}

inline std::string find_executable_on_path(const std::string& name) {
    const char* path_env = std::getenv("PATH");
    if (!path_env) return {};
    std::string paths(path_env);
    size_t start = 0;
    while (start <= paths.size()) {
        const size_t end = paths.find(':', start);
        const std::string dir = paths.substr(start, end == std::string::npos ? end : end - start);
        if (!dir.empty()) {
            const auto candidate = std::filesystem::path(dir) / name;
            if (is_executable_file(candidate)) return candidate.string();
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}

inline std::string find_verovio(const std::string& configured_path) {
    if (!configured_path.empty()) {
        if (configured_path.find('/') != std::string::npos)
            return is_executable_file(configured_path) ? configured_path : std::string();
        const std::string found = find_executable_on_path(configured_path);
        return found.empty() ? configured_path : found;
    }

    if (const char* env_path = std::getenv("VEROVIO_PATH")) {
        if (*env_path) {
            if (std::string(env_path).find('/') != std::string::npos)
                return is_executable_file(env_path) ? std::string(env_path) : std::string();
            const std::string found = find_executable_on_path(env_path);
            return found.empty() ? std::string(env_path) : found;
        }
    }

    const std::string from_path = find_executable_on_path("verovio");
    if (!from_path.empty()) return from_path;

    for (const char* candidate : {"/opt/homebrew/bin/verovio", "/usr/local/bin/verovio", "/usr/bin/verovio"})
        if (is_executable_file(candidate)) return candidate;

    return {};
}

inline std::string find_svg_rasterizer() {
    if (const char* configured = std::getenv("SVG_TO_PNG_PATH")) {
        if (*configured) {
            if (std::string(configured).find('/') != std::string::npos)
                return is_executable_file(configured) ? std::string(configured) : std::string();
            const std::string found = find_executable_on_path(configured);
            return found.empty() ? std::string(configured) : found;
        }
    }
    for (const char* name : {"rsvg-convert", "magick", "convert"}) {
        const std::string found = find_executable_on_path(name);
        if (!found.empty()) return found;
    }
    for (const char* candidate : {"/opt/homebrew/bin/rsvg-convert", "/usr/local/bin/rsvg-convert",
                                  "/usr/bin/rsvg-convert", "/opt/homebrew/bin/magick",
                                  "/usr/local/bin/magick", "/usr/bin/magick",
                                  "/usr/local/bin/convert", "/usr/bin/convert"})
        if (is_executable_file(candidate)) return candidate;
    return {};
}

inline bool run_process(const std::string& executable,
                        const std::vector<std::string>& arguments,
                        std::string& error) {
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 2);
    argv.push_back(const_cast<char*>(executable.c_str()));
    for (const auto& arg : arguments) argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);

    pid_t child = -1;
    const int spawn_status = (executable.find('/') == std::string::npos)
        ? ::posix_spawnp(&child, executable.c_str(), nullptr, nullptr, argv.data(), environ)
        : ::posix_spawn(&child, executable.c_str(), nullptr, nullptr, argv.data(), environ);
    if (spawn_status != 0) {
        error = "Could not start " + executable + " (posix_spawn error " +
                std::to_string(spawn_status) + ")";
        return false;
    }

    int status = 0;
    while (::waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            error = "Failed while waiting for " + executable;
            return false;
        }
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        error = executable + " failed with exit status " +
                std::to_string(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        return false;
    }
    return true;
}

inline bool render_file(const std::string& musicxml_path,
                        const std::string& svg_path,
                        const std::string& configured_verovio_path,
                        std::string& error) {
    const std::string executable = find_verovio(configured_verovio_path);
    if (executable.empty()) {
        error = "Verovio not found; install it or set verovio_path / VEROVIO_PATH";
        return false;
    }
    if (!std::filesystem::is_regular_file(musicxml_path)) {
        error = "MusicXML input does not exist: " + musicxml_path;
        return false;
    }

    // One tall page: "-a" writes name_001.svg, name_002.svg for long scores, which this path can't find.
    if (!run_process(executable,
                     {"-f", "xml", "--breaks", "auto", "--page-height", "60000",
                      "--adjust-page-height", "-o", svg_path, musicxml_path}, error))
        return false;
    if (!std::filesystem::is_regular_file(svg_path) || std::filesystem::file_size(svg_path) == 0) {
        error = "Verovio reported success but did not produce an SVG file";
        return false;
    }
    return true;
}

inline bool render_svg_to_png(const std::string& svg_path,
                              const std::string& png_path,
                              std::string& error) {
    const std::string rasterizer = find_svg_rasterizer();
    if (rasterizer.empty()) {
        error = "SVG rasterizer not found; install librsvg or ImageMagick, or set SVG_TO_PNG_PATH";
        return false;
    }
    const std::string name = std::filesystem::path(rasterizer).filename().string();
    const bool is_rsvg = name == "rsvg-convert";
    const std::vector<std::string> args = is_rsvg
        ? std::vector<std::string>{"-o", png_path, svg_path}
        : std::vector<std::string>{svg_path, png_path};
    if (!run_process(rasterizer, args, error)) return false;
    if (!std::filesystem::is_regular_file(png_path) || std::filesystem::file_size(png_path) == 0) {
        error = "SVG rasterizer reported success but did not produce a PNG file";
        return false;
    }
    return true;
}

inline bool render_png_from_musicxml(const std::string& musicxml,
                                     const std::string& png_path,
                                     const std::string& configured_verovio_path,
                                     std::string& error) {
    const std::string suffix = ".tmp." + std::to_string(static_cast<long long>(::getpid()));
    const std::string xml_path = png_path + suffix + ".xml";
    const std::string svg_path = png_path + suffix + ".svg";
    {
        std::ofstream xml_out(xml_path, std::ios::binary);
        if (!xml_out) {
            error = "Could not create temporary MusicXML file: " + xml_path;
            return false;
        }
        xml_out << musicxml;
        if (!xml_out) {
            error = "Could not write temporary MusicXML file: " + xml_path;
            std::error_code ec; std::filesystem::remove(xml_path, ec);
            return false;
        }
    }

    bool ok = render_file(xml_path, svg_path, configured_verovio_path, error);
    if (ok) ok = render_svg_to_png(svg_path, png_path, error);
    std::error_code ec;
    std::filesystem::remove(xml_path, ec);
    std::filesystem::remove(svg_path, ec);
    return ok;
}

}  // namespace MusicXMLSvgRenderer

#endif  // MUSICXML_SVG_RENDERER_HH
