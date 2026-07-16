#pragma once
#include "core/engine.hpp"
#include "tools/helpers.hpp"
#include "tools/broadphase.hpp"
#include <cstdio>

namespace smallgine {

// FPS demo: capsule walker on the level's scene, crosshair hitscan on target*.
class FpsMode : public IGameMode
{
    glm::vec3 playerPos{0.0f};
    float playerVelY = 0.0f;
    bool onGround = false, jumpReq = false;
    const float eyeHeight = 1.6f, playerRadius = 0.3f;
    int score = 0;

    // Push the player capsule out of solid, opaque scene boxes (walls / crates).
    void collide(Scene& scene)
    {
        glm::vec3 half(playerRadius, eyeHeight * 0.5f, playerRadius);
        glm::vec3 center(playerPos.x, playerPos.y + eyeHeight * 0.5f, playerPos.z);
        AABB pbox = AABB::fromCenter(center, half);
        std::vector<tools::DrawItem> items;
        tools::collect(scene.MainNode, glm::mat4(1.0f), items);
        for (const tools::DrawItem& it : items)
        {
            const Node* nd = it.node;
            if (nd->Name == "ground" || nd->Name == "terrain" || nd->material.alpha < 1.0f) continue;
            glm::vec3 bh(0.5f * glm::length(glm::vec3(it.global[0])),
                         0.5f * glm::length(glm::vec3(it.global[1])),
                         0.5f * glm::length(glm::vec3(it.global[2])));
            glm::vec3 bc(it.global[3]);
            AABB box = AABB::fromCenter(bc, bh);
            if (!pbox.overlaps(box)) continue;
            if (bc.y + bh.y < playerPos.y + 0.05f) continue; // low enough to stand on
            float px = std::min(pbox.max.x - box.min.x, box.max.x - pbox.min.x);
            float pz = std::min(pbox.max.z - box.min.z, box.max.z - pbox.min.z);
            if (px < pz) playerPos.x += (center.x < bc.x ? -px : px);
            else         playerPos.z += (center.z < bc.z ? -pz : pz);
            center = glm::vec3(playerPos.x, playerPos.y + eyeHeight * 0.5f, playerPos.z);
            pbox = AABB::fromCenter(center, half);
        }
    }

public:
    void setup(Engine& e) override
    {
        Camera& cam = e.gameCamera();
        cam.pitch = 0.0f; cam.updateVectors(); // level gaze meets eye-level targets
        playerPos = glm::vec3(cam.position.x, e.gameGroundY(cam.position.x, cam.position.z), cam.position.z);
        playerVelY = 0.0f;
    }

    void onKeyPress(Engine&, int key) override
    {
        if (key == GLFW_KEY_SPACE) jumpReq = true;
    }

    bool onClick(Engine& e, int button, int action) override
    {
        if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return false;
        e.gamePick();
        const std::string& sel = e.gameSelected();
        if (sel.rfind("target", 0) == 0)
        {
            if (Node* n = e.gameScene().MainNode.find(sel))
            {
                bool alreadyHit = n->material.color.g > 0.9f && n->material.color.r < 0.2f;
                if (!alreadyHit)
                {
                    n->material.color = glm::vec3(0.1f, 1.0f, 0.2f);
                    score++;
                    e.gameAudio().playTone(880.0f, 0.08f);
                    std::cout << "Hit " << sel << "  score=" << score << std::endl;
                }
            }
        }
        return true;
    }

    void update(Engine& e, float dt) override
    {
        Camera& cam = e.gameCamera();
        glm::vec3 fwd = cam.front; fwd.y = 0.0f;
        if (glm::length(fwd) > 1e-4f) fwd = glm::normalize(fwd);
        glm::vec3 rt = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
        glm::vec3 wish(0.0f);
        if (e.kW()) wish += fwd;
        if (e.kS()) wish -= fwd;
        if (e.kD()) wish += rt;
        if (e.kA()) wish -= rt;
        if (glm::length(wish) > 1e-4f) wish = glm::normalize(wish);
        playerPos += wish * 3.2f * dt;

        playerVelY -= 14.0f * dt;
        if (jumpReq && onGround) { playerVelY = 5.5f; onGround = false; }
        jumpReq = false;
        playerPos.y += playerVelY * dt;

        float ground = e.gameGroundY(playerPos.x, playerPos.z);
        if (playerPos.y <= ground) { playerPos.y = ground; playerVelY = 0.0f; onGround = true; }
        else onGround = false;

        collide(e.gameScene());
        cam.position = playerPos + glm::vec3(0.0f, eyeHeight, 0.0f);
    }

    void hud(Engine& e) override
    {
        TextRenderer& t = e.gameText();
        int w = e.gameScreenW(), h = e.gameScreenH();
        t.draw("+", w * 0.5f - 5.0f, h * 0.5f + 6.0f, w, h, glm::vec3(1.0f));
        t.draw("SCORE " + std::to_string(score), w - 150.0f, 26.0f, w, h, glm::vec3(0.3f, 1.0f, 0.4f));
        t.draw("WASD move | SPACE jump | LMB shoot targets | TAB cursor | ESC quit",
               12.0f, (float)h - 14.0f, w, h, glm::vec3(0.9f, 0.9f, 0.6f));
    }
};

} // namespace smallgine
