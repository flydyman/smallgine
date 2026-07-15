#pragma once
#include <string>
#include <vector>
#include <memory>
#include "mesh.hpp"
#include "../core/material.hpp"

namespace smallgine {

// Decode a base64 string (glTF data-URI buffers).
std::vector<unsigned char> base64Decode(const std::string& s);

// glTF 2.0 loader: whole node hierarchy flattened (transforms baked) across all
// mesh primitives, PBR material factors + baseColor texture, .gltf and binary .glb.
std::shared_ptr<Mesh> loadGLTF(const std::string& path, Material* outMat = nullptr, bool* outHas = nullptr);

} // namespace smallgine
