// Skins, animation, posing and CPU skinning of glTF models (engine/gfx3d/model_anim.cpp, model_pose.cpp), using
// tests/fixtures/models/bend.glb (tools/art/make_bend_model.py) and damaged copies of it.
//
// The model: a column 3 units tall, three joints (root at the bottom, middle 1 up, tip 2 up). A node "base" moves
// everything by (1, 0, 2) and turns it a quarter turn about Y. In base-local coordinates a point (x, y, z) therefore
// ends up at (1 + z, y, 2 - x) in model space; the tests do their hand calculations in base-local space and convert
// with toModel().
#include "model_anim_fixture.h"

TEST(anim_loads_skin_and_clips) {
  Posed p;
  CHECK(p.ok);
  CHECK(p.m.skins.size() == 1);
  CHECK(p.root >= 0 && p.middle >= 0 && p.tip >= 0 && p.column >= 0);
  const ModelSkin& s = p.m.skins[0];
  CHECK(s.joints.size() == 3 && s.inverseBind.size() == 3);
  CHECK(s.joints[0] == p.root && s.joints[1] == p.middle && s.joints[2] == p.tip);
  CHECK(p.m.nodes[(size_t)p.column].skin == 0);
  CHECK(p.m.nodes[(size_t)nodeNamed(p.m, "base")].skin == -1);
  // Inverse bind times the joint's rest matrix is the identity, and the root's is the inverse of base's.
  for (int j = 0; j < 3; ++j) {
    Mat4 id = s.inverseBind[(size_t)j] * p.m.nodeMatrix(s.joints[(size_t)j]);
    for (int i = 0; i < 16; ++i) CHECK(near(id.m[i], i % 5 == 0 ? 1.0f : 0.0f, 1e-4f));
  }
  CHECK(near(s.inverseBind[2].m[13], -2.0f, 1e-4f));  // base-local y 2, unaffected by the quarter turn about Y

  CHECK(p.m.clips.size() == 3);  // the "unused" node's channel is skipped, not the clip
  const ModelClip& bend = p.m.clips[(size_t)p.m.findClip("Bend")];
  const ModelClip& hop = p.m.clips[(size_t)p.m.findClip("Hop")];
  const ModelClip& spline = p.m.clips[(size_t)p.m.findClip("Spline")];
  CHECK(near(bend.duration, 1) && bend.tracks.size() == 2);
  CHECK(near(hop.duration, 1) && hop.tracks.size() == 2);
  CHECK(near(spline.duration, 2) && spline.tracks.size() == 1);
  CHECK(bend.tracks[0].path == ModelTrack::Rotation && bend.tracks[0].times.size() == 3 && bend.tracks[0].values.size() == 12);
  CHECK(!bend.tracks[0].step && bend.tracks[0].node == p.middle);
  CHECK(hop.tracks[1].path == ModelTrack::Scale && hop.tracks[1].step && hop.tracks[1].node == p.column);
  // Cubic spline: only the value of each (in-tangent, value, out-tangent) triple is kept, and it blends linearly.
  CHECK(spline.tracks[0].times.size() == 3 && spline.tracks[0].values.size() == 12 && !spline.tracks[0].step);
  CHECK(near(spline.tracks[0].values[4 + 2], std::sin(rad(80) / 2), 1e-5f));
  CHECK(p.m.animated());
}

TEST(anim_part_weights_and_bounds) {
  Posed p;
  CHECK(p.ok);
  CHECK(p.m.parts.size() == 1);
  const ModelPart& pt = p.m.parts[0];
  CHECK(pt.skinned());
  const size_t n = pt.mesh.vertices.size();
  CHECK(n == 44);
  CHECK(pt.joints.size() == n * 4 && pt.weights.size() == n * 4);
  for (size_t v = 0; v < n; ++v) {
    float sum = 0;
    for (size_t k = 0; k < 4; ++k) {
      CHECK(pt.joints[v * 4 + k] < 3);
      CHECK(pt.weights[v * 4 + k] >= 0);
      sum += pt.weights[v * 4 + k];
    }
    CHECK(near(sum, 1, 1e-5f));
    Vec3 l = toLocal(Vec3(pt.mesh.vertices[v].x, pt.mesh.vertices[v].y, pt.mesh.vertices[v].z));
    if (near(l.y, 0)) CHECK(pt.joints[v * 4] == 0 && pt.weights[v * 4] > 0.999f);          // bottom: root only
    if (near(l.y, 3)) CHECK(pt.joints[v * 4] == 2 && pt.weights[v * 4] > 0.999f);          // top: tip only
    if (near(l.y, 0.75f)) CHECK(pt.weights[v * 4] > 0.2f && pt.weights[v * 4 + 1] > 0.2f);  // between root and middle
  }
  // Bounds are the rest pose's, with the skinned vertices taken as they are (they are in model space).
  CHECK(near(p.m.boundsMin.y, 0) && near(p.m.boundsMax.y, 3));
  CHECK(near(p.m.boundsMin.x, 0.8f) && near(p.m.boundsMax.x, 1.2f));
  CHECK(near(p.m.boundsMin.z, 1.8f) && near(p.m.boundsMax.z, 2.2f));
}

