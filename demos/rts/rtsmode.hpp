#pragma once
#include "core/engine.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace smallgine {

// RTS demo: top-down selection + move/attack orders over a ground plane.
class RtsMode : public IGameMode
{
    struct Unit {
        std::string name;
        glm::vec3 goal{0.0f};
        glm::vec3 base{0.4f};
        bool selected = false, enemy = false, hasGoal = false;
        std::string target;
        float hp = 3.0f, cooldown = 0.0f;
    };
    std::vector<Unit> units;
    const float groundY = k::PhysGroundTop;
    const float bound = 18.0f;
    bool dragging = false;
    double dragX0 = 0.0, dragY0 = 0.0;
    int selCount = 0;

    glm::vec3 groundPick(Engine& e, double mx, double my)
    {
        int w = e.gameScreenW() > 0 ? e.gameScreenW() : 1;
        int h = e.gameScreenH() > 0 ? e.gameScreenH() : 1;
        Camera& cam = e.gameCamera();
        glm::mat4 vp = glm::perspective(glm::radians(cam.fov), (float)w / h,
                                        k::PerspectiveNear, k::PerspectiveFar) * cam.view();
        glm::mat4 inv = glm::inverse(vp);
        float nx = (float)(2.0 * mx / w - 1.0), ny = (float)(1.0 - 2.0 * my / h);
        glm::vec4 pn = inv * glm::vec4(nx, ny, -1.0f, 1.0f); pn /= pn.w;
        glm::vec4 pf = inv * glm::vec4(nx, ny,  1.0f, 1.0f); pf /= pf.w;
        glm::vec3 ro(pn), rd = glm::normalize(glm::vec3(pf) - glm::vec3(pn));
        if (std::fabs(rd.y) < 1e-5f) return glm::vec3(ro.x, groundY, ro.z);
        return ro + rd * ((groundY - ro.y) / rd.y);
    }

    void highlight(Engine& e, Unit& u)
    {
        if (Node* n = e.gameScene().MainNode.find(u.name))
            n->material.color = u.selected ? glm::mix(u.base, glm::vec3(1.0f), 0.55f) : u.base;
    }
    void clearSelection(Engine& e)
    {
        for (Unit& u : units) if (u.selected) { u.selected = false; highlight(e, u); }
        selCount = 0;
    }

    void selectAt(Engine& e, double mx, double my)
    {
        glm::vec3 p = groundPick(e, mx, my);
        clearSelection(e);
        int best = -1; float bestD = 1.4f;
        for (size_t i = 0; i < units.size(); ++i)
        {
            if (units[i].enemy) continue;
            Node* n = e.gameScene().MainNode.find(units[i].name);
            if (!n) continue;
            float d = glm::length(glm::vec2(n->Position.x - p.x, n->Position.z - p.z));
            if (d < bestD) { bestD = d; best = (int)i; }
        }
        if (best >= 0) { units[best].selected = true; highlight(e, units[best]); selCount = 1; }
        std::cout << "RTS select: " << selCount << std::endl;
    }

    void boxSelect(Engine& e, double x0, double y0, double x1, double y1)
    {
        glm::vec3 a = groundPick(e, x0, y0), b = groundPick(e, x1, y1);
        float minx = std::min(a.x, b.x), maxx = std::max(a.x, b.x);
        float minz = std::min(a.z, b.z), maxz = std::max(a.z, b.z);
        clearSelection(e);
        for (Unit& u : units)
        {
            if (u.enemy) continue;
            Node* n = e.gameScene().MainNode.find(u.name);
            if (!n) continue;
            if (n->Position.x >= minx && n->Position.x <= maxx &&
                n->Position.z >= minz && n->Position.z <= maxz)
            { u.selected = true; highlight(e, u); selCount++; }
        }
        std::cout << "RTS box-select: " << selCount << std::endl;
    }

