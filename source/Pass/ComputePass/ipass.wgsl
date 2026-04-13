const f32_max           : f32 = 10000.0f;
const verts_per_patch   : u32 = 16;
const patches_per_group : u32 = 16;
const tiles_per_patch   : u32 = 9;
const group_size        : u32 = 256;  // verts_per_patch * patches_per_group

struct vert
{
  pos:   vec4<f32>,
  color: vec4<f32>,
  tex:   vec2<f32>
}

@group(0) @binding(0) var<storage, read>       vertices:   array<vert>;
@group(0) @binding(1) var<storage, read_write> patches: array<f32>;

@group(1) @binding(0) var<uniform> mvp: mat4x4<f32>;
@group(1) @binding(1) var<uniform> vert_count: u32;
@group(1) @binding(2) var<uniform> pixel_size: f32;

const slefe_lower_3_3 = array<f32, 8>(
  // base 1
	 0.0000000000,
	-0.3703703704,
	-0.2962962963,
	 0.0000000000,
	// base 2
	 0.0000000000,
	-0.2962962963,
	-0.3703703704,
	 0.0000000000
);

const slefe_upper_3_3 = array<f32, 8>(
	// base 1
	-0.0695214343,
	-0.4398918047,
	-0.3153515940,
	-0.0087327217,
	// base 2
	-0.0087327217,
	-0.3153515940,
	-0.4398918047,
	-0.0695214343
);

const tile_indices = array<vec4<u32>, 9>(
  vec4u(0,  1,  4,  5),
  vec4u(1,  2,  5,  6),
  vec4u(2,  3,  6,  7),
  vec4u(4,  5,  8,  9),
  vec4u(5,  6,  9,  10),
  vec4u(6,  7,  10, 11),
  vec4u(8,  9,  12, 13),
  vec4u(9,  10, 13, 14),
  vec4u(10, 11, 14, 15),
);

var<workgroup> control_points : array<vec4<f32>, group_size>;

var<workgroup> upper          : array<vec4<f32>, group_size>;
var<workgroup> lower          : array<vec4<f32>, group_size>;

var<workgroup> d2b_u          : array<vec4<f32>, group_size>;
var<workgroup> d2b_upper      : array<vec4<f32>, group_size>;
var<workgroup> d2b_lower      : array<vec4<f32>, group_size>;

var<workgroup> local_max      : array<f32, group_size>;

var<workgroup> tess_level     : array<atomic<u32>, patches_per_group>;


fn add_up_basis(
  seg_upper  : ptr<function, vec4<f32>>,
  seg_lower  : ptr<function, vec4<f32>>,
  d2         : vec4<f32>,
  upper_coef : f32,
  lower_coef : f32,
)
{
  let mask : vec4<f32> = step(vec4f(0.0), d2);
  *seg_upper += mix(vec4(upper_coef), vec4(lower_coef), mask) * d2;
  *seg_lower += mix(vec4(lower_coef), vec4(upper_coef), mask) * d2;
}

fn calc_bounding_box(
  sb_min   : vec4<f32>,
  sb_max   : vec4<f32>,
  cube_min : ptr<function, vec3<f32>>,
  cube_max : ptr<function, vec3<f32>>,
)
{
  *cube_min = vec3f( f32_max);
  *cube_max = vec3f(-f32_max);

  for (var i: u32 = 0; i < 8; i++)
  {
    let p : vec3<f32> = select(
      sb_min.xyz,
      sb_max.xyz,
      vec3<bool>((i & 1) != 0, (i & 2) != 0, (i & 4) != 0)
    );

    let tp  : vec4<f32> = mvp * vec4f(p, 1.0f);
    let ntp : vec3<f32> = tp.xyz / tp.w;

    *cube_min = min(*cube_min, ntp);
    *cube_max = max(*cube_max, ntp);
  }
}


