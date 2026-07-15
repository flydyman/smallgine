#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>
#include "glmjson.hpp"

namespace smallgine {

// Free-fly FPS camera. Movement handled by the engine; look driven by mouse.
struct Camera
{
    glm::vec3 position{0.0f, 0.0f, 3.0f};
    glm::vec3 front{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float speed = 2.0f;        // units per second
    float sensitivity = 0.1f;  // degrees per pixel
    float yaw = -90.0f;        // -90 => looking down -Z
    float pitch = 0.0f;
    float fov = 45.0f;         // vertical field of view, degrees

    glm::vec3 right() const
    {
        return glm::normalize(glm::cross(front, up));
    }

    glm::mat4 view() const
    {
        return glm::lookAt(position, position + front, up);
    }

    // Recompute front from yaw/pitch (euler angles, degrees).
    void updateVectors()
    {
        glm::vec3 f;
        f.x = cosf(glm::radians(yaw)) * cosf(glm::radians(pitch));
        f.y = sinf(glm::radians(pitch));
        f.z = sinf(glm::radians(yaw)) * cosf(glm::radians(pitch));
        front = glm::normalize(f);
    }

    // Scroll wheel: positive dy zooms in (narrower fov).
    void addZoom(float dy)
    {
        fov -= dy * 2.0f;
        if (fov < 15.0f) fov = 15.0f;
        if (fov > 90.0f) fov = 90.0f;
    }

    // Apply a mouse delta (pixels). dy is already y-up.
    void addLook(float dx, float dy)
    {
        yaw += dx * sensitivity;
        pitch += dy * sensitivity;
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        updateVectors();
    }
};

inline void to_json(nlohmann::json& j, const Camera& c)
{
    j = nlohmann::json{{"position", c.position}, {"yaw", c.yaw}, {"pitch", c.pitch}, {"fov", c.fov}};
}
inline void from_json(const nlohmann::json& j, Camera& c)
{
    if (j.contains("position")) c.position = j.at("position").get<glm::vec3>();
    c.yaw = j.value("yaw", c.yaw);
    c.pitch = j.value("pitch", c.pitch);
    c.fov = j.value("fov", c.fov);
    c.updateVectors();
}

} // namespace smallgine
