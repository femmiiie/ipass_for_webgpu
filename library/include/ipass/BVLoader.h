#pragma once

#include <string>
#include "PatchData.h"

namespace ipass {

PatchData LoadBV(const std::string& filepath, Status* status = nullptr, uint32_t max_patches = 0);

}
