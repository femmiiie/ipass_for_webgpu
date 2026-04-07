struct vsInput {
  @location(0) position: vec4f,
  @location(1) normal: vec4f,
  @location(2) color: vec4f,
  @location(3) tex: vec2f,
  @location(4) patch_idx: f32,
  @location(5) bary_id: f32,
}

struct fsInput {
  @builtin(position) position: vec4f,
  @location(0) normal: vec4f,
  @location(1) color: vec4f,
  @location(2) eyevector: vec4f,
}

struct MVP {
  M    : mat4x4f,
  M_inv: mat4x4f,
  V    : mat4x4f,
  P    : mat4x4f
}

@group(0) @binding(0) var<uniform> mvp: MVP;

fn cameraWorldPos() -> vec3f {
  return vec3f(
    -dot(mvp.V[0].xyz, mvp.V[3].xyz),
    -dot(mvp.V[1].xyz, mvp.V[3].xyz),
    -dot(mvp.V[2].xyz, mvp.V[3].xyz)
  );
}

@vertex
fn vs_main(input: vsInput) -> fsInput {
  var out: fsInput;
  let worldPos     = mvp.M * input.position;
  out.position     = mvp.P * mvp.V * worldPos;
  out.normal       = transpose(mvp.M_inv) * input.normal;
  out.color        = input.color;
  out.eyevector    = vec4f(cameraWorldPos() - worldPos.xyz, 0.0);

  return out;
}

@fragment
fn fs_main(input: fsInput) -> @location(0) vec4f {
  let facing = abs(dot(normalize(input.normal), normalize(input.eyevector)));
  let color = clamp(0.5 * (0.2 + 0.8 * facing) * input.color.rgb,
                    vec3f(0.0), vec3f(0.5));
  return vec4f(color, 1.0);
}
