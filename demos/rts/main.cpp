// smallgine RTS demo. Config-driven like the FPS demo: "sandbox": false for a
// clean field, "rtsMode": true for top-down select / move / attack control.
#include "platform/window.hpp"
#include "platform/paths.hpp"
#include "core/config.hpp"
#include "rtsmode.hpp"
#include <memory>

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
    engine.setGameMode(std::make_unique<RtsMode>());
    win.run();
    return 0;
}
