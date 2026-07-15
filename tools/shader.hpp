#pragma once
#include "../platform/glcontext.hpp"
#include "../platform/paths.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

namespace tools {

    // Read a whole text file (empty string on failure).
    inline std::string readFile(const std::string& path)
    {
        std::ifstream f(path);
        if (!f) { std::cout << "Shader file missing: " << path << std::endl; return ""; }
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    // Compile a single shader stage. Returns 0 on failure.
    static GLuint compileShader(GLenum type, const char* src)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint ok = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            GLint len = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
            std::string log(len > 0 ? len : 1, '\0');
            glGetShaderInfoLog(shader, (GLsizei)log.size(), nullptr, log.data());
            std::cout << "Shader compile error: " << log << std::endl;
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    // Compile + link a vertex/fragment program. Returns 0 on failure.
    static GLuint linkProgram(const char* vertSrc, const char* fragSrc)
    {
        GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
        GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);
        if (vert == 0 || frag == 0)
        {
            if (vert) glDeleteShader(vert);
            if (frag) glDeleteShader(frag);
            return 0;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vert);
        glAttachShader(program, frag);
        glLinkProgram(program);

        // Shaders no longer needed once linked.
        glDeleteShader(vert);
        glDeleteShader(frag);

        GLint ok = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            GLint len = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
            std::string log(len > 0 ? len : 1, '\0');
            glGetProgramInfoLog(program, (GLsizei)log.size(), nullptr, log.data());
            std::cout << "Program link error: " << log << std::endl;
            glDeleteProgram(program);
            return 0;
        }
        return program;
    }

    // Link a program from external GLSL files (resolved exe-relative). 0 on failure.
    inline GLuint linkProgramFiles(const std::string& vertPath, const std::string& fragPath)
    {
        std::string v = readFile(smallgine::resolvePath(vertPath));
        std::string f = readFile(smallgine::resolvePath(fragPath));
        if (v.empty() || f.empty()) return 0;
        return linkProgram(v.c_str(), f.c_str());
    }

}
