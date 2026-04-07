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
  @location(0) bary: vec2f,
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

  if (input.bary_id < 0.5) {
    out.bary = vec2f(1.0, 0.0);
  } else if (input.bary_id < 1.5) {
    out.bary = vec2f(0.0, 1.0);
  } else {
    out.bary = vec2f(0.0, 0.0);
  }

  return out;
}

@fragment
fn fs_main(input: fsInput) -> @location(0) vec4f {
  let db1dx = dpdx(input.bary.x);
  let db1dy = dpdy(input.bary.x);
  let db2dx = dpdx(input.bary.y);
  let db2dy = dpdy(input.bary.y);
  let det = abs(db1dx * db2dy - db1dy * db2dx);

  // triangle has area 0.5 in barycentric space
  let area = 0.5 / max(det, 1e-10);

  // grey >= 20px, red < 20px, green < 10px, blue < 5px
  if (area < 5.0) {
    return vec4f(0.0, 0.0, 1.0, 1.0);
  } else if (area < 10.0) {
    return vec4f(0.0, 1.0, 0.0, 1.0);
  } else if (area < 20.0) {
    return vec4f(1.0, 0.0, 0.0, 1.0);
  }
  return vec4f(1.0, 1.0, 1.0, 1.0);
}
