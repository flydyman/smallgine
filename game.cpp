#include "platform/window.hpp"
#include "platform/paths.hpp"
#include "core/config.hpp"

namespace smallgine {
    Engine engine;
}

int main(int /*argc*/, char* /*argv*/[])
{
    using namespace smallgine;

    Config cfg = loadConfig(resolvePath("settings.json"));

    GLWindow win;
    int res = win.init(cfg.width, cfg.height, cfg.title.c_str());
    if (res != 0)
    {
        return res;
    }
    engine.configure(cfg);
    win.run();
    return 0;
}