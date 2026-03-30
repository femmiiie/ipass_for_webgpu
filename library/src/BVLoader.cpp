#include "ipass/BVLoader.h"
#include "BVParser.h"
#include "Elevation.h"

#include <iostream>
#include <cstring>

namespace ipass {
namespace BVLoader {

PatchData Load(const std::string& filepath, Status* status)
{
    return Load(filepath, 0u, status); // 0 means no limit
}

PatchData Load(const std::string& filepath, uint32_t max_patches, Status* status)
{
    BVParser parser;
    parser.Parse(filepath);

    const auto& patches = parser.Get();
    const auto& dims    = parser.GetDims();

    if (patches.empty()) {
        std::cerr << "[BVLoader] No patches loaded from " << filepath << std::endl;
        if (status) *status = Status::LoadFailed;
        return {};
    }

    PatchData result;
    uint32_t count = 0;
    uint32_t limit = (max_patches > 0) ? max_patches : static_cast<uint32_t>(patches.size());

    // elevate all patches to bicubic and collect control points
    for (size_t pi = 0; pi < patches.size() && count < limit; pi++) {
        if (patches[pi].empty()) continue;
        auto elevated = elevation::elevatePatchPositions(
            patches[pi], dims[pi].first, dims[pi].second);
        result.control_points.insert(result.control_points.end(),
            elevated.begin(), elevated.end());
        count++;
    }

    result.num_patches = count;
    if (count == 0) {
        if (status) *status = Status::LoadFailed;
        return result;
    }

    constexpr uint32_t corner_cp_offsets[4] = {0, 12, 15, 3};

    std::vector<std::pair<glm::vec3, uint32_t>> vertexMap;
    uint32_t nextVertId = 0;
    result.corner_indices.resize(count * 4);

    for (uint32_t i = 0; i < count; i++) {
        for (int c = 0; c < 4; c++) {
            glm::vec3 p(result.control_points[i * 16 + corner_cp_offsets[c]]);
            uint32_t vid = nextVertId;
            bool found = false;
            for (const auto& [vpos, existingId] : vertexMap) {
                if (glm::distance(p, vpos) < 1e-5f) {
                    vid = existingId;
                    found = true;
                    break;
                }
            }
            if (!found) {
                vertexMap.push_back({p, nextVertId});
                vid = nextVertId;
                nextVertId++;
            }
            result.corner_indices[i * 4 + c] = vid;
        }
    }

    if (status) *status = Status::Success;
    return result;
}

}
}
