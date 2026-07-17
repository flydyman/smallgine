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
    float carVelY = 0.0f;      // vertical velocity (gravity + ramp launches)
    float carPitch = 0.0f;     // smoothed nose pitch to match the surface slope
    bool  airborne = false;
    glm::vec2 airVel{0.0f};    // horizontal velocity frozen at launch (inertial flight)
    const float groundY = k::PhysGroundTop;
    const float bound = 78.0f;
    std::vector<glm::vec3> checkpoints;
    size_t nextCp = 1;
    int lap = 0;
    double lapClock = 0.0, bestLap = 0.0;
    bool lapValid = false;

    // A launch ramp: an inclined strip rising `rise` over `len` along `dir` from
    // `base` (at ground level), `halfWid` to each side. Driving off the top with
    // speed launches the car (see surfaceHeight + the vertical integrator).
    struct Ramp { glm::vec3 base; glm::vec3 dir; float len; float halfWid; float rise; };
    std::vector<Ramp> ramps;

    // World surface height at (x,z): the flat ground, raised over any ramp.
    float surfaceHeight(float x, float z) const
    {
        float h = groundY;
        for (const Ramp& r : ramps)
        {
            float px = x - r.base.x, pz = z - r.base.z;
            float s = px * r.dir.x + pz * r.dir.z;       // distance along the ramp
            float t = px * r.dir.z - pz * r.dir.x;       // signed distance across it
            if (s < 0.0f || s > r.len || std::fabs(t) > r.halfWid) continue;
            h = std::max(h, groundY + r.rise * (s / r.len));
        }
        return h;
    }

    // Surface height plus the rate it rises *as the car travels* (gradient · vel).
    // Using the analytic slope — not a frame-to-frame delta — means only genuinely
    // driving up a ramp builds vertical speed; stepping onto a ramp from the side
    // (a boundary discontinuity) does not, so the car no longer rockets skyward.
    float surfaceHeight(float x, float z, glm::vec2 vel, float& climbOut) const
    {
        float h = groundY; climbOut = 0.0f;
        for (const Ramp& r : ramps)
        {
            float px = x - r.base.x, pz = z - r.base.z;
            float s = px * r.dir.x + pz * r.dir.z;
            float t = px * r.dir.z - pz * r.dir.x;
            if (s < 0.0f || s > r.len || std::fabs(t) > r.halfWid) continue;
            float rh = groundY + r.rise * (s / r.len);
            if (rh > h)
            {
                h = rh;
                climbOut = (r.rise / r.len) * (vel.x * r.dir.x + vel.y * r.dir.z); // gradient·velocity
            }
        }
        return h;
    }

    // Build a ramp (physics record + a tilted slab to see it) centered on a gate,
    // aligned with the racing-line direction so the car drives straight up it.
    void addRamp(Engine& e, const glm::vec3& gate, const glm::vec3& dir,
                 float len, float halfWid, float rise, GLuint tex, int idx)
    {
        glm::vec3 d(dir.x, 0.0f, dir.z);
        d = glm::normalize(d);
        glm::vec3 base(gate.x - d.x * len * 0.5f, groundY, gate.z - d.z * len * 0.5f);
        ramps.push_back({ base, d, len, halfWid, rise });

        // Exact inclined slab: local X = width, Y = normal, Z = up-slope.
        const float thick = 0.3f;
        float hyp = std::sqrt(len * len + rise * rise);
        glm::vec3 along = glm::normalize(d * len + glm::vec3(0.0f, rise, 0.0f));
        glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), d));
        glm::vec3 normal = glm::normalize(glm::cross(along, right));
        Node r; r.Name = "ramp" + std::to_string(idx);
        r.useRotMatrix = true;
        r.RotMatrix = glm::mat4(glm::vec4(right, 0.0f), glm::vec4(normal, 0.0f),
                                glm::vec4(along, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        r.Position = base + along * (hyp * 0.5f) - normal * (thick * 0.5f);
        r.Scale = glm::vec3(halfWid * 2.0f, thick, hyp);
        r.material.color = glm::vec3(0.85f, 0.5f, 0.15f); // orange ramp
        r.material.texture = "assets/test.tga"; r.materialSet = true;
        r.material.roughness = 0.7f;
        r.mesh = e.gameCube(); r.texId = tex;
        e.gameScene().MainNode.Children.push_back(r);
    }

    // Car collider footprint (world units): half-length along its forward axis,
    // half-width along its right axis, half-height for vertical overlap.
    static constexpr float kCarHalfLen = 0.85f;
    static constexpr float kCarHalfWid = 0.50f;
    static constexpr float kCarHalfHt  = 0.40f;

    // Push the car out of solid scene boxes (posts); bleed speed on hit. The car
    // is an oriented box that rotates with carYaw (fixes the "collider doesn't
    // turn with the car" sliding) — resolved against axis-aligned bricks by a 2D
    // (XZ) separating-axis test, pushing out along the min-penetration axis.
    void collide(Scene& scene)
    {
        // Car footprint axes in the XZ plane (rotate with yaw).
        glm::vec2 fwd(std::sin(carYaw), std::cos(carYaw));
        glm::vec2 rgt(fwd.y, -fwd.x);

        std::vector<tools::DrawItem> items;
        tools::collect(scene.MainNode, glm::mat4(1.0f), items);
        for (const tools::DrawItem& it : items)
        {
            const Node* nd = it.node;
            if (nd->Name == "car" || nd->material.alpha < 1.0f) continue;
            if (nd->Name.rfind("ramp", 0) == 0) continue; // ramps are drive-on surfaces, not walls
            glm::vec3 bh(0.5f * glm::length(glm::vec3(it.global[0])),
                         0.5f * glm::length(glm::vec3(it.global[1])),
                         0.5f * glm::length(glm::vec3(it.global[2])));
            if (bh.y < 0.2f) continue; // flat floor plates aren't walls
            glm::vec3 bc3(it.global[3]);
            if (std::fabs((carPos.y + kCarHalfHt) - bc3.y) > kCarHalfHt + bh.y) continue; // vertical miss

            glm::vec2 c(carPos.x, carPos.z), bc(bc3.x, bc3.z), bhw(bh.x, bh.z);
            // Separating-axis test: brick's world X/Z axes + car's forward/right.
            const glm::vec2 axes[4] = { {1.0f, 0.0f}, {0.0f, 1.0f}, fwd, rgt };
            float minOv = 1e30f; glm::vec2 mtvAxis(0.0f); bool separated = false;
            for (const glm::vec2& ax : axes)
            {
                float rCar   = std::fabs(glm::dot(fwd, ax)) * kCarHalfLen + std::fabs(glm::dot(rgt, ax)) * kCarHalfWid;
                float rBrick = std::fabs(ax.x) * bhw.x + std::fabs(ax.y) * bhw.y;
                float delta  = glm::dot(c - bc, ax);
                float ov     = (rCar + rBrick) - std::fabs(delta);
                if (ov <= 0.0f) { separated = true; break; }
                if (ov < minOv) { minOv = ov; mtvAxis = (delta < 0.0f) ? -ax : ax; }
            }
            if (separated) continue;
            carPos.x += mtvAxis.x * minOv;
            carPos.z += mtvAxis.y * minOv;
            carSpeed *= 0.35f;
        }
    }

public:
    void setup(Engine& e) override
    {
        GLuint mtex = e.gameTexture("assets/test.tga");
        const int N = 12; const float rx = 58.0f, rz = 38.0f, gateHalf = 5.0f;
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

        // Launch ramps on the straights between gates, aligned with the racing
        // line so the car drives up and jumps (gravity arcs it back down).
        const int rampAt[] = { 1, 4, 7, 10 };
        int ri = 0;
        for (int idx : rampAt)
        {
            glm::vec3 mid = 0.5f * (checkpoints[idx] + checkpoints[(idx + 1) % N]);
            glm::vec3 dir = glm::normalize(checkpoints[(idx + 1) % N] - checkpoints[idx]);
            addRamp(e, mid, dir, 9.0f, 4.0f, 2.6f, mtex, ri++);
        }
        {
            Node car; car.Name = "car";
            car.material.color = glm::vec3(0.9f, 0.15f, 0.15f); car.material.texture = "assets/test.tga";
            car.material.metallic = 0.5f; car.material.roughness = 0.35f; car.materialSet = true;

            // Imported car model (assets/race.glb), with a cube fallback if the
            // asset is missing so the demo still runs.
            LoadedModel& model = e.gameModel("assets/race.glb");
            if (model.mesh && model.mesh != e.gameCube())
            {
                car.mesh = model.mesh;
                car.Scale = glm::vec3(1.0f);            // model authored at its own scale
                if (model.hasMaterial) { car.material = model.material; car.materialSet = true; }
                // Use the model's texture only if it actually ships (e.g. a Kenney
                // kit colormap); otherwise a neutral texture beats a checker.
                std::string tex = car.material.texture;
                if (tex.empty() || !assetExists(tex)) tex = "assets/test.tga";
                car.material.texture = tex;
                car.texId = e.gameTexture(tex);
            }
            else
            {
                car.Scale = glm::vec3(0.9f, 0.55f, 1.6f); // fallback box roughly car-shaped
                car.mesh = e.gameCube(); car.texId = mtex;
            }
            e.gameScene().MainNode.Children.push_back(car);
        }
        carPos = checkpoints[0];
        carPos.y = groundY;
        glm::vec3 fwd0 = glm::normalize(checkpoints[1] - checkpoints[0]);
        carYaw = std::atan2(fwd0.x, fwd0.z);
        Camera& cam = e.gameCamera();
        cam.position = carPos - fwd0 * 9.0f + glm::vec3(0.0f, 4.2f, 0.0f);
        cam.front = glm::normalize(carPos + glm::vec3(0.0f, 1.1f, 0.0f) - cam.position);
        std::cout << "Race: " << checkpoints.size() << " checkpoints, car ready" << std::endl;
    }

    void update(Engine& e, float dt) override
    {
        const float accel = 23.0f, brakePow = 30.0f, maxSpeed = 36.0f, reverseMax = 7.0f;
        const float linDrag = 0.9f, turnRate = glm::radians(115.0f);

        bool grounded = !airborne; // wheels only bite the ground

        // Throttle/brake/steer act only on the ground. In the air the car coasts
        // on its launch momentum (see airVel) — engine + wheels do nothing.
        if (grounded)
        {
            float throttle = (e.kW() ? 1.0f : 0.0f) - (e.kS() ? 1.0f : 0.0f);
            if (throttle > 0.0f)      carSpeed += accel * dt;
            else if (throttle < 0.0f) carSpeed -= brakePow * dt;
            carSpeed -= carSpeed * linDrag * dt;
            if (throttle == 0.0f && std::fabs(carSpeed) < 0.4f) carSpeed = 0.0f;
            carSpeed = std::max(-reverseMax, std::min(maxSpeed, carSpeed));

            float steer = (e.kA() ? 1.0f : 0.0f) - (e.kD() ? 1.0f : 0.0f);
            float grip = std::min(std::fabs(carSpeed) / 6.0f, 1.0f);
            carYaw += steer * turnRate * dt * grip * (carSpeed >= 0.0f ? 1.0f : -1.0f);
        }

        glm::vec3 fwd(std::sin(carYaw), 0.0f, std::cos(carYaw));
        glm::vec2 vel = grounded ? glm::vec2(fwd.x, fwd.z) * carSpeed : airVel; // inertial in air
        float oldX = carPos.x, oldZ = carPos.z;
        carPos.x += vel.x * dt;
        carPos.z += vel.y * dt;
        carPos.x = std::max(-bound, std::min(bound, carPos.x));
        carPos.z = std::max(-bound, std::min(bound, carPos.z));

        // Ledge block: a grounded car can only rise a small step per frame (drive
        // up a ramp's gentle front face). A big upward step — the ramp's vertical
        // back or side — acts as a wall, so the car can't climb onto it from there.
        const float kStepMax = 0.5f;
        if (grounded && surfaceHeight(carPos.x, carPos.z) - carPos.y > kStepMax)
        {
            carPos.x = oldX; carPos.z = oldZ; // pushed back off the ledge
            carSpeed *= 0.3f;
            vel = glm::vec2(0.0f);
        }
        collide(e.gameScene());

        // Vertical physics: gravity + ramp surface. Climbing a ramp gives upward
        // velocity (slope · travel speed), so leaving the top launches the car; it
        // arcs under gravity and lands when it meets the surface again. Deriving
        // the climb from the analytic slope (not a frame delta) stops the car from
        // rocketing off when it steps onto a ramp sideways.
        bool wasAirborne = airborne;
        float climbRate = 0.0f;
        float grNow = surfaceHeight(carPos.x, carPos.z, vel, climbRate);
        carVelY -= k::PhysGravity * dt;
        carPos.y += carVelY * dt;
        if (carPos.y <= grNow)
        {
            carPos.y = grNow;               // snap onto the surface (no launch from a step)
            carVelY = std::max(0.0f, climbRate); // only up-slope travel builds launch speed
            airborne = false;
        }
        else airborne = true;

        // Ground <-> air transitions carry momentum both ways (inertial flight).
        if (!wasAirborne && airborne)
            airVel = glm::vec2(fwd.x, fwd.z) * carSpeed;               // freeze launch velocity
        else if (wasAirborne && !airborne)                            // landed: resume ground control
            carSpeed = glm::length(airVel) *
                       (glm::dot(airVel, glm::vec2(fwd.x, fwd.z)) >= 0.0f ? 1.0f : -1.0f);

        // Nose pitch: follow the slope on the ground, ease back to level in the air.
        float ahead  = surfaceHeight(carPos.x + fwd.x * 1.0f, carPos.z + fwd.z * 1.0f);
        float behind = surfaceHeight(carPos.x - fwd.x * 1.0f, carPos.z - fwd.z * 1.0f);
        float targetPitch = airborne ? std::min(0.5f, std::max(-0.5f, carVelY * 0.06f))
                                     : std::atan2(ahead - behind, 2.0f);
        carPitch += (targetPitch - carPitch) * std::min(1.0f, 8.0f * dt);

        if (Node* c = e.gameScene().MainNode.find("car"))
        {
            // Orient from an explicit basis (yaw + pitch correct at any heading):
            // local X = right, Y = up, Z = forward.
            glm::vec3 right(std::cos(carYaw), 0.0f, -std::sin(carYaw));
            float cp = std::cos(carPitch), sp = std::sin(carPitch);
            glm::vec3 fwd3(fwd.x * cp, sp, fwd.z * cp);
            glm::vec3 up = glm::normalize(glm::cross(fwd3, right));
            c->useRotMatrix = true;
            c->RotMatrix = glm::mat4(glm::vec4(right, 0.0f), glm::vec4(up, 0.0f),
                                     glm::vec4(fwd3, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            c->Position = glm::vec3(carPos.x, carPos.y + 0.3f, carPos.z);
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
        glm::vec3 want = carPos - fwd * 9.0f + glm::vec3(0.0f, 4.2f, 0.0f);
        float k = std::min(1.0f, 6.0f * dt);
        cam.position += (want - cam.position) * k;
        cam.front = glm::normalize((carPos + glm::vec3(0.0f, 1.1f, 0.0f) + fwd * 2.0f) - cam.position);
    }

    // F3 collider view: the car's true oriented footprint (rotates with yaw) plus
    // the axis-aligned brick boxes it tests against — matching collide() exactly.
    bool drawsColliders() const override { return true; }
    void debugColliders(Engine& e, DebugLines& dl) override
    {
        glm::vec3 fwd(std::sin(carYaw), 0.0f, std::cos(carYaw));
        glm::vec3 rgt(std::cos(carYaw), 0.0f, -std::sin(carYaw));
        glm::vec3 center(carPos.x, carPos.y + kCarHalfHt, carPos.z);
        dl.obb(center, fwd, rgt, kCarHalfLen, kCarHalfWid, kCarHalfHt, glm::vec3(1.0f, 0.85f, 0.1f));

        std::vector<tools::DrawItem> items;
        tools::collect(e.gameScene().MainNode, glm::mat4(1.0f), items);
        for (const tools::DrawItem& it : items)
        {
            const Node* nd = it.node;
            if (nd->Name == "car" || nd->material.alpha < 1.0f) continue;
            if (nd->Name.rfind("ramp", 0) == 0) continue;
            glm::vec3 bh(0.5f * glm::length(glm::vec3(it.global[0])),
                         0.5f * glm::length(glm::vec3(it.global[1])),
                         0.5f * glm::length(glm::vec3(it.global[2])));
            if (bh.y < 0.2f) continue; // flat plates aren't walls (skipped in collide too)
            dl.box(AABB::fromCenter(glm::vec3(it.global[3]), bh), glm::vec3(0.15f, 1.0f, 0.35f));
        }
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
