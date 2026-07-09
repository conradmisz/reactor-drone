/**
 * script_loader.cpp - Load and save debug sessions as JSON script files
 */

#include "script_loader.hpp"

#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

CommandLineOptions load_script(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        throw std::runtime_error("Cannot open script file: " + path);
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(f);
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("Script JSON parse error in " + path + ": " + e.what());
    }

    CommandLineOptions opts;

    if (j.contains("fps"))        opts.fps        = j["fps"].get<int>();
    if (j.contains("stop_frame")) opts.stop_frame = j["stop_frame"].get<uint64_t>();
    if (j.contains("paused"))     opts.paused     = j["paused"].get<bool>();
    if (j.contains("verbose"))    opts.verbose    = j["verbose"].get<bool>();
    if (j.contains("clear_logs")) opts.clear_logs = j["clear_logs"].get<bool>();
    if (j.contains("seed")) {
        opts.seed = j["seed"].get<uint64_t>();
    }

    if (j.contains("keys")) {
        for (const auto& k : j["keys"]) {
            opts.keys.push_back({k["frame"].get<uint64_t>(), k["key"].get<std::string>()});
        }
    }
    if (j.contains("clicks")) {
        for (const auto& c : j["clicks"]) {
            opts.clicks.push_back({
                c["frame"].get<uint64_t>(),
                c["x"].get<int>(),
                c["y"].get<int>()
            });
        }
    }
    if (j.contains("dump")) {
        for (const auto& v : j["dump"]) opts.dump_frames.push_back(v.get<uint64_t>());
    }
    if (j.contains("trace")) {
        for (const auto& v : j["trace"]) opts.trace_frames.push_back(v.get<uint64_t>());
    }
    if (j.contains("screenshot")) {
        for (const auto& v : j["screenshot"]) opts.screenshot_frames.push_back(v.get<uint64_t>());
    }

    return opts;
}

void save_script(const CommandLineOptions& opts, const std::string& path) {
    nlohmann::json j = nlohmann::json::object();

    if (opts.fps > 0)                  j["fps"]        = opts.fps;
    if (opts.stop_frame.has_value())   j["stop_frame"] = opts.stop_frame.value();
    if (opts.paused)                   j["paused"]     = opts.paused;
    if (opts.verbose)                  j["verbose"]    = opts.verbose;
    if (!opts.clear_logs)              j["clear_logs"] = opts.clear_logs;
    if (opts.seed.has_value())         j["seed"]       = opts.seed.value();

    if (!opts.keys.empty()) {
        auto arr = nlohmann::json::array();
        for (const auto& ka : opts.keys) {
            arr.push_back({{"frame", ka.frame}, {"key", ka.key}});
        }
        j["keys"] = std::move(arr);
    }
    if (!opts.clicks.empty()) {
        auto arr = nlohmann::json::array();
        for (const auto& ca : opts.clicks) {
            arr.push_back({{"frame", ca.frame}, {"x", ca.x}, {"y", ca.y}});
        }
        j["clicks"] = std::move(arr);
    }
    if (!opts.dump_frames.empty())       j["dump"]       = opts.dump_frames;
    if (!opts.trace_frames.empty())      j["trace"]      = opts.trace_frames;
    if (!opts.screenshot_frames.empty()) j["screenshot"] = opts.screenshot_frames;

    std::ofstream f(path);
    if (!f) {
        throw std::runtime_error("Cannot write script file: " + path);
    }
    f << j.dump(2) << "\n";
}
