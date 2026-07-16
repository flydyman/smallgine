#pragma once
#include "core/engine.hpp"
#include "tools/helpers.hpp"
#include "tools/broadphase.hpp"
#include <cstdio>

namespace smallgine {

// Racing demo: arcade car (throttle/steer), chase camera, lap checkpoints.
class RaceMode : public IGameMode
{
    glm::vec3 carPos{0.0f};
    float carYaw = 0.0f, carSpeed = 0.0f;
    const float groundY = k::PhysGroundTop;
    const float bound = 26.0f;
    std::vector<glm::vec3> checkpoints;
    size_t nextCp = 1;
    int lap = 0;
    double lapClock = 0.0, bestLap = 0.0;
    bool lapValid = false;

    // Push the car out of solid scene boxes (posts); bleed speed on hit.
    void collide(Scene& scene)
    {
        const float rad = 0.7f;
        glm::vec3 center(carPos.x, groundY + 0.4f, carPos.z);
        AABB cbox = AABB::fromCenter(center, glm::vec3(rad, 0.4f, rad));
        std::vector<tools::DrawItem> items;
        tools::collect(scene.MainNode, glm::mat4(1.0f), items);
        for (const tools::DrawItem& it : items)
        {
            const Node* nd = it.node;
            if (nd->Name == "car" || nd->material.alpha < 1.0f) continue;
            glm::vec3 bh(0.5f * glm::length(glm::vec3(it.global[0])),
                         0.5f * glm::length(glm::vec3(it.global[1])),
                         0.5f * glm::length(glm::vec3(it.global[2])));
            if (bh.y < 0.2f) continue; // flat floor plates aren't walls
            glm::vec3 bc(it.global[3]);
            AABB box = AABB::fromCenter(bc, bh);
            if (!cbox.overlaps(box)) continue;
            float px = std::min(cbox.max.x - box.min.x, box.max.x - cbox.min.x);
            float pz = std::min(cbox.max.z - box.min.z, box.max.z - cbox.min.z);
            if (px < pz) carPos.x += (center.x < bc.x ? -px : px);
            else         carPos.z += (center.z < bc.z ? -pz : pz);
            center = glm::vec3(carPos.x, groundY + 0.4f, carPos.z);
            cbox = AABB::fromCenter(center, glm::vec3(rad, 0.4f, rad));
            carSpeed *= 0.35f;
        }
    }

public:
    void setup(Engine& e) override
    {
        GLuint mtex = e.gameTexture("assets/test.tga");
        const int N = 8; const float rx = 16.0f, rz = 11.0f, gateHalf = 3.8f;
        for (int i = 0; i < N; ++i)
        {
            float ang = 6.2831853f * (float)i / (float)N;
            glm::vec3 center(rx * std::cos(ang), groundY, rz * std::sin(ang));
            checkpoints.push_back(center);
            glm::vec3 dir = glm::normalize(glm::vec3(-rx * std::sin(ang), 0.0f, rz * std::cos(ang)));
            glm::vec3 perp(dir.z, 0.0f, -dir.x);
            for (int s = -1; s <= 1; s += 2)
            {
                Node p; p.Name = "post" + std::to_string(i) + (s < 0 ? "a" : "b");
                p.Position = center + perp * (gateHalf * (float)s) + glm::vec3(0.0f, 0.7f, 0.0f);
                p.Scale = glm::vec3(0.5f, 1.4f, 0.5f);
                p.material.color = (i == 0) ? glm::vec3(1.0f, 0.9f, 0.2f) : glm::vec3(0.9f, 0.95f, 1.0f);
                p.material.texture = "assets/test.tga"; p.materialSet = true;
                p.mesh = e.gameCube(); p.texId = mtex;
                e.gameScene().MainNode.Children.push_back(p);
            }
            Node m; m.Name = "cpMark" + std::to_string(i);
            m.Position = center + glm::vec3(0.0f, 0.02f, 0.0f);
            m.Scale = glm::vec3(1.4f, 0.04f, 1.4f);
            m.material.color = (i == 0) ? glm::vec3(1.0f, 0.85f, 0.1f) : glm::vec3(0.25f, 0.25f, 0.28f);
            m.material.texture = "assets/test.tga"; m.materialSet = true;
            m.mesh = e.gameCube(); m.texId = mtex;
            e.gameScene().MainNode.Children.push_back(m);
        }
        {
            Node car; car.Name = "car"; car.Scale = glm::vec3(0.9f, 0.55f, 1.6f);
            car.material.color = glm::vec3(0.9f, 0.15f, 0.15f); car.material.texture = "assets/test.tga";
            car.material.metallic = 0.5f; car.material.roughness = 0.35f; car.materialSet = true;
            car.mesh = e.gameCube(); car.texId = mtex;
            e.gameScene().MainNode.Children.push_back(car);
        }
        carPos = checkpoints[0];
        glm::vec3 fwd0 = glm::normalize(checkpoints[1] - checkpoints[0]);
        carYaw = std::atan2(fwd0.x, fwd0.z);
        Camera& cam = e.gameCamera();
        cam.position = carPos - fwd0 * 7.0f + glm::vec3(0.0f, 3.4f, 0.0f);
        cam.front = glm::normalize(carPos + glm::vec3(0.0f, 1.1f, 0.0f) - cam.position);
        std::cout << "Race: " << checkpoints.size() << " checkpoints, car ready" << std::endl;
    }

