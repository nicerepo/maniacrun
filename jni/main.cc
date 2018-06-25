//===------------------------------------------------------------------------------------------===//
//
//                        The MANIAC Dynamic Binary Instrumentation Engine
//
//===------------------------------------------------------------------------------------------===//
//
// Copyright (C) 2018 Libre.io Developers
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
// even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
//===------------------------------------------------------------------------------------------===//
//
// main.cc: die Android-Laufzeitumgebung für MANIAC
//
//===------------------------------------------------------------------------------------------===//

#include <experimental/filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "vendor/nlohmann/json.h"

#if defined(__aarch64__)
#define ARCH "arm64-v8a"
#elif defined(__arm__)
#define ARCH "armeabi-v7a"
#elif defined(__x86_64__)
#define ARCH "x86_64"
#elif defined(__i386__)
#define ARCH "x86"
#endif

#define DATA_ROOT "/data/data/io.libre.maniac"
#define MOD_ROOT DATA_ROOT "/private/mods"
#define UTIL_ROOT DATA_ROOT "/private/utils"
#define INJECTOR UTIL_ROOT "/" ARCH "/maniacj"

namespace nl = nlohmann;
namespace fs = std::experimental::filesystem;

using namespace std;

class Controller {
public:
    Controller(const string& app_id, const string& mod_id, const string& config_path)
        : m_app_id(app_id)
        , m_mod_id(mod_id)
        , m_config_path(config_path)
    {
    }

    void run()
    {
        ifstream cf(m_config_path);

        if (!cf)
            return;

        nl::json j;
        cf >> j;

        for (auto&& i : j) {
            const auto action = i["action"].get<string>();
            const auto parameters = i["parameters"].get<vector<string>>();
            fmap[action](m_app_id, m_mod_id, parameters);
        }
    }

    static void do_elf_inject(
        const string& app_id, const string& mod_id, const vector<string>& parameters)
    {
        auto path = fs::path(MOD_ROOT) / app_id / mod_id / "payloads" / parameters[0].data();
        auto cmd = string(INJECTOR) + " " + app_id + " " + path.string();
        chmod(INJECTOR, 0700);
        system(cmd.data());
    }

private:
    const string m_app_id;
    const string m_mod_id;
    const string m_config_path;

    typedef function<void(const string&, const string&, const vector<string>&)> callback_t;
    map<string, callback_t> fmap{ { "elf-inject", do_elf_inject } };
};

class Runtime {
public:
    Runtime(const string& app_id)
        : m_app_id(app_id)
    {
    }

    void run()
    {
        string dir = string(MOD_ROOT) + "/" + m_app_id;

        if (!fs::exists(dir))
            return;

        // for each mod_id in app_id
        for (auto&& i : fs::directory_iterator(dir)) {
            Controller c(m_app_id, i.path().filename(), (i.path() / "control.json").string());
            c.run();
        }
    }

private:
    const string m_app_id;
};

int main(int argc, char** argv)
{
    if (argc != 2)
        return -1;

    Runtime runtime(argv[1]);
    runtime.run();

    return 0;
}