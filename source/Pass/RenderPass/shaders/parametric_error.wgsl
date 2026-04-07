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
  @location(1) tex: vec2f,
  @location(2) eyevector: vec4f,
  @location(3) @interpolate(flat) patch_idx: f32,
}

struct MVP {
  M    : mat4x4f,
  M_inv: mat4x4f,
  V    : mat4x4f,
  P    : mat4x4f
}

struct Viewport {
  x: f32,
  y: f32,
  width: f32,
  height: f32,
}

@group(0) @binding(0) var<uniform> mvp: MVP;
@group(0) @binding(1) var<uniform> viewport: Viewport;
@group(0) @binding(2) var<storage, read> controlPoints: array<vec4f>;

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
  out.tex          = input.tex;
  out.eyevector    = vec4f(cameraWorldPos() - worldPos.xyz, 0.0);
  out.patch_idx    = input.patch_idx;

  return out;
}

// Evaluate bicubic B-spline position at UV on the given patch
fn eval_bicubic_pos(patch_idx: u32, uv: vec2f) -> vec3f {
  let u = uv.x;
  let v = uv.y;
  let u2 = u * u;
  let u3 = u2 * u;
  let v2 = v * v;
  let v3 = v2 * v;

  var bu = array<f32, 4>(1.0 - 3.0*u + 3.0*u2 -     u3,
                               3.0*u - 6.0*u2 + 3.0*u3,
                               3.0*u2 - 3.0*u3,
                               u3);
  var bv = array<f32, 4>(1.0 - 3.0*v + 3.0*v2 -     v3,
                               3.0*v - 6.0*v2 + 3.0*v3,
                               3.0*v2 - 3.0*v3,
                               v3);

  let offset = patch_idx * 16u;
  var pos = vec4f(0.0);
  for (var row = 0u; row < 4u; row++) {
    for (var col = 0u; col < 4u; col++) {
      let cp = controlPoints[offset + row * 4u + col];
      pos += bu[row] * bv[col] * cp;
    }
  }
  return pos.xyz / pos.w;
}

@fragment
fn fs_main(input: fsInput) -> @location(0) vec4f {
  let true_pos = vec4f(eval_bicubic_pos(u32(input.patch_idx), input.tex), 1.0);

  let clip = mvp.P * mvp.V * mvp.M * true_pos;
  let ndc = clip.xy / clip.w;
  let true_pixel = vec2f(
    viewport.x + (ndc.x + 1.0) * viewport.width  * 0.5,
    viewport.y + (1.0 - ndc.y) * viewport.height * 0.5
  );

  let pixel_dist = length(true_pixel - input.position.xy);

  // grey <= 0.1px, blue <= 0.5px, green <= 1.0px, red otherwise
  var color: vec4f;
  if (pixel_dist <= 0.1) {
    color = vec4f(1.0, 1.0, 1.0, 1.0);
  } else if (pixel_dist <= 0.5) {
    color = vec4f(0.0, 0.0, 1.0, 1.0);
  } else if (pixel_dist <= 1.0) {
    color = vec4f(0.0, 1.0, 0.0, 1.0);
  } else {
    color = vec4f(1.0, 0.0, 0.0, 1.0);
  }

  let facing = abs(dot(normalize(input.normal), normalize(input.eyevector)));
  let shaded = clamp(0.5 * (0.2 + 0.8 * facing) * color.rgb,
                     vec3f(0.0), vec3f(0.5));
  return vec4f(shaded, 1.0);
}