    void command(Engine& e, double mx, double my)
    {
        glm::vec3 p = groundPick(e, mx, my);
        std::string enemyHit; float bestD = 1.2f;
        for (Unit& u : units)
        {
            if (!u.enemy) continue;
            Node* n = e.gameScene().MainNode.find(u.name);
            if (!n) continue;
            float d = glm::length(glm::vec2(n->Position.x - p.x, n->Position.z - p.z));
            if (d < bestD) { bestD = d; enemyHit = u.name; }
        }
        int idx = 0, cols = 4;
        for (Unit& u : units)
        {
            if (!u.selected) continue;
            if (!enemyHit.empty()) { u.target = enemyHit; u.hasGoal = false; }
            else
            {
                float ox = (float)(idx % cols - cols / 2) * 1.1f;
                float oz = (float)(idx / cols) * 1.1f;
                u.goal = glm::vec3(p.x + ox, groundY, p.z + oz);
                u.hasGoal = true; u.target.clear(); idx++;
            }
        }
        std::cout << (enemyHit.empty() ? "RTS move order" : "RTS attack order") << std::endl;
    }

    void spawn(Engine& e, const std::string& name, glm::vec3 pos, glm::vec3 col, bool enemy)
    {
        Node u; u.Name = name; u.Scale = glm::vec3(0.5f);
        u.Position = glm::vec3(pos.x, groundY + 0.25f, pos.z);
        u.material.color = col; u.material.texture = "assets/test.tga";
        u.material.metallic = 0.1f; u.material.roughness = 0.6f; u.materialSet = true;
        u.mesh = e.gameCube(); u.texId = e.gameTexture("assets/test.tga");
        e.gameScene().MainNode.Children.push_back(u);
        Unit ru; ru.name = name; ru.base = col; ru.enemy = enemy; ru.hp = enemy ? 3.0f : 5.0f;
        units.push_back(ru);
    }

public:
    void setup(Engine& e) override
    {
        Camera& cam = e.gameCamera();
        cam.position = glm::vec3(0.0f, 16.0f, 12.0f);
        cam.yaw = -90.0f; cam.pitch = -62.0f; cam.updateVectors();
        e.gameCaptureMouse(false);
        int id = 0;
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                spawn(e, "unit" + std::to_string(id++),
                      glm::vec3(-6.0f + c * 1.2f, 0.0f, 5.0f - r * 1.2f),
                      glm::vec3(0.2f, 0.45f, 0.95f), false);
        int eid = 0;
        for (int r = 0; r < 2; ++r)
            for (int c = 0; c < 4; ++c)
                spawn(e, "enemy" + std::to_string(eid++),
                      glm::vec3(4.0f + c * 1.2f, 0.0f, -5.0f + r * 1.2f),
                      glm::vec3(0.9f, 0.2f, 0.2f), true);
        std::cout << "RTS: " << units.size() << " units (12 friendly, 8 enemy)" << std::endl;
    }

