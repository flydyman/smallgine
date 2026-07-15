#pragma once
#include <string>
#include <memory>
#include "mesh.hpp"
#include "../core/material.hpp"

namespace smallgine {

// Parse a .mtl file, filling the first material found (Kd color, map_Kd texture).
// Returns true if a material was read. `objDir` prefixes a relative map_Kd path.
bool loadMTL(const std::string& path, const std::string& objDir, Material& out);

// Minimal Wavefront OBJ loader: v / vt / vn / f (any of v, v/vt, v//vn, v/vt/vn),
// polygons fan-triangulated. Missing normals are computed per face. Returns
// nullptr on failure. Requires a current GL context (builds the Mesh).
std::shared_ptr<Mesh> loadOBJ(const std::string& path,
                              Material* outMat = nullptr, bool* outHasMat = nullptr);

} // namespace smallgine
