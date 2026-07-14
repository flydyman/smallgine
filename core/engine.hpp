#pragma once
#include <nlohmann/json.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include "scene.hpp"
#include "../tools/helpers.hpp"

using json = nlohmann::json;

class Engine {
private:
    bool isInitialized;
    bool isLooped;
    json settings;

public:
    Engine()
    {
        isInitialized = true;
        isLooped = false;

        std::cout << "Engine created" << std::endl;
    }

    bool looped()
    {
        return isLooped;
    }

    void create()
    {
        isLooped = true;

        std::cout << "Engine initialized" << std::endl;
    }


    void draw(GLFWwindow * window)
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float ratio = width / (float)height;
        glClearColor(0.5f, 0.2f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void process(double dt)
    {

    }

    void input(int key, int scancode, int action, int mods)
    {
        #ifdef DEBUG
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            isLooped = false;
            return;
        }
        #endif
    }

    void cleanup()
    {
        std::cout << "Engine destroyed" << std::endl;
    }
} engine;