#include "script.hpp"
#include "../tools/shader.hpp"   // tools::fileMtime
#include <iostream>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace smallgine {

// Single-threaded bridge: the bound C functions operate on this scene.
static Scene* g_scene = nullptr;

static int l_node_set_pos(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2), y = (float)luaL_checknumber(L, 3), z = (float)luaL_checknumber(L, 4);
    if (g_scene) if (Node* n = g_scene->MainNode.find(name)) n->Position = glm::vec3(x, y, z);
    return 0;
}
static int l_node_set_rot(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2), y = (float)luaL_checknumber(L, 3), z = (float)luaL_checknumber(L, 4);
    if (g_scene) if (Node* n = g_scene->MainNode.find(name)) n->Rotation = glm::vec3(x, y, z);
    return 0;
}
static int l_node_get_pos(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    if (g_scene) if (Node* n = g_scene->MainNode.find(name))
    {
        lua_pushnumber(L, n->Position.x); lua_pushnumber(L, n->Position.y); lua_pushnumber(L, n->Position.z);
        return 3;
    }
    return 0;
}

bool ScriptSystem::init(const std::string& scriptPath, Scene* scene)
{
    g_scene = scene;
    scenePtr = scene;
    path = scriptPath;
    lua_State* l = luaL_newstate();
    if (!l) { std::cout << "Lua: newstate failed" << std::endl; return false; }
    luaL_openlibs(l);
    lua_register(l, "node_set_pos", l_node_set_pos);
    lua_register(l, "node_set_rot", l_node_set_rot);
    lua_register(l, "node_get_pos", l_node_get_pos);
    std::string src = readAssetText(scriptPath);
    if (src.empty() ||
        luaL_loadbuffer(l, src.c_str(), src.size(), scriptPath.c_str()) != 0 ||
        lua_pcall(l, 0, 0, 0) != 0)
    {
        std::cout << "Lua load error: " << (lua_isstring(l, -1) ? lua_tostring(l, -1) : "empty script") << std::endl;
        lua_close(l);
        return false;
    }
    L = l;
    mtime = tools::fileMtime(scriptPath);
    std::cout << "Lua script loaded: " << scriptPath << std::endl;
    return true;
}

void ScriptSystem::reloadIfChanged()
{
    long mt = tools::fileMtime(path);
    if (!mt || mt == mtime) return;
    mtime = mt;
    lua_State* l = (lua_State*)L;
    if (!l) return;
    std::string src = readAssetText(path);
    if (!src.empty() &&
        luaL_loadbuffer(l, src.c_str(), src.size(), path.c_str()) == 0 &&
        lua_pcall(l, 0, 0, 0) == 0)
        std::cout << "Lua script reloaded" << std::endl;
    else
        std::cout << "Lua reload error: " << (lua_isstring(l, -1) ? lua_tostring(l, -1) : "load failed") << std::endl;
}

void ScriptSystem::update(float dt, float time)
{
    lua_State* l = (lua_State*)L;
    if (!l) return;
    g_scene = scenePtr; // rebind the process-global bridge to this system's scene
    lua_getglobal(l, "update");
    if (!lua_isfunction(l, -1)) { lua_pop(l, 1); return; }
    lua_pushnumber(l, dt);
    lua_pushnumber(l, time);
    if (lua_pcall(l, 2, 0, 0) != 0)
    {
        std::cout << "Lua update error: " << lua_tostring(l, -1) << std::endl;
        lua_pop(l, 1);
    }
}

void ScriptSystem::free()
{
    if (L) lua_close((lua_State*)L);
    L = nullptr;
}

} // namespace smallgine
