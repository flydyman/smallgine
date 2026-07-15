#pragma once
#include "../platform/glcontext.hpp"
#include <iostream>

namespace tools {

    // Drain the GL error queue, logging each with a label. Cheap; call after
    // meaningful GL work (uploads, draws) to surface otherwise-silent failures.
    inline void glCheck(const char* label)
    {
        GLenum err;
        while ((err = glGetError()) != GL_NO_ERROR)
        {
            std::cout << "GL error (" << label << "): 0x"
                      << std::hex << err << std::dec << std::endl;
        }
    }

}
