#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace smallgine {

// Runtime settings loaded from a JSON file. Missing file/keys fall back to defaults.
struct Config
{
    int width = 800;
    int height = 600;
    std::string title = "smallgine";
    std::string texture = "assets/test.tga"; // empty => procedural checker
    std::string scene;                        // scene JSON path; empty => built-in demo
    std::string pack = "assets.sgpk";         // asset pack file mounted if present (empty => none)
    float cameraSpeed = 2.0f;
    float mouseSensitivity = 0.1f;
    bool  sandbox = true;                      // false => skip built-in engine props (demos set this)
};

inline Config loadConfig(const std::string& path)
{
    Config c;
    std::ifstream f(path);
    if (!f)
    {
        std::cout << "Config not found (" << path << "), using defaults" << std::endl;
        return c;
    }

    try
    {
        nlohmann::json j;
        f >> j;
        c.width = j.value("width", c.width);
        c.height = j.value("height", c.height);
        c.title = j.value("title", c.title);
        c.texture = j.value("texture", c.texture);
        c.scene = j.value("scene", c.scene);
        c.pack = j.value("pack", c.pack);
        c.cameraSpeed = j.value("cameraSpeed", c.cameraSpeed);
        c.mouseSensitivity = j.value("mouseSensitivity", c.mouseSensitivity);
        c.sandbox = j.value("sandbox", c.sandbox);
    }
    catch (const std::exception& e)
    {
        std::cout << "Config parse error: " << e.what() << ", using defaults" << std::endl;
    }

    return c;
}

} // namespace smallgine
