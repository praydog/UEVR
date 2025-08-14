/*
This license governs this file (FS.cpp), and is separate from the license for the rest of the UEVR codebase.

The MIT License

Copyright (c) 2023-2025 praydog

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <filesystem>
#include <regex>
#include <fstream>
#include <filesystem>

#include "../LuaLoader.hpp"

#include "FS.hpp"

namespace fs = std::filesystem;

std::optional<::fs::path> get_correct_subpath(sol::this_state l, const std::string& filepath, bool allow_dll = false);

namespace api::fs {
namespace detail {
::fs::path get_datadir(std::string wanted_subdir = "") {
    static const std::string modpath = []() {
        std::string result{};
        result.resize(1024, 0);
        result.resize(GetModuleFileName(nullptr, result.data(), result.size()));

        return result;
    }();

    if (!wanted_subdir.empty() && wanted_subdir.find("$") != std::string::npos) {
        if (wanted_subdir.find("$scripts") != std::string::npos) {
            auto datadir = Framework::get_persistent_dir() / "scripts";

            ::fs::create_directories(datadir);

            return datadir;
        }
        
        // todo, other subdirs?
    }

    auto datadir = Framework::get_persistent_dir() / "data";

    ::fs::create_directories(datadir);

    return datadir;
}

::fs::path fix_subdir(::fs::path subdir) {
    ::fs::path out{subdir};

    return out;
}
}

sol::table glob(sol::this_state l, const char* filter, const char* modifier) {
    sol::state_view state{l};
    std::regex filter_regex{filter};
    auto results = state.create_table();
    auto datadir = detail::get_datadir(modifier != nullptr ? modifier : "");
    auto i = 0;

    for (const auto& entry : ::fs::recursive_directory_iterator{datadir}) {
        if (!entry.is_regular_file() && !entry.is_symlink()) {
            continue;
        }

        auto relpath = relative(entry.path(), datadir).string();

        if (std::regex_match(relpath, filter_regex)) {
            results[++i] = relpath;
        }
    }

    return results;
}

void write(sol::this_state l, const std::string& filepath, const std::string& data) {
    const auto path = get_correct_subpath(l, filepath);

    if (!path) {
        lua_pushstring(l, "fs.write: unknown error");
        lua_error(l);
    }

    ::fs::create_directories(path->parent_path());

    std::ofstream file{*path};

    file << data;
}

std::string read(sol::this_state l, const std::string& filepath) {
    const auto path = get_correct_subpath(l, filepath);

    if (!path) {
        lua_pushstring(l, "fs.read: unknown error");
        lua_error(l);
    }

    if (!exists(*path)) {
        return "";
    }

    ::fs::create_directories(path->parent_path());

    std::ifstream file{*path};
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
}

std::optional<::fs::path> get_correct_subpath(sol::this_state l, const std::string& filepath, bool allow_dll) {
    if (filepath.find("..") != std::string::npos) {
        lua_pushstring(l, "This API does not allow access to parent directories");
        lua_error(l);
        return {};
    }

    if (std::filesystem::path(filepath).is_absolute()) {
        lua_pushstring(l, "This API does not allow the use of absolute paths");
        lua_error(l);
        return {};
    }

    const auto corrected_subpath = api::fs::detail::fix_subdir(::fs::path{filepath});

    if (corrected_subpath.string().find("..") != std::string::npos) {
        lua_pushstring(l, "This API does not allow access to parent directories");
        lua_error(l);
        return {};
    }

    if (corrected_subpath.is_absolute()) {
        lua_pushstring(l, "This API does not allow the use of absolute paths");
        lua_error(l);
        return {};
    }

    const auto path = api::fs::detail::get_datadir(filepath) / (corrected_subpath.has_filename() ? corrected_subpath.filename() : "");
    const auto extension_lower = [&]() {
        auto ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext;
    }();

    if (!allow_dll) {
        // it's not a good idea to allow DLLs to be written to disk from Lua scripts.
        if (extension_lower == ".dll" || extension_lower == ".exe") {
            lua_pushstring(l, "This API does not allow interacting with executables or DLLs");
            lua_error(l);
            return {};
        }
    }

    return path;
}

void bindings::open_fs(sol::state_view& lua) {
    auto fs = lua.create_table();

    fs["glob"] = api::fs::glob;
    fs["write"] = api::fs::write;
    fs["read"] = api::fs::read;
    lua["fs"] = fs;

    lua.open_libraries(sol::lib::io);

    // Replace the io functions with safe versions that can't be used to access parent directories and can't be used to open files outside of the data directory.
    // Addendum: Now io APIs can access the "natives" directory relative to the executable.
    auto io = lua["io"];

    sol::function old_open = io["open"];

    io["open"] = [=](sol::this_state l, const std::string& filepath, sol::object mode) -> sol::object {
        auto path = get_correct_subpath(l, filepath);

        if (!path) {
            lua_pushstring(l, "io.open: unknown error");
            lua_error(l);
            return sol::make_object(l, sol::nil);
        }

        ::fs::create_directories(path->parent_path());

        return old_open(path->string().c_str(), mode);
    };

    /*sol::function old_popen = io["popen"];

    io["popen"] = [=](sol::this_state l, const std::string& filepath, sol::object mode) -> sol::object {
        if (filepath.find("..") != std::string::npos) {
            lua_pushstring(l, "io.popen does not allow access to parent directories");
            lua_error(l);
            return sol::make_object(l, sol::nil);
        }

        auto path = api::fs::detail::get_datadir() / filepath;
        auto warning = (std::stringstream{} << "A script wants to run an executable named \"reframework/data/" << filepath << "\". Allow this?").str();

        auto ret = MessageBoxA(nullptr, warning.c_str(), "REFramework", MB_YESNO | MB_ICONWARNING);

        if (ret == IDYES) {
            return old_popen(path.string().c_str(), mode);
        }

        return sol::make_object(l, sol::nil);
    };*/

    io["popen"] = sol::make_object(lua, sol::nil); // on second thought, I don't want to allow this. If someone really wants this functionality, they can just make a C++ plugin and use the C++ API.

    // These functions can take nil as the first argument and they will return the default filehandle associated with stdin, stdout, or stderr.
    // So they should be safe in that respect.
    sol::function old_lines = io["lines"];

    io["lines"] = [=](sol::this_state l, sol::object filepath_or_nil) -> sol::object {
        if (filepath_or_nil.is<sol::nil_t>()) {
            return old_lines(l, sol::make_object(l, sol::nil));
        }

        if (!filepath_or_nil.is<std::string>()) {
            lua_pushstring(l, "io.lines: expected a string or nil as the first argument");
            lua_error(l);
            return sol::make_object(l, sol::nil);
        }

        auto filepath = filepath_or_nil.as<std::string>();
        auto path = get_correct_subpath(l, filepath);

        if (!path) {
            lua_pushstring(l, "io.lines: unknown error");
            lua_error(l);
            return sol::make_object(l, sol::nil);
        }

        ::fs::create_directories(path->parent_path());

        return old_lines(path->string().c_str());
    };

    sol::function old_input = io["input"];

    io["input"] = [=](sol::this_state l, sol::object filepath_or_nil) -> sol::object {
        if (filepath_or_nil.is<sol::nil_t>()) {
             return old_input(l, sol::make_object(l, sol::nil));
        }

        if (!filepath_or_nil.is<std::string>()) {
            lua_pushstring(l, "io.input: expected a string or nil as the first argument");
            lua_error(l);
            return sol::make_object(l, sol::nil);
        }

        auto filepath = filepath_or_nil.as<std::string>();
        auto path = get_correct_subpath(l, filepath);

        if (!path) {
            lua_pushstring(l, "io.input: unknown error");
            lua_error(l);
            return sol::make_object(l, sol::nil);
        }

        ::fs::create_directories(path->parent_path());

        return old_input(path->string().c_str());
    };

    sol::function old_output = io["output"];

    io["output"] = [=](sol::this_state l, sol::object filepath_or_nil) -> sol::object {
        if (filepath_or_nil.is<sol::nil_t>()) {
             return old_output(l, sol::make_object(l, sol::nil));
        }

        if (!filepath_or_nil.is<std::string>()) {
            lua_pushstring(l, "io.output: expected a string or nil as the first argument");
            lua_error(l);
            return sol::make_object(l, sol::nil);
        }

        auto filepath = filepath_or_nil.as<std::string>();
        auto path = get_correct_subpath(l, filepath);

        if (!path) {
            lua_pushstring(l, "io.output: unknown error");
            lua_error(l);
            return sol::make_object(l, sol::nil);
        }

        ::fs::create_directories(path->parent_path());

        return old_output(path->string().c_str());
    };

    sol::function old_require = lua["require"];

    lua["require"] = [=](sol::this_state l, const std::string& filepath) -> sol::object {
        if (filepath.find("..") != std::string::npos) {
            lua_pushstring(l, "require does not allow access to parent directories");
            lua_error(l);
        }

        if (std::filesystem::path(filepath).is_absolute()) {
            lua_pushstring(l, "require does not allow the use of absolute paths");
            lua_error(l);
        }

        // Do not allow modification of the package path or cpath. We restore it to a pristine value before calling the original require.
        auto lua = sol::state_view{l};
        lua["package"]["path"] = lua.registry()["package_path"];
        lua["package"]["cpath"] = lua.registry()["package_cpath"];
        lua["package"]["searchers"] = lua.create_table();

        sol::table searchers = lua.registry()["package_searchers"];
        for (auto&& [k, v] : searchers) {
            lua["package"]["searchers"][k] = v;
        }

        return old_require(filepath.c_str());
    };

    sol::function old_loadlib = lua["package"]["loadlib"];

    lua["package"]["loadlib"] = [=](sol::this_state l, const std::string& filepath, const std::string& funcname) -> sol::object {
        const auto path = get_correct_subpath(l, filepath, true);

        if (!path) {
            lua_pushstring(l, "package.loadlib: unknown error");
            lua_error(l);
        }

        const auto extension_lower = [&]() {
            auto ext = path->extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            return ext;
        }();

        if (extension_lower != ".dll") {
            lua_pushstring(l, "package.loadlib: only DLLs are allowed");
            lua_error(l);
        }

        return old_loadlib(filepath.c_str(), funcname.c_str());
    };
}