    void update(Engine& e, float dt) override
    {
        const float accel = 17.0f, brakePow = 26.0f, maxSpeed = 24.0f, reverseMax = 6.0f;
        const float linDrag = 0.9f, turnRate = glm::radians(115.0f);

        float throttle = (e.kW() ? 1.0f : 0.0f) - (e.kS() ? 1.0f : 0.0f);
        if (throttle > 0.0f)      carSpeed += accel * dt;
        else if (throttle < 0.0f) carSpeed -= brakePow * dt;
        carSpeed -= carSpeed * linDrag * dt;
        if (throttle == 0.0f && std::fabs(carSpeed) < 0.4f) carSpeed = 0.0f;
        carSpeed = std::max(-reverseMax, std::min(maxSpeed, carSpeed));

        float steer = (e.kA() ? 1.0f : 0.0f) - (e.kD() ? 1.0f : 0.0f);
        float grip = std::min(std::fabs(carSpeed) / 6.0f, 1.0f);
        carYaw += steer * turnRate * dt * grip * (carSpeed >= 0.0f ? 1.0f : -1.0f);

        glm::vec3 fwd(std::sin(carYaw), 0.0f, std::cos(carYaw));
        carPos += fwd * carSpeed * dt;
        carPos.y = groundY;
        carPos.x = std::max(-bound, std::min(bound, carPos.x));
        carPos.z = std::max(-bound, std::min(bound, carPos.z));
        collide(e.gameScene());

        if (Node* c = e.gameScene().MainNode.find("car"))
        {
            c->Position = glm::vec3(carPos.x, groundY + 0.3f, carPos.z);
            c->Rotation.y = glm::degrees(carYaw);
        }

        if (!checkpoints.empty())
        {
            glm::vec3 cp = checkpoints[nextCp];
            if (glm::length(glm::vec2(carPos.x - cp.x, carPos.z - cp.z)) < 3.2f)
            {
                nextCp++;
                if (nextCp >= checkpoints.size())
                {
                    nextCp = 0; lap++;
                    if (lapValid && (bestLap == 0.0 || lapClock < bestLap)) bestLap = lapClock;
                    lapClock = 0.0; lapValid = true;
                    e.gameAudio().playTone(760.0f, 0.15f);
                    std::cout << "Lap " << lap << "  best " << bestLap << std::endl;
                }
                else { e.gameAudio().playTone(520.0f, 0.05f); std::cout << "Checkpoint " << nextCp << "/" << checkpoints.size() << std::endl; }
            }
        }
        lapClock += dt;

        Camera& cam = e.gameCamera();
        glm::vec3 want = carPos - fwd * 7.0f + glm::vec3(0.0f, 3.4f, 0.0f);
        float k = std::min(1.0f, 6.0f * dt);
        cam.position += (want - cam.position) * k;
        cam.front = glm::normalize((carPos + glm::vec3(0.0f, 1.1f, 0.0f) + fwd * 2.0f) - cam.position);
    }

    void hud(Engine& e) override
    {
        int w = e.gameScreenW(), h = e.gameScreenH();
        TextRenderer& t = e.gameText();
        char line[96];
        std::snprintf(line, sizeof(line), "SPEED %3d km/h", (int)(std::fabs(carSpeed) * 3.6f));
        t.draw(line, w - 220.0f, 30.0f, w, h, glm::vec3(0.4f, 1.0f, 0.6f));
        std::snprintf(line, sizeof(line), "LAP %d   time %4.1fs   best %4.1fs", lap, lapClock, bestLap);
        t.draw(line, 12.0f, 50.0f, w, h, glm::vec3(0.8f, 0.95f, 1.0f));
        t.draw("W throttle | S brake/reverse | A/D steer | ESC quit",
               12.0f, (float)h - 14.0f, w, h, glm::vec3(0.9f, 0.9f, 0.6f));
    }
};

} // namespace smallgine
