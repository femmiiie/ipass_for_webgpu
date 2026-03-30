#pragma once

#include <string>
#include "PatchData.h"

namespace ipass {

namespace BVLoader {
    PatchData Load(const std::string& filepath, Status* status = nullptr);
    PatchData Load(const std::string& filepath, uint32_t max_patches, Status* status = nullptr);
}

}