TEST(anim_pose_at_rest_is_node_matrix) {
  Posed p;
  CHECK(p.ok);
  const int clips[] = {-1, -5, 3, 99};  // none, and indices that do not exist
  for (int c : clips) {
    poseModel(p.m, c, 0.7f, true, p.pose);
    CHECK(p.pose.world.size() == p.m.nodes.size());
    for (size_t i = 0; i < p.m.nodes.size(); ++i) {
      Mat4 want = p.m.nodeMatrix((int)i);
      for (int k = 0; k < 16; ++k) CHECK(near(p.pose.world[i].m[k], want.m[k], 1e-5f));
    }
  }
}

TEST(anim_pose_samples_keys) {
  Posed p;
  CHECK(p.ok);
  // Bend: middle turns 0 / 45 / 90 degrees at 0 / .5 / 1 s, tip 0 / 30 / 60 degrees.
  p.at("Bend", 0);
  CHECK(near(p.tipPos().x, 1, 1e-4f) && near(p.tipPos().y, 2, 1e-4f) && near(p.tipPos().z, 2, 1e-4f));
  const struct { float t; float a1, a2; } cases[] = {
      {0.0f, 0, 0}, {0.5f, 45, 30}, {1.0f, 90, 60}, {0.25f, 22.5f, 15}, {0.75f, 67.5f, 45},
      {-3.0f, 0, 0},   // before the first key: the first value
      {7.0f, 90, 60},  // after the last: the last value
  };
  for (const auto& c : cases) {
    p.at("Bend", c.t);
    Vec3 want = expectedTip(rad(c.a1));
    // The tip joint sits 1 above the middle one in the middle's turned frame: (0,1,0) -> Rz(a1)(0,1,0).
    CHECK(near(p.tipPos().x, want.x) && near(p.tipPos().y, want.y) && near(p.tipPos().z, want.z));
    CHECK(p.xAxisIs(p.tip, rad(c.a1 + c.a2)));
    CHECK(p.xAxisIs(p.middle, rad(c.a1)));
  }
  // Looping wraps the time by the duration (1 s).
  p.at("Bend", 1.25f, true);
  CHECK(p.xAxisIs(p.middle, rad(22.5f)));
  p.at("Bend", 2.5f, true);
  CHECK(p.xAxisIs(p.middle, rad(45)));
  p.at("Bend", -0.75f, true);
  CHECK(p.xAxisIs(p.middle, rad(22.5f)));
  p.at("Bend", 1.25f, false);  // not looping: held at the end
  CHECK(p.xAxisIs(p.middle, rad(90)));
  // Zero duration: always time 0.
  Model3D flat = p.m;
  for (ModelClip& c : flat.clips) c.duration = 0;
  poseModel(flat, flat.findClip("Bend"), 0.6f, true, p.pose);
  CHECK(p.xAxisIs(p.middle, 0));
  // Nodes the clip does not touch keep their rest values.
  p.at("Bend", 0.5f);
  Mat4 rest = p.m.nodeMatrix(p.root);
  for (int k = 0; k < 16; ++k) CHECK(near(p.pose.world[(size_t)p.root].m[k], rest.m[k], 1e-5f));
}

TEST(anim_pose_translation_step_and_spline) {
  Posed p;
  CHECK(p.ok);
  // Hop: the root rises 0 / 0.5 / 0 over 1 s.
  p.at("Hop", 0.25f);
  CHECK(near(p.pose.world[(size_t)p.root].m[13], 0.25f));
  CHECK(near(p.pose.world[(size_t)p.tip].m[13], 2.25f));
  p.at("Hop", 0.5f);
  CHECK(near(p.pose.world[(size_t)p.root].m[13], 0.5f));
  p.at("Hop", 1.5f, true);
  CHECK(near(p.pose.world[(size_t)p.root].m[13], 0.5f));
  // The scale of "column" is a STEP track: (1,1,1) from 0, (1,2,1) from .25, (1,1,3) from .75.
  const float t[] = {0.0f, 0.1f, 0.25f, 0.5f, 0.74f, 0.75f, 0.9f, 4.0f};
  const float sy[] = {1, 1, 2, 2, 2, 1, 1, 1}, sz[] = {1, 1, 1, 1, 1, 3, 3, 3};
  for (size_t i = 0; i < 8; ++i) {
    p.at("Hop", t[i]);
    const Mat4& w = p.pose.world[(size_t)p.column];
    CHECK(near(std::sqrt(w.m[4] * w.m[4] + w.m[5] * w.m[5] + w.m[6] * w.m[6]), sy[i]));
    CHECK(near(std::sqrt(w.m[8] * w.m[8] + w.m[9] * w.m[9] + w.m[10] * w.m[10]), sz[i]));
  }
  // Spline: the middle joint turns 0 / 80 / -40 degrees at 0 / 1 / 2 s (tangents are ignored).
  const struct { float t, a; } sp[] = {{0, 0}, {0.5f, 40}, {1, 80}, {1.5f, 20}, {2, -40}, {9, -40}};
  for (const auto& c : sp) {
    p.at("Spline", c.t);
    CHECK(p.xAxisIs(p.middle, rad(c.a)));
  }
}

