#include "Elevation.h"

using Vertex3D = utils::Vertex3D;

namespace elevation {

std::vector<glm::vec4> elevate1D(const std::vector<glm::vec4>& pts)
{
  uint32_t m = static_cast<uint32_t>(pts.size()) - 1;
  std::vector<glm::vec4> result(m + 2);
  result[0] = pts[0];
  for (uint32_t i = 1; i <= m; i++) {
    float t  = static_cast<float>(i) / static_cast<float>(m + 1);
    result[i] = t * pts[i - 1] + (1.0f - t) * pts[i];
  }
  result[m + 1] = pts[m];
  return result;
}

std::vector<glm::vec4> elevateTo(const std::vector<glm::vec4>& pts, uint32_t targetDeg)
{
  std::vector<glm::vec4> cur = pts;
  while (static_cast<uint32_t>(cur.size()) - 1 < targetDeg)
    cur = elevate1D(cur);
  return cur;
}

std::vector<Vertex3D> elevatePatchFull(
  const std::vector<Vertex3D>& patch,
  uint32_t rows, uint32_t cols)
{
  std::vector<glm::vec4> pos_in(rows * cols);
  std::vector<glm::vec4> col_in(rows * cols);
  std::vector<glm::vec4> tex_in(rows * cols);
  for (uint32_t r = 0; r < rows; r++) {
    for (uint32_t c = 0; c < cols; c++) {
      const Vertex3D& v = patch[r * cols + c];
      pos_in[r * cols + c] = v.pos;
      col_in[r * cols + c] = v.color;
      tex_in[r * cols + c] = glm::vec4(v.tex, v._pad);
    }
  }

  // elevate in v direction
  std::vector<glm::vec4> pos1(rows * 4), col1(rows * 4), tex1(rows * 4);
  for (uint32_t r = 0; r < rows; r++) {
    std::vector<glm::vec4> rp(cols), rc(cols), rt(cols);
    for (uint32_t c = 0; c < cols; c++) {
      rp[c] = pos_in[r * cols + c];
      rc[c] = col_in[r * cols + c];
      rt[c] = tex_in[r * cols + c];
    }
    auto ep = elevateTo(rp, 3);
    auto ec = elevateTo(rc, 3);
    auto et = elevateTo(rt, 3);
    for (uint32_t c = 0; c < 4; c++) {
      pos1[r * 4 + c] = ep[c];
      col1[r * 4 + c] = ec[c];
      tex1[r * 4 + c] = et[c];
    }
  }

  // elevate in u direction
  std::vector<glm::vec4> pos2(16), col2(16), tex2(16);
  for (uint32_t c = 0; c < 4; c++) {
    std::vector<glm::vec4> cp(rows), cc(rows), ct(rows);
    for (uint32_t r = 0; r < rows; r++) {
      cp[r] = pos1[r * 4 + c];
      cc[r] = col1[r * 4 + c];
      ct[r] = tex1[r * 4 + c];
    }
    auto ep = elevateTo(cp, 3);
    auto ec = elevateTo(cc, 3);
    auto et = elevateTo(ct, 3);
    for (uint32_t r = 0; r < 4; r++) {
      pos2[r * 4 + c] = ep[r];
      col2[r * 4 + c] = ec[r];
      tex2[r * 4 + c] = et[r];
    }
  }

  std::vector<Vertex3D> result(16);
  for (uint32_t i = 0; i < 16; i++) {
    result[i].pos   = pos2[i];
    result[i].color = col2[i];
    result[i].tex   = glm::vec2(tex2[i].x, tex2[i].y);
    result[i]._pad  = glm::vec2(tex2[i].z, tex2[i].w);
  }
  return result;
}

std::vector<glm::vec4> elevatePatchPositions(
  const std::vector<Vertex3D>& patch,
  uint32_t rows, uint32_t cols)
{
  std::vector<glm::vec4> pos(rows * cols);
  for (uint32_t r = 0; r < rows; r++)
    for (uint32_t c = 0; c < cols; c++)
      pos[r * cols + c] = patch[r * cols + c].pos;

  std::vector<glm::vec4> pos1(rows * 4);
  for (uint32_t r = 0; r < rows; r++) {
    std::vector<glm::vec4> row(cols);
    for (uint32_t c = 0; c < cols; c++)
      row[c] = pos[r * cols + c];
    auto ep = elevateTo(row, 3);
    for (uint32_t c = 0; c < 4; c++)
      pos1[r * 4 + c] = ep[c];
  }

  std::vector<glm::vec4> pos2(16);
  for (uint32_t c = 0; c < 4; c++) {
    std::vector<glm::vec4> col(rows);
    for (uint32_t r = 0; r < rows; r++)
      col[r] = pos1[r * 4 + c];
    auto ep = elevateTo(col, 3);
    for (uint32_t r = 0; r < 4; r++)
      pos2[r * 4 + c] = ep[r];
  }

  return pos2;
}

} // namespace elevation
