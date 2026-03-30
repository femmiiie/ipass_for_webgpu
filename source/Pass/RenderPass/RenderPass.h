#pragma once

#include "Pass.h"
#include "Context.h"

#include <string>
#include <exception>


class RenderPass : public Pass
{
public:
  RenderPass(Context& ctx) : Pass(ctx), context(ctx) {}
  Context& context;
  virtual ~RenderPass() = default;
  virtual void Execute(wgpu::RenderPassEncoder& pass) = 0;

  wgpu::RenderPipeline pipeline;
  uint32_t vertexCount;
  wgpu::Buffer vertexBuffer;
};

class RenderPassException : public std::exception
{
private:
  std::string message;

public:
  RenderPassException(std::string m) : message(m) {}
  RenderPassException(const char* m) { this->message = m; }
  const char* what() const noexcept override { return this->message.c_str(); }
};