TEST(anim_skin_part_at_rest_and_bent) {
  Posed p;
  CHECK(p.ok);
  const ModelPart& pt = p.m.parts[0];
  MeshData out;
  p.at("Bend", -1);  // before the first key = rest values (the clip is at its first key)
  skinPart(p.m, p.pose, p.column, 0, out);
  CHECK(out.vertices.size() == pt.mesh.vertices.size() && out.indices == pt.mesh.indices);
  for (size_t v = 0; v < out.vertices.size(); ++v) {
    const Vertex3D &a = out.vertices[v], &b = pt.mesh.vertices[v];
    CHECK(near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z));
    CHECK(near(a.nx, b.nx) && near(a.ny, b.ny) && near(a.nz, b.nz));
  }
  poseModel(p.m, -1, 0, false, p.pose);
  skinPart(p.m, p.pose, p.column, 0, out);
  for (size_t v = 0; v < out.vertices.size(); ++v)
    CHECK(near(out.vertices[v].x, pt.mesh.vertices[v].x) && near(out.vertices[v].z, pt.mesh.vertices[v].z));

  // Bent to the end (middle 90, tip 60 degrees): bottom vertices stay, top vertices follow the tip by hand.
  p.at("Bend", 1);
  skinPart(p.m, p.pose, p.column, 0, out);
  const float a1 = rad(90), a2 = rad(60);
  int bottom = 0, top = 0;
  for (size_t v = 0; v < out.vertices.size(); ++v) {
    const Vertex3D &a = out.vertices[v], &r = pt.mesh.vertices[v];
    Vec3 l = toLocal(Vec3(r.x, r.y, r.z));
    CHECK(near(std::sqrt(a.nx * a.nx + a.ny * a.ny + a.nz * a.nz), 1, 1e-4f));
    CHECK(a.u == r.u && a.r == r.r && a.a == r.a);  // the other fields are copied
    if (near(l.y, 0)) {
      ++bottom;
      CHECK(near(a.x, r.x) && near(a.y, r.y) && near(a.z, r.z));
    }
    if (near(l.y, 3)) {
      ++top;
      Vec3 q = rotZ(Vec3(l.x, 1, l.z), a2) + Vec3(0, 1, 0);  // relative to the tip joint, then to the middle joint
      q = rotZ(q, a1) + Vec3(0, 1, 0);
      Vec3 want = toModel(q.x, q.y, q.z);
      CHECK(near(a.x, want.x) && near(a.y, want.y) && near(a.z, want.z));
      if (near(r.ny, 1)) CHECK(near(a.nx, 0) && near(a.ny, std::cos(a1 + a2)) && near(a.nz, std::sin(a1 + a2)));
    }
  }
  CHECK(bottom == 8 && top == 8);  // a ring and a cap each

  // Hop: everything lifted by the root.
  p.at("Hop", 0.5f);
  skinPart(p.m, p.pose, p.column, 0, out);
  for (size_t v = 0; v < out.vertices.size(); ++v) {
    CHECK(near(out.vertices[v].y, pt.mesh.vertices[v].y + 0.5f, 1e-3f));
    CHECK(near(out.vertices[v].x, pt.mesh.vertices[v].x, 1e-3f));
  }
}

