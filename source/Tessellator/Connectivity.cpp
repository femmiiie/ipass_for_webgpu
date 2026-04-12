#include "Connectivity.h"

#include <unordered_map>
#include <array>
#include <utility>
#include <cstdio>

// (3)--[2]--(2)
//  |         |
// [3]       [1]
//  |         |
// (0)--[0]--(1)
// [edge] and (vertex) ids per quad

static uint64_t edgeKey(uint32_t a, uint32_t b) {
    if (a > b) std::swap(a, b);
    return (static_cast<uint64_t>(a) << 32) | b;
}

std::vector<glm::ivec4> buildQuadConnectivity(const uint32_t * indices, int num_indices) {
    const uint32_t num_quads = static_cast<uint32_t>(num_indices);
    std::vector<glm::ivec4> connections(num_quads*2, glm::ivec4(-1));
    // Usage Guide for contributors
    // for quad with index "quad":
    //  connections[2*quad+0][edge] = index of neighboring quad for edge "edge"
    //  connections[2*quad+1][edge] = index of neighboring edge (within quad 0-3) for edge "edge"

    constexpr int edge_verts[4][2] = {{0,1}, {1,2}, {2,3}, {3,0}};

    // Edge -> (quad, edge index)
    std::unordered_map<uint64_t, std::pair<uint32_t, int32_t>> edge_map;
    edge_map.reserve(num_quads * 4);

    for (uint32_t quad = 0; quad < num_quads; quad++) {
        for (int edge = 0; edge < 4; edge++) {
            const uint32_t vertex_1 = indices[quad * 4 + static_cast<uint32_t>(edge_verts[edge][0])];
            const uint32_t vertex_2 = indices[quad * 4 + static_cast<uint32_t>(edge_verts[edge][1])];
            uint64_t key = edgeKey(vertex_1, vertex_2);

            auto it = edge_map.find(key);
            if (it != edge_map.end()) {
                auto [neighboring_quad, neighboring_edge] = it->second;

                if (connections[2*quad + 0][edge] != -1) {
                    fprintf(stderr, "WARNING: edge with indices (%u,%u) is shared by more than 2 quads!\n",
                            static_cast<uint32_t>(key >> 32), static_cast<uint32_t>(key & 0xFFFFFFFFu));
                    continue;
                }

                connections[2*quad + 0][edge] = neighboring_quad;
                connections[2*quad + 1][edge] = neighboring_edge;

                connections[2*neighboring_quad + 0][neighboring_edge] = quad;
                connections[2*neighboring_quad + 1][neighboring_edge] = edge;
            } else {
                edge_map[key] = {quad, edge};
            }
        }
    }

    return connections;
}