@compute @workgroup_size(group_size, 1, 1)
fn ipass(@builtin(global_invocation_id) id: vec3<u32>)
{
  if (id.x >= vert_count) { return; }

  let patch_id  : u32 = id.x / verts_per_patch;
  let group_id  : u32 = patch_id % patches_per_group;
  let thread_id : u32 = id.x % verts_per_patch;

  let base_index : u32 = group_id * verts_per_patch;

  let row : u32 = thread_id / 4;
  let col : u32 = thread_id % 4;

  var seg_lower : vec4<f32>;
  var seg_upper : vec4<f32>;
  var final_lower : vec4<f32>;
  var final_upper : vec4<f32>;

  control_points[thread_id + base_index] = vertices[id.x].pos;

  workgroupBarrier();

  // u-direction 2nd derivative (row-major, cols 0-1 only)
  if (col < 2)
  {
    let index: u32 = col + 4*row + base_index;
    d2b_u[index] =
      control_points[index]
      - (2 * control_points[index + 1])
      + control_points[index + 2];
  }

  workgroupBarrier();

  // upper/lower row SLEFE
  let u : f32 = f32(col) / 3.0f;
  seg_upper = (1 - u) * control_points[4*row + base_index]
            + u * control_points[4*row + 3 + base_index];
  seg_lower = seg_upper;

  for (var i: u32 = 0; i < 2; i++)
  {
    add_up_basis(
      &seg_upper,
      &seg_lower,
      d2b_u[4*row + i + base_index],
      slefe_upper_3_3[col + 4*i],
      slefe_lower_3_3[col + 4*i],
    );
  }

  upper[thread_id + base_index] = seg_upper;
  lower[thread_id + base_index] = seg_lower;

  workgroupBarrier();

  if (row < 2)
  {
    let index: u32 = col + 4*row + base_index;
    d2b_upper[index] = upper[index] - (2 * upper[index + 4]) + upper[index + 8];
    d2b_lower[index] = lower[index] - (2 * lower[index + 4]) + lower[index + 8];
  }

  workgroupBarrier();

  // upper SLEFE of upper
  let v : f32 = f32(row) / 3.0f;
  seg_upper = (1 - v) * upper[col + base_index]
            + v * upper[4*row + col + base_index];
  seg_lower = seg_upper;

  for (var i: u32 = 0; i < 2; i++)
  {
    add_up_basis(
      &seg_upper,
      &seg_lower,
      d2b_upper[4*i + col + base_index],
      slefe_upper_3_3[row + 4*i],
      slefe_lower_3_3[row + 4*i],
    );
  }

  final_upper = seg_upper;

  // lower SLEFE of lower
  seg_upper = (1 - v) * lower[col + base_index]
            + v * lower[4*row + col + base_index];
  seg_lower = seg_upper;

  for (var i: u32 = 0; i < 2; i++)
  {
    add_up_basis(
      &seg_upper,
      &seg_lower,
      d2b_lower[4*i + col + base_index],
      slefe_upper_3_3[row + 4*i],
      slefe_lower_3_3[row + 4*i],
    );
  }

  final_lower = seg_lower;

  workgroupBarrier();

  upper[thread_id + base_index] = final_upper;
  lower[thread_id + base_index] = final_lower;

  workgroupBarrier();

  var thread_max : f32 = 0.0;
  if (thread_id < 9)
  {
    let i : vec4<u32> = tile_indices[thread_id] + vec4u(base_index);

    let u1 = (upper[i.x] + upper[i.w]) / 2.0f;
    let u2 = (upper[i.y] + upper[i.z]) / 2.0f;

    let l1 = (lower[i.x] + lower[i.w]) / 2.0f;
    let l2 = (lower[i.y] + lower[i.z]) / 2.0f;

    var tile_upper : array<vec4<f32>, 2>;
    var tile_lower : array<vec4<f32>, 2>;

    tile_upper[0] = (u1 + u2) / 2.0f;
    tile_upper[1] = max(u1, u2);

    tile_lower[0] = min(l1, l2);
    tile_lower[1] = (l1 + l2) / 2.0f;

    let center_upper = vec4f(
      select(tile_upper[0], tile_upper[1], mvp[0][2] < 0.0).x,
      select(tile_upper[0], tile_upper[1], mvp[1][2] < 0.0).y,
      select(tile_upper[0], tile_upper[1], mvp[2][2] < 0.0).z,
      0.0
    );

    let center_lower = vec4f(
      select(tile_lower[0], tile_lower[1], mvp[0][2] < 0.0).x,
      select(tile_lower[0], tile_lower[1], mvp[1][2] < 0.0).y,
      select(tile_lower[0], tile_lower[1], mvp[2][2] < 0.0).z,
      0.0
    );

    var bb_min : vec3<f32>;
    var bb_max : vec3<f32>;
    calc_bounding_box(center_lower, center_upper, &bb_min, &bb_max);

    let p_error   : vec3<f32> = bb_max - bb_min;
    let max_error : f32       = max(p_error.x, max(p_error.y, p_error.z));
    thread_max = clamp(3 * sqrt(2 * max_error / pixel_size), 1.0f, 64.0f);
  }

  var bb_min : vec3<f32>;
  var bb_max : vec3<f32>;
  calc_bounding_box(final_lower, final_upper, &bb_min, &bb_max);

  let p_error   : vec3<f32> = bb_max - bb_min;
  let max_error : f32       = max(p_error.x, max(p_error.y, p_error.z));
  let vert_max  : f32       = clamp(3 * sqrt(2 * max_error / pixel_size), 1.0f, 64.0f);

  local_max[thread_id + base_index] = max(thread_max, vert_max);

  workgroupBarrier();

  if (thread_id == 0)
  {
    var m : f32 = 1.0f;
    for (var i : u32 = 0; i < verts_per_patch; i++) {
      m = max(m, local_max[base_index + i]);
    }
    patches[patch_id] = m;
  }
}