// Small 3D maths for the 3D drawing layer. Conventions (the same as glTF and
// OpenGL): right-handed, +Y up, a camera looks down -Z; matrices are
// column-major (m[column * 4 + row]) and multiply column vectors, so
// `proj * view * model * position`. Angles are radians.
#pragma once
#include <cmath>

namespace ote {

struct Vec3 {
  float x = 0, y = 0, z = 0;
  Vec3() = default;
  Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
  Vec3 operator+(Vec3 o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
  Vec3 operator-(Vec3 o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
  Vec3 operator*(float k) const { return Vec3(x * k, y * k, z * k); }
};
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(Vec3 a) {
  float l = length(a);
  return l > 1e-12f ? a * (1.0f / l) : Vec3(0, 0, 0);
}

struct Vec4 {
  float x = 0, y = 0, z = 0, w = 0;
  Vec4() = default;
  Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

// A rotation. (x, y, z, w) like glTF.
struct Quat {
  float x = 0, y = 0, z = 0, w = 1;
  Quat() = default;
  Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
  static Quat axisAngle(Vec3 axis, float angle) {
    Vec3 a = normalize(axis);
    float s = std::sin(angle * 0.5f);
    return Quat(a.x * s, a.y * s, a.z * s, std::cos(angle * 0.5f));
  }
};
inline Quat normalize(Quat q) {
  float l = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  return l > 1e-12f ? Quat(q.x / l, q.y / l, q.z / l, q.w / l) : Quat();
}
// Shortest-path blend between two rotations, t = 0..1 (for animation).
inline Quat nlerp(Quat a, Quat b, float t) {
  float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w, s = d < 0 ? -1.0f : 1.0f;
  return normalize(Quat(a.x + (b.x * s - a.x) * t, a.y + (b.y * s - a.y) * t, a.z + (b.z * s - a.z) * t,
                        a.w + (b.w * s - a.w) * t));
}

struct Mat4 {
  float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

  float& at(int row, int col) { return m[col * 4 + row]; }
  float at(int row, int col) const { return m[col * 4 + row]; }

  Mat4 operator*(const Mat4& o) const {
    Mat4 r;
    for (int c = 0; c < 4; ++c)
      for (int row = 0; row < 4; ++row) {
        float s = 0;
        for (int k = 0; k < 4; ++k) s += at(row, k) * o.at(k, c);
        r.at(row, c) = s;
      }
    return r;
  }
  Vec4 operator*(Vec4 v) const {
    return Vec4(m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w, m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
                m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w, m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w);
  }
  Vec3 point(Vec3 p) const {  // a position (w = 1), without the perspective divide
    Vec4 r = *this * Vec4(p.x, p.y, p.z, 1);
    return Vec3(r.x, r.y, r.z);
  }
  Vec3 direction(Vec3 d) const {  // a direction (w = 0)
    Vec4 r = *this * Vec4(d.x, d.y, d.z, 0);
    return Vec3(r.x, r.y, r.z);
  }

  static Mat4 translate(Vec3 t) {
    Mat4 r;
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
  }
  static Mat4 scale(Vec3 s) {
    Mat4 r;
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    return r;
  }
  static Mat4 rotate(Quat q) {
    q = normalize(q);
    float x = q.x, y = q.y, z = q.z, w = q.w;
    Mat4 r;
    r.m[0] = 1 - 2 * (y * y + z * z);
    r.m[1] = 2 * (x * y + z * w);
    r.m[2] = 2 * (x * z - y * w);
    r.m[4] = 2 * (x * y - z * w);
    r.m[5] = 1 - 2 * (x * x + z * z);
    r.m[6] = 2 * (y * z + x * w);
    r.m[8] = 2 * (x * z + y * w);
    r.m[9] = 2 * (y * z - x * w);
    r.m[10] = 1 - 2 * (x * x + y * y);
    return r;
  }
  // translation * rotation * scale: a glTF node's transform.
  static Mat4 trs(Vec3 t, Quat r, Vec3 s) { return translate(t) * rotate(r) * scale(s); }

  // A camera at `eye` looking at `target`.
  static Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) {
    Vec3 f = normalize(target - eye), s = normalize(cross(f, up)), u = cross(s, f);
    Mat4 r;
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;   r.m[12] = -dot(s, eye);
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;   r.m[13] = -dot(u, eye);
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z; r.m[14] = dot(f, eye);
    return r;
  }
  // fovY = full vertical angle. Depth ends up in -1 (near) .. 1 (far).
  static Mat4 perspective(float fovY, float aspect, float zNear, float zFar) {
    float f = 1.0f / std::tan(fovY * 0.5f);
    Mat4 r;
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (zFar + zNear) / (zNear - zFar);
    r.m[11] = -1;
    r.m[14] = 2 * zFar * zNear / (zNear - zFar);
    r.m[15] = 0;
    return r;
  }
  // A view without perspective (the isometric camera): `halfHeight` world
  // units from the middle of the picture to its top edge.
  static Mat4 ortho(float halfHeight, float aspect, float zNear, float zFar) {
    Mat4 r;
    r.m[0] = 1.0f / (halfHeight * aspect);
    r.m[5] = 1.0f / halfHeight;
    r.m[10] = 2.0f / (zNear - zFar);
    r.m[14] = (zFar + zNear) / (zNear - zFar);
    return r;
  }
};

}  // namespace ote
