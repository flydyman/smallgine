#pragma once
#include "../core/scene.hpp"
#include <string>

namespace smallgine {

// LuaJIT scripting: runs a behavior script's update(dt, t) each frame with a
// small API bound to the scene (move/rotate nodes by name). Hot-reloadable via
// the asset watcher (the .lua file re-loads on change).
class ScriptSystem {
private:
    void* L = nullptr;   // lua_State* (opaque to keep Lua headers out of the engine TU)
    long mtime = 0;
    std::string path;
    Scene* scenePtr = nullptr;

public:
    bool init(const std::string& scriptPath, Scene* scene);
    void reloadIfChanged();
    void update(float dt, float time);
    void free();
    bool ready() const { return L != nullptr; }
};

} // namespace smallgine