TEST(anim_skin_part_guards_bad_indices) {
  Posed p;
  CHECK(p.ok);
  p.at("Bend", 0.5f);
  MeshData out;
  out.vertices.resize(5);
  skinPart(p.m, p.pose, p.column, 7, out);  // no such part: empty
  CHECK(out.vertices.empty() && out.indices.empty());
  skinPart(p.m, p.pose, p.column, -1, out);
  CHECK(out.vertices.empty());
  // A node that has no skin, or does not exist: a copy of the rest mesh.
  const int bad[] = {-1, 99, nodeNamed(p.m, "base")};
  for (int node : bad) {
    skinPart(p.m, p.pose, node, 0, out);
    CHECK(out.vertices.size() == p.m.parts[0].mesh.vertices.size());
    CHECK(near(out.vertices[5].x, p.m.parts[0].mesh.vertices[5].x) && near(out.vertices[5].y, p.m.parts[0].mesh.vertices[5].y));
  }
  // A pose of the wrong size.
  ModelPose empty;
  skinPart(p.m, empty, p.column, 0, out);
  CHECK(out.vertices.size() == p.m.parts[0].mesh.vertices.size());
  // Damaged data inside the model: a joint index out of range and a node that is not there.
  Model3D m = p.m;
  m.parts[0].joints[8] = 40;
  m.skins[0].joints[1] = 123;
  skinPart(m, p.pose, p.column, 0, out);
  CHECK(out.vertices.size() == m.parts[0].mesh.vertices.size());
  m.nodes[(size_t)p.column].skin = 9;
  skinPart(m, p.pose, p.column, 0, out);
  CHECK(out.vertices.size() == m.parts[0].mesh.vertices.size());
}

TEST(anim_split_keeps_joints_and_weights) {
  Posed whole;
  CHECK(whole.ok);
  Caps3D caps;
  caps.maxMeshVertices = 12;
  Model3D split;
  CHECK(loadBend(split, caps));
  CHECK(split.parts.size() > 3);
  const int col = nodeNamed(split, "column");
  CHECK(split.nodes[(size_t)col].parts.size() == split.parts.size());
  ModelPose pose;
  poseModel(split, split.findClip("Bend"), 0.7f, false, pose);
  whole.at("Bend", 0.7f);
  MeshData full;
  skinPart(whole.m, whole.pose, whole.column, 0, full);
  size_t total = 0;
  for (size_t pi = 0; pi < split.parts.size(); ++pi) {
    const ModelPart& pt = split.parts[pi];
    CHECK(pt.mesh.vertices.size() <= 12);
    CHECK(pt.joints.size() == pt.mesh.vertices.size() * 4 && pt.weights.size() == pt.joints.size());
    MeshData out;
    skinPart(split, pose, col, (int)pi, out);
    for (size_t v = 0; v < out.vertices.size(); ++v) {
      ++total;
      // The same rest vertex (position and normal) in the unsplit part must land in the same place.
      const Vertex3D& r = pt.mesh.vertices[v];
      bool found = false;
      for (size_t w = 0; w < whole.m.parts[0].mesh.vertices.size() && !found; ++w) {
        const Vertex3D& q = whole.m.parts[0].mesh.vertices[w];
        if (!(near(q.x, r.x) && near(q.y, r.y) && near(q.z, r.z) && near(q.nx, r.nx) && near(q.ny, r.ny) && near(q.nz, r.nz))) continue;
        found = true;
        CHECK(near(full.vertices[w].x, out.vertices[v].x) && near(full.vertices[w].y, out.vertices[v].y) &&
              near(full.vertices[w].z, out.vertices[v].z));
      }
      CHECK(found);
    }
  }
  CHECK(total >= 44);
}

TEST(anim_refuses_and_skips_damaged_content) {
  std::vector<uint8_t> good = readFixture();
  CHECK(good.size() > 1000);
  Model3D m;
  std::string err;
  CHECK(loadBytes(good, m, err));

  // A joint that is not in the scene (node 5): the file is refused, in plain words.
  std::vector<uint8_t> b = good;
  CHECK(patch(b, "\"joints\":[1,2,3]", "\"joints\":[1,2,5]"));
  CHECK(!loadBytes(b, m, err));
  CHECK(err.find("not part of the scene") != std::string::npos);
  // A joint index in the vertices beyond the skin's joint count.
  b = good;
  CHECK(patch(b, "\"joints\":[1,2,3]", "\"joints\":[1,2  ]"));
  CHECK(!loadBytes(b, m, err));
  // A clip without a name is called "Animation N".
  b = good;
  CHECK(patch(b, "\"name\":\"Hop\"", "\"nome\":\"Hop\""));
  CHECK(loadBytes(b, m, err));
  CHECK(m.clips.size() == 3 && m.clips[1].name == "Animation 2");
  // A channel for a path that is not known is skipped; the clip survives with its other channel.
  b = good;
  CHECK(patch(b, "\"path\":\"scale\"", "\"path\":\"sca1e\""));
  CHECK(loadBytes(b, m, err));
  CHECK(m.clips[(size_t)m.findClip("Hop")].tracks.size() == 1);
  // A sampler that cannot be used (unknown interpolation) loses its channel; a clip left empty is dropped.
  b = good;
  CHECK(patch(b, "\"interpolation\":\"CUBICSPLINE\"", "\"interpolation\":\"CUBICSPLINX\""));
  CHECK(loadBytes(b, m, err));
  CHECK(m.clips.size() == 2 && m.findClip("Spline") < 0);
}

