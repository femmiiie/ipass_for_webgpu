#ifndef COMPUTEPASS_H_
#define COMPUTEPASS_H_

#include "Pass.h"


class ComputePass : public Pass
{
public:
  ComputePass(Context& ctx) : Pass(ctx) {}
  virtual ~ComputePass() = default;
  virtual wgpu::Buffer& Execute(wgpu::ComputePassEncoder& pass) = 0;

  wgpu::ComputePipeline pipeline;
};

class ComputePassException : public std::exception
{
private:
  std::string message;

public:
  ComputePassException(std::string m) : message(m) {}
  ComputePassException(const char* m) { this->message = m; }
  const char* what() const noexcept override { return this->message.c_str(); }
};

#endif
