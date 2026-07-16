// smallgine procedural-noise demo. Config-driven: "sandbox": false for a bare
// stage; the NoiseMode builds a terrain from tools/noise.hpp generators that you
// can cycle through at runtime.
#include "platform/window.hpp"
#include "platform/paths.hpp"
#include "core/config.hpp"
#include "noisemode.hpp"
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
    engine.setGameMode(std::make_unique<NoiseMode>());
    win.run();
    return 0;
}
