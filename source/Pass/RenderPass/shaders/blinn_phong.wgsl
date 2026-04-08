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
  @location(3) viewPosition: vec4f,
}

struct MVP {
  M    : mat4x4f,
  M_inv: mat4x4f,
  V    : mat4x4f,
  P    : mat4x4f
}

struct Light {
  position: vec4f,
  color: vec4f,
  power: f32,
}

struct LightArray {
  lights: array<Light, 4>,
}

@group(0) @binding(0) var<uniform> mvp: MVP;
@group(0) @binding(1) var<uniform> lights: LightArray;

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
  out.viewPosition = worldPos;

  return out;
}

fn blinn_phong(base_color: vec4f, spec_scale: f32, spec_exp: f32,
               n: vec4f, e: vec4f, world_pos: vec4f) -> vec3f
{
  let spec_color: vec4f    = spec_scale * base_color;
  let ambient_share: vec4f = (0.2 / 4.0) * base_color; // split evenly across lights so it sums to avg-attenuated ambient
  var result: vec4f        = vec4f(0.0);

  for (var i: i32 = 0; i < 4; i++) {
    let light = lights.lights[i];

    let light_direction: vec4f = light.position - world_pos;
    let light_distance: f32    = max(length(light_direction), 0.001);
    let l: vec4f               = normalize(light_direction);

    var diffuse: vec4f  = base_color * light.color * light.power
                          * clamp(dot(n, l), 0.0, 1.0)
                          / (light_distance * light_distance);
    let r: vec4f        = reflect(-l, n);
    var specular: vec4f = spec_color * light.color * light.power
                          * pow(clamp(dot(e, r), 0.0, 1.0), spec_exp)
                          / (light_distance * light_distance);

    let attenuation: f32 = 1.0 / (1.0 + 0.22 * light_distance + 0.20 * (light_distance * light_distance));
    result += (ambient_share + diffuse + specular) * attenuation;
  }

  return clamp(result, vec4f(0.0), vec4f(1.0)).xyz;
}

@fragment
fn fs_main(input: fsInput) -> @location(0) vec4f {
  let color = blinn_phong(input.color, 0.1, 10.0,
                          normalize(input.normal), normalize(input.eyevector),
                          input.viewPosition);
  return vec4f(color, 1.0);
}
