// smallgine space flight demo. Config-driven: "sandbox": false for a bare stage,
// "spaceMode": true for the 6DOF ship controller + momentum + chase camera.
#include "platform/window.hpp"
#include "platform/paths.hpp"
#include "core/config.hpp"

namespace smallgine {
    Engine engine; // the window callbacks reference this global instance
}

int main(int /*argc*/, char* /*argv*/[])
{
    using namespace smallgine;

    Config cfg = loadConfig(resolvePath("settings.json"));

    GLWindow win;
    WindowError res = win.init(cfg.width, cfg.height, cfg.title.c_str());
    if (res != WindowError::Ok)
    {
        return static_cast<int>(res);
    }
    engine.configure(cfg);
    win.run();
    return 0;
}
