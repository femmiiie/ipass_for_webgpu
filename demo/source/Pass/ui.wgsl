struct vsInput {
  @location(0) position: vec2f,
  @location(1) tex: vec2f,
  @location(2) color: vec4f
}

struct fsInput {
  @builtin(position) position: vec4f,
  @location(1) tex: vec2f,
  @location(2) color: vec4f
}

@group(0) @binding(0) var texSampler: sampler;
@group(0) @binding(1) var texture: texture_2d<f32>;
@group(0) @binding(2) var<uniform> projection: mat4x4f;

@vertex
fn vs_main(input: vsInput) -> fsInput {
  return fsInput(
    projection * vec4f(input.position, 0.0, 1.0),
    input.tex,
    input.color
  );
}

@fragment
fn fs_main(input: fsInput) -> @location(0) vec4f {
  return input.color * textureSample(texture, texSampler, input.tex);
}