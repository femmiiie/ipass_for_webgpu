#ifndef IPASS_H_
#define IPASS_H_

#include "ComputePass.h"


class IPass : public ComputePass
{
public:
  const int MAX_PATCHES = 65536; //double check this

  IPass(Context& ctx);
  ~IPass();
  wgpu::Buffer& Execute(wgpu::ComputePassEncoder& pass) override;

  // group 0 storage buffers
  wgpu::Buffer verticesBuffer;
  wgpu::Buffer patchesBuffer;

  // group 1 uniform buffers
  wgpu::Buffer mvpBuffer;
  wgpu::Buffer vertCountBuffer;
  wgpu::Buffer pixelSizeBuffer;

private:
  wgpu::BindGroup storageBindGroup;
  wgpu::BindGroup uniformBindGroup;
};

#endif
