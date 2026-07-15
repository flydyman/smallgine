// smallgine FPS demo. Reuses the engine as-is; the scene + settings drive it:
// "sandbox": false strips the baked engine sandbox props, "playerStart": true
// begins in the capsule player controller (walk / jump / crosshair hitscan).
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
