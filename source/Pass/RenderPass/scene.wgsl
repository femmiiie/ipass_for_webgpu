struct vsInput {
  @location(0) position: vec4f,
  @location(1) normal: vec4f,
  @location(2) color: vec4f,
  @location(3) tex: vec2f
}

struct fsInput {
  @builtin(position) position: vec4f,
  @location(0) normal: vec4f,
  @location(1) color: vec4f,
  @location(2) tex: vec2f,
  @location(3) eyevector: vec4f,
  @location(4) viewPosition: vec4f
}

struct MVP {
  M    : mat4x4f,
  M_inv: mat4x4f,
  V    : mat4x4f,
  P    : mat4x4f
}

struct lightData {
  position: vec4f,
  color: vec4f,
  power: f32,
  shadingMode: u32
}

@group(0) @binding(0) var<uniform> mvp: MVP;
@group(0) @binding(1) var<uniform> light: lightData;

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
  out.tex          = input.tex;
  out.eyevector    = vec4f(cameraWorldPos() - worldPos.xyz, 0.0);
  out.viewPosition = worldPos;

  return out;
}

fn shade_blinn_phong(color: vec4f, view_pos: vec4f, n: vec4f, eye: vec4f) -> vec3f {
  return blinn_phong(color, 0.1, 10.0, normalize(n), normalize(eye), view_pos);
}

fn shade_flat(color: vec4f, n: vec4f, eye: vec4f) -> vec3f {
  let facing = abs(dot(n, normalize(eye)));
  return clamp(0.5 * (0.2 + 0.8 * facing) * color.rgb, vec3f(0.0f, 0.0f, 0.0f), vec3f(0.5f, 0.5f, 0.5f));
}

@fragment
fn fs_main(input: fsInput) -> @location(0) vec4f {
  var color: vec3f;
  switch (light.shadingMode) {
    case 1u: { color = shade_flat(input.color, input.normal, input.eyevector); }
    default: { color = shade_blinn_phong(input.color, input.viewPosition, input.normal, input.eyevector); }
  }
  return vec4f(color, 1.0);
}

fn blinn_phong(base_color: vec4f, spec_scale: f32, spec_exp: f32,
               n: vec4f, e: vec4f, world_pos: vec4f              ) -> vec3f 
{
  let spec_color: vec4f      = spec_scale * base_color;
  let light_direction: vec4f = light.position - world_pos;
  let light_distance: f32    = max(length(light_direction), 0.001);
  let l: vec4f               = normalize(light_direction);

  var diffuse: vec4f  = base_color * light.color * light.power
                        * clamp(dot(n, l), 0.0, 1.0)
                        / (light_distance * light_distance);
  var ambient: vec4f  = 0.2 * base_color;
  let r: vec4f        = reflect(-l, n);
  var specular: vec4f = spec_color * light.color * light.power
                        * pow(clamp(dot(e, r), 0.0, 1.0), spec_exp)
                        / (light_distance * light_distance);

  let attenuation: f32 = 1.0 / (1.0 + 0.22 * light_distance + 0.20 * (light_distance * light_distance));
  diffuse  *= attenuation;
  specular *= attenuation;
  ambient  *= attenuation;

  return clamp(diffuse + ambient + specular, vec4f(0.0), vec4f(1.0)).xyz;
}