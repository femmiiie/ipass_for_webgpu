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
  power: f32
}

@group(0) @binding(0) var<uniform> mvp: MVP;
@group(0) @binding(1) var<uniform> light: lightData;

@vertex
fn vs_main(input: vsInput) -> fsInput {
  var output: fsInput;

  output.position     = mvp.P * mvp.V * mvp.M * input.position;
  output.normal       = transpose(mvp.M_inv) * input.normal;
  output.color        = input.color;
  output.tex          = input.tex;
  output.eyevector    = -(mvp.V * mvp.M * input.position);
  output.viewPosition = mvp.M * input.position;

  return output;
}



@fragment
fn fs_main(input: fsInput) -> @location(0) vec4f {
  let base_color: vec4f  = input.color;
  let spec_color: vec4f  = 0.1 * base_color;

  let light_direction: vec4f = light.position - input.viewPosition;
  let light_distance: f32    = length(light_direction);

//diffuse
  let n: vec4f = normalize(input.normal);
  let l: vec4f = normalize(light_direction);
  var diffuse: vec4f = input.color * light.color * light.power * clamp(dot(n, l), 0.0, 1.0) / (light_distance * light_distance);

//ambient
  var ambient: vec4f = 0.1 * input.color;

//specular
  let e: vec4f = normalize(input.eyevector);
  let r: vec4f = reflect(-l, n);
  var specular: vec4f = spec_color * light.color * light.power * pow(clamp(dot(e, r), 0.0, 1.0), 10.0) / (light_distance * light_distance);

//attenuation
  let attenuation: f32 = 1.0 / (1.0 + 0.22 * light_distance + 0.20 * (light_distance * light_distance));
  diffuse  *= attenuation;
  specular *= attenuation;
  ambient  *= attenuation;

  let lit = clamp(diffuse + ambient + specular, vec4f(0), vec4f(1)).xyz;
  return vec4f(lit, 1.0);
}