    bool onClick(Engine& e, int button, int action) override
    {
        double mx = e.gameMouseX(), my = e.gameMouseY();
        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            if (action == GLFW_PRESS) { dragging = true; dragX0 = mx; dragY0 = my; }
            else if (action == GLFW_RELEASE && dragging)
            {
                dragging = false;
                double dx = mx - dragX0, dy = my - dragY0;
                if (dx * dx + dy * dy < 64.0) selectAt(e, mx, my);
                else boxSelect(e, dragX0, dragY0, mx, my);
            }
            return true;
        }
        if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) { command(e, mx, my); return true; }
        return false;
    }

    void update(Engine& e, float dt) override
    {
        // Camera pan (WASD), kept level.
        Camera& cam = e.gameCamera();
        float v = 10.0f * dt;
        glm::vec3 f(cam.front.x, 0.0f, cam.front.z);
        if (glm::length(f) > 1e-4f) f = glm::normalize(f);
        glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3(0, 1, 0)));
        if (e.kW()) cam.position += f * v;
        if (e.kS()) cam.position -= f * v;
        if (e.kD()) cam.position += r * v;
        if (e.kA()) cam.position -= r * v;
        float b = bound + 6.0f;
        cam.position.x = std::max(-b, std::min(b, cam.position.x));
        cam.position.z = std::max(-b, std::min(b, cam.position.z));

        Scene& scene = e.gameScene();
        for (Unit& u : units)
        {
            Node* n = scene.MainNode.find(u.name);
            if (!n) continue;
            if (u.cooldown > 0.0f) u.cooldown -= dt;
            if (!u.target.empty())
            {
                Unit* tgt = nullptr;
                for (Unit& en : units) if (en.name == u.target) { tgt = &en; break; }
                Node* tn = tgt ? scene.MainNode.find(tgt->name) : nullptr;
                if (!tgt || !tn) u.target.clear();
                else
                {
                    glm::vec3 d = tn->Position - n->Position; d.y = 0.0f;
                    float dist = glm::length(d);
                    if (dist > 1.3f) n->Position += d / dist * 4.0f * dt;
                    else if (u.cooldown <= 0.0f) { tgt->hp -= 1.0f; u.cooldown = 0.6f; e.gameAudio().playTone(300.0f, 0.05f); }
                    continue;
                }
            }
            if (u.hasGoal)
            {
                glm::vec3 d = u.goal - n->Position; d.y = 0.0f;
                float dist = glm::length(d);
                if (dist <= 0.08f) u.hasGoal = false;
                else n->Position += d / dist * 4.5f * dt;
            }
            n->Position.y = groundY + n->Scale.y * 0.5f;
        }

        for (size_t i = 0; i < units.size();)
        {
            if (units[i].enemy && units[i].hp <= 0.0f)
            {
                std::string dead = units[i].name;
                scene.MainNode.removeChild(dead);
                for (Unit& u : units) if (u.target == dead) u.target.clear();
                std::cout << "RTS: " << dead << " destroyed" << std::endl;
                units.erase(units.begin() + i);
            }
            else ++i;
        }

        for (size_t i = 0; i < units.size(); ++i)
        {
            Node* a = scene.MainNode.find(units[i].name);
            if (!a) continue;
            for (size_t j = i + 1; j < units.size(); ++j)
            {
                Node* bn = scene.MainNode.find(units[j].name);
                if (!bn) continue;
                glm::vec3 d = bn->Position - a->Position; d.y = 0.0f;
                float dist = glm::length(d), mind = 0.85f;
                if (dist > 1e-4f && dist < mind)
                {
                    glm::vec3 push = d / dist * (mind - dist) * 0.5f;
                    a->Position -= push; bn->Position += push;
                }
            }
        }
    }

    void hud(Engine& e) override
    {
        int w = e.gameScreenW(), h = e.gameScreenH();
        if (dragging)
        {
            float x0 = (float)std::min(dragX0, e.gameMouseX()), y0 = (float)std::min(dragY0, e.gameMouseY());
            float rw = (float)std::fabs(e.gameMouseX() - dragX0), rh = (float)std::fabs(e.gameMouseY() - dragY0);
            e.gameUI().rect({x0, y0, rw, rh}, glm::vec4(0.3f, 0.9f, 0.4f, 0.25f), w, h);
        }
        int friendly = 0, enemies = 0;
        for (const Unit& u : units) (u.enemy ? enemies : friendly)++;
        TextRenderer& t = e.gameText();
        t.draw("RTS   units " + std::to_string(friendly) + "   enemies " + std::to_string(enemies) +
               "   selected " + std::to_string(selCount), 12.0f, 50.0f, w, h, glm::vec3(0.7f, 0.95f, 1.0f));
        t.draw("LMB select / drag-box | RMB move or attack | WASD pan | ESC quit",
               12.0f, (float)h - 14.0f, w, h, glm::vec3(0.9f, 0.9f, 0.6f));
    }
};

} // namespace smallgine
