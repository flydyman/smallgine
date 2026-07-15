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
    float cameraSpeed = 2.0f;
    float mouseSensitivity = 0.1f;
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
        c.cameraSpeed = j.value("cameraSpeed", c.cameraSpeed);
        c.mouseSensitivity = j.value("mouseSensitivity", c.mouseSensitivity);
    }
    catch (const std::exception& e)
    {
        std::cout << "Config parse error: " << e.what() << ", using defaults" << std::endl;
    }

    return c;
}

} // namespace smallgine
