#pragma once
#include "../platform/glcontext.hpp"
#include "../platform/paths.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>

namespace tools {

    // Last-modified time of a file (0 if missing). Used for shader hot-reload.
    inline long fileMtime(const std::string& path)
    {
        struct stat st;
        return (stat(path.c_str(), &st) == 0) ? (long)st.st_mtime : 0;
    }

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

    // Link a single-stage compute program from an external file. 0 on failure.
    inline GLuint linkComputeFile(const std::string& path)
    {
        std::string s = readFile(smallgine::resolvePath(path));
        if (s.empty()) return 0;
        GLuint cs = compileShader(GL_COMPUTE_SHADER, s.c_str());
        if (!cs) return 0;
        GLuint prog = glCreateProgram();
        glAttachShader(prog, cs);
        glLinkProgram(prog);
        glDeleteShader(cs);
        GLint ok = GL_FALSE;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            GLint len = 0; glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
            std::string log(len > 0 ? len : 1, '\0');
            glGetProgramInfoLog(prog, (GLsizei)log.size(), nullptr, log.data());
            std::cout << "Compute link error: " << log << std::endl;
            glDeleteProgram(prog);
            return 0;
        }
        return prog;
    }

}
