#pragma once

#include "PatchData.h"
#include "LODPass.h"
#include "TessellationPass.h"
#include "Pipeline.h"

namespace ipass {

PatchData LoadBV(const std::string& filepath, Status* status = nullptr, uint32_t max_patches = 0);

